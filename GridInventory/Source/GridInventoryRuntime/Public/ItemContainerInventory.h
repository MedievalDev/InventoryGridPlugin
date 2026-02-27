// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InventoryGrid.h"
#include "InventoryItemInstance.h"
#include "ItemContainerInventory.generated.h"

class UInventoryItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContainerInventoryChanged);

/**
 * Lightweight sub-inventory for container items (herb pouch, quiver, belt bag, etc.).
 *
 * This is NOT an ActorComponent - it's a plain UObject owned by the item instance.
 * It uses the same optimized FInventoryGrid as the main inventory.
 *
 * Rule: Container items (bIsContainer = true) cannot be placed inside another container item.
 * They CAN be placed in world containers (chests, etc.) and in the main grid inventory.
 */
UCLASS(BlueprintType)
class GRIDINVENTORYRUNTIME_API UItemContainerInventory : public UObject
{
	GENERATED_BODY()

public:
	UItemContainerInventory();

	/**
	 * Initialize from an item definition's container settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	void Initialize(UInventoryItemDefinition* ContainerItemDef);

	/** Whether this container has been initialized */
	UFUNCTION(BlueprintPure, Category = "Item Container")
	bool IsInitialized() const { return bInitialized; }

	// ========================
	// Events
	// ========================

	UPROPERTY(BlueprintAssignable, Category = "Item Container|Events")
	FOnContainerInventoryChanged OnContainerChanged;

	// ========================
	// Core Functions
	// ========================

	/**
	 * Try to add an item. Enforces container type restrictions.
	 * Container items (bIsContainer) are rejected.
	 * If ContainerAcceptedTypes is set, only matching types are accepted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	bool TryAddItem(UInventoryItemDefinition* ItemDef, int32 Count = 1);

	/** Try to add at a specific position */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	bool TryAddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count = 1);

	/** Remove by unique ID */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	bool RemoveItem(FGuid UniqueID, int32 Count = 0);

	/** Move within this container */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	bool MoveItem(FGuid UniqueID, FIntPoint NewPosition, bool bRotated);

	/** Rotate an item */
	UFUNCTION(BlueprintCallable, Category = "Item Container")
	bool RotateItem(FGuid UniqueID);

	// ========================
	// Query
	// ========================

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	const TArray<FInventoryItemInstance>& GetAllItems() const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	FInventoryItemInstance GetItemByID(FGuid UniqueID) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	FInventoryItemInstance GetItemAtPosition(FIntPoint Position) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	float GetRemainingWeight() const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	bool HasSpaceFor(UInventoryItemDefinition* ItemDef, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	int32 GetItemCount(UInventoryItemDefinition* ItemDef) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	bool IsPositionFree(FIntPoint Position) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	bool CanPlaceAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated) const;

	/** Check if this container accepts a given item (type check + no containers-in-containers) */
	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	bool AcceptsItem(UInventoryItemDefinition* ItemDef) const;

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	int32 GetGridWidth() const { return GridWidth; }

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	int32 GetGridHeight() const { return GridHeight; }

	UFUNCTION(BlueprintPure, Category = "Item Container|Query")
	int32 GetFreeCellCount() const;

	/** Get the underlying grid (for widget binding) */
	const FInventoryGrid& GetGrid() const { return Grid; }

	// ========================
	// Utility
	// ========================

	UFUNCTION(BlueprintCallable, Category = "Item Container")
	void SortContainer();

	UFUNCTION(BlueprintCallable, Category = "Item Container")
	void ClearContainer();

private:
	bool bInitialized;

	UPROPERTY()
	int32 GridWidth;

	UPROPERTY()
	int32 GridHeight;

	UPROPERTY()
	float MaxWeight;

	/** Accepted types (empty = all non-container types) */
	UPROPERTY()
	TArray<FName> AcceptedTypes;

	UPROPERTY()
	FInventoryGrid Grid;

	UPROPERTY()
	TArray<FInventoryItemInstance> Items;

	float CachedWeight;

	/** O(1) lookup: UniqueID → index in Items array */
	TMap<FGuid, int32> ItemIndexMap;

	void RecalculateWeight();
	bool CanAffordWeight(float Additional) const;

	/** O(1) item lookup via ItemIndexMap */
	int32 FindItemIndex(const FGuid& UniqueID) const;

	/**
	 * Swap-remove an item at the given array index.
	 * O(1) — swaps last element into removed position and updates ItemIndexMap.
	 */
	void SwapRemoveItemAtIndex(int32 Index);

	/** Rebuild ItemIndexMap from Items array */
	void RebuildItemIndexMap();

	int32 TryStackOntoExisting(UInventoryItemDefinition* ItemDef, int32 Count);
	bool Internal_AddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count);
	bool Internal_RemoveItem(const FGuid& UniqueID, int32 Count);
};
