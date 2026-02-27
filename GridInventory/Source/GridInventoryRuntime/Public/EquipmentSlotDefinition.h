// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EquipmentSlotDefinition.generated.h"

/**
 * Defines the configuration of a single equipment slot.
 * You add these to the EquipmentComponent's SlotDefinitions array in the editor.
 *
 * Example slots:
 *   SlotID: "Head",       AcceptedItemType: "Helmet",     bAllowStacking: false
 *   SlotID: "MainHand",   AcceptedItemType: "Weapon_1H",  bAllowStacking: false
 *   SlotID: "PotionSlot1",AcceptedItemType: "Consumable",  bAllowStacking: true, MaxStackSize: 10
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FEquipmentSlotDefinition
{
	GENERATED_BODY()

	/** Unique ID for this slot (e.g. "Head", "MainHand", "PotionSlot1") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	FName SlotID;

	/** Display name for UI (e.g. "Helm", "Haupthand", "Trank 1") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	FText DisplayName;

	/**
	 * Which item type this slot accepts.
	 * Must match the ItemType on InventoryItemDefinition.
	 * Use NAME_None to accept any item type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	FName AcceptedItemType;

	/**
	 * Optional: Additional accepted types.
	 * Useful for slots that accept multiple types (e.g. MainHand accepts "Weapon_1H" and "Weapon_2H").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	TArray<FName> AdditionalAcceptedTypes;

	/** Whether stacking is allowed in this slot (true for consumables, false for armor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	bool bAllowStacking;

	/** Max stack size if stacking is allowed (overrides item's MaxStackSize if smaller) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot", meta = (ClampMin = "1", EditCondition = "bAllowStacking"))
	int32 MaxStackSize;

	/** Optional placeholder icon shown when slot is empty (soft reference) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment Slot")
	TSoftObjectPtr<UTexture2D> EmptySlotIcon;

	FEquipmentSlotDefinition()
		: SlotID(NAME_None)
		, AcceptedItemType(NAME_None)
		, bAllowStacking(false)
		, MaxStackSize(1)
	{
	}

	/** Check if an item type is accepted by this slot */
	bool AcceptsItemType(FName InItemType) const
	{
		// NAME_None = accepts anything
		if (AcceptedItemType == NAME_None)
		{
			return true;
		}

		if (AcceptedItemType == InItemType)
		{
			return true;
		}

		return AdditionalAcceptedTypes.Contains(InItemType);
	}
};
