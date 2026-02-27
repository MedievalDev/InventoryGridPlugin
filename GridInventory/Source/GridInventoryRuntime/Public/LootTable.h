// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "InventoryItemDefinition.h"
#include "InventoryItemInstance.h"
#include "LootTable.generated.h"

/**
 * A single entry in a loot table.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FLootTableEntry
{
	GENERATED_BODY()

	/** The item that can drop (soft reference - only loaded when loot is generated) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSoftObjectPtr<UInventoryItemDefinition> ItemDef;

	/** Minimum drop count (for stackable items) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount;

	/** Maximum drop count (for stackable items) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount;

	/**
	 * Optional: Override the item's LootWeights with a fixed weight.
	 * 0 = use the item's own LootWeights (default behavior).
	 * > 0 = ignore item LootWeights and use this value instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float FixedWeightOverride;

	FLootTableEntry()
		: MinCount(1)
		, MaxCount(1)
		, FixedWeightOverride(0.0f)
	{
	}
};

/** Result of a loot generation */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FLootResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	UInventoryItemDefinition* ItemDef;

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	int32 Count;

	FLootResult() : ItemDef(nullptr), Count(0) {}
	FLootResult(UInventoryItemDefinition* InDef, int32 InCount) : ItemDef(InDef), Count(InCount) {}
};

/**
 * Reusable loot table. Create as DataAsset.
 * Assign to InventoryContainers for automatic loot generation.
 *
 * Algorithm:
 * 1. For each entry, compute: effectiveWeight = ItemDef->GetLootWeightForLevel(Level) * WeightMultiplier
 *    (or use FixedWeightOverride if set)
 * 2. Entries with effectiveWeight <= 0 are excluded
 * 3. Weighted random selection (without replacement) picks MinItemDrops..MaxItemDrops items
 * 4. For each picked item, random count between MinCount..MaxCount
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYRUNTIME_API ULootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULootTable();

	/** All possible items in this loot table */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot Table")
	TArray<FLootTableEntry> Entries;

	/** Minimum number of different items to drop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot Table", meta = (ClampMin = "0"))
	int32 MinItemDrops;

	/** Maximum number of different items to drop */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot Table", meta = (ClampMin = "0"))
	int32 MaxItemDrops;

	/**
	 * Generate loot based on level and weight multiplier.
	 * @param Level The current level for weight lookup
	 * @param WeightMultiplier Container-specific multiplier (1.0 = normal, 3.0 = rare chest)
	 * @return Array of items to add to the container
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot Table")
	TArray<FLootResult> GenerateLoot(int32 Level, float WeightMultiplier = 1.0f) const;

	/**
	 * Preview the effective weights for a given level (useful for debugging in editor).
	 */
	UFUNCTION(BlueprintPure, Category = "Loot Table")
	TArray<float> PreviewWeightsForLevel(int32 Level, float WeightMultiplier = 1.0f) const;

	/**
	 * Async loot generation. Loads all soft references first, then generates.
	 * Use this for containers opened during gameplay to avoid hitches.
	 * @param Level The current level
	 * @param WeightMultiplier Container-specific multiplier
	 * @param OnComplete Callback with generated loot results
	 */
	void GenerateLootAsync(int32 Level, float WeightMultiplier,
		TFunction<void(const TArray<FLootResult>&)> OnComplete) const;

	/** Collect all soft object paths that need loading */
	TArray<FSoftObjectPath> GetAllItemPaths() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
