// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryItemDefinition.h"
#include "InventoryJsonImporter.generated.h"

/**
 * Editor-only utility to import alchemy ingredients from a JSON file
 * into UInventoryItemDefinition DataAssets.
 *
 * JSON format expected: data.json from the Alchimist design document.
 * Each category under "Alchemie Zutaten" is parsed, HTML tables extracted,
 * and DataAssets created/overwritten in Content/Items/AlchemieZutaten/{Category}/
 *
 * Usage:
 *   Console: Inv.ImportJSON C:/path/to/data.json
 *   Console: Inv.ImportJSON (uses /Game/Data/data.json)
 *   Blueprint: UInventoryJsonImporter::ImportFromJsonFile(Path)
 */
UCLASS()
class GRIDINVENTORYEDITOR_API UInventoryJsonImporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Import all alchemy ingredients from a JSON file.
	 * @param JsonFilePath Absolute file path to data.json
	 * @param OutputBasePath Content path for assets (default: /Game/Items/AlchemieZutaten)
	 * @return Number of items successfully imported
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Import", meta = (CallInEditor = "true"))
	static int32 ImportFromJsonFile(const FString& JsonFilePath, const FString& OutputBasePath = TEXT("/Game/Items/AlchemieZutaten"));

	/** Register console commands (called by module startup) */
	static void RegisterConsoleCommands();

	/** Unregister console commands (called by module shutdown) */
	static void UnregisterConsoleCommands();

private:

	/** Parse HTML table <td> values from content string */
	static TArray<FString> ParseTableValues(const FString& HtmlContent);

	/** Extract description text from <p> tags */
	static FString ParseDescription(const FString& HtmlContent);

	/** Parse duration string ("2 Min" -> 2.0, "-" -> 0.0) */
	static float ParseDuration(const FString& DurationStr);

	/** Parse numeric value ("-" -> 0.0, "225" -> 225.0, "-100" -> -100.0) */
	static float ParseNumericValue(const FString& ValueStr);

	/** Sanitize name for use as asset name (remove umlauts, special chars) */
	static FString SanitizeAssetName(const FString& Name);

	/** Sanitize category name for folder */
	static FString SanitizeCategoryName(const FString& CategoryName);

	/** Create or overwrite a DataAsset at the given package path */
	static UInventoryItemDefinition* CreateOrOverwriteAsset(const FString& PackagePath, const FString& AssetName);

	/** Console command handler */
	static void HandleImportCommand(const TArray<FString>& Args);

	/** Stored console command reference */
	static FAutoConsoleCommand* ImportCommand;
};
