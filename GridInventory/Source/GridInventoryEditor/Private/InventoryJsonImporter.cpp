// Copyright Marco. All Rights Reserved.

#include "InventoryJsonImporter.h"
#include "InventoryItemDefinition.h"
#include "IngredientSpawnData.h"
#include "ItemEffectValue.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "HAL/PlatformFilemanager.h"
#include "Internationalization/Regex.h"

// ============================================================================
// Console Command
// ============================================================================

FAutoConsoleCommand* UInventoryJsonImporter::ImportCommand = nullptr;

void UInventoryJsonImporter::RegisterConsoleCommands()
{
	if (!ImportCommand)
	{
		ImportCommand = new FAutoConsoleCommand(
			TEXT("Inv.ImportJSON"),
			TEXT("Import alchemy ingredients from JSON. Usage: Inv.ImportJSON <path> [output_base_path]"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&UInventoryJsonImporter::HandleImportCommand)
		);
	}
}

void UInventoryJsonImporter::UnregisterConsoleCommands()
{
	if (ImportCommand)
	{
		delete ImportCommand;
		ImportCommand = nullptr;
	}
}

void UInventoryJsonImporter::HandleImportCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[JsonImporter] Usage: Inv.ImportJSON <path_to_data.json> [output_base_path]"));
		UE_LOG(LogTemp, Warning, TEXT("[JsonImporter] Example: Inv.ImportJSON C:/Game/data.json"));
		UE_LOG(LogTemp, Warning, TEXT("[JsonImporter] Example: Inv.ImportJSON C:/Game/data.json /Game/Items/Zutaten"));
		return;
	}

	const FString JsonPath = Args[0];
	const FString OutputBase = Args.Num() > 1 ? Args[1] : TEXT("/Game/Items/AlchemieZutaten");

	const int32 Count = ImportFromJsonFile(JsonPath, OutputBase);
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Import abgeschlossen: %d Items erstellt/aktualisiert."), Count);
}

// ============================================================================
// Main Import Function
// ============================================================================

int32 UInventoryJsonImporter::ImportFromJsonFile(const FString& JsonFilePath, const FString& OutputBasePath)
{
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Starte Import von: %s"), *JsonFilePath);
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Zielordner: %s"), *OutputBasePath);

	// ---- Load JSON file ----
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[JsonImporter] Konnte Datei nicht laden: %s"), *JsonFilePath);
		return 0;
	}

	// ---- Parse JSON ----
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[JsonImporter] JSON Parse-Fehler in: %s"), *JsonFilePath);
		return 0;
	}

	// ---- Find categories ----
	const TArray<TSharedPtr<FJsonValue>>* CategoriesArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("categories"), CategoriesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[JsonImporter] Kein 'categories' Array im JSON gefunden."));
		return 0;
	}

	int32 TotalImported = 0;

	for (const TSharedPtr<FJsonValue>& CatValue : *CategoriesArray)
	{
		const TSharedPtr<FJsonObject>& CatObject = CatValue->AsObject();
		if (!CatObject.IsValid()) continue;

		// Only process "Alchemie Zutaten"
		FString SuperCat;
		CatObject->TryGetStringField(TEXT("supercat"), SuperCat);
		if (SuperCat != TEXT("Alchemie Zutaten")) continue;

		FString CategoryName;
		CatObject->TryGetStringField(TEXT("name"), CategoryName);

		const FString SafeCategoryName = SanitizeCategoryName(CategoryName);
		const FString CategoryPath = OutputBasePath / SafeCategoryName;

		UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Kategorie: %s -> %s"), *CategoryName, *CategoryPath);

		// ---- Process posts (items) ----
		const TArray<TSharedPtr<FJsonValue>>* PostsArray = nullptr;
		if (!CatObject->TryGetArrayField(TEXT("posts"), PostsArray)) continue;

		for (const TSharedPtr<FJsonValue>& PostValue : *PostsArray)
		{
			const TSharedPtr<FJsonObject>& PostObject = PostValue->AsObject();
			if (!PostObject.IsValid()) continue;

			FString ItemTitle;
			PostObject->TryGetStringField(TEXT("title"), ItemTitle);

			FString HtmlContent;
			PostObject->TryGetStringField(TEXT("content"), HtmlContent);

			if (ItemTitle.IsEmpty() || HtmlContent.IsEmpty()) continue;

			// ---- Parse HTML table values ----
			TArray<FString> TableValues = ParseTableValues(HtmlContent);

			// Need at least 13 columns (Name + 12 effect columns)
			if (TableValues.Num() < 13)
			{
				UE_LOG(LogTemp, Warning, TEXT("[JsonImporter] %s: Nur %d Tabellenwerte (13 erwartet), ueberspringe."),
					*ItemTitle, TableValues.Num());
				continue;
			}

			// ---- Create DataAsset ----
			const FString SafeItemName = SanitizeAssetName(ItemTitle);
			const FString AssetName = FString::Printf(TEXT("DA_%s"), *SafeItemName);
			const FString FullPackagePath = CategoryPath / AssetName;

			UInventoryItemDefinition* ItemDef = CreateOrOverwriteAsset(FullPackagePath, AssetName);
			if (!ItemDef)
			{
				UE_LOG(LogTemp, Error, TEXT("[JsonImporter] Konnte Asset nicht erstellen: %s"), *FullPackagePath);
				continue;
			}

			// ========================
			// Set Basic Properties
			// ========================

			ItemDef->ItemID = FName(*SafeItemName);
			ItemDef->DisplayName = FText::FromString(ItemTitle);
			ItemDef->Description = FText::FromString(ParseDescription(HtmlContent));
			ItemDef->ItemType = FName(TEXT("AlchemyIngredient"));
			ItemDef->CategoryTag = FName(*SafeCategoryName);

			// Grid: All ingredients are 1x1, stackable
			ItemDef->GridSize = FIntPoint(1, 1);
			ItemDef->bCanRotate = false;
			ItemDef->bCanStack = true;
			ItemDef->MaxStackSize = 99;
			ItemDef->Weight = 0.1f;

			// Not mergeable, not container
			ItemDef->bCanMergeUpgrade = false;
			ItemDef->bIsContainer = false;

			// ========================
			// Parse Effect Values from Table
			// ========================
			// Column layout:
			// [0] Name  [1] Wirkung  [2] HP  [3] Mana  [4] Stamina
			// [5] Lebenskraft  [6] Geschick  [7] Staerke  [8] Willenskraft
			// [9] SchutzVor  [10] Gegengift  [11] Sonstiges  [12] Dauer

			ItemDef->BaseEffects.Empty();

			// Wirkung -> WirkungTag
			const FString& Wirkung = TableValues[1];
			if (Wirkung != TEXT("-") && !Wirkung.IsEmpty())
			{
				ItemDef->WirkungTag = FName(*Wirkung);
			}
			else
			{
				ItemDef->WirkungTag = NAME_None;
			}

			// Numeric effects -> BaseEffects array
			struct FEffectMapping
			{
				int32 ColumnIndex;
				FName EffectID;
			};

			const FEffectMapping EffectMappings[] =
			{
				{ 2,  FName(TEXT("HP")) },
				{ 3,  FName(TEXT("Mana")) },
				{ 4,  FName(TEXT("Stamina")) },
				{ 5,  FName(TEXT("Lebenskraft")) },
				{ 6,  FName(TEXT("Geschick")) },
				{ 7,  FName(TEXT("Staerke")) },
				{ 8,  FName(TEXT("Willenskraft")) },
			};

			for (const FEffectMapping& Mapping : EffectMappings)
			{
				if (Mapping.ColumnIndex < TableValues.Num())
				{
					const float Value = ParseNumericValue(TableValues[Mapping.ColumnIndex]);
					if (Value != 0.0f)
					{
						ItemDef->BaseEffects.Add(FItemEffectValue(Mapping.EffectID, Value));
					}
				}
			}

			// String effects
			if (TableValues.Num() > 9)
			{
				const FString& Schutz = TableValues[9];
				ItemDef->SchutzVor = (Schutz != TEXT("-") && !Schutz.IsEmpty()) ? Schutz : TEXT("");
			}

			if (TableValues.Num() > 10)
			{
				const FString& Gift = TableValues[10];
				ItemDef->Gegengift = (Gift != TEXT("-") && !Gift.IsEmpty()) ? Gift : TEXT("");
			}

			if (TableValues.Num() > 11)
			{
				const FString& Sonst = TableValues[11];
				ItemDef->Sonstiges = (Sonst != TEXT("-") && !Sonst.IsEmpty()) ? Sonst : TEXT("");
			}

			// Duration
			if (TableValues.Num() > 12)
			{
				ItemDef->DurationMinutes = ParseDuration(TableValues[12]);
			}

			// Consumable: Items with Wirkung = "Heilung" or HP/Mana/Stamina effects
			const bool bHasHealEffect = (Wirkung == TEXT("Heilung") || Wirkung == TEXT("Mana") || Wirkung == TEXT("Stamina"));
			ItemDef->bIsConsumable = bHasHealEffect;
			ItemDef->bDestroyOnConsume = bHasHealEffect;

			// ========================
			// Parse Spawn Data
			// ========================

			const TSharedPtr<FJsonObject>* SpawnObject = nullptr;
			if (PostObject->TryGetObjectField(TEXT("spawn"), SpawnObject) && SpawnObject->IsValid())
			{
				FIngredientSpawnData& Spawn = ItemDef->SpawnData;
				Spawn.bHasSpawnData = true;

				// Biomes
				Spawn.Biomes.Empty();
				const TArray<TSharedPtr<FJsonValue>>* BiomeArray = nullptr;
				if ((*SpawnObject)->TryGetArrayField(TEXT("biome"), BiomeArray))
				{
					for (const TSharedPtr<FJsonValue>& BiomeVal : *BiomeArray)
					{
						Spawn.Biomes.Add(FName(*BiomeVal->AsString()));
					}
				}

				// Light
				double TempDouble = 0;
				if ((*SpawnObject)->TryGetNumberField(TEXT("licht_min"), TempDouble))
					Spawn.LichtMin = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("licht_max"), TempDouble))
					Spawn.LichtMax = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));

				// Soil color
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_r_min"), TempDouble))
					Spawn.BodenR_Min = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_r_max"), TempDouble))
					Spawn.BodenR_Max = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_g_min"), TempDouble))
					Spawn.BodenG_Min = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_g_max"), TempDouble))
					Spawn.BodenG_Max = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_b_min"), TempDouble))
					Spawn.BodenB_Min = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
				if ((*SpawnObject)->TryGetNumberField(TEXT("boden_b_max"), TempDouble))
					Spawn.BodenB_Max = static_cast<uint8>(FMath::Clamp((int32)TempDouble, 0, 255));
			}
			else
			{
				ItemDef->SpawnData.bHasSpawnData = false;
			}

			// ========================
			// Save Asset
			// ========================

			ItemDef->MarkPackageDirty();

			UPackage* Package = ItemDef->GetOutermost();
			const FString PackageFilename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());

			// UE 4.27 SavePackage signature (without FSavePackageArgs)
			const bool bSaved = UPackage::SavePackage(Package, ItemDef,
				RF_Public | RF_Standalone, *PackageFilename);

			if (bSaved)
			{
				UE_LOG(LogTemp, Log, TEXT("[JsonImporter]   + %s (%d Effects, Wirkung=%s, Dauer=%.1f Min)"),
					*ItemTitle,
					ItemDef->BaseEffects.Num(),
					*ItemDef->WirkungTag.ToString(),
					ItemDef->DurationMinutes);
				TotalImported++;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[JsonImporter]   ! %s konnte nicht gespeichert werden."), *ItemTitle);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] ========================================"));
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Import abgeschlossen: %d / 138 Items"), TotalImported);
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] Assets in: %s"), *OutputBasePath);
	UE_LOG(LogTemp, Log, TEXT("[JsonImporter] ========================================"));

	return TotalImported;
}

// ============================================================================
// HTML Parsing
// ============================================================================

TArray<FString> UInventoryJsonImporter::ParseTableValues(const FString& HtmlContent)
{
	TArray<FString> Values;

	// Find all <td>...</td> content (second row of the table = data row)
	// The first row is headers (<th>), we want <td> only
	const FRegexPattern Pattern(TEXT("<td>(.*?)</td>"));
	FRegexMatcher Matcher(Pattern, HtmlContent);

	while (Matcher.FindNext())
	{
		FString Value = Matcher.GetCaptureGroup(1);
		// Strip any nested HTML tags from the value
		FRegexPattern StripPattern(TEXT("<[^>]*>"));
		Value = FRegexMatcher(StripPattern, Value).GetCaptureGroup(0).IsEmpty()
			? Value : Value; // Simple approach: just use raw value

		// Actually strip HTML tags properly
		FString Clean;
		bool bInTag = false;
		for (const TCHAR Char : Value)
		{
			if (Char == '<') { bInTag = true; continue; }
			if (Char == '>') { bInTag = false; continue; }
			if (!bInTag) Clean.AppendChar(Char);
		}

		Values.Add(Clean.TrimStartAndEnd());
	}

	return Values;
}

FString UInventoryJsonImporter::ParseDescription(const FString& HtmlContent)
{
	FString Description;

	// Extract text from all <p>...</p> tags
	const FRegexPattern Pattern(TEXT("<p>(.*?)</p>"));
	FRegexMatcher Matcher(Pattern, HtmlContent);

	while (Matcher.FindNext())
	{
		FString ParagraphText = Matcher.GetCaptureGroup(1);

		// Strip HTML tags
		FString Clean;
		bool bInTag = false;
		for (const TCHAR Char : ParagraphText)
		{
			if (Char == '<') { bInTag = true; continue; }
			if (Char == '>') { bInTag = false; continue; }
			if (!bInTag) Clean.AppendChar(Char);
		}

		if (!Description.IsEmpty())
		{
			Description += TEXT("\n\n");
		}
		Description += Clean.TrimStartAndEnd();
	}

	return Description;
}

// ============================================================================
// Value Parsing
// ============================================================================

float UInventoryJsonImporter::ParseDuration(const FString& DurationStr)
{
	FString Trimmed = DurationStr.TrimStartAndEnd();

	// "-" or empty = 0 (permanent)
	if (Trimmed == TEXT("-") || Trimmed.IsEmpty())
	{
		return 0.0f;
	}

	// Try to extract number from strings like "2 Min", "5 Min", "10 Min"
	const FRegexPattern Pattern(TEXT("(\\d+\\.?\\d*)\\s*[Mm]in"));
	FRegexMatcher Matcher(Pattern, Trimmed);

	if (Matcher.FindNext())
	{
		return FCString::Atof(*Matcher.GetCaptureGroup(1));
	}

	// If it's just a number, assume minutes
	if (Trimmed.IsNumeric())
	{
		return FCString::Atof(*Trimmed);
	}

	// Non-numeric strings like "Hochentzuendlich", "Brennbar" etc.
	// These are not durations but got placed in the Dauer column
	// Return 0 (permanent)
	return 0.0f;
}

float UInventoryJsonImporter::ParseNumericValue(const FString& ValueStr)
{
	FString Trimmed = ValueStr.TrimStartAndEnd();

	// "-" or empty = 0
	if (Trimmed == TEXT("-") || Trimmed.IsEmpty())
	{
		return 0.0f;
	}

	// Try direct conversion (handles "225", "-100", "1.5" etc.)
	if (Trimmed.IsNumeric() || (Trimmed.StartsWith(TEXT("-")) && Trimmed.Mid(1).IsNumeric()))
	{
		return FCString::Atof(*Trimmed);
	}

	// Extract first number from mixed strings like "+10%"
	const FRegexPattern Pattern(TEXT("(-?\\d+\\.?\\d*)"));
	FRegexMatcher Matcher(Pattern, Trimmed);

	if (Matcher.FindNext())
	{
		return FCString::Atof(*Matcher.GetCaptureGroup(1));
	}

	return 0.0f;
}

// ============================================================================
// Name Sanitization
// ============================================================================

FString UInventoryJsonImporter::SanitizeAssetName(const FString& Name)
{
	FString Result = Name;

	// Replace German umlauts
	Result = Result.Replace(TEXT("\u00e4"), TEXT("ae"));
	Result = Result.Replace(TEXT("\u00f6"), TEXT("oe"));
	Result = Result.Replace(TEXT("\u00fc"), TEXT("ue"));
	Result = Result.Replace(TEXT("\u00c4"), TEXT("Ae"));
	Result = Result.Replace(TEXT("\u00d6"), TEXT("Oe"));
	Result = Result.Replace(TEXT("\u00dc"), TEXT("Ue"));
	Result = Result.Replace(TEXT("\u00df"), TEXT("ss"));

	// Replace spaces and special characters
	Result = Result.Replace(TEXT(" "), TEXT("_"));
	Result = Result.Replace(TEXT("/"), TEXT("_"));
	Result = Result.Replace(TEXT("'"), TEXT(""));
	Result = Result.Replace(TEXT("("), TEXT(""));
	Result = Result.Replace(TEXT(")"), TEXT(""));
	Result = Result.Replace(TEXT("-"), TEXT("_"));
	Result = Result.Replace(TEXT("."), TEXT(""));
	Result = Result.Replace(TEXT(","), TEXT(""));

	// Remove any remaining non-alphanumeric characters (except underscore)
	FString Clean;
	for (const TCHAR Char : Result)
	{
		if (FChar::IsAlnum(Char) || Char == '_')
		{
			Clean.AppendChar(Char);
		}
	}

	return Clean;
}

FString UInventoryJsonImporter::SanitizeCategoryName(const FString& CategoryName)
{
	FString Result = SanitizeAssetName(CategoryName);

	// Short readable folder names
	if (CategoryName.Contains(TEXT("Basis"))) return TEXT("BasisZutaten");
	if (CategoryName.Contains(TEXT("Pflanzen"))) return TEXT("PflanzenKraeuter");
	if (CategoryName.Contains(TEXT("Pilze"))) return TEXT("Pilze");
	if (CategoryName.Contains(TEXT("Beeren"))) return TEXT("BeerenFruechte");
	if (CategoryName.Contains(TEXT("Mineral"))) return TEXT("MineralienGesteine");
	if (CategoryName.Contains(TEXT("Organ"))) return TEXT("Organisches");
	if (CategoryName.Contains(TEXT("Magisch"))) return TEXT("MagischeZutaten");
	if (CategoryName.Contains(TEXT("Sonst"))) return TEXT("Sonstiges");

	return Result;
}

// ============================================================================
// Asset Creation
// ============================================================================

UInventoryItemDefinition* UInventoryJsonImporter::CreateOrOverwriteAsset(const FString& PackagePath, const FString& AssetName)
{
	// Check if asset already exists
	UPackage* ExistingPackage = FindPackage(nullptr, *PackagePath);
	if (ExistingPackage)
	{
		UInventoryItemDefinition* Existing = FindObject<UInventoryItemDefinition>(ExistingPackage, *AssetName);
		if (Existing)
		{
			// Overwrite: clear and reuse
			Existing->BaseEffects.Empty();
			Existing->SpawnData = FIngredientSpawnData();
			return Existing;
		}
	}

	// Create new package and asset (UE 4.27 signature)
	UPackage* Package = CreatePackage(nullptr, *PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[JsonImporter] CreatePackage fehlgeschlagen: %s"), *PackagePath);
		return nullptr;
	}

	Package->FullyLoad();

	UInventoryItemDefinition* NewAsset = NewObject<UInventoryItemDefinition>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_MarkAsRootSet);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
	}

	return NewAsset;
}
