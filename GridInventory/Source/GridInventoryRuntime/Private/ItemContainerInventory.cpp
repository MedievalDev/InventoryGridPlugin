// Copyright Marco. All Rights Reserved.

#include "ItemContainerInventory.h"
#include "InventoryItemDefinition.h"

UItemContainerInventory::UItemContainerInventory()
	: bInitialized(false)
	, GridWidth(0)
	, GridHeight(0)
	, MaxWeight(0.0f)
	, CachedWeight(0.0f)
{
}

void UItemContainerInventory::Initialize(UInventoryItemDefinition* ContainerItemDef)
{
	if (!ContainerItemDef || !ContainerItemDef->bIsContainer)
	{
		return;
	}

	GridWidth = FMath::Max(1, ContainerItemDef->ContainerGridSize.X);
	GridHeight = FMath::Max(1, ContainerItemDef->ContainerGridSize.Y);
	MaxWeight = ContainerItemDef->ContainerMaxWeight;
	AcceptedTypes = ContainerItemDef->ContainerAcceptedTypes;

	Grid.Initialize(GridWidth, GridHeight);
	Items.Empty();
	ItemIndexMap.Empty();
	CachedWeight = 0.0f;
	bInitialized = true;
}

// ============================================================================
// Core Functions
// ============================================================================

bool UItemContainerInventory::AcceptsItem(UInventoryItemDefinition* ItemDef) const
{
	if (!ItemDef) return false;

	// RULE: Container items cannot go into container items
	if (ItemDef->bIsContainer) return false;

	// Check accepted types
	if (AcceptedTypes.Num() > 0)
	{
		return AcceptedTypes.Contains(ItemDef->ItemType);
	}

	return true; // Empty list = accept all non-containers
}

bool UItemContainerInventory::TryAddItem(UInventoryItemDefinition* ItemDef, int32 Count)
{
	if (!bInitialized || !ItemDef || Count <= 0) return false;
	if (!AcceptsItem(ItemDef)) return false;
	if (!CanAffordWeight(ItemDef->Weight * Count)) return false;

	int32 Remaining = Count;

	// Try stacking first
	if (ItemDef->bCanStack)
	{
		Remaining = TryStackOntoExisting(ItemDef, Remaining);
		if (Remaining <= 0)
		{
			RecalculateWeight();
			OnContainerChanged.Broadcast();
			return true;
		}
	}

	// Place remaining
	while (Remaining > 0)
	{
		const int32 PlaceCount = ItemDef->bCanStack
			? FMath::Min(Remaining, ItemDef->MaxStackSize) : 1;

		FGridPlacementResult Slot = Grid.FindFirstFreeSlot(ItemDef->GridSize, ItemDef->bCanRotate);
		if (!Slot.bSuccess) break;

		if (Internal_AddItemAt(ItemDef, Slot.Position, Slot.bRotated, PlaceCount))
		{
			Remaining -= PlaceCount;
		}
		else break;
	}

	RecalculateWeight();
	OnContainerChanged.Broadcast();
	return Remaining < Count;
}

bool UItemContainerInventory::TryAddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count)
{
	if (!bInitialized || !ItemDef || Count <= 0) return false;
	if (!AcceptsItem(ItemDef)) return false;
	if (!CanAffordWeight(ItemDef->Weight * Count)) return false;
	if (bRotated && !ItemDef->bCanRotate) bRotated = false;

	const FIntPoint EffSize = ItemDef->GetEffectiveSize(bRotated);
	if (!Grid.CanPlaceAt(Position, EffSize)) return false;

	const int32 PlaceCount = ItemDef->bCanStack
		? FMath::Min(Count, ItemDef->MaxStackSize) : 1;

	if (Internal_AddItemAt(ItemDef, Position, bRotated, PlaceCount))
	{
		RecalculateWeight();
		OnContainerChanged.Broadcast();
		return true;
	}
	return false;
}

bool UItemContainerInventory::RemoveItem(FGuid UniqueID, int32 Count)
{
	if (!UniqueID.IsValid()) return false;

	if (Internal_RemoveItem(UniqueID, Count))
	{
		RecalculateWeight();
		OnContainerChanged.Broadcast();
		return true;
	}
	return false;
}

bool UItemContainerInventory::MoveItem(FGuid UniqueID, FIntPoint NewPosition, bool bRotated)
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];
	UInventoryItemDefinition* Def = Item.GetItemDef();
	if (bRotated && (!Def || !Def->bCanRotate)) bRotated = false;

	const FIntPoint EffSize = Def->GetEffectiveSize(bRotated);
	if (!Grid.CanPlaceAtIgnoring(NewPosition, EffSize, UniqueID)) return false;

	// O(item_area) remove instead of O(total_cells)
	Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
	Grid.PlaceItem(UniqueID, NewPosition, EffSize);
	Item.GridPosition = NewPosition;
	Item.bIsRotated = bRotated;

	OnContainerChanged.Broadcast();
	return true;
}

bool UItemContainerInventory::RotateItem(FGuid UniqueID)
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];
	UInventoryItemDefinition* Def = Item.GetItemDef();
	if (!Def || !Def->bCanRotate) return false;

	if (Def->GridSize.X == Def->GridSize.Y)
	{
		Item.bIsRotated = !Item.bIsRotated;
		return true;
	}

	const bool bNewRot = !Item.bIsRotated;
	const FIntPoint NewSize = Def->GetEffectiveSize(bNewRot);

	if (Grid.CanPlaceAtIgnoring(Item.GridPosition, NewSize, UniqueID))
	{
		// O(item_area) remove instead of O(total_cells)
		Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
		Grid.PlaceItem(UniqueID, Item.GridPosition, NewSize);
		Item.bIsRotated = bNewRot;
		OnContainerChanged.Broadcast();
		return true;
	}
	return false;
}

// ============================================================================
// Query
// ============================================================================

const TArray<FInventoryItemInstance>& UItemContainerInventory::GetAllItems() const { return Items; }

FInventoryItemInstance UItemContainerInventory::GetItemByID(FGuid UniqueID) const
{
	const int32 Idx = FindItemIndex(UniqueID);
	return (Idx != INDEX_NONE) ? Items[Idx] : FInventoryItemInstance();
}

FInventoryItemInstance UItemContainerInventory::GetItemAtPosition(FIntPoint Position) const
{
	const FGuid ID = Grid.GetItemAt(Position);
	return ID.IsValid() ? GetItemByID(ID) : FInventoryItemInstance();
}

float UItemContainerInventory::GetCurrentWeight() const { return CachedWeight; }

float UItemContainerInventory::GetRemainingWeight() const
{
	return (MaxWeight <= 0.0f) ? TNumericLimits<float>::Max() : FMath::Max(0.0f, MaxWeight - CachedWeight);
}

bool UItemContainerInventory::HasSpaceFor(UInventoryItemDefinition* ItemDef, int32 Count) const
{
	if (!ItemDef || Count <= 0 || !AcceptsItem(ItemDef)) return false;
	if (MaxWeight > 0.0f && (CachedWeight + ItemDef->Weight * Count) > MaxWeight) return false;

	int32 Remaining = Count;
	if (ItemDef->bCanStack)
	{
		for (const FInventoryItemInstance& Item : Items)
		{
			if (Item.GetItemDef() == ItemDef && Item.StackCount < ItemDef->MaxStackSize)
			{
				Remaining -= (ItemDef->MaxStackSize - Item.StackCount);
				if (Remaining <= 0) return true;
			}
		}
	}
	return Grid.FindFirstFreeSlot(ItemDef->GridSize, ItemDef->bCanRotate).bSuccess;
}

int32 UItemContainerInventory::GetItemCount(UInventoryItemDefinition* ItemDef) const
{
	if (!ItemDef) return 0;
	int32 Total = 0;
	for (const FInventoryItemInstance& Item : Items)
	{
		if (Item.GetItemDef() == ItemDef) Total += Item.StackCount;
	}
	return Total;
}

bool UItemContainerInventory::IsPositionFree(FIntPoint Position) const { return Grid.IsCellFree(Position); }

bool UItemContainerInventory::CanPlaceAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated) const
{
	if (!ItemDef || !AcceptsItem(ItemDef)) return false;
	return Grid.CanPlaceAt(Position, ItemDef->GetEffectiveSize(bRotated));
}

int32 UItemContainerInventory::GetFreeCellCount() const { return Grid.GetFreeCellCount(); }

// ============================================================================
// Utility
// ============================================================================

void UItemContainerInventory::SortContainer()
{
	TArray<FInventoryItemInstance> Copy = Items;
	Copy.Sort([](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
	{
		return (A.GetEffectiveSize().X * A.GetEffectiveSize().Y) > (B.GetEffectiveSize().X * B.GetEffectiveSize().Y);
	});

	Grid.Clear();
	Items.Empty();
	ItemIndexMap.Empty();

	for (FInventoryItemInstance& Item : Copy)
	{
		UInventoryItemDefinition* Def = Item.GetItemDef();
		if (!Def) continue;
		FGridPlacementResult Slot = Grid.FindFirstFreeSlot(Def->GridSize, Def->bCanRotate);
		if (Slot.bSuccess)
		{
			Item.GridPosition = Slot.Position;
			Item.bIsRotated = Slot.bRotated;
			Grid.PlaceItem(Item.UniqueID, Slot.Position, Def->GetEffectiveSize(Slot.bRotated));
			ItemIndexMap.Add(Item.UniqueID, Items.Num());
			Items.Add(Item);
		}
	}
	OnContainerChanged.Broadcast();
}

void UItemContainerInventory::ClearContainer()
{
	Items.Empty();
	ItemIndexMap.Empty();
	Grid.Clear();
	CachedWeight = 0.0f;
	OnContainerChanged.Broadcast();
}

// ============================================================================
// Internal
// ============================================================================

void UItemContainerInventory::RecalculateWeight()
{
	CachedWeight = 0.0f;
	for (const FInventoryItemInstance& Item : Items)
	{
		CachedWeight += Item.GetTotalWeight();
	}
}

bool UItemContainerInventory::CanAffordWeight(float Additional) const
{
	return (MaxWeight <= 0.0f) || (CachedWeight + Additional) <= MaxWeight;
}

int32 UItemContainerInventory::FindItemIndex(const FGuid& UniqueID) const
{
	const int32* Found = ItemIndexMap.Find(UniqueID);
	return Found ? *Found : INDEX_NONE;
}

void UItemContainerInventory::SwapRemoveItemAtIndex(int32 Index)
{
	if (Index < 0 || Index >= Items.Num()) return;

	ItemIndexMap.Remove(Items[Index].UniqueID);

	const int32 LastIndex = Items.Num() - 1;
	if (Index < LastIndex)
	{
		Items[Index] = Items[LastIndex];
		ItemIndexMap.Add(Items[Index].UniqueID, Index);
	}
	Items.RemoveAt(LastIndex);
}

void UItemContainerInventory::RebuildItemIndexMap()
{
	ItemIndexMap.Empty(Items.Num());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		ItemIndexMap.Add(Items[i].UniqueID, i);
	}
}

int32 UItemContainerInventory::TryStackOntoExisting(UInventoryItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || !ItemDef->bCanStack) return Count;
	int32 Remaining = Count;
	for (FInventoryItemInstance& Item : Items)
	{
		if (Remaining <= 0) break;
		if (Item.GetItemDef() == ItemDef && Item.StackCount < ItemDef->MaxStackSize)
		{
			const int32 ToAdd = FMath::Min(Remaining, ItemDef->MaxStackSize - Item.StackCount);
			Item.StackCount += ToAdd;
			Remaining -= ToAdd;
		}
	}
	return Remaining;
}

bool UItemContainerInventory::Internal_AddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count)
{
	if (!ItemDef) return false;

	FInventoryItemInstance NewItem = FInventoryItemInstance::CreateNew(ItemDef, Count);
	NewItem.GridPosition = Position;
	NewItem.bIsRotated = bRotated;

	if (!Grid.PlaceItem(NewItem.UniqueID, Position, ItemDef->GetEffectiveSize(bRotated)))
		return false;

	// Update map BEFORE adding (index = current array size)
	ItemIndexMap.Add(NewItem.UniqueID, Items.Num());
	Items.Add(NewItem);
	return true;
}

bool UItemContainerInventory::Internal_RemoveItem(const FGuid& UniqueID, int32 Count)
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];

	if (Count <= 0 || Count >= Item.StackCount)
	{
		// O(item_area) grid remove + O(1) array swap
		Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
		SwapRemoveItemAtIndex(Index);
	}
	else
	{
		Item.StackCount -= Count;
	}
	return true;
}
