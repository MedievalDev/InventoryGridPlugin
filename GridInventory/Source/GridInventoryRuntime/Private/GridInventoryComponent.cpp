// Copyright Marco. All Rights Reserved.

#include "GridInventoryComponent.h"
#include "ItemContainerInventory.h"
#include "InventorySaveGame.h"
#include "EquipmentComponent.h"
#include "InventoryContainer.h"
#include "RuntimeItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UGridInventoryComponent::UGridInventoryComponent()
	: GridWidth(10)
	, GridHeight(10)
	, MaxWeight(0.0f)
	, CachedWeight(0.0f)
	, InternalGold(0.0f)
	, ExternalGoldPtr(nullptr)
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

	// Currency items skip the grid — add BaseValue * Count directly to gold
	if (ItemDef->bIsCurrency)
	{
		if (ItemDef->BaseValue <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Currency item '%s' has BaseValue 0!"),
				*ItemDef->DisplayName.ToString());
		}

		const float GoldToAdd = static_cast<float>(ItemDef->BaseValue) * Count;
		AddGold(GoldToAdd);
		OnCurrencyCollected.Broadcast(ItemDef, Count, GoldToAdd);

		UE_LOG(LogTemp, Log, TEXT("[GridInventory] Currency collected: %s x%d = +%.0f gold"),
			*ItemDef->DisplayName.ToString(), Count, GoldToAdd);
		return true;
	}

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

		// Currency items bypass the grid — add gold directly
		if (Req.ItemDef->bIsCurrency)
		{
			const float GoldToAdd = static_cast<float>(Req.ItemDef->BaseValue) * Req.Count;
			AddGold(GoldToAdd);
			OnCurrencyCollected.Broadcast(Req.ItemDef, Req.Count, GoldToAdd);
			AddedCount++;
			continue;
		}

		int32 Remaining = Req.Count;

		if (Req.ItemDef->bCanStack)
		{
			Remaining = TryStackOntoExisting(Req.ItemDef, Remaining);
		}

		if (Remaining > 0)
		{
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

	// Forward iteration with SwapRemove: after swap, re-check same index
	int32 i = 0;
	while (i < Items.Num() && Remaining > 0)
	{
		if (Items[i].ItemDef == ItemDef)
		{
			const int32 ToRemove = FMath::Min(Remaining, Items[i].StackCount);
			Items[i].StackCount -= ToRemove;
			Remaining -= ToRemove;
			RemovedTotal += ToRemove;

			if (Items[i].StackCount <= 0)
			{
				// O(item_area) grid remove instead of O(4800)
				Grid.RemoveItemAt(Items[i].UniqueID, Items[i].GridPosition, Items[i].GetEffectiveSize());
				BroadcastItemRemoved(Items[i]);
				SwapRemoveItemAtIndex(i);
				// Don't increment — check swapped-in item at same index
				continue;
			}
		}
		++i;
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

	// O(item_area) remove instead of O(4800)
	Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
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
		Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
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

	FInventoryItemInstance ConsumedItemCopy = Item;
	OnItemConsumed.Broadcast(ConsumedItemCopy);

	if (Item.ItemDef->bDestroyOnConsume)
	{
		Item.StackCount -= 1;
		if (Item.StackCount <= 0)
		{
			Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
			BroadcastItemRemoved(ConsumedItemCopy);
			SwapRemoveItemAtIndex(Index);
		}
		RecalculateWeight();
		BroadcastChanged();
	}

	return true;
}

bool UGridInventoryComponent::SetItemInstanceEffect(FGuid UniqueID, FName EffectID, float Value)
{
	if (!UniqueID.IsValid() || EffectID == NAME_None) return false;

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return false;

	FInventoryItemInstance& Item = Items[Index];

	// Update existing or add new
	for (FItemEffectValue& Eff : Item.InstanceEffects)
	{
		if (Eff.EffectID == EffectID)
		{
			Eff.Value = Value;
			BroadcastChanged();
			return true;
		}
	}

	Item.InstanceEffects.Add(FItemEffectValue(EffectID, Value));
	BroadcastChanged();
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

	FInventoryItemInstance DroppedCopy = Item;
	DroppedCopy.StackCount = DropCount;

	Internal_RemoveItem(UniqueID, DropCount);
	RecalculateWeight();
	BroadcastChanged();

	OnItemDropped.Broadcast(DroppedCopy, SpawnedActor);
	return SpawnedActor;
}

void UGridInventoryComponent::DropItemAsync(FGuid UniqueID, FVector SpawnLocation, FRotator SpawnRotation, int32 Count)
{
	if (!UniqueID.IsValid()) return;

	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return;

	FInventoryItemInstance& Item = Items[Index];
	if (!Item.ItemDef) return;

	const int32 DropCount = (Count <= 0) ? Item.StackCount : FMath::Min(Count, Item.StackCount);

	// Capture data before removing from inventory
	FInventoryItemInstance DroppedCopy = Item;
	DroppedCopy.StackCount = DropCount;
	TSoftClassPtr<AActor> WorldActorClass = Item.ItemDef->WorldActorClass;

	// Remove from inventory immediately (no hitch)
	Internal_RemoveItem(UniqueID, DropCount);
	RecalculateWeight();
	BroadcastChanged();

	// Async load world actor class — spawn in callback on game thread
	if (!WorldActorClass.IsNull())
	{
		TWeakObjectPtr<UGridInventoryComponent> WeakThis(this);
		FVector CapturedLoc = SpawnLocation;
		FRotator CapturedRot = SpawnRotation;

		// Check if already loaded (common case — avoids async overhead)
		UClass* AlreadyLoaded = WorldActorClass.Get();
		if (AlreadyLoaded)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				AActor* SpawnedActor = World->SpawnActor<AActor>(AlreadyLoaded, CapturedLoc, CapturedRot, Params);
				OnItemDropped.Broadcast(DroppedCopy, SpawnedActor);
			}
			return;
		}

		// Async load path — no game thread hitch
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamableManager.RequestAsyncLoad(WorldActorClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateLambda([WeakThis, DroppedCopy, CapturedLoc, CapturedRot, WorldActorClass]()
			{
				if (!WeakThis.IsValid()) return;

				UClass* LoadedClass = WorldActorClass.Get();
				UWorld* World = WeakThis->GetWorld();
				if (LoadedClass && World)
				{
					FActorSpawnParameters Params;
					Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					AActor* SpawnedActor = World->SpawnActor<AActor>(LoadedClass, CapturedLoc, CapturedRot, Params);
					WeakThis->OnItemDropped.Broadcast(DroppedCopy, SpawnedActor);
				}
			})
		);
	}
	else
	{
		OnItemDropped.Broadcast(DroppedCopy, nullptr);
	}
}

bool UGridInventoryComponent::PickupItem(UInventoryItemDefinition* ItemDef, int32 Count, AActor* WorldActor)
{
	if (!ItemDef || Count <= 0) return false;

	if (TryAddItem(ItemDef, Count))
	{
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

	const int32 SacIdx = FindItemIndex(SacrificeID);

	// Destroy sacrifice — O(item_area) grid remove + O(1) array swap
	FInventoryItemInstance SacCopy = Items[SacIdx];
	Grid.RemoveItemAt(SacrificeID, Items[SacIdx].GridPosition, Items[SacIdx].GetEffectiveSize());
	BroadcastItemRemoved(SacCopy);
	SwapRemoveItemAtIndex(SacIdx);

	// Upgrade target — O(1) lookup via ItemIndexMap
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
		Total += Item.GetTotalWeight();
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
	ItemIndexMap.Empty();

	for (FInventoryItemInstance& Item : ItemsCopy)
	{
		FGridPlacementResult Slot = Grid.FindFirstFreeSlot(Item.ItemDef->GridSize, Item.ItemDef->bCanRotate);
		if (Slot.bSuccess)
		{
			Item.GridPosition = Slot.Position;
			Item.bIsRotated = Slot.bRotated;
			Grid.PlaceItem(Item.UniqueID, Slot.Position, Item.ItemDef->GetEffectiveSize(Slot.bRotated));
			ItemIndexMap.Add(Item.UniqueID, Items.Num());
			Items.Add(Item);
		}
	}

	BroadcastChanged();
}

void UGridInventoryComponent::ClearInventory()
{
	for (const FInventoryItemInstance& Item : Items) BroadcastItemRemoved(Item);
	Items.Empty();
	ItemIndexMap.Empty();
	Grid.Clear();
	RuntimeDefinitions.Empty();
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
// Runtime Item Creation
// ============================================================================

URuntimeItemDefinition* UGridInventoryComponent::CreateRuntimeItem(
	FName ItemID,
	FText DisplayName,
	const TArray<FItemEffectValue>& Effects,
	const TArray<FName>& SourceIngredients)
{
	URuntimeItemDefinition* Def = NewObject<URuntimeItemDefinition>(this);
	Def->ItemID = ItemID;
	Def->DisplayName = DisplayName;
	Def->BaseEffects = Effects;
	Def->SourceIngredients = SourceIngredients;

	Def->ItemType = FName(TEXT("Potion"));
	Def->GridSize = FIntPoint(1, 1);
	Def->bCanRotate = false;
	Def->bCanStack = true;
	Def->MaxStackSize = 10;
	Def->Weight = 0.5f;
	Def->bIsConsumable = true;
	Def->bDestroyOnConsume = true;

	RuntimeDefinitions.Add(Def);

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] Created runtime item '%s' with %d effects"),
		*ItemID.ToString(), Effects.Num());

	return Def;
}

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
		URuntimeItemDefinition* RuntimeDef = Cast<URuntimeItemDefinition>(Item.ItemDef);
		if (RuntimeDef)
		{
			Entry.bIsRuntimeCreated = true;
			Entry.RuntimeItemJSON = RuntimeDef->ToJSON();
		}
		else
		{
			Entry.ItemDefPath = FSoftObjectPath(Item.ItemDef);
			Entry.bIsRuntimeCreated = false;
		}
	}

	Entry.GridPosition = Item.GridPosition;
	Entry.bIsRotated = Item.bIsRotated;
	Entry.StackCount = Item.StackCount;
	Entry.CurrentClassLevel = Item.CurrentClassLevel;
	Entry.UniqueID = Item.UniqueID;
	Entry.InstanceEffects = Item.InstanceEffects;

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
		URuntimeItemDefinition* RuntimeDef = URuntimeItemDefinition::FromJSON(this, Entry.RuntimeItemJSON);
		if (!RuntimeDef)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveLoad: Failed to parse runtime item JSON"));
			return false;
		}
		RuntimeDefinitions.Add(RuntimeDef);
		ItemDef = RuntimeDef;
	}

	FInventoryItemInstance NewItem;
	NewItem.UniqueID = Entry.UniqueID;
	NewItem.ItemDef = ItemDef;
	NewItem.GridPosition = Entry.GridPosition;
	NewItem.bIsRotated = Entry.bIsRotated;
	NewItem.StackCount = Entry.StackCount;
	NewItem.CurrentClassLevel = Entry.CurrentClassLevel;
	NewItem.InstanceEffects = Entry.InstanceEffects;

	const FIntPoint EffSize = ItemDef->GetEffectiveSize(NewItem.bIsRotated);
	if (!Grid.PlaceItem(NewItem.UniqueID, NewItem.GridPosition, EffSize))
	{
		// Saved position unavailable — try auto-placement as fallback
		FGridPlacementResult AltSlot = Grid.FindFirstFreeSlot(EffSize, ItemDef->bCanRotate);
		if (AltSlot.bSuccess)
		{
			Grid.PlaceItem(NewItem.UniqueID, AltSlot.Position, ItemDef->GetEffectiveSize(AltSlot.bRotated));
			NewItem.GridPosition = AltSlot.Position;
			NewItem.bIsRotated = AltSlot.bRotated;
			UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveLoad: Repositioned '%s' to (%d,%d)"),
				*ItemDef->DisplayName.ToString(), AltSlot.Position.X, AltSlot.Position.Y);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GridInventory] SaveLoad: LOST item '%s' — no space!"),
				*ItemDef->DisplayName.ToString());
			return false;
		}
	}

	// Update index map BEFORE adding so the index is correct
	ItemIndexMap.Add(NewItem.UniqueID, Items.Num());
	Items.Add(NewItem);

	return true;
}

UInventorySaveGame* UGridInventoryComponent::BuildSaveGameObject(int32 SlotIndex)
{
	UInventorySaveGame* SaveGame = NewObject<UInventorySaveGame>();
	SaveGame->GridWidth = GridWidth;
	SaveGame->GridHeight = GridHeight;
	SaveGame->SavedGold = GetGold();

	SaveGame->InventoryItems.Reserve(Items.Num());
	for (const FInventoryItemInstance& Item : Items)
	{
		SaveGame->InventoryItems.Add(CreateSaveEntry(Item));

		// Collect sub-inventory contents separately (avoid struct recursion)
		if (Item.SubInventory && Item.SubInventory->IsInitialized() && Item.SubInventory->GetAllItems().Num() > 0)
		{
			FSubInventorySaveData SubData;
			SubData.ParentItemID = Item.UniqueID;
			for (const FInventoryItemInstance& SubItem : Item.SubInventory->GetAllItems())
			{
				SubData.Items.Add(CreateSaveEntry(SubItem));
			}
			SaveGame->SubInventories.Add(SubData);
		}
	}

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

	UWorld* World = GetWorld();
	if (World)
	{
		TArray<AActor*> ContainerActors;
		UGameplayStatics::GetAllActorsOfClass(World, AInventoryContainer::StaticClass(), ContainerActors);

		for (AActor* Actor : ContainerActors)
		{
			AInventoryContainer* Container = Cast<AInventoryContainer>(Actor);
			if (!Container || Container->ContainerID == NAME_None) continue;

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

	return SaveGame;
}

bool UGridInventoryComponent::ProcessLoadedSaveGame(UInventorySaveGame* SaveGame, int32 SlotIndex)
{
	if (!SaveGame) return false;

	ClearInventory();
	SetGold(SaveGame->SavedGold);

	int32 RestoredCount = 0;
	for (const FItemSaveEntry& Entry : SaveGame->InventoryItems)
	{
		if (RestoreItemFromEntry(Entry))
		{
			RestoredCount++;
		}
	}

	// Restore sub-inventories (container item contents)
	for (const FSubInventorySaveData& SubData : SaveGame->SubInventories)
	{
		const int32 ParentIdx = FindItemIndex(SubData.ParentItemID);
		if (ParentIdx == INDEX_NONE) continue;

		FInventoryItemInstance& ParentItem = Items[ParentIdx];
		if (!ParentItem.ItemDef || !ParentItem.ItemDef->bIsContainer) continue;

		UItemContainerInventory* SubInv = ParentItem.GetOrCreateSubInventory(this);
		if (!SubInv) continue;

		for (const FItemSaveEntry& SubEntry : SubData.Items)
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
				if (!Container || Container->ContainerID != ContainerData.ContainerID) continue;

				Container->bIsLocked = ContainerData.bIsLocked;
				Container->RequiredKeyItemID = ContainerData.RequiredKeyItemID;
				Container->bConsumeKey = ContainerData.bConsumeKey;
				Container->bLootGenerated = ContainerData.bLootGenerated;

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

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] LoadInventory — Slot %d: %d/%d items, %d equipment, %d containers"),
		SlotIndex, RestoredCount, SaveGame->InventoryItems.Num(),
		SaveGame->EquipmentSlotIDs.Num(), SaveGame->Containers.Num());

	return true;
}

bool UGridInventoryComponent::SaveInventory(int32 SlotIndex)
{
	UInventorySaveGame* SaveGame = BuildSaveGameObject(SlotIndex);
	const FString SlotName = GetSaveSlotName(SlotIndex);
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SaveInventory — Slot %d: %d items → %s"),
		SlotIndex, SaveGame->InventoryItems.Num(), bSuccess ? TEXT("OK") : TEXT("FAILED"));

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

	return ProcessLoadedSaveGame(SaveGame, SlotIndex);
}

void UGridInventoryComponent::SaveInventoryAsync(int32 SlotIndex)
{
	// Build the save object on game thread (accesses UObjects)
	UInventorySaveGame* SaveGame = BuildSaveGameObject(SlotIndex);
	const FString SlotName = GetSaveSlotName(SlotIndex);

	TWeakObjectPtr<UGridInventoryComponent> WeakThis(this);
	const int32 CapturedSlot = SlotIndex;

	// Disk I/O runs on background thread
	UGameplayStatics::AsyncSaveGameToSlot(SaveGame, SlotName, 0,
		FAsyncSaveGameToSlotDelegate::CreateLambda(
			[WeakThis, CapturedSlot](const FString& /*SlotName*/, const int32 /*UserIndex*/, bool bSuccess)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnSaveComplete.Broadcast(CapturedSlot, bSuccess);
				}
			}
		)
	);
}

void UGridInventoryComponent::LoadInventoryAsync(int32 SlotIndex)
{
	const FString SlotName = GetSaveSlotName(SlotIndex);

	TWeakObjectPtr<UGridInventoryComponent> WeakThis(this);
	const int32 CapturedSlot = SlotIndex;

	// Disk I/O runs on background thread, callback fires on game thread
	UGameplayStatics::AsyncLoadGameFromSlot(SlotName, 0,
		FAsyncLoadGameFromSlotDelegate::CreateLambda(
			[WeakThis, CapturedSlot](const FString& /*SlotName*/, const int32 /*UserIndex*/, USaveGame* LoadedGame)
			{
				if (!WeakThis.IsValid()) return;

				UInventorySaveGame* SaveGame = Cast<UInventorySaveGame>(LoadedGame);
				const bool bSuccess = WeakThis->ProcessLoadedSaveGame(SaveGame, CapturedSlot);
				WeakThis->OnLoadComplete.Broadcast(CapturedSlot, bSuccess);
			}
		)
	);
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
// Internal — Optimized O(1) lookup and O(1) removal
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
	const int32* Found = ItemIndexMap.Find(UniqueID);
	return Found ? *Found : INDEX_NONE;
}

void UGridInventoryComponent::SwapRemoveItemAtIndex(int32 Index)
{
	if (Index < 0 || Index >= Items.Num()) return;

	// Remove from map
	ItemIndexMap.Remove(Items[Index].UniqueID);

	// Swap with last element and pop — O(1)
	const int32 LastIndex = Items.Num() - 1;
	if (Index < LastIndex)
	{
		Items[Index] = Items[LastIndex];
		// Update the swapped item's index in the map
		ItemIndexMap.Add(Items[Index].UniqueID, Index);
	}
	Items.RemoveAt(LastIndex);
}

void UGridInventoryComponent::RebuildItemIndexMap()
{
	ItemIndexMap.Empty(Items.Num());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		ItemIndexMap.Add(Items[i].UniqueID, i);
	}
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
	RebuildItemIndexMap();
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

	// Update map BEFORE adding (index = current array size)
	ItemIndexMap.Add(NewItem.UniqueID, Items.Num());
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
		// O(item_area) grid remove instead of O(4800)
		Grid.RemoveItemAt(UniqueID, Item.GridPosition, Item.GetEffectiveSize());
		FInventoryItemInstance Removed = Item;
		SwapRemoveItemAtIndex(Index);
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

	if (bBroadcastPending)
	{
		bBroadcastPending = false;
		bInventoryDirty = false;
		OnInventoryChanged.Broadcast();
	}

	SetComponentTickEnabled(false);
}

// ============================================================================
// Gold / Currency
// ============================================================================

float UGridInventoryComponent::GetGold() const
{
	return ExternalGoldPtr ? *ExternalGoldPtr : InternalGold;
}

void UGridInventoryComponent::SetGold(float Amount)
{
	const float OldGold = GetGold();
	const float NewGold = FMath::Max(0.0f, Amount);

	if (ExternalGoldPtr)
	{
		*ExternalGoldPtr = NewGold;
	}
	else
	{
		InternalGold = NewGold;
	}

	const float Delta = NewGold - OldGold;
	if (!FMath::IsNearlyZero(Delta))
	{
		OnGoldChanged.Broadcast(NewGold, Delta);
	}
}

void UGridInventoryComponent::AddGold(float Amount)
{
	SetGold(GetGold() + Amount);
}

bool UGridInventoryComponent::CanAffordGold(float Cost) const
{
	return GetGold() >= Cost;
}

float UGridInventoryComponent::GetItemSellPrice(FGuid UniqueID, float PriceMultiplier) const
{
	const int32 Index = FindItemIndex(UniqueID);
	if (Index == INDEX_NONE) return 0.0f;

	const FInventoryItemInstance& Item = Items[Index];
	if (!Item.ItemDef) return 0.0f;

	return static_cast<float>(Item.ItemDef->BaseValue) * Item.StackCount * PriceMultiplier;
}

float UGridInventoryComponent::GetInventoryTotalValue() const
{
	float Total = 0.0f;
	for (const FInventoryItemInstance& Item : Items)
	{
		if (Item.ItemDef)
		{
			Total += static_cast<float>(Item.ItemDef->BaseValue) * Item.StackCount;
		}
	}
	return Total;
}

void UGridInventoryComponent::BindExternalGold(float* GoldPtr)
{
	if (GoldPtr)
	{
		// Sync: take the external value as current gold
		InternalGold = *GoldPtr;
	}
	ExternalGoldPtr = GoldPtr;
}
