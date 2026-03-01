// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ItemRequirement.generated.h"

/**
 * A single requirement for equipping or using an item.
 * Completely data-driven — you define the name and minimum value.
 *
 * Examples:
 *   RequirementID: "Level",           MinValue: 8
 *   RequirementID: "Staerke",         MinValue: 10
 *   RequirementID: "Geschicklichkeit", MinValue: 5
 *   RequirementID: "Intelligenz",     MinValue: 15
 *
 * The EquipmentComponent asks your Blueprint for the player's current
 * stat value via the OnGetPlayerStat delegate.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FItemRequirement
{
	GENERATED_BODY()

	/** Name of the requirement (matches your player's stat variable names) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement")
	FName RequirementID;

	/** Minimum value the player must have to equip/use this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement")
	float MinValue;

	FItemRequirement()
		: RequirementID(NAME_None)
		, MinValue(0.0f)
	{
	}

	FItemRequirement(FName InID, float InMinValue)
		: RequirementID(InID)
		, MinValue(InMinValue)
	{
	}
};
