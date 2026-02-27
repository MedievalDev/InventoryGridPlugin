// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GridInventoryComponent.h"
#include "EquipmentComponent.h"
#include "InventoryFunctionLibrary.generated.h"

class UItemContainerInventory;

/**
 * Static helper and search functions for the Grid Inventory system.
 * All accessible from any Blueprint.
 */
UCLASS()
class GRIDINVENTORYRUNTIME_API UInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========================
	// Search & Filter
	// ========================

	/** Find all items matching an item type (e.g. "Weapon_1H", "Consumable") */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindItemsByType(UGridInventoryComponent* Inventory, FName ItemType);

	/** Find all items matching a specific item definition */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindItemsByDef(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef);

	/** Search items by display name (case-insensitive, partial match) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindItemsByName(UGridInventoryComponent* Inventory, const FString& SearchText);

	/** Get all stackable items in the inventory */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindStackableItems(UGridInventoryComponent* Inventory);

	/** Get all items that are not fully stacked (have room for more) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindPartialStacks(UGridInventoryComponent* Inventory);

	/** Find items that can be equipped in a specific equipment slot */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindEquippableItems(
		UGridInventoryComponent* Inventory,
		FName SlotID,
		UEquipmentComponent* EquipmentComp);

	/** Get the N heaviest items (sorted by total stack weight, descending) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> GetHeaviestItems(UGridInventoryComponent* Inventory, int32 Count = 5);

	/** Get the N lightest items (sorted ascending, excludes weightless) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> GetLightestItems(UGridInventoryComponent* Inventory, int32 Count = 5);

	/** Get all items with weight above threshold */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FilterByMinWeight(UGridInventoryComponent* Inventory, float MinWeight);

	/** Get all items with weight below threshold */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FilterByMaxWeight(UGridInventoryComponent* Inventory, float MaxWeight);

	/** Get all items that can be rotated */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindRotatableItems(UGridInventoryComponent* Inventory);

	/** Get all items larger than a given cell area */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Search")
	static TArray<FInventoryItemInstance> FindItemsLargerThan(UGridInventoryComponent* Inventory, int32 MinCellArea);

	// ========================
	// Counting & Stats
	// ========================

	/** Count total items of a specific type across all stacks */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Stats")
	static int32 GetTotalItemCount(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef);

	/** Count unique item types in the inventory */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Stats")
	static int32 GetUniqueItemTypeCount(UGridInventoryComponent* Inventory);

	/** Get total number of individual items (sum of all stack counts) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Stats")
	static int32 GetTotalIndividualItemCount(UGridInventoryComponent* Inventory);

	// ========================
	// Multi-Inventory
	// ========================

	/** Find an item by unique ID across multiple inventories */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Multi")
	static FInventoryItemInstance FindItemInInventories(
		const TArray<UGridInventoryComponent*>& Inventories,
		FGuid UniqueID,
		UGridInventoryComponent*& OutInventory);

	/** Swap two items (within same or between different inventories) */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Multi")
	static bool SwapItems(UGridInventoryComponent* InvA, FGuid ItemA_ID,
		UGridInventoryComponent* InvB, FGuid ItemB_ID);

	// ========================
	// Convenience
	// ========================

	/** Get inventory component from an actor */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Utility", meta = (DefaultToSelf = "Actor"))
	static UGridInventoryComponent* GetInventoryComponent(AActor* Actor);

	/** Get equipment component from an actor */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Utility", meta = (DefaultToSelf = "Actor"))
	static UEquipmentComponent* GetEquipmentComponent(AActor* Actor);

	/** Sort inventory for optimal packing */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Utility")
	static void SortInventory(UGridInventoryComponent* Inventory);

	// ========================
	// Item Container Search
	// ========================

	/** Find all container items in the inventory */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Container Search")
	static TArray<FInventoryItemInstance> FindContainerItems(UGridInventoryComponent* Inventory);

	/** Find container items that accept a specific item type */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Container Search")
	static TArray<FInventoryItemInstance> FindContainersForItemType(UGridInventoryComponent* Inventory, FName ItemType);

	/**
	 * Search across all sub-inventories for items matching a name.
	 * Useful for "where did I put that herb?" scenarios.
	 * Returns items found + which container they're in.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Container Search")
	static TArray<FInventoryItemInstance> DeepSearchByName(
		UGridInventoryComponent* Inventory,
		const FString& SearchText,
		FGuid& OutContainerItemID);

	/**
	 * Search across all sub-inventories for items of a specific type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Container Search")
	static TArray<FInventoryItemInstance> DeepSearchByType(
		UGridInventoryComponent* Inventory,
		FName ItemType);

	/**
	 * Count total items of a type across main inventory AND all sub-inventories.
	 */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Container Search")
	static int32 DeepCountByDef(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef);

	// ========================
	// Effect Values
	// ========================

	/** Get a specific effect value from an item instance (resolved with class multiplier) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Effects")
	static float GetItemEffectValue(const FInventoryItemInstance& Item, FName EffectID);

	/** Get all effect values from an item instance */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Effects")
	static TArray<FItemEffectValue> GetAllItemEffects(const FInventoryItemInstance& Item);

	/** Set an instance-specific effect override on an item in the inventory */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Effects")
	static bool SetInstanceEffect(UGridInventoryComponent* Inventory, FGuid UniqueID, FName EffectID, float Value);

	/** Get display name with class suffix */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Effects")
	static FText GetItemDisplayName(const FInventoryItemInstance& Item);

	// ========================
	// Merge Helpers
	// ========================

	/** Find all items that can be merged with a target item */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Merge")
	static TArray<FInventoryItemInstance> FindMergeCandidates(UGridInventoryComponent* Inventory, FGuid TargetID);

	/** Get current class level of an item */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Merge")
	static int32 GetItemClassLevel(const FInventoryItemInstance& Item);
};
