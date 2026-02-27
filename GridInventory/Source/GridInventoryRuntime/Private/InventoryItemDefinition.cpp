// Copyright Marco. All Rights Reserved.

#include "InventoryItemDefinition.h"

UInventoryItemDefinition::UInventoryItemDefinition()
	: ItemID(NAME_None)
	, ItemType(NAME_None)
	, GridSize(1, 1)
	, bCanRotate(true)
	, bCanStack(false)
	, MaxStackSize(1)
	, Weight(0.0f)
	, bIsConsumable(false)
	, bDestroyOnConsume(true)
	, bCanMergeUpgrade(false)
	, MaxClassLevel(1)
	, bIsContainer(false)
	, ContainerGridSize(6, 6)
	, ContainerMaxWeight(0.0f)
	, CategoryTag(NAME_None)
	, WirkungTag(NAME_None)
	, DurationMinutes(0.0f)
{
}

FIntPoint UInventoryItemDefinition::GetEffectiveSize(bool bRotated) const
{
	if (bRotated && bCanRotate)
	{
		return FIntPoint(GridSize.Y, GridSize.X);
	}
	return GridSize;
}

float UInventoryItemDefinition::GetStackWeight(int32 StackCount) const
{
	return Weight * FMath::Max(1, StackCount);
}

float UInventoryItemDefinition::GetBaseEffectValue(FName EffectID) const
{
	return ItemEffectHelpers::GetValue(BaseEffects, EffectID);
}

float UInventoryItemDefinition::GetClassMultiplier(int32 ClassLevel) const
{
	if (!bCanMergeUpgrade || ClassMultipliers.Num() == 0)
	{
		return 1.0f;
	}

	// Find the multiplier for the exact level, or the highest level below
	float BestMultiplier = 1.0f;
	int32 BestLevel = 0;

	for (const FItemClassMultiplier& CM : ClassMultipliers)
	{
		if (CM.ClassLevel == ClassLevel)
		{
			return CM.EffectMultiplier;
		}
		if (CM.ClassLevel < ClassLevel && CM.ClassLevel > BestLevel)
		{
			BestLevel = CM.ClassLevel;
			BestMultiplier = CM.EffectMultiplier;
		}
	}

	return BestMultiplier;
}

FItemClassMultiplier UInventoryItemDefinition::GetClassInfo(int32 ClassLevel) const
{
	for (const FItemClassMultiplier& CM : ClassMultipliers)
	{
		if (CM.ClassLevel == ClassLevel)
		{
			return CM;
		}
	}

	FItemClassMultiplier Default;
	Default.ClassLevel = ClassLevel;
	Default.EffectMultiplier = GetClassMultiplier(ClassLevel);
	return Default;
}

float UInventoryItemDefinition::GetLootWeightForLevel(int32 Level) const
{
	if (LootWeights.Num() == 0)
	{
		return 0.0f; // No loot config = won't drop
	}

	float BestWeight = 0.0f;
	int32 BestMinLevel = 0;

	for (const FLootWeightEntry& LW : LootWeights)
	{
		if (LW.MinLevel <= Level && LW.MinLevel >= BestMinLevel)
		{
			BestMinLevel = LW.MinLevel;
			BestWeight = LW.Weight;
		}
	}

	return BestWeight;
}

bool UInventoryItemDefinition::ContainerAcceptsItemType(FName InItemType) const
{
	if (!bIsContainer) return false;
	if (ContainerAcceptedTypes.Num() == 0) return true;
	return ContainerAcceptedTypes.Contains(InItemType);
}

FPrimaryAssetId UInventoryItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType("InventoryItem"), ItemID);
}
