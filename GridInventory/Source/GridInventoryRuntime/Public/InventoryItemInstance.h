// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemDefinition.h"
#include "ItemEffectValue.h"
#include "InventoryItemInstance.generated.h"

class UItemContainerInventory;

/**
 * Represents a concrete instance of an item inside an inventory.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FInventoryItemInstance
{
	GENERATED_BODY()

	FInventoryItemInstance();

	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	FGuid UniqueID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Instance")
	UInventoryItemDefinition* ItemDef;

	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	FIntPoint GridPosition;

	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	bool bIsRotated;

	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	int32 StackCount;

	/**
	 * Instance-specific effect overrides/additions.
	 * These take priority over BaseEffects from the ItemDefinition.
	 * Used for random loot rolls, enchantments, etc.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
	TArray<FItemEffectValue> InstanceEffects;

	/**
	 * Current class level (1 = base).
	 * Increases through merging identical items.
	 * All effects are multiplied by the class multiplier.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	int32 CurrentClassLevel;

	/**
	 * Sub-inventory for container items.
	 * Created lazily via GetOrCreateSubInventory().
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
	UItemContainerInventory* SubInventory;

	// ========================
	// Effect Value Resolution
	// ========================

	/**
	 * Get the final effect value for a given effect ID.
	 * Resolution order:
	 * 1. Check InstanceEffects (override)
	 * 2. Fall back to ItemDef->BaseEffects
	 * 3. Multiply by class level multiplier
	 * Returns 0 if effect not found.
	 */
	float GetEffectValue(FName EffectID) const;

	/** Get all final effect values (merged base + instance, with class multiplier applied) */
	TArray<FItemEffectValue> GetAllEffectValues() const;

	/** Get the display name including class suffix */
	FText GetDisplayName() const;

	// ========================
	// Core
	// ========================

	FIntPoint GetEffectiveSize() const;
	float GetTotalWeight() const;
	float GetOwnWeight() const;
	bool IsValid() const;
	bool IsContainer() const;
	bool CanMergeWith(const FInventoryItemInstance& Other) const;

	UItemContainerInventory* GetOrCreateSubInventory(UObject* Outer);

	bool operator==(const FInventoryItemInstance& Other) const { return UniqueID == Other.UniqueID; }
	bool operator!=(const FInventoryItemInstance& Other) const { return UniqueID != Other.UniqueID; }

	static FInventoryItemInstance CreateNew(UInventoryItemDefinition* InItemDef, int32 InStackCount = 1);
};
