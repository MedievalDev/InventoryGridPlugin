// Copyright Marco. All Rights Reserved.

#include "InventoryFunctionLibrary.h"
#include "InventoryItemDefinition.h"
#include "ItemContainerInventory.h"

// ============================================================================
// Search & Filter
// ============================================================================

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindItemsByType(UGridInventoryComponent* Inventory, FName ItemType)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && Item.GetItemDef()->ItemType == ItemType)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindItemsByDef(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || !ItemDef) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() == ItemDef)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindItemsByName(UGridInventoryComponent* Inventory, const FString& SearchText)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || SearchText.IsEmpty()) return Result;

	const FString LowerSearch = SearchText.ToLower();

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef())
		{
			const FString DisplayName = Item.GetItemDef()->DisplayName.ToString().ToLower();
			if (DisplayName.Contains(LowerSearch))
			{
				Result.Add(Item);
			}
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindStackableItems(UGridInventoryComponent* Inventory)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && Item.GetItemDef()->bCanStack)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindPartialStacks(UGridInventoryComponent* Inventory)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && Item.GetItemDef()->bCanStack && Item.StackCount < Item.GetItemDef()->MaxStackSize)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindEquippableItems(
	UGridInventoryComponent* Inventory, FName SlotID, UEquipmentComponent* EquipmentComp)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || !EquipmentComp) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && EquipmentComp->CanEquipInSlot(SlotID, Item.GetItemDef()))
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::GetHeaviestItems(UGridInventoryComponent* Inventory, int32 Count)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || Count <= 0) return Result;

	Result = Inventory->GetAllItems();
	Result.Sort([](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
	{
		return A.GetTotalWeight() > B.GetTotalWeight();
	});

	if (Result.Num() > Count)
	{
		Result.SetNum(Count);
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::GetLightestItems(UGridInventoryComponent* Inventory, int32 Count)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || Count <= 0) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetTotalWeight() > 0.0f)
		{
			Result.Add(Item);
		}
	}

	Result.Sort([](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
	{
		return A.GetTotalWeight() < B.GetTotalWeight();
	});

	if (Result.Num() > Count)
	{
		Result.SetNum(Count);
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FilterByMinWeight(UGridInventoryComponent* Inventory, float MinWeight)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetTotalWeight() >= MinWeight)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FilterByMaxWeight(UGridInventoryComponent* Inventory, float MaxWeight)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetTotalWeight() <= MaxWeight)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindRotatableItems(UGridInventoryComponent* Inventory)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && Item.GetItemDef()->bCanRotate && Item.GetItemDef()->GridSize.X != Item.GetItemDef()->GridSize.Y)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindItemsLargerThan(UGridInventoryComponent* Inventory, int32 MinCellArea)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef())
		{
			const int32 Area = Item.GetItemDef()->GridSize.X * Item.GetItemDef()->GridSize.Y;
			if (Area >= MinCellArea)
			{
				Result.Add(Item);
			}
		}
	}
	return Result;
}

// ============================================================================
// Counting & Stats
// ============================================================================

int32 UInventoryFunctionLibrary::GetTotalItemCount(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef)
{
	return Inventory ? Inventory->GetItemCount(ItemDef) : 0;
}

int32 UInventoryFunctionLibrary::GetUniqueItemTypeCount(UGridInventoryComponent* Inventory)
{
	if (!Inventory) return 0;

	TSet<UInventoryItemDefinition*> UniqueDefs;
	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef())
		{
			UniqueDefs.Add(Item.GetItemDef());
		}
	}
	return UniqueDefs.Num();
}

int32 UInventoryFunctionLibrary::GetTotalIndividualItemCount(UGridInventoryComponent* Inventory)
{
	if (!Inventory) return 0;

	int32 Total = 0;
	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		Total += Item.StackCount;
	}
	return Total;
}

// ============================================================================
// Multi-Inventory
// ============================================================================

FInventoryItemInstance UInventoryFunctionLibrary::FindItemInInventories(
	const TArray<UGridInventoryComponent*>& Inventories, FGuid UniqueID, UGridInventoryComponent*& OutInventory)
{
	OutInventory = nullptr;
	for (UGridInventoryComponent* Inv : Inventories)
	{
		if (!Inv) continue;
		FInventoryItemInstance Found = Inv->GetItemByID(UniqueID);
		if (Found.IsValid())
		{
			OutInventory = Inv;
			return Found;
		}
	}
	return FInventoryItemInstance();
}

bool UInventoryFunctionLibrary::SwapItems(UGridInventoryComponent* InvA, FGuid ItemA_ID,
	UGridInventoryComponent* InvB, FGuid ItemB_ID)
{
	if (!InvA || !InvB || !ItemA_ID.IsValid() || !ItemB_ID.IsValid()) return false;

	FInventoryItemInstance ItemA = InvA->GetItemByID(ItemA_ID);
	FInventoryItemInstance ItemB = InvB->GetItemByID(ItemB_ID);
	if (!ItemA.IsValid() || !ItemB.IsValid()) return false;

	const FIntPoint PosA = ItemA.GridPosition;
	const bool RotA = ItemA.bIsRotated;
	const FIntPoint PosB = ItemB.GridPosition;
	const bool RotB = ItemB.bIsRotated;

	InvA->RemoveItem(ItemA_ID, 0);
	InvB->RemoveItem(ItemB_ID, 0);

	bool bSuccess = true;
	if (!InvB->TryAddItemAt(ItemA.GetItemDef(), PosB, RotA, ItemA.StackCount)) bSuccess = false;
	if (bSuccess && !InvA->TryAddItemAt(ItemB.GetItemDef(), PosA, RotB, ItemB.StackCount)) bSuccess = false;

	if (!bSuccess)
	{
		InvA->TryAddItemAt(ItemA.GetItemDef(), PosA, RotA, ItemA.StackCount);
		InvB->TryAddItemAt(ItemB.GetItemDef(), PosB, RotB, ItemB.StackCount);
		return false;
	}
	return true;
}

// ============================================================================
// Convenience
// ============================================================================

UGridInventoryComponent* UInventoryFunctionLibrary::GetInventoryComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGridInventoryComponent>() : nullptr;
}

UEquipmentComponent* UInventoryFunctionLibrary::GetEquipmentComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UEquipmentComponent>() : nullptr;
}

void UInventoryFunctionLibrary::SortInventory(UGridInventoryComponent* Inventory)
{
	if (Inventory) Inventory->SortInventory();
}

// ============================================================================
// Item Container Search
// ============================================================================

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindContainerItems(UGridInventoryComponent* Inventory)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.IsContainer())
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindContainersForItemType(UGridInventoryComponent* Inventory, FName ItemType)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.IsContainer() && Item.GetItemDef()->ContainerAcceptsItemType(ItemType))
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::DeepSearchByName(
	UGridInventoryComponent* Inventory, const FString& SearchText, FGuid& OutContainerItemID)
{
	TArray<FInventoryItemInstance> Result;
	OutContainerItemID = FGuid();
	if (!Inventory || SearchText.IsEmpty()) return Result;

	const FString LowerSearch = SearchText.ToLower();

	// Search main inventory
	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef())
		{
			if (Item.GetItemDef()->DisplayName.ToString().ToLower().Contains(LowerSearch))
			{
				Result.Add(Item);
			}
		}
	}

	// Search all sub-inventories
	for (const FInventoryItemInstance& ContainerItem : Inventory->GetAllItems())
	{
		if (ContainerItem.SubInventory && ContainerItem.SubInventory->IsInitialized())
		{
			for (const FInventoryItemInstance& SubItem : ContainerItem.SubInventory->GetAllItems())
			{
				if (SubItem.GetItemDef() && SubItem.GetItemDef()->DisplayName.ToString().ToLower().Contains(LowerSearch))
				{
					Result.Add(SubItem);
					// Report which container it was found in (last found)
					OutContainerItemID = ContainerItem.UniqueID;
				}
			}
		}
	}

	return Result;
}

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::DeepSearchByType(
	UGridInventoryComponent* Inventory, FName ItemType)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory) return Result;

	// Main inventory
	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.GetItemDef() && Item.GetItemDef()->ItemType == ItemType)
		{
			Result.Add(Item);
		}
	}

	// Sub-inventories
	for (const FInventoryItemInstance& ContainerItem : Inventory->GetAllItems())
	{
		if (ContainerItem.SubInventory && ContainerItem.SubInventory->IsInitialized())
		{
			for (const FInventoryItemInstance& SubItem : ContainerItem.SubInventory->GetAllItems())
			{
				if (SubItem.GetItemDef() && SubItem.GetItemDef()->ItemType == ItemType)
				{
					Result.Add(SubItem);
				}
			}
		}
	}

	return Result;
}

int32 UInventoryFunctionLibrary::DeepCountByDef(UGridInventoryComponent* Inventory, UInventoryItemDefinition* ItemDef)
{
	if (!Inventory || !ItemDef) return 0;

	int32 Total = Inventory->GetItemCount(ItemDef);

	// Also count in sub-inventories
	for (const FInventoryItemInstance& ContainerItem : Inventory->GetAllItems())
	{
		if (ContainerItem.SubInventory && ContainerItem.SubInventory->IsInitialized())
		{
			Total += ContainerItem.SubInventory->GetItemCount(ItemDef);
		}
	}

	return Total;
}

// ============================================================================
// Effect Values
// ============================================================================

float UInventoryFunctionLibrary::GetItemEffectValue(const FInventoryItemInstance& Item, FName EffectID)
{
	return Item.GetEffectValue(EffectID);
}

TArray<FItemEffectValue> UInventoryFunctionLibrary::GetAllItemEffects(const FInventoryItemInstance& Item)
{
	return Item.GetAllEffectValues();
}

bool UInventoryFunctionLibrary::SetInstanceEffect(UGridInventoryComponent* Inventory, FGuid UniqueID, FName EffectID, float Value)
{
	if (!Inventory || !UniqueID.IsValid()) return false;
	return Inventory->SetItemInstanceEffect(UniqueID, EffectID, Value);
}

FText UInventoryFunctionLibrary::GetItemDisplayName(const FInventoryItemInstance& Item)
{
	return Item.GetDisplayName();
}

// ============================================================================
// Merge Helpers
// ============================================================================

TArray<FInventoryItemInstance> UInventoryFunctionLibrary::FindMergeCandidates(UGridInventoryComponent* Inventory, FGuid TargetID)
{
	TArray<FInventoryItemInstance> Result;
	if (!Inventory || !TargetID.IsValid()) return Result;

	FInventoryItemInstance Target = Inventory->GetItemByID(TargetID);
	if (!Target.IsValid() || !Target.GetItemDef() || !Target.GetItemDef()->bCanMergeUpgrade) return Result;

	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.UniqueID != TargetID && Item.GetItemDef() == Target.GetItemDef())
		{
			Result.Add(Item);
		}
	}

	return Result;
}

int32 UInventoryFunctionLibrary::GetItemClassLevel(const FInventoryItemInstance& Item)
{
	return Item.CurrentClassLevel;
}
