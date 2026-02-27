// Copyright Marco. All Rights Reserved.

#include "EquipmentComponent.h"
#include "GridInventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "Net/UnrealNetwork.h"

UEquipmentComponent::UEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	LinkedInventory = nullptr;
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find linked inventory on same actor if not set
	if (!LinkedInventory)
	{
		LinkedInventory = GetOwner()->FindComponentByClass<UGridInventoryComponent>();
	}
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEquipmentComponent, EquippedItemsArray);
	DOREPLIFETIME(UEquipmentComponent, EquippedSlotIDs);
}

// ============================================================================
// Core Functions
// ============================================================================

bool UEquipmentComponent::EquipItem(FName SlotID, UInventoryItemDefinition* ItemDef, int32 StackCount)
{
	if (!ItemDef || SlotID == NAME_None || StackCount <= 0)
	{
		return false;
	}

	const FEquipmentSlotDefinition* SlotDef = FindSlotDef(SlotID);
	if (!SlotDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("Equipment: Slot '%s' not found"), *SlotID.ToString());
		return false;
	}

	// Check if item type is accepted
	if (!SlotDef->AcceptsItemType(ItemDef->ItemType))
	{
		return false;
	}

	// Check stacking rules
	if (!SlotDef->bAllowStacking && StackCount > 1)
	{
		StackCount = 1;
	}

	if (SlotDef->bAllowStacking)
	{
		StackCount = FMath::Min(StackCount, SlotDef->MaxStackSize);
	}

	// Check if slot already has something
	const int32 ExistingIdx = FindEquippedIndex(SlotID);
	if (ExistingIdx != INDEX_NONE)
	{
		// Try stacking onto existing
		FInventoryItemInstance& Existing = EquippedItemsArray[ExistingIdx];
		if (Existing.ItemDef == ItemDef && SlotDef->bAllowStacking)
		{
			const int32 CanAdd = SlotDef->MaxStackSize - Existing.StackCount;
			if (CanAdd > 0)
			{
				const int32 ToAdd = FMath::Min(StackCount, CanAdd);
				Existing.StackCount += ToAdd;
				OnEquipmentChanged.Broadcast();
				return true;
			}
			return false; // Stack full
		}

		// Swap: unequip existing first
		UnequipItem(SlotID, 0);
	}

	// Create the equipped item instance
	FInventoryItemInstance NewItem = FInventoryItemInstance::CreateNew(ItemDef, StackCount);

	Internal_SetSlot(SlotID, NewItem);

	OnItemEquipped.Broadcast(SlotID, NewItem);
	OnEquipmentChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::EquipFromInventory(FName SlotID, FGuid InventoryItemID)
{
	if (!LinkedInventory || !InventoryItemID.IsValid())
	{
		return false;
	}

	FInventoryItemInstance InvItem = LinkedInventory->GetItemByID(InventoryItemID);
	if (!InvItem.IsValid())
	{
		return false;
	}

	// Check compatibility
	if (!CanEquipInSlot(SlotID, InvItem.ItemDef))
	{
		return false;
	}

	// Check if slot has something to swap
	const int32 ExistingIdx = FindEquippedIndex(SlotID);
	FInventoryItemInstance OldItem;
	if (ExistingIdx != INDEX_NONE)
	{
		OldItem = EquippedItemsArray[ExistingIdx];
	}

	// Remove from inventory
	LinkedInventory->RemoveItem(InventoryItemID, 0);

	// If there was an old item, return it to inventory
	if (OldItem.IsValid())
	{
		Internal_ClearSlot(SlotID);
		OnItemUnequipped.Broadcast(SlotID, OldItem);
		LinkedInventory->TryAddItem(OldItem.ItemDef, OldItem.StackCount);
	}

	// Equip new item
	FInventoryItemInstance EquipItem = FInventoryItemInstance::CreateNew(InvItem.ItemDef, InvItem.StackCount);
	Internal_SetSlot(SlotID, EquipItem);

	OnItemEquipped.Broadcast(SlotID, EquipItem);
	OnEquipmentChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::UnequipItem(FName SlotID, int32 Count)
{
	const int32 Idx = FindEquippedIndex(SlotID);
	if (Idx == INDEX_NONE)
	{
		return false;
	}

	FInventoryItemInstance& Item = EquippedItemsArray[Idx];

	// Partial unequip from stack
	if (Count > 0 && Count < Item.StackCount)
	{
		// Return partial stack to inventory
		if (LinkedInventory)
		{
			LinkedInventory->TryAddItem(Item.ItemDef, Count);
		}
		Item.StackCount -= Count;
		OnEquipmentChanged.Broadcast();
		return true;
	}

	// Full unequip
	FInventoryItemInstance RemovedItem = Item;

	// Return to inventory
	if (LinkedInventory)
	{
		if (!LinkedInventory->TryAddItem(RemovedItem.ItemDef, RemovedItem.StackCount))
		{
			UE_LOG(LogTemp, Warning, TEXT("Equipment: Inventory full, cannot unequip '%s'"),
				*RemovedItem.ItemDef->DisplayName.ToString());
			return false;
		}
	}

	Internal_ClearSlot(SlotID);

	OnItemUnequipped.Broadcast(SlotID, RemovedItem);
	OnEquipmentChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::SwapSlots(FName SlotA, FName SlotB)
{
	const int32 IdxA = FindEquippedIndex(SlotA);
	const int32 IdxB = FindEquippedIndex(SlotB);

	// At least one must be occupied
	if (IdxA == INDEX_NONE && IdxB == INDEX_NONE)
	{
		return false;
	}

	FInventoryItemInstance ItemA = (IdxA != INDEX_NONE) ? EquippedItemsArray[IdxA] : FInventoryItemInstance();
	FInventoryItemInstance ItemB = (IdxB != INDEX_NONE) ? EquippedItemsArray[IdxB] : FInventoryItemInstance();

	// Check type compatibility
	if (ItemA.IsValid())
	{
		const FEquipmentSlotDefinition* DefB = FindSlotDef(SlotB);
		if (DefB && !DefB->AcceptsItemType(ItemA.ItemDef->ItemType))
		{
			return false;
		}
	}
	if (ItemB.IsValid())
	{
		const FEquipmentSlotDefinition* DefA = FindSlotDef(SlotA);
		if (DefA && !DefA->AcceptsItemType(ItemB.ItemDef->ItemType))
		{
			return false;
		}
	}

	// Perform swap
	Internal_ClearSlot(SlotA);
	Internal_ClearSlot(SlotB);

	if (ItemB.IsValid()) Internal_SetSlot(SlotA, ItemB);
	if (ItemA.IsValid()) Internal_SetSlot(SlotB, ItemA);

	OnEquipmentChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::ConsumeFromSlot(FName SlotID, int32 Count)
{
	const int32 Idx = FindEquippedIndex(SlotID);
	if (Idx == INDEX_NONE)
	{
		return false;
	}

	FInventoryItemInstance& Item = EquippedItemsArray[Idx];
	if (Item.StackCount < Count)
	{
		return false;
	}

	Item.StackCount -= Count;

	if (Item.StackCount <= 0)
	{
		FInventoryItemInstance Removed = Item;
		Internal_ClearSlot(SlotID);
		OnItemUnequipped.Broadcast(SlotID, Removed);
	}

	OnEquipmentChanged.Broadcast();
	return true;
}

// ============================================================================
// Query Functions
// ============================================================================

FInventoryItemInstance UEquipmentComponent::GetItemInSlot(FName SlotID) const
{
	const int32 Idx = FindEquippedIndex(SlotID);
	if (Idx != INDEX_NONE)
	{
		return EquippedItemsArray[Idx];
	}
	return FInventoryItemInstance();
}

bool UEquipmentComponent::IsSlotOccupied(FName SlotID) const
{
	return FindEquippedIndex(SlotID) != INDEX_NONE;
}

bool UEquipmentComponent::IsSlotEmpty(FName SlotID) const
{
	return FindEquippedIndex(SlotID) == INDEX_NONE;
}

bool UEquipmentComponent::CanEquipInSlot(FName SlotID, UInventoryItemDefinition* ItemDef) const
{
	if (!ItemDef) return false;

	const FEquipmentSlotDefinition* SlotDef = FindSlotDef(SlotID);
	if (!SlotDef) return false;

	return SlotDef->AcceptsItemType(ItemDef->ItemType);
}

FName UEquipmentComponent::FindEmptySlotForItem(UInventoryItemDefinition* ItemDef) const
{
	if (!ItemDef) return NAME_None;

	for (const FEquipmentSlotDefinition& Def : SlotDefinitions)
	{
		if (Def.AcceptsItemType(ItemDef->ItemType) && IsSlotEmpty(Def.SlotID))
		{
			return Def.SlotID;
		}
	}

	return NAME_None;
}

TArray<FEquipmentSlotDefinition> UEquipmentComponent::GetAllSlotDefinitions() const
{
	return SlotDefinitions;
}

FEquipmentSlotDefinition UEquipmentComponent::GetSlotDefinition(FName SlotID) const
{
	const FEquipmentSlotDefinition* Def = FindSlotDef(SlotID);
	return Def ? *Def : FEquipmentSlotDefinition();
}

TMap<FName, FInventoryItemInstance> UEquipmentComponent::GetAllEquippedItems() const
{
	TMap<FName, FInventoryItemInstance> Result;
	for (int32 i = 0; i < EquippedSlotIDs.Num(); ++i)
	{
		if (EquippedItemsArray.IsValidIndex(i))
		{
			Result.Add(EquippedSlotIDs[i], EquippedItemsArray[i]);
		}
	}
	return Result;
}

float UEquipmentComponent::GetTotalEquipmentWeight() const
{
	float Total = 0.0f;
	for (const FInventoryItemInstance& Item : EquippedItemsArray)
	{
		Total += Item.GetTotalWeight();
	}
	return Total;
}

// ============================================================================
// Replication
// ============================================================================

void UEquipmentComponent::OnRep_EquippedItems()
{
	OnEquipmentChanged.Broadcast();
}

// ============================================================================
// Internal
// ============================================================================

const FEquipmentSlotDefinition* UEquipmentComponent::FindSlotDef(FName SlotID) const
{
	for (const FEquipmentSlotDefinition& Def : SlotDefinitions)
	{
		if (Def.SlotID == SlotID)
		{
			return &Def;
		}
	}
	return nullptr;
}

int32 UEquipmentComponent::FindEquippedIndex(FName SlotID) const
{
	return EquippedSlotIDs.IndexOfByKey(SlotID);
}

void UEquipmentComponent::Internal_SetSlot(FName SlotID, const FInventoryItemInstance& Item)
{
	const int32 Idx = FindEquippedIndex(SlotID);
	if (Idx != INDEX_NONE)
	{
		EquippedItemsArray[Idx] = Item;
	}
	else
	{
		EquippedSlotIDs.Add(SlotID);
		EquippedItemsArray.Add(Item);
	}
}

void UEquipmentComponent::Internal_ClearSlot(FName SlotID)
{
	const int32 Idx = FindEquippedIndex(SlotID);
	if (Idx != INDEX_NONE)
	{
		EquippedSlotIDs.RemoveAt(Idx);
		EquippedItemsArray.RemoveAt(Idx);
	}
}

// ============================================================================
// Save/Load Support
// ============================================================================

void UEquipmentComponent::RestoreEquipmentSlot(FName SlotID, const FInventoryItemInstance& Item)
{
	Internal_ClearSlot(SlotID);
	Internal_SetSlot(SlotID, Item);
}

void UEquipmentComponent::ClearAllSlots()
{
	EquippedSlotIDs.Empty();
	EquippedItemsArray.Empty();
	OnEquipmentChanged.Broadcast();
}
