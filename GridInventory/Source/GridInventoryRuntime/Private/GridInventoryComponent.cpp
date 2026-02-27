// Copyright Marco. All Rights Reserved.

#include "GridInventoryComponent.h"
#include "ItemContainerInventory.h"
#include "InventorySaveGame.h"
#include "EquipmentComponent.h"
#include "InventoryContainer.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

UGridInventoryComponent::UGridInventoryComponent()
	: GridWidth(10)
	, GridHeight(10)
	, MaxWeight(0.0f)
	, CachedWeight(0.0f)
	, bInventoryDirty(false)
	, bBroadcastPending(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick when dirty
	SetIsReplicatedByDefault(true);
}

void UGridInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Grid.Initialize(GridWidth, GridHeight);
}

void UGridInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGridInventoryComponent, Items);
}

// ============================================================================
// Core Functions
// ============================================================================

bool UGridInventoryComponent::TryAddItem(UInventoryItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || Count <= 0) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerTryAddItem(ItemDef, Count);
		return true;
	}

	const float AdditionalWeight = ItemDef->Weight * Count;
	if (!CanAffordWeight(AdditionalWeight)) return false;

	int32 Remaining = Count;

	// Stack onto existing first
	if (ItemDef->bCanStack)
	{
		Remaining = TryStackOntoExisting(ItemDef, Remaining);
		if (Remaining <= 0)
		{
			RecalculateWeight();
			BroadcastChanged();
			return true;
		}
	}

	// Place remaining as new items
	while (Remaining > 0)
	{
		const int32 PlaceCount = ItemDef->bCanStack
			? FMath::Min(Remaining, ItemDef->MaxStackSize)
			: 1;

		FGridPlacementResult Slot = Grid.FindFirstFreeSlot(ItemDef->GridSize, ItemDef->bCanRotate);
		if (!Slot.bSuccess) break;

		if (Internal_AddItemAt(ItemDef, Slot.Position, Slot.bRotated, PlaceCount))
		{
			Remaining -= PlaceCount;
		}
		else
		{
			break;
		}
	}

	RecalculateWeight();
	BroadcastChanged();
	return Remaining < Count;
}

bool UGridInventoryComponent::TryAddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count)
{
	if (!ItemDef || Count <= 0) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerTryAddItem(ItemDef, Count);
		return true;
	}

	if (!CanAffordWeight(ItemDef->Weight * Count)) return false;
	if (bRotated && !ItemDef->bCanRotate) bRotated = false;

	const FIntPoint EffSize = ItemDef->GetEffectiveSize(bRotated);
	if (!Grid.CanPlaceAt(Position, EffSize)) return false;

	const int32 PlaceCount = ItemDef->bCanStack
		? FMath::Min(Count, ItemDef->MaxStackSize) : 1;

	if (Internal_AddItemAt(ItemDef, Position, bRotated, PlaceCount))
	{
		RecalculateWeight();
		BroadcastChanged();
		return true;
	}
	return false;
}

int32 UGridInventoryComponent::TryAddItemsBatch(const TArray<FItemAddRequest>& Requests)
{
	if (Requests.Num() == 0) return 0;

	// First: try stacking all stackable items
	TArray<FItemAddRequest> NeedPlacement;
	int32 AddedCount = 0;

	for (const FItemAddRequest& Req : Requests)
	{
		if (!Req.ItemDef || Req.Count <= 0) continue;

		int32 Remaining = Req.Count;

		if (Req.ItemDef->bCanStack)
		{
			Remaining = TryStackOntoExisting(Req.ItemDef, Remaining);
		}

		if (Remaining > 0)
		{
			// Split into max-stack-size chunks
			while (Remaining > 0)
			{
				const int32 Chunk = Req.ItemDef->bCanStack
					? FMath::Min(Remaining, Req.ItemDef->MaxStackSize) : 1;

				NeedPlacement.Add(FItemAddRequest(Req.ItemDef, Chunk));
				Remaining -= Chunk;
			}
		}
		else
		{
			AddedCount++;
		}
	}

	// Batch find slots for remaining items
	if (NeedPlacement.Num() > 0)
	{
		TArray<FGridPlacementResult> Slots = Grid.FindSlotsForBatch(NeedPlacement);

		for (int32 i = 0; i < NeedPlacement.Num(); ++i)
		{
			if (Slots[i].bSuccess)
			{
				const FItemAddRequest& Req = NeedPlacement[i];
				if (!CanAffordWeight(Req.ItemDef->Weight * Req.Count)) continue;

				if (Internal_AddItemAt(Req.ItemDef, Slots[i].Position, Slots[i].bRotated, Req.Count))
				{
					AddedCount++;
				}
			}
		}
	}

	RecalculateWeight();
	BroadcastChanged();
	return AddedCount;
}

bool UGridInventoryComponent::RemoveItem(FGuid UniqueID, int32 Count)
{
	if (!UniqueID.IsValid()) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerRemoveItem(UniqueID, Count);
		return true;
	}

	if (Internal_RemoveItem(UniqueID, Count))
	{
		RecalculateWeight();
		BroadcastChanged();
		return true;
	}
	return false;
}

int32 UGridInventoryComponent::RemoveItemByDef(UInventoryItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || Count <= 0) return 0;

	int32 RemovedTotal = 0;
	int32 Remaining = Count;

	for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i)
	{
		if (Items[i].ItemDef == ItemDef)
		{
			const int32 ToRemove = FMath::Min(Remaining, Items[i].StackCount);
			Items[i].StackCount -= ToRemove;
			Remaining -= ToRemove;
			RemovedTotal += ToRemove;

			if (Items[i].StackCount <= 0)
			{
				Grid.RemoveItem(Items[i].UniqueID);
				BroadcastItemRemoved(Items[i]);
				Items.RemoveAt(i);
			}
		}
	}

	if (RemovedTotal > 0)
	{
		RecalculateWeight();
		BroadcastChanged();
	}
	return RemovedTotal;
}

bool UGridInventoryComponent::MoveItem(FGuid UniqueID, FIntPoint NewPosition, bool bRotated)
{
	if (!UniqueID.IsValid()) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerMoveItem(UniqueID, NewPosition, bRotated);
		return true;
	}

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];
	if (bRotated && (!Item.ItemDef || !Item.ItemDef->bCanRotate)) bRotated = false;

	const FIntPoint EffSize = Item.ItemDef->GetEffectiveSize(bRotated);
	if (!Grid.CanPlaceAtIgnoring(NewPosition, EffSize, UniqueID)) return false;

	Grid.RemoveItem(UniqueID);
	Grid.PlaceItem(UniqueID, NewPosition, EffSize);
	Item.GridPosition = NewPosition;
	Item.bIsRotated = bRotated;

	BroadcastItemMoved(Item);
	BroadcastChanged();
	return true;
}

bool UGridInventoryComponent::RotateItem(FGuid UniqueID)
{
	if (!UniqueID.IsValid()) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerRotateItem(UniqueID);
		return true;
	}

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];
	if (!Item.ItemDef || !Item.ItemDef->bCanRotate) return false;

	if (Item.ItemDef->GridSize.X == Item.ItemDef->GridSize.Y)
	{
		Item.bIsRotated = !Item.bIsRotated;
		return true;
	}

	const bool bNewRotated = !Item.bIsRotated;
	const FIntPoint NewSize = Item.ItemDef->GetEffectiveSize(bNewRotated);

	if (Grid.CanPlaceAtIgnoring(Item.GridPosition, NewSize, UniqueID))
	{
		Grid.RemoveItem(UniqueID);
		Grid.PlaceItem(UniqueID, Item.GridPosition, NewSize);
		Item.bIsRotated = bNewRotated;
		BroadcastItemMoved(Item);
		BroadcastChanged();
		return true;
	}
	return false;
}

bool UGridInventoryComponent::TransferItem(FGuid UniqueID, UGridInventoryComponent* TargetInventory, int32 Count)
{
	if (!TargetInventory || !UniqueID.IsValid() || TargetInventory == this) return false;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerTransferItem(UniqueID, TargetInventory, Count);
		return true;
	}

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	const FInventoryItemInstance& Item = Items[Index];
	const int32 TransferCount = (Count <= 0) ? Item.StackCount : FMath::Min(Count, Item.StackCount);

	if (TargetInventory->TryAddItem(Item.ItemDef, TransferCount))
	{
		Internal_RemoveItem(UniqueID, TransferCount);
		RecalculateWeight();
		BroadcastChanged();
		return true;
	}
	return false;
}

bool UGridInventoryComponent::TransferItemTo(FGuid UniqueID, UGridInventoryComponent* TargetInventory, FIntPoint TargetPosition, bool bRotated, int32 Count)
{
	if (!TargetInventory || !UniqueID.IsValid() || TargetInventory == this) return false;

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	const FInventoryItemInstance& Item = Items[Index];
	const int32 TransferCount = (Count <= 0) ? Item.StackCount : FMath::Min(Count, Item.StackCount);

	if (TargetInventory->TryAddItemAt(Item.ItemDef, TargetPosition, bRotated, TransferCount))
	{
		Internal_RemoveItem(UniqueID, TransferCount);
		RecalculateWeight();
		BroadcastChanged();
		return true;
	}
	return false;
}

// ============================================================================
// Query Functions
// ============================================================================

const TArray<FInventoryItemInstance>& UGridInventoryComponent::GetAllItems() const { return Items; }

// ============================================================================
// Consume / Use
// ============================================================================

bool UGridInventoryComponent::ConsumeItem(FGuid UniqueID)
{
	if (!UniqueID.IsValid()) return false;

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];
	if (!Item.ItemDef || !Item.ItemDef->bIsConsumable) return false;

	// Copy for the event (before we potentially destroy it)
	FInventoryItemInstance ConsumedItemCopy = Item;

	// Fire the consume event so gameplay logic can respond
	OnItemConsumed.Broadcast(ConsumedItemCopy);

	// Destroy if configured
	if (Item.ItemDef->bDestroyOnConsume)
	{
		Item.StackCount -= 1;
		if (Item.StackCount <= 0)
		{
			Grid.RemoveItem(UniqueID);
			Items.RemoveAt(Index);
			BroadcastItemRemoved(ConsumedItemCopy);
		}
		RecalculateWeight();
		BroadcastChanged();
	}

	return true;
}

// ============================================================================
// Drop / Pickup
// ============================================================================

AActor* UGridInventoryComponent::DropItem(FGuid UniqueID, FVector SpawnLocation, FRotator SpawnRotation, int32 Count)
{
	if (!UniqueID.IsValid()) return nullptr;

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return nullptr;

	FInventoryItemInstance& Item = Items[Index];
	if (!Item.ItemDef) return nullptr;

	const int32 DropCount = (Count <= 0) ? Item.StackCount : FMath::Min(Count, Item.StackCount);

	// Spawn world actor (load soft class ref)
	AActor* SpawnedActor = nullptr;
	if (!Item.ItemDef->WorldActorClass.IsNull())
	{
		UClass* LoadedClass = Item.ItemDef->WorldActorClass.LoadSynchronous();
		UWorld* World = GetWorld();
		if (LoadedClass && World)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnedActor = World->SpawnActor<AActor>(LoadedClass, SpawnLocation, SpawnRotation, Params);
		}
	}

	// Copy for event
	FInventoryItemInstance DroppedCopy = Item;
	DroppedCopy.StackCount = DropCount;

	// Remove from inventory
	Internal_RemoveItem(UniqueID, DropCount);
	RecalculateWeight();
	BroadcastChanged();

	OnItemDropped.Broadcast(DroppedCopy, SpawnedActor);
	return SpawnedActor;
}

bool UGridInventoryComponent::PickupItem(UInventoryItemDefinition* ItemDef, int32 Count, AActor* WorldActor)
{
	if (!ItemDef || Count <= 0) return false;

	if (TryAddItem(ItemDef, Count))
	{
		// Destroy world actor if provided
		if (WorldActor)
		{
			WorldActor->Destroy();
		}
		return true;
	}

	return false;
}

// ============================================================================
// Merge / Upgrade
// ============================================================================

bool UGridInventoryComponent::CanMergeItems(FGuid TargetID, FGuid SacrificeID) const
{
	if (!TargetID.IsValid() || !SacrificeID.IsValid() || TargetID == SacrificeID) return false;

	const int32 TargetIdx = FindItemIndex(TargetID);
	const int32 SacIdx = FindItemIndex(SacrificeID);
	if (TargetIdx == INDEX_NONE || SacIdx == INDEX_NONE) return false;

	return Items[TargetIdx].CanMergeWith(Items[SacIdx]);
}

bool UGridInventoryComponent::MergeItems(FGuid TargetID, FGuid SacrificeID)
{
	if (!CanMergeItems(TargetID, SacrificeID)) return false;

	const int32 TargetIdx = FindItemIndex(TargetID);
	const int32 SacIdx = FindItemIndex(SacrificeID);

	// Destroy sacrifice
	FInventoryItemInstance SacCopy = Items[SacIdx];
	Grid.RemoveItem(SacrificeID);
	BroadcastItemRemoved(SacCopy);
	Items.RemoveAt(SacIdx);

	// Upgrade target (re-find index since array may have shifted)
	const int32 NewTargetIdx = FindItemIndex(TargetID);
	if (NewTargetIdx != INDEX_NONE)
	{
		Items[NewTargetIdx].CurrentClassLevel += 1;
		OnItemMerged.Broadcast(Items[NewTargetIdx], Items[NewTargetIdx].CurrentClassLevel);
	}

	RecalculateWeight();
	BroadcastChanged();
	return true;
}

FInventoryItemInstance UGridInventoryComponent::GetItemByID(FGuid UniqueID) const
{
	const int32 Idx = FindItemIndex(UniqueID);
	return (Idx != INDEX_NONE) ? Items[Idx] : FInventoryItemInstance();
}

FInventoryItemInstance UGridInventoryComponent::GetItemAtPosition(FIntPoint Position) const
{
	const FGuid ID = Grid.GetItemAt(Position);
	return ID.IsValid() ? GetItemByID(ID) : FInventoryItemInstance();
}

float UGridInventoryComponent::GetCurrentWeight() const { return CachedWeight; }

float UGridInventoryComponent::GetRemainingWeight() const
{
	return (MaxWeight <= 0.0f) ? TNumericLimits<float>::Max() : FMath::Max(0.0f, MaxWeight - CachedWeight);
}

bool UGridInventoryComponent::HasSpaceFor(UInventoryItemDefinition* ItemDef, int32 Count) const
{
	if (!ItemDef || Count <= 0) return false;

	if (MaxWeight > 0.0f && (CachedWeight + ItemDef->Weight * Count) > MaxWeight) return false;

	int32 Remaining = Count;
	if (ItemDef->bCanStack)
	{
		for (const FInventoryItemInstance& Item : Items)
		{
			if (Item.ItemDef == ItemDef && Item.StackCount < ItemDef->MaxStackSize)
			{
				Remaining -= (ItemDef->MaxStackSize - Item.StackCount);
				if (Remaining <= 0) return true;
			}
		}
	}

	return Grid.FindFirstFreeSlot(ItemDef->GridSize, ItemDef->bCanRotate).bSuccess;
}

int32 UGridInventoryComponent::GetItemCount(UInventoryItemDefinition* ItemDef) const
{
	if (!ItemDef) return 0;
	int32 Total = 0;
	for (const FInventoryItemInstance& Item : Items)
	{
		if (Item.ItemDef == ItemDef) Total += Item.StackCount;
	}
	return Total;
}

bool UGridInventoryComponent::ContainsItem(UInventoryItemDefinition* ItemDef, int32 MinCount) const
{
	return GetItemCount(ItemDef) >= MinCount;
}

bool UGridInventoryComponent::IsPositionFree(FIntPoint Position) const { return Grid.IsCellFree(Position); }

bool UGridInventoryComponent::CanPlaceAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated) const
{
	if (!ItemDef) return false;
	return Grid.CanPlaceAt(Position, ItemDef->GetEffectiveSize(bRotated));
}

int32 UGridInventoryComponent::GetFreeCellCount() const { return Grid.GetFreeCellCount(); }

float UGridInventoryComponent::GetOccupancyPercent() const
{
	const int32 Total = Grid.GetTotalCells();
	return (Total > 0) ? (float)Grid.GetOccupiedCellCount() / (float)Total : 0.0f;
}

// ============================================================================
// Item Container Access
// ============================================================================

UItemContainerInventory* UGridInventoryComponent::OpenContainerItem(FGuid UniqueID)
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return nullptr;

	FInventoryItemInstance& Item = Items[Index];
	if (!Item.IsContainer()) return nullptr;

	return Item.GetOrCreateSubInventory(this);
}

UItemContainerInventory* UGridInventoryComponent::GetContainerInventory(FGuid UniqueID) const
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return nullptr;
	return Items[Index].SubInventory;
}

bool UGridInventoryComponent::IsContainerItem(FGuid UniqueID) const
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;
	return Items[Index].IsContainer();
}

float UGridInventoryComponent::GetTotalWeightRecursive() const
{
	float Total = 0.0f;
	for (const FInventoryItemInstance& Item : Items)
	{
		Total += Item.GetTotalWeight(); // Already includes sub-inventory weight
	}
	return Total;
}

// ============================================================================
// Utility
// ============================================================================

void UGridInventoryComponent::SortInventory()
{
	if (GetOwnerRole() < ROLE_Authority) return;

	TArray<FInventoryItemInstance> ItemsCopy = Items;

	ItemsCopy.Sort([](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
	{
		const int32 AreaA = A.GetEffectiveSize().X * A.GetEffectiveSize().Y;
		const int32 AreaB = B.GetEffectiveSize().X * B.GetEffectiveSize().Y;
		return AreaA > AreaB;
	});

	Grid.Clear();
	Items.Empty();

	for (FInventoryItemInstance& Item : ItemsCopy)
	{
		FGridPlacementResult Slot = Grid.FindFirstFreeSlot(Item.ItemDef->GridSize, Item.ItemDef->bCanRotate);
		if (Slot.bSuccess)
		{
			Item.GridPosition = Slot.Position;
			Item.bIsRotated = Slot.bRotated;
			Grid.PlaceItem(Item.UniqueID, Slot.Position, Item.ItemDef->GetEffectiveSize(Slot.bRotated));
			Items.Add(Item);
		}
	}

	BroadcastChanged();
}

void UGridInventoryComponent::ClearInventory()
{
	for (const FInventoryItemInstance& Item : Items) BroadcastItemRemoved(Item);
	Items.Empty();
	Grid.Clear();
	CachedWeight = 0.0f;
	BroadcastChanged();
}

void UGridInventoryComponent::InitializeGrid()
{
	ClearInventory();
	Grid.Initialize(GridWidth, GridHeight);
}

// ============================================================================
// Network RPCs
// ============================================================================

bool UGridInventoryComponent::ServerTryAddItem_Validate(UInventoryItemDefinition* ItemDef, int32 Count)
{ return ItemDef != nullptr && Count > 0 && Count < 10000; }
void UGridInventoryComponent::ServerTryAddItem_Implementation(UInventoryItemDefinition* ItemDef, int32 Count)
{ TryAddItem(ItemDef, Count); }

bool UGridInventoryComponent::ServerMoveItem_Validate(FGuid UniqueID, FIntPoint NewPosition, bool bRotated)
{ return UniqueID.IsValid(); }
void UGridInventoryComponent::ServerMoveItem_Implementation(FGuid UniqueID, FIntPoint NewPosition, bool bRotated)
{ MoveItem(UniqueID, NewPosition, bRotated); }

bool UGridInventoryComponent::ServerRotateItem_Validate(FGuid UniqueID) { return UniqueID.IsValid(); }
void UGridInventoryComponent::ServerRotateItem_Implementation(FGuid UniqueID) { RotateItem(UniqueID); }

bool UGridInventoryComponent::ServerRemoveItem_Validate(FGuid UniqueID, int32 Count) { return UniqueID.IsValid(); }
void UGridInventoryComponent::ServerRemoveItem_Implementation(FGuid UniqueID, int32 Count) { RemoveItem(UniqueID, Count); }

bool UGridInventoryComponent::ServerTransferItem_Validate(FGuid UniqueID, UGridInventoryComponent* T, int32 Count)
{ return UniqueID.IsValid() && T != nullptr; }
void UGridInventoryComponent::ServerTransferItem_Implementation(FGuid UniqueID, UGridInventoryComponent* T, int32 Count)
{ TransferItem(UniqueID, T, Count); }

void UGridInventoryComponent::OnRep_Items() { RebuildGridFromItems(); BroadcastChanged(); }

// ============================================================================
// Save / Load
// ============================================================================

FString UGridInventoryComponent::GetSaveSlotName(int32 SlotIndex)
{
	return FString::Printf(TEXT("Inventory_%d"), SlotIndex);
}

FItemSaveEntry UGridInventoryComponent::CreateSaveEntry(const FInventoryItemInstance& Item)
{
	FItemSaveEntry Entry;

	if (Item.ItemDef)
	{
		Entry.ItemDefPath = FSoftObjectPath(Item.ItemDef);
		Entry.bIsRuntimeCreated = false;
		// TODO: detect URuntimeItemDefinition and serialize as JSON
	}

	Entry.GridPosition = Item.GridPosition;
	Entry.bIsRotated = Item.bIsRotated;
	Entry.StackCount = Item.StackCount;
	Entry.CurrentClassLevel = Item.CurrentClassLevel;
	Entry.UniqueID = Item.UniqueID;
	Entry.InstanceEffects = Item.InstanceEffects;

	// Save sub-inventory contents (for container items)
	if (Item.SubInventory && Item.SubInventory->IsInitialized())
	{
		for (const FInventoryItemInstance& SubItem : Item.SubInventory->GetAllItems())
		{
			Entry.SubInventoryItems.Add(CreateSaveEntry(SubItem));
		}
	}

	return Entry;
}

bool UGridInventoryComponent::RestoreItemFromEntry(const FItemSaveEntry& Entry)
{
	UInventoryItemDefinition* ItemDef = nullptr;

	if (!Entry.bIsRuntimeCreated)
	{
		ItemDef = Cast<UInventoryItemDefinition>(Entry.ItemDefPath.TryLoad());
		if (!ItemDef)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveLoad: Failed to load ItemDef '%s'"),
				*Entry.ItemDefPath.ToString());
			return false;
		}
	}
	else
	{
		// TODO: Create URuntimeItemDefinition from RuntimeItemJSON
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveLoad: Runtime items not yet supported"));
		return false;
	}

	// Create the item instance with the SAVED UniqueID (not a new one)
	FInventoryItemInstance NewItem;
	NewItem.UniqueID = Entry.UniqueID;
	NewItem.ItemDef = ItemDef;
	NewItem.GridPosition = Entry.GridPosition;
	NewItem.bIsRotated = Entry.bIsRotated;
	NewItem.StackCount = Entry.StackCount;
	NewItem.CurrentClassLevel = Entry.CurrentClassLevel;
	NewItem.InstanceEffects = Entry.InstanceEffects;

	// Place on the grid
	const FIntPoint EffSize = ItemDef->GetEffectiveSize(NewItem.bIsRotated);
	if (!Grid.PlaceItem(NewItem.UniqueID, NewItem.GridPosition, EffSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveLoad: Cannot place '%s' at (%d,%d)"),
			*ItemDef->DisplayName.ToString(), NewItem.GridPosition.X, NewItem.GridPosition.Y);
		return false;
	}

	Items.Add(NewItem);

	// Restore sub-inventory if this is a container item
	if (Entry.SubInventoryItems.Num() > 0 && ItemDef->bIsContainer)
	{
		FInventoryItemInstance& AddedItem = Items.Last();
		UItemContainerInventory* SubInv = AddedItem.GetOrCreateSubInventory(this);
		if (SubInv)
		{
			for (const FItemSaveEntry& SubEntry : Entry.SubInventoryItems)
			{
				UInventoryItemDefinition* SubDef = nullptr;
				if (!SubEntry.bIsRuntimeCreated)
				{
					SubDef = Cast<UInventoryItemDefinition>(SubEntry.ItemDefPath.TryLoad());
				}
				if (SubDef)
				{
					SubInv->TryAddItemAt(SubDef, SubEntry.GridPosition, SubEntry.bIsRotated, SubEntry.StackCount);
				}
			}
		}
	}

	return true;
}

bool UGridInventoryComponent::SaveInventory(int32 SlotIndex)
{
	UInventorySaveGame* SaveGame = NewObject<UInventorySaveGame>();
	SaveGame->GridWidth = GridWidth;
	SaveGame->GridHeight = GridHeight;

	// 1. Save player inventory items
	for (const FInventoryItemInstance& Item : Items)
	{
		SaveGame->InventoryItems.Add(CreateSaveEntry(Item));
	}

	// 2. Save equipment (find on same actor)
	if (GetOwner())
	{
		UEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UEquipmentComponent>();
		if (Equipment)
		{
			TMap<FName, FInventoryItemInstance> EquippedItems = Equipment->GetAllEquippedItems();
			for (auto& Pair : EquippedItems)
			{
				SaveGame->EquipmentSlotIDs.Add(Pair.Key);
				SaveGame->EquipmentItems.Add(CreateSaveEntry(Pair.Value));
			}
		}
	}

	// 3. Save world containers
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<AActor*> ContainerActors;
		UGameplayStatics::GetAllActorsOfClass(World, AInventoryContainer::StaticClass(), ContainerActors);

		for (AActor* Actor : ContainerActors)
		{
			AInventoryContainer* Container = Cast<AInventoryContainer>(Actor);
			if (!Container || Container->ContainerID == NAME_None)
			{
				continue;
			}

			FContainerSaveData Data;
			Data.ContainerID = Container->ContainerID;
			Data.bIsLocked = Container->bIsLocked;
			Data.RequiredKeyItemID = Container->RequiredKeyItemID;
			Data.bConsumeKey = Container->bConsumeKey;
			Data.bLootGenerated = Container->bLootGenerated;

			if (Container->InventoryComponent)
			{
				for (const FInventoryItemInstance& Item : Container->InventoryComponent->GetAllItems())
				{
					Data.Items.Add(CreateSaveEntry(Item));
				}
			}

			SaveGame->Containers.Add(Data);
		}
	}

	// 4. Write to disk
	const FString SlotName = GetSaveSlotName(SlotIndex);
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveInventory — Slot %d: %d items, %d equipment, %d containers → %s"),
		SlotIndex, SaveGame->InventoryItems.Num(),
		SaveGame->EquipmentSlotIDs.Num(), SaveGame->Containers.Num(),
		bSuccess ? TEXT("OK") : TEXT("FAILED"));

	return bSuccess;
}

bool UGridInventoryComponent::LoadInventory(int32 SlotIndex)
{
	const FString SlotName = GetSaveSlotName(SlotIndex);

	UInventorySaveGame* SaveGame = Cast<UInventorySaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));

	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] LoadInventory — Slot %d not found"), SlotIndex);
		return false;
	}

	// 1. Clear current inventory
	ClearInventory();

	// 2. Restore player inventory items
	int32 RestoredCount = 0;
	for (const FItemSaveEntry& Entry : SaveGame->InventoryItems)
	{
		if (RestoreItemFromEntry(Entry))
		{
			RestoredCount++;
		}
	}

	// 3. Restore equipment
	if (GetOwner())
	{
		UEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UEquipmentComponent>();
		if (Equipment)
		{
			Equipment->ClearAllSlots();

			for (int32 i = 0; i < SaveGame->EquipmentSlotIDs.Num(); ++i)
			{
				if (!SaveGame->EquipmentItems.IsValidIndex(i)) break;

				const FItemSaveEntry& Entry = SaveGame->EquipmentItems[i];
				UInventoryItemDefinition* ItemDef = nullptr;

				if (!Entry.bIsRuntimeCreated)
				{
					ItemDef = Cast<UInventoryItemDefinition>(Entry.ItemDefPath.TryLoad());
				}

				if (ItemDef)
				{
					FInventoryItemInstance EquipItem;
					EquipItem.UniqueID = Entry.UniqueID;
					EquipItem.ItemDef = ItemDef;
					EquipItem.StackCount = Entry.StackCount;
					EquipItem.CurrentClassLevel = Entry.CurrentClassLevel;
					EquipItem.InstanceEffects = Entry.InstanceEffects;

					Equipment->RestoreEquipmentSlot(SaveGame->EquipmentSlotIDs[i], EquipItem);
				}
			}
		}
	}

	// 4. Restore world containers
	UWorld* World = GetWorld();
	if (World && SaveGame->Containers.Num() > 0)
	{
		TArray<AActor*> ContainerActors;
		UGameplayStatics::GetAllActorsOfClass(World, AInventoryContainer::StaticClass(), ContainerActors);

		for (const FContainerSaveData& ContainerData : SaveGame->Containers)
		{
			for (AActor* Actor : ContainerActors)
			{
				AInventoryContainer* Container = Cast<AInventoryContainer>(Actor);
				if (!Container || Container->ContainerID != ContainerData.ContainerID)
				{
					continue;
				}

				// Restore container state
				Container->bIsLocked = ContainerData.bIsLocked;
				Container->RequiredKeyItemID = ContainerData.RequiredKeyItemID;
				Container->bConsumeKey = ContainerData.bConsumeKey;
				Container->bLootGenerated = ContainerData.bLootGenerated;

				// Restore container items
				if (Container->InventoryComponent)
				{
					Container->InventoryComponent->ClearInventory();

					for (const FItemSaveEntry& Entry : ContainerData.Items)
					{
						UInventoryItemDefinition* ItemDef = nullptr;
						if (!Entry.bIsRuntimeCreated)
						{
							ItemDef = Cast<UInventoryItemDefinition>(Entry.ItemDefPath.TryLoad());
						}

						if (ItemDef)
						{
							Container->InventoryComponent->TryAddItemAt(
								ItemDef, Entry.GridPosition, Entry.bIsRotated, Entry.StackCount);
						}
					}
				}

				break;
			}
		}
	}

	RecalculateWeight();
	BroadcastChanged();

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] LoadInventory — Slot %d: %d/%d items restored, %d equipment, %d containers"),
		SlotIndex, RestoredCount, SaveGame->InventoryItems.Num(),
		SaveGame->EquipmentSlotIDs.Num(), SaveGame->Containers.Num());

	return true;
}

bool UGridInventoryComponent::DoesSaveExist(int32 SlotIndex)
{
	return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SlotIndex), 0);
}

bool UGridInventoryComponent::DeleteSave(int32 SlotIndex)
{
	return UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SlotIndex), 0);
}

// ============================================================================
// Internal
// ============================================================================

void UGridInventoryComponent::RecalculateWeight()
{
	CachedWeight = 0.0f;
	for (const FInventoryItemInstance& Item : Items) CachedWeight += Item.GetTotalWeight();
}

bool UGridInventoryComponent::CanAffordWeight(float AdditionalWeight) const
{
	return (MaxWeight <= 0.0f) || (CachedWeight + AdditionalWeight) <= MaxWeight;
}

int32 UGridInventoryComponent::FindItemIndex(const FGuid& UniqueID) const
{
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].UniqueID == UniqueID) return i;
	}
	return INDEX_NONE;
}

int32 UGridInventoryComponent::TryStackOntoExisting(UInventoryItemDefinition* ItemDef, int32 Count)
{
	if (!ItemDef || !ItemDef->bCanStack) return Count;

	int32 Remaining = Count;
	for (FInventoryItemInstance& Item : Items)
	{
		if (Remaining <= 0) break;
		if (Item.ItemDef == ItemDef && Item.StackCount < ItemDef->MaxStackSize)
		{
			const int32 ToAdd = FMath::Min(Remaining, ItemDef->MaxStackSize - Item.StackCount);
			Item.StackCount += ToAdd;
			Remaining -= ToAdd;
		}
	}
	return Remaining;
}

void UGridInventoryComponent::RebuildGridFromItems()
{
	Grid.Initialize(GridWidth, GridHeight);
	for (const FInventoryItemInstance& Item : Items)
	{
		if (Item.IsValid())
		{
			Grid.PlaceItem(Item.UniqueID, Item.GridPosition, Item.GetEffectiveSize());
		}
	}
	RecalculateWeight();
}

bool UGridInventoryComponent::Internal_AddItemAt(UInventoryItemDefinition* ItemDef, FIntPoint Position, bool bRotated, int32 Count)
{
	if (!ItemDef) return false;

	FInventoryItemInstance NewItem = FInventoryItemInstance::CreateNew(ItemDef, Count);
	NewItem.GridPosition = Position;
	NewItem.bIsRotated = bRotated;

	if (!Grid.PlaceItem(NewItem.UniqueID, Position, ItemDef->GetEffectiveSize(bRotated)))
		return false;

	Items.Add(NewItem);
	BroadcastItemAdded(NewItem);
	return true;
}

bool UGridInventoryComponent::Internal_RemoveItem(const FGuid& UniqueID, int32 Count)
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];

	if (Count <= 0 || Count >= Item.StackCount)
	{
		Grid.RemoveItem(UniqueID);
		FInventoryItemInstance Removed = Item;
		Items.RemoveAt(Index);
		BroadcastItemRemoved(Removed);
	}
	else
	{
		Item.StackCount -= Count;
	}
	return true;
}

void UGridInventoryComponent::BroadcastItemAdded(const FInventoryItemInstance& Item) { OnItemAdded.Broadcast(Item); }
void UGridInventoryComponent::BroadcastItemRemoved(const FInventoryItemInstance& Item) { OnItemRemoved.Broadcast(Item); }
void UGridInventoryComponent::BroadcastItemMoved(const FInventoryItemInstance& Item) { OnItemMoved.Broadcast(Item); }
void UGridInventoryComponent::BroadcastChanged()
{
	// Deferred broadcast: set dirty flag, enable tick for next frame
	if (!bBroadcastPending)
	{
		bBroadcastPending = true;
		bInventoryDirty = true;
		SetComponentTickEnabled(true);
	}
}

void UGridInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Batch all changes from this frame into one broadcast
	if (bBroadcastPending)
	{
		bBroadcastPending = false;
		bInventoryDirty = false;
		OnInventoryChanged.Broadcast();
	}

	// Disable tick until next change
	SetComponentTickEnabled(false);
}
