// Copyright Marco. All Rights Reserved.

#include "RuntimeItemDefinition.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

URuntimeItemDefinition::URuntimeItemDefinition()
	: bIsRuntimeCreated(true)
{
}

// ============================================================================
// JSON Serialization
// ============================================================================

FString URuntimeItemDefinition::ToJSON() const
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());

	// Basic info
	Obj->SetStringField(TEXT("ItemID"), ItemID.ToString());
	Obj->SetStringField(TEXT("DisplayName"), DisplayName.ToString());
	Obj->SetStringField(TEXT("Description"), Description.ToString());
	Obj->SetStringField(TEXT("ItemType"), ItemType.ToString());
	Obj->SetStringField(TEXT("CategoryTag"), CategoryTag.ToString());

	// Grid
	Obj->SetNumberField(TEXT("GridSizeX"), GridSize.X);
	Obj->SetNumberField(TEXT("GridSizeY"), GridSize.Y);
	Obj->SetBoolField(TEXT("bCanRotate"), bCanRotate);

	// Stacking
	Obj->SetBoolField(TEXT("bCanStack"), bCanStack);
	Obj->SetNumberField(TEXT("MaxStackSize"), MaxStackSize);

	// Weight
	Obj->SetNumberField(TEXT("Weight"), Weight);

	// Consumable
	Obj->SetBoolField(TEXT("bIsConsumable"), bIsConsumable);
	Obj->SetBoolField(TEXT("bDestroyOnConsume"), bDestroyOnConsume);

	// Merge
	Obj->SetBoolField(TEXT("bCanMergeUpgrade"), bCanMergeUpgrade);
	Obj->SetNumberField(TEXT("MaxClassLevel"), MaxClassLevel);

	// Alchemy properties
	Obj->SetStringField(TEXT("WirkungTag"), WirkungTag.ToString());
	Obj->SetStringField(TEXT("SchutzVor"), SchutzVor);
	Obj->SetStringField(TEXT("Gegengift"), Gegengift);
	Obj->SetStringField(TEXT("Sonstiges"), Sonstiges);
	Obj->SetNumberField(TEXT("DurationMinutes"), DurationMinutes);

	// Base effects
	TArray<TSharedPtr<FJsonValue>> EffectsArray;
	for (const FItemEffectValue& Effect : BaseEffects)
	{
		TSharedPtr<FJsonObject> EffObj = MakeShareable(new FJsonObject());
		EffObj->SetStringField(TEXT("EffectID"), Effect.EffectID.ToString());
		EffObj->SetNumberField(TEXT("Value"), Effect.Value);
		EffectsArray.Add(MakeShareable(new FJsonValueObject(EffObj)));
	}
	Obj->SetArrayField(TEXT("BaseEffects"), EffectsArray);

	// Source ingredients
	TArray<TSharedPtr<FJsonValue>> IngredientsArray;
	for (const FName& Name : SourceIngredients)
	{
		IngredientsArray.Add(MakeShareable(new FJsonValueString(Name.ToString())));
	}
	Obj->SetArrayField(TEXT("SourceIngredients"), IngredientsArray);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	return OutputString;
}

URuntimeItemDefinition* URuntimeItemDefinition::FromJSON(UObject* Outer, const FString& JSON)
{
	if (JSON.IsEmpty() || !Outer) return nullptr;

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSON);

	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] RuntimeItem FromJSON: Parse error"));
		return nullptr;
	}

	URuntimeItemDefinition* Def = NewObject<URuntimeItemDefinition>(Outer);

	// Basic info
	FString TempStr;
	if (Obj->TryGetStringField(TEXT("ItemID"), TempStr))
		Def->ItemID = FName(*TempStr);
	if (Obj->TryGetStringField(TEXT("DisplayName"), TempStr))
		Def->DisplayName = FText::FromString(TempStr);
	if (Obj->TryGetStringField(TEXT("Description"), TempStr))
		Def->Description = FText::FromString(TempStr);
	if (Obj->TryGetStringField(TEXT("ItemType"), TempStr))
		Def->ItemType = FName(*TempStr);
	if (Obj->TryGetStringField(TEXT("CategoryTag"), TempStr))
		Def->CategoryTag = FName(*TempStr);

	// Grid
	double TempNum = 0;
	if (Obj->TryGetNumberField(TEXT("GridSizeX"), TempNum))
		Def->GridSize.X = (int32)TempNum;
	if (Obj->TryGetNumberField(TEXT("GridSizeY"), TempNum))
		Def->GridSize.Y = (int32)TempNum;
	Obj->TryGetBoolField(TEXT("bCanRotate"), Def->bCanRotate);

	// Stacking
	Obj->TryGetBoolField(TEXT("bCanStack"), Def->bCanStack);
	if (Obj->TryGetNumberField(TEXT("MaxStackSize"), TempNum))
		Def->MaxStackSize = (int32)TempNum;

	// Weight
	if (Obj->TryGetNumberField(TEXT("Weight"), TempNum))
		Def->Weight = (float)TempNum;

	// Consumable
	Obj->TryGetBoolField(TEXT("bIsConsumable"), Def->bIsConsumable);
	Obj->TryGetBoolField(TEXT("bDestroyOnConsume"), Def->bDestroyOnConsume);

	// Merge
	Obj->TryGetBoolField(TEXT("bCanMergeUpgrade"), Def->bCanMergeUpgrade);
	if (Obj->TryGetNumberField(TEXT("MaxClassLevel"), TempNum))
		Def->MaxClassLevel = (int32)TempNum;

	// Alchemy
	if (Obj->TryGetStringField(TEXT("WirkungTag"), TempStr))
		Def->WirkungTag = FName(*TempStr);
	Obj->TryGetStringField(TEXT("SchutzVor"), Def->SchutzVor);
	Obj->TryGetStringField(TEXT("Gegengift"), Def->Gegengift);
	Obj->TryGetStringField(TEXT("Sonstiges"), Def->Sonstiges);
	if (Obj->TryGetNumberField(TEXT("DurationMinutes"), TempNum))
		Def->DurationMinutes = (float)TempNum;

	// Base effects
	const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
	if (Obj->TryGetArrayField(TEXT("BaseEffects"), EffectsArray))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EffectsArray)
		{
			const TSharedPtr<FJsonObject>& EffObj = Val->AsObject();
			if (!EffObj.IsValid()) continue;

			FString EffID;
			double EffValue = 0;
			EffObj->TryGetStringField(TEXT("EffectID"), EffID);
			EffObj->TryGetNumberField(TEXT("Value"), EffValue);

			if (!EffID.IsEmpty())
			{
				Def->BaseEffects.Add(FItemEffectValue(FName(*EffID), (float)EffValue));
			}
		}
	}

	// Source ingredients
	const TArray<TSharedPtr<FJsonValue>>* IngredientsArray = nullptr;
	if (Obj->TryGetArrayField(TEXT("SourceIngredients"), IngredientsArray))
	{
		for (const TSharedPtr<FJsonValue>& Val : *IngredientsArray)
		{
			Def->SourceIngredients.Add(FName(*Val->AsString()));
		}
	}

	return Def;
}
