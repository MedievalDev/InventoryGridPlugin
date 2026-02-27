// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ItemClassMultiplier.generated.h"

/**
 * Defines the effect multiplier for a specific item class level.
 * Add entries in the editor on the ItemDefinition.
 *
 * Example (Eisenschwert):
 *   Class 1: Multiplier 1.0  -> 10 Damage
 *   Class 2: Multiplier 1.5  -> 15 Damage
 *   Class 3: Multiplier 2.2  -> 22 Damage
 *   Class 4: Multiplier 3.0  -> 30 Damage (Max)
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FItemClassMultiplier
{
	GENERATED_BODY()

	/** The class level (1, 2, 3, ...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Class", meta = (ClampMin = "1"))
	int32 ClassLevel;

	/** Effect value multiplier at this class level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Class", meta = (ClampMin = "0.0"))
	float EffectMultiplier;

	/** Optional display suffix ("+1", "+2", "Meisterhaft", etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Class")
	FText ClassSuffix;

	/** Optional overlay icon for the UI (stars, colored border, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Class")
	TSoftObjectPtr<UTexture2D> ClassOverlayIcon;

	FItemClassMultiplier()
		: ClassLevel(1)
		, EffectMultiplier(1.0f)
	{
	}
};

/**
 * Defines the loot drop weight for a specific level range.
 *
 * Example (Eisenschwert):
 *   MinLevel 1,  Weight 0.1  -> rare at low levels
 *   MinLevel 5,  Weight 2.0  -> common at mid levels
 *   MinLevel 15, Weight 0.3  -> rare again at high levels
 *
 * The system picks the entry with the highest MinLevel <= current level.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FLootWeightEntry
{
	GENERATED_BODY()

	/** Minimum level from which this weight applies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinLevel;

	/** Drop weight (higher = more likely). 0 = won't drop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight;

	FLootWeightEntry() : MinLevel(1), Weight(0.1f) {}
	FLootWeightEntry(int32 InLevel, float InWeight) : MinLevel(InLevel), Weight(InWeight) {}
};
