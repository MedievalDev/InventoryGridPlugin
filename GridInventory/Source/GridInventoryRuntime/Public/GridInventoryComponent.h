// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryGrid.h"
#include "InventoryItemInstance.h"
#include "InventoryItemDefinition.h"
#include "GridInventoryComponent.generated.h"

class UItemContainerInventory;
class URuntimeItemDefinition;
struct FItemSaveEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemAdded, const FInventoryItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemRemoved, const FInventoryItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemMoved, const FInventoryItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/**
 * Main inventory component. Attach to any Actor.
 * Optimized for large grids (up to 60x80) with bitmap + free-rect caching.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent), BlueprintType)
class GRIDINVENTORYRUNTIME_API UGridInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGridInventoryComponent();

	// ========================
	// Configuration
	// ========================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Inventory|Config", meta = (ClampMin = "1", ClampMax = "80"))
	int32 GridWidth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Inventory|Config", meta = (ClampMin = "1", ClampMax = "80"))
	int32 GridHeight;

	/** Maximum weight capacity. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Inventory|Config", meta = (ClampMin = "0.0"))
	float MaxWeight;

	// ========================
	// Events
	// ========================

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnInventoryItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnInventoryItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnInventoryItemMoved OnItemMoved;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnInventoryChanged OnInventoryChanged;

	// ========================
	// Core Functions
	// ========================

	/** Try to add an item (auto-find slot, try stacking first) */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool TryAddItem(UInventoryItemDefinition* ItemDef, int32 Count = 1);

	/** Try to add at a specific position */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool TryAddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count = 1);

	/**
	 * Batch-add multiple items at once.
	 * More efficient than calling TryAddItem repeatedly because it uses
	 * the grid's batch slot finder (simulates placements).
	 * @return Number of items that were successfully added
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	int32 TryAddItemsBatch(const TArray<FItemAddRequest>& Requests);

	/** Remove by unique ID. Count 0 = remove entire stack. */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool RemoveItem(FGuid UniqueID, int32 Count = 0);

	/** Remove by item definition. Returns count actually removed. */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	int32 RemoveItemByDef(UInventoryItemDefinition* ItemDef, int32 Count = 1);

	/** Move within this inventory */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool MoveItem(FGuid UniqueID, FIntPoint NewPosition, bool bRotated);

	/** Rotate item 90 degrees */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool RotateItem(FGuid UniqueID);

	/** Transfer to another inventory (auto-find slot) */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool TransferItem(FGuid UniqueID, UGridInventoryComponent* TargetInventory, int32 Count = 0);

	/** Transfer to specific position in another inventory */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	bool TransferItemTo(FGuid UniqueID, UGridInventoryComponent* TargetInventory, FIntPoint TargetPosition, bool bRotated, int32 Count = 0);

	// ========================
	// Consume / Use
	// ========================

	/**
	 * Use/consume an item from the inventory.
	 * Fires OnItemConsumed delegate with the item data so you can apply gameplay effects.
	 * If bDestroyOnConsume is true on the ItemDef, reduces stack by 1 (or removes if last).
	 * @return true if consumed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Use")
	bool ConsumeItem(FGuid UniqueID);

	// ========================
	// Drop / Pickup
	// ========================

	/**
	 * Drop an item from inventory into the world.
	 * Spawns the WorldActorClass at the given location and removes from inventory.
	 * @param UniqueID Item to drop
	 * @param SpawnLocation Where to spawn the world actor
	 * @param SpawnRotation Rotation for the spawned actor
	 * @param Count How many to drop (0 = entire stack)
	 * @return The spawned actor, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|World")
	AActor* DropItem(FGuid UniqueID, FVector SpawnLocation, FRotator SpawnRotation, int32 Count = 0);

	/**
	 * Pick up an item from the world into the inventory.
	 * @param ItemDef The item definition to add
	 * @param Count How many
	 * @param WorldActor Optional: the actor to destroy after pickup
	 * @return true if picked up successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|World")
	bool PickupItem(UInventoryItemDefinition* ItemDef, int32 Count, AActor* WorldActor = nullptr);

	// ========================
	// Merge / Upgrade
	// ========================

	/**
	 * Merge two identical items to upgrade the target's class level.
	 * The sacrifice item is destroyed, target's CurrentClassLevel increases by 1.
	 * Both items must have the same ItemDef with bCanMergeUpgrade = true.
	 * @return true if merge was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Merge")
	bool MergeItems(FGuid TargetID, FGuid SacrificeID);

	/**
	 * Check if two items can be merged.
	 */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Merge")
	bool CanMergeItems(FGuid TargetID, FGuid SacrificeID) const;

	// ========================
	// Consume / Drop / Merge Events
	// ========================

	/** Fired when an item is consumed. Wire your gameplay logic here. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemConsumed, const FInventoryItemInstance&, Item);

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnItemConsumed OnItemConsumed;

	/** Fired when an item is dropped into the world */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, const FInventoryItemInstance&, Item, AActor*, WorldActor);

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnItemDropped OnItemDropped;

	/** Fired when items are merged */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemMerged, const FInventoryItemInstance&, UpgradedItem, int32, NewClassLevel);

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnItemMerged OnItemMerged;

	// ========================
	// Query Functions
	// ========================

	/** Get all items (const reference - no copy) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	const TArray<FInventoryItemInstance>& GetAllItems() const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	FInventoryItemInstance GetItemByID(FGuid UniqueID) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	FInventoryItemInstance GetItemAtPosition(FIntPoint Position) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	float GetRemainingWeight() const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	bool HasSpaceFor(UInventoryItemDefinition* ItemDef, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	int32 GetItemCount(UInventoryItemDefinition* ItemDef) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	bool ContainsItem(UInventoryItemDefinition* ItemDef, int32 MinCount = 1) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	bool IsPositionFree(FIntPoint Position) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	bool CanPlaceAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated) const;

	/** Get number of free cells */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	int32 GetFreeCellCount() const;

	/** Get occupancy percentage (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	float GetOccupancyPercent() const;

	// ========================
	// Item Container Access
	// ========================

	/**
	 * Open a container item's sub-inventory (creates it lazily if needed).
	 * Use this when the player double-clicks a container item.
	 * @return The sub-inventory, or nullptr if item is not a container
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Container")
	UItemContainerInventory* OpenContainerItem(FGuid UniqueID);

	/**
	 * Get a container item's sub-inventory without creating it.
	 * Returns nullptr if not yet opened or not a container.
	 */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Container")
	UItemContainerInventory* GetContainerInventory(FGuid UniqueID) const;

	/**
	 * Check if an item is a container type.
	 */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Container")
	bool IsContainerItem(FGuid UniqueID) const;

	/**
	 * Get total weight including all sub-inventory contents.
	 * This is the "real" total weight of the entire inventory.
	 */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Query")
	float GetTotalWeightRecursive() const;

	const FInventoryGrid& GetGrid() const { return Grid; }

	// ========================
	// Runtime Item Creation
	// ========================

	/**
	 * Create a runtime item definition (e.g. a brewed potion).
	 * The definition is stored in this component's RuntimeDefinitions array
	 * to prevent garbage collection. Use the returned pointer like any ItemDef.
	 *
	 * @param ItemID Unique ID string (e.g. "BrauterTrank_001")
	 * @param DisplayName Display name
	 * @param Effects Base effect values
	 * @param SourceIngredients Names of ingredients used
	 * @return The new definition, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Runtime")
	URuntimeItemDefinition* CreateRuntimeItem(
		FName ItemID,
		FText DisplayName,
		const TArray<FItemEffectValue>& Effects,
		const TArray<FName>& SourceIngredients);

	/** Get all runtime definitions held by this component (GC roots) */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Runtime")
	const TArray<URuntimeItemDefinition*>& GetRuntimeDefinitions() const { return RuntimeDefinitions; }

	// ========================
	// Save / Load
	// ========================

	/**
	 * Save the complete inventory state to a slot (synchronous).
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
	bool SaveInventory(int32 SlotIndex);

	/**
	 * Load the complete inventory state from a slot (synchronous).
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
	bool LoadInventory(int32 SlotIndex);

	/**
	 * Save asynchronously — disk I/O on background thread.
	 * Fires OnSaveComplete when done.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
	void SaveInventoryAsync(int32 SlotIndex);

	/**
	 * Load asynchronously — disk I/O on background thread.
	 * Fires OnLoadComplete when done.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
	void LoadInventoryAsync(int32 SlotIndex);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveLoadComplete, int32, SlotIndex, bool, bSuccess);

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnSaveLoadComplete OnSaveComplete;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnSaveLoadComplete OnLoadComplete;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Save")
	static bool DoesSaveExist(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
	static bool DeleteSave(int32 SlotIndex);

	// ========================
	// Utility
	// ========================

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Utility")
	void SortInventory();

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Utility")
	void ClearInventory();

	/** Reinitialize the grid (after changing GridWidth/GridHeight at runtime). */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Utility")
	void InitializeGrid();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Network RPCs
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryAddItem(UInventoryItemDefinition* ItemDef, int32 Count);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerMoveItem(FGuid UniqueID, FIntPoint NewPosition, bool bRotated);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRotateItem(FGuid UniqueID);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRemoveItem(FGuid UniqueID, int32 Count);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTransferItem(FGuid UniqueID, UGridInventoryComponent* TargetInventory, int32 Count);

	UFUNCTION()
	void OnRep_Items();

private:
	FInventoryGrid Grid;

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FInventoryItemInstance> Items;

	/** O(1) lookup: UniqueID → index in Items array */
	TMap<FGuid, int32> ItemIndexMap;

	/** GC-safe storage for runtime-created item definitions */
	UPROPERTY()
	TArray<URuntimeItemDefinition*> RuntimeDefinitions;

	float CachedWeight;

	/** Dirty flag for batched UI updates - prevents multiple refreshes per frame */
	bool bInventoryDirty;

	/** Whether we have a pending broadcast queued */
	bool bBroadcastPending;

#if !UE_BUILD_SHIPPING
	friend class UInventoryDebugSubsystem;
#endif

	void RecalculateWeight();
	bool CanAffordWeight(float AdditionalWeight) const;

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
	void RebuildGridFromItems();

	void BroadcastItemAdded(const FInventoryItemInstance& Item);
	void BroadcastItemRemoved(const FInventoryItemInstance& Item);
	void BroadcastItemMoved(const FInventoryItemInstance& Item);
	void BroadcastChanged();

	bool Internal_AddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count);
	bool Internal_RemoveItem(const FGuid& UniqueID, int32 Count);

	// Save/Load helpers
	static FString GetSaveSlotName(int32 SlotIndex);
	static FItemSaveEntry CreateSaveEntry(const FInventoryItemInstance& Item);
	bool RestoreItemFromEntry(const FItemSaveEntry& Entry);

	/** Build save game object on game thread (used by both sync and async) */
	class UInventorySaveGame* BuildSaveGameObject(int32 SlotIndex);

	/** Process a loaded save game (used by both sync and async) */
	bool ProcessLoadedSaveGame(class UInventorySaveGame* SaveGame, int32 SlotIndex);
};
