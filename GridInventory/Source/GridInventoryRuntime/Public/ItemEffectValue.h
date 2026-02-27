// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ItemEffectValue.generated.h"

/**
 * A single dynamic effect value on an item.
 * Examples: "Damage" 25.0, "FireResistance" 15.0, "HealAmount" 50.0, "Durability" 100.0
 *
 * Stored as a simple struct in a TArray for optimal cache performance.
 * At 5-20 effects per item, linear search beats TMap by a wide margin.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FItemEffectValue
{
	GENERATED_BODY()

	/** Unique effect identifier (e.g. "Damage", "Armor", "HealAmount", "ManaRegen") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Effect")
	FName EffectID;

	/** The value of this effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Effect")
	float Value;

	FItemEffectValue() : EffectID(NAME_None), Value(0.0f) {}
	FItemEffectValue(FName InID, float InValue) : EffectID(InID), Value(InValue) {}

	bool operator==(const FItemEffectValue& Other) const { return EffectID == Other.EffectID; }
};

/**
 * Helper to work with arrays of FItemEffectValue.
 */
namespace ItemEffectHelpers
{
	/** Find an effect value by ID. Returns nullptr if not found. */
	inline const FItemEffectValue* FindEffect(const TArray<FItemEffectValue>& Effects, FName EffectID)
	{
		for (const FItemEffectValue& E : Effects)
		{
			if (E.EffectID == EffectID) return &E;
		}
		return nullptr;
	}

	/** Get effect value, returns DefaultValue if not found. */
	inline float GetValue(const TArray<FItemEffectValue>& Effects, FName EffectID, float DefaultValue = 0.0f)
	{
		const FItemEffectValue* Found = FindEffect(Effects, EffectID);
		return Found ? Found->Value : DefaultValue;
	}

	/** Set or add an effect value. */
	inline void SetValue(TArray<FItemEffectValue>& Effects, FName EffectID, float Value)
	{
		for (FItemEffectValue& E : Effects)
		{
			if (E.EffectID == EffectID)
			{
				E.Value = Value;
				return;
			}
		}
		Effects.Add(FItemEffectValue(EffectID, Value));
	}

	/** Remove an effect. Returns true if found and removed. */
	inline bool RemoveEffect(TArray<FItemEffectValue>& Effects, FName EffectID)
	{
		for (int32 i = 0; i < Effects.Num(); ++i)
		{
			if (Effects[i].EffectID == EffectID)
			{
				Effects.RemoveAt(i);
				return true;
			}
		}
		return false;
	}
}
