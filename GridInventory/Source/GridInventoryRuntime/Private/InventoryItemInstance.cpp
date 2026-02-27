// Copyright Marco. All Rights Reserved.

#include "InventoryItemInstance.h"
#include "ItemContainerInventory.h"

FInventoryItemInstance::FInventoryItemInstance()
	: UniqueID()
	, ItemDef(nullptr)
	, GridPosition(FIntPoint::ZeroValue)
	, bIsRotated(false)
	, StackCount(1)
	, CurrentClassLevel(1)
	, SubInventory(nullptr)
{
}

// ============================================================================
// Effect Value Resolution
// ============================================================================

float FInventoryItemInstance::GetEffectValue(FName EffectID) const
{
	if (!ItemDef) return 0.0f;

	float BaseValue = 0.0f;

	// 1. Check instance overrides first
	const FItemEffectValue* InstanceVal = ItemEffectHelpers::FindEffect(InstanceEffects, EffectID);
	if (InstanceVal)
	{
		BaseValue = InstanceVal->Value;
	}
	else
	{
		// 2. Fall back to item definition base effects
		BaseValue = ItemDef->GetBaseEffectValue(EffectID);
	}

	// 3. Apply class level multiplier
	if (ItemDef->bCanMergeUpgrade && CurrentClassLevel > 1)
	{
		BaseValue *= ItemDef->GetClassMultiplier(CurrentClassLevel);
	}

	return BaseValue;
}

TArray<FItemEffectValue> FInventoryItemInstance::GetAllEffectValues() const
{
	TArray<FItemEffectValue> Result;
	if (!ItemDef) return Result;

	const float ClassMult = (ItemDef->bCanMergeUpgrade && CurrentClassLevel > 1)
		? ItemDef->GetClassMultiplier(CurrentClassLevel) : 1.0f;

	// Start with base effects
	for (const FItemEffectValue& Base : ItemDef->BaseEffects)
	{
		// Check for instance override
		const FItemEffectValue* Override = ItemEffectHelpers::FindEffect(InstanceEffects, Base.EffectID);
		const float Value = Override ? Override->Value : Base.Value;
		Result.Add(FItemEffectValue(Base.EffectID, Value * ClassMult));
	}

	// Add instance-only effects (not in base)
	for (const FItemEffectValue& Inst : InstanceEffects)
	{
		const FItemEffectValue* InBase = ItemEffectHelpers::FindEffect(ItemDef->BaseEffects, Inst.EffectID);
		if (!InBase)
		{
			Result.Add(FItemEffectValue(Inst.EffectID, Inst.Value * ClassMult));
		}
	}

	return Result;
}

FText FInventoryItemInstance::GetDisplayName() const
{
	if (!ItemDef) return FText::GetEmpty();

	if (ItemDef->bCanMergeUpgrade && CurrentClassLevel > 1)
	{
		FItemClassMultiplier ClassInfo = ItemDef->GetClassInfo(CurrentClassLevel);
		if (!ClassInfo.ClassSuffix.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("GridInv", "ClassedName", "{0} {1}"),
				ItemDef->DisplayName, ClassInfo.ClassSuffix);
		}
	}

	return ItemDef->DisplayName;
}

// ============================================================================
// Core
// ============================================================================

FIntPoint FInventoryItemInstance::GetEffectiveSize() const
{
	if (!ItemDef) return FIntPoint(1, 1);
	return ItemDef->GetEffectiveSize(bIsRotated);
}

float FInventoryItemInstance::GetTotalWeight() const
{
	if (!ItemDef) return 0.0f;
	float W = ItemDef->GetStackWeight(StackCount);
	if (SubInventory && SubInventory->IsInitialized())
	{
		W += SubInventory->GetCurrentWeight();
	}
	return W;
}

float FInventoryItemInstance::GetOwnWeight() const
{
	if (!ItemDef) return 0.0f;
	return ItemDef->GetStackWeight(StackCount);
}

bool FInventoryItemInstance::IsValid() const
{
	return UniqueID.IsValid() && ItemDef != nullptr;
}

bool FInventoryItemInstance::IsContainer() const
{
	return ItemDef != nullptr && ItemDef->bIsContainer;
}

bool FInventoryItemInstance::CanMergeWith(const FInventoryItemInstance& Other) const
{
	if (!IsValid() || !Other.IsValid()) return false;
	if (!ItemDef->bCanMergeUpgrade) return false;
	if (ItemDef != Other.ItemDef) return false; // Must be same item type
	if (CurrentClassLevel >= ItemDef->MaxClassLevel) return false; // Already max
	return true;
}

UItemContainerInventory* FInventoryItemInstance::GetOrCreateSubInventory(UObject* Outer)
{
	if (!IsContainer()) return nullptr;

	if (!SubInventory)
	{
		SubInventory = NewObject<UItemContainerInventory>(Outer ? Outer : GetTransientPackage());
		SubInventory->Initialize(ItemDef);
	}
	return SubInventory;
}

FInventoryItemInstance FInventoryItemInstance::CreateNew(UInventoryItemDefinition* InItemDef, int32 InStackCount)
{
	FInventoryItemInstance Instance;
	Instance.UniqueID = FGuid::NewGuid();
	Instance.ItemDef = InItemDef;
	Instance.GridPosition = FIntPoint(-1, -1);
	Instance.bIsRotated = false;
	Instance.StackCount = FMath::Max(1, InStackCount);
	Instance.CurrentClassLevel = 1;
	Instance.SubInventory = nullptr;
	return Instance;
}
