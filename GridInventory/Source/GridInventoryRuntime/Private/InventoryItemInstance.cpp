// Copyright Marco. All Rights Reserved.

#include "InventoryItemInstance.h"
#include "ItemContainerInventory.h"

FInventoryItemInstance::FInventoryItemInstance()
	: UniqueID()
	, GridPosition(FIntPoint::ZeroValue)
	, bIsRotated(false)
	, StackCount(1)
	, CurrentClassLevel(1)
	, SubInventory(nullptr)
{
}

UInventoryItemDefinition* FInventoryItemInstance::GetItemDef() const
{
	if (ItemDefSoft.IsNull())
	{
		return nullptr;
	}
	return ItemDefSoft.LoadSynchronous();
}

// ============================================================================
// Effect Value Resolution
// ============================================================================

float FInventoryItemInstance::GetEffectValue(FName EffectID) const
{
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return 0.0f;

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
		BaseValue = Def->GetBaseEffectValue(EffectID);
	}

	// 3. Apply class level multiplier
	if (Def->bCanMergeUpgrade && CurrentClassLevel > 1)
	{
		BaseValue *= Def->GetClassMultiplier(CurrentClassLevel);
	}

	return BaseValue;
}

TArray<FItemEffectValue> FInventoryItemInstance::GetAllEffectValues() const
{
	TArray<FItemEffectValue> Result;
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return Result;

	const float ClassMult = (Def->bCanMergeUpgrade && CurrentClassLevel > 1)
		? Def->GetClassMultiplier(CurrentClassLevel) : 1.0f;

	// Start with base effects
	for (const FItemEffectValue& Base : Def->BaseEffects)
	{
		// Check for instance override
		const FItemEffectValue* Override = ItemEffectHelpers::FindEffect(InstanceEffects, Base.EffectID);
		const float Value = Override ? Override->Value : Base.Value;
		Result.Add(FItemEffectValue(Base.EffectID, Value * ClassMult));
	}

	// Add instance-only effects (not in base)
	for (const FItemEffectValue& Inst : InstanceEffects)
	{
		const FItemEffectValue* InBase = ItemEffectHelpers::FindEffect(Def->BaseEffects, Inst.EffectID);
		if (!InBase)
		{
			Result.Add(FItemEffectValue(Inst.EffectID, Inst.Value * ClassMult));
		}
	}

	return Result;
}

FText FInventoryItemInstance::GetDisplayName() const
{
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return FText::GetEmpty();

	if (Def->bCanMergeUpgrade && CurrentClassLevel > 1)
	{
		FItemClassMultiplier ClassInfo = Def->GetClassInfo(CurrentClassLevel);
		if (!ClassInfo.ClassSuffix.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("GridInv", "ClassedName", "{0} {1}"),
				Def->DisplayName, ClassInfo.ClassSuffix);
		}
	}

	return Def->DisplayName;
}

// ============================================================================
// Core
// ============================================================================

FIntPoint FInventoryItemInstance::GetEffectiveSize() const
{
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return FIntPoint(1, 1);
	return Def->GetEffectiveSize(bIsRotated);
}

float FInventoryItemInstance::GetTotalWeight() const
{
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return 0.0f;
	float W = Def->GetStackWeight(StackCount);
	if (SubInventory && SubInventory->IsInitialized())
	{
		W += SubInventory->GetCurrentWeight();
	}
	return W;
}

float FInventoryItemInstance::GetOwnWeight() const
{
	UInventoryItemDefinition* Def = GetItemDef();
	if (!Def) return 0.0f;
	return Def->GetStackWeight(StackCount);
}

bool FInventoryItemInstance::IsValid() const
{
	return UniqueID.IsValid() && !ItemDefSoft.IsNull();
}

bool FInventoryItemInstance::IsContainer() const
{
	UInventoryItemDefinition* Def = GetItemDef();
	return Def != nullptr && Def->bIsContainer;
}

bool FInventoryItemInstance::CanMergeWith(const FInventoryItemInstance& Other) const
{
	if (!IsValid() || !Other.IsValid()) return false;
	UInventoryItemDefinition* Def = GetItemDef();
	UInventoryItemDefinition* OtherDef = Other.GetItemDef();
	if (!Def || !OtherDef) return false;
	if (!Def->bCanMergeUpgrade) return false;
	if (Def != OtherDef) return false; // Must be same item type
	if (CurrentClassLevel >= Def->MaxClassLevel) return false; // Already max
	return true;
}

UItemContainerInventory* FInventoryItemInstance::GetOrCreateSubInventory(UObject* Outer)
{
	if (!IsContainer()) return nullptr;

	if (!SubInventory)
	{
		SubInventory = NewObject<UItemContainerInventory>(Outer ? Outer : GetTransientPackage());
		SubInventory->Initialize(GetItemDef());
	}
	return SubInventory;
}

FInventoryItemInstance FInventoryItemInstance::CreateNew(UInventoryItemDefinition* InItemDef, int32 InStackCount)
{
	FInventoryItemInstance Instance;
	Instance.UniqueID = FGuid::NewGuid();
	Instance.ItemDefSoft = InItemDef;
	Instance.GridPosition = FIntPoint(-1, -1);
	Instance.bIsRotated = false;
	Instance.StackCount = FMath::Max(1, InStackCount);
	Instance.CurrentClassLevel = 1;
	Instance.SubInventory = nullptr;
	return Instance;
}
