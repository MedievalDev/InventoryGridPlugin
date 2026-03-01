// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EquipmentSlotDefinition.h"
#include "InventoryItemInstance.h"
#include "ItemRequirement.h"
#include "EquipmentComponent.generated.h"

class UGridInventoryComponent;
class UInventoryItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemEquipped, FName, SlotID, const FInventoryItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUnequipped, FName, SlotID, const FInventoryItemInstance&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);

/**
 * Delegate that the EquipmentComponent calls to query a player stat value.
 * Bind this in Blueprint to provide your own attribute values.
 * Return the current value of the requested stat (e.g. "Level" -> 8, "Staerke" -> 12).
 */
DECLARE_DYNAMIC_DELEGATE_ReturnVal_OneParam(float, FGetPlayerStatDelegate, FName, StatID);

/**
 * Equipment component that manages named equipment slots.
 * Define slots in the editor array, then equip/unequip items.
 *
 * Works with GridInventoryComponent: equipping removes from inventory,
 * unequipping returns to inventory.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent), BlueprintType)
class GRIDINVENTORYRUNTIME_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent();

	// ========================
	// Configuration
	// ========================

	/**
	 * Define all available equipment slots here.
	 * Add entries in the editor: Head, Chest, MainHand, PotionSlot1, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Config")
	TArray<FEquipmentSlotDefinition> SlotDefinitions;

	/**
	 * Optional: linked inventory component.
	 * If set, equipping auto-removes from inventory, unequipping auto-returns.
	 * If null, you manage the item transfer yourself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Config")
	UGridInventoryComponent* LinkedInventory;

	// ========================
	// Events
	// ========================

	UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
	FOnItemEquipped OnItemEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
	FOnItemUnequipped OnItemUnequipped;

	UPROPERTY(BlueprintAssignable, Category = "Equipment|Events")
	FOnEquipmentChanged OnEquipmentChanged;

	// ========================
	// Requirements
	// ========================

	/**
	 * Bind this delegate to a Blueprint function that returns the player's
	 * current stat value for a given stat name.
	 *
	 * Blueprint setup:
	 *   Bind Event to OnGetPlayerStat → Custom Event with return value
	 *   Switch on StatID: "Level" → PlayerLevel, "Staerke" → Strength, etc.
	 *
	 * If not bound, all requirement checks pass (no restrictions).
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Equipment|Requirements")
	FGetPlayerStatDelegate OnGetPlayerStat;

	/**
	 * Check if the player meets all requirements for an item.
	 * Queries OnGetPlayerStat for each requirement on the item.
	 * Returns true if all requirements are met, or if no delegate is bound.
	 */
	UFUNCTION(BlueprintPure, Category = "Equipment|Requirements")
	bool MeetsRequirements(UInventoryItemDefinition* ItemDef) const;

	/**
	 * Get which requirements the player does NOT meet for an item.
	 * Useful for showing "you need 3 more Strength" in the UI.
	 */
	UFUNCTION(BlueprintPure, Category = "Equipment|Requirements")
	TArray<FItemRequirement> GetUnmetRequirements(UInventoryItemDefinition* ItemDef) const;

	// ========================
	// Core Functions
	// ========================

	/**
	 * Equip an item into a slot.
	 * If LinkedInventory is set, removes the item from inventory.
	 * If the slot already has an item, it will be swapped (old item goes to inventory).
	 * @param SlotID Which slot to equip into
	 * @param ItemDef The item definition
	 * @param StackCount How many (for stackable slots)
	 * @return true if equipped successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool EquipItem(FName SlotID, UInventoryItemDefinition* ItemDef, int32 StackCount = 1);

	/**
	 * Equip from an inventory item instance (e.g. after drag from grid).
	 * Automatically uses the item's definition and stack count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool EquipFromInventory(FName SlotID, FGuid InventoryItemID);

	/**
	 * Unequip the item from a slot.
	 * If LinkedInventory is set, tries to return item to inventory.
	 * @param SlotID Which slot to unequip
	 * @param Count How many to remove (0 = all, relevant for stacked consumables)
	 * @return true if unequipped successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool UnequipItem(FName SlotID, int32 Count = 0);

	/**
	 * Swap items between two equipment slots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SwapSlots(FName SlotA, FName SlotB);

	/**
	 * Use one charge from a stackable equipment slot (e.g. drink a potion).
	 * @return true if there was at least one item to consume
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ConsumeFromSlot(FName SlotID, int32 Count = 1);

	// ========================
	// Query Functions
	// ========================

	/** Get the item currently in a slot */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	FInventoryItemInstance GetItemInSlot(FName SlotID) const;

	/** Check if a slot has an item */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	bool IsSlotOccupied(FName SlotID) const;

	/** Check if a slot is empty */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	bool IsSlotEmpty(FName SlotID) const;

	/** Check if an item definition can be equipped in a specific slot */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	bool CanEquipInSlot(FName SlotID, UInventoryItemDefinition* ItemDef) const;

	/** Find the first slot that accepts this item type and is empty */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	FName FindEmptySlotForItem(UInventoryItemDefinition* ItemDef) const;

	/** Get all slot definitions */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	TArray<FEquipmentSlotDefinition> GetAllSlotDefinitions() const;

	/** Get a slot definition by ID */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	FEquipmentSlotDefinition GetSlotDefinition(FName SlotID) const;

	/** Get all currently equipped items */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	TMap<FName, FInventoryItemInstance> GetAllEquippedItems() const;

	/** Get total weight of all equipped items */
	UFUNCTION(BlueprintPure, Category = "Equipment|Query")
	float GetTotalEquipmentWeight() const;

	// ========================
	// Save/Load Support
	// ========================

	/**
	 * Restore an item directly into a slot (used by save/load system).
	 * Bypasses inventory interaction and type checks — the save data is assumed valid.
	 * Clears any existing item in the slot first.
	 */
	void RestoreEquipmentSlot(FName SlotID, const FInventoryItemInstance& Item);

	/** Clear all equipment slots (used before loading saved state) */
	void ClearAllSlots();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_EquippedItems();

private:
	/** Map of SlotID -> equipped item instance */
	UPROPERTY(ReplicatedUsing = OnRep_EquippedItems)
	TArray<FInventoryItemInstance> EquippedItemsArray;

	/**
	 * We use an array for replication (TMaps can't replicate directly).
	 * SlotID is stored in GridPosition.X as a hash for fast lookup.
	 * The actual SlotID is preserved separately.
	 */
	UPROPERTY(Replicated)
	TArray<FName> EquippedSlotIDs;

	const FEquipmentSlotDefinition* FindSlotDef(FName SlotID) const;
	int32 FindEquippedIndex(FName SlotID) const;
	void Internal_SetSlot(FName SlotID, const FInventoryItemInstance& Item);
	void Internal_ClearSlot(FName SlotID);
};
