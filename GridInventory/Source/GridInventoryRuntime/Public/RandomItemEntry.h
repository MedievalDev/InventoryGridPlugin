// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RandomItemEntry.generated.h"

class UInventoryItemDefinition;

/**
 * A single entry in a random item list (containers, merchants, etc.).
 * Each entry has a spawn chance and a random count range.
 *
 * Examples:
 *   Heilkraut:  SpawnChance 0.8, MinCount 1, MaxCount 5  -> often, 1-5 items
 *   Seltenes Schwert: SpawnChance 0.1, MinCount 1, MaxCount 1  -> rare, exactly 1
 *   Gold Nugget: SpawnChance 0.5, MinCount 2, MaxCount 10 -> 50/50, 2-10 items
 *
 * On containers, SpawnChance is multiplied by the DropWeightMultiplier.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FRandomItemEntry
{
	GENERATED_BODY()

	/** The item that may appear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item")
	UInventoryItemDefinition* ItemDef;

	/** Minimum count if the item spawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item", meta = (ClampMin = "1"))
	int32 MinCount;

	/** Maximum count if the item spawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item", meta = (ClampMin = "1"))
	int32 MaxCount;

	/**
	 * Chance that this item appears (0.0 = never, 1.0 = always).
	 * On containers, this is multiplied by DropWeightMultiplier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance;

	FRandomItemEntry()
		: ItemDef(nullptr)
		, MinCount(1)
		, MaxCount(1)
		, SpawnChance(0.5f)
	{
	}

	FRandomItemEntry(UInventoryItemDefinition* InDef, int32 InMin, int32 InMax, float InChance)
		: ItemDef(InDef)
		, MinCount(InMin)
		, MaxCount(InMax)
		, SpawnChance(InChance)
	{
	}
};
