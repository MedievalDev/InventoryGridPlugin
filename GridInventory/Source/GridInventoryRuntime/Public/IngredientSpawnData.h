// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IngredientSpawnData.generated.h"

/**
 * Spawn conditions for alchemy ingredients.
 * Defines where an ingredient can be found in the world based on
 * biome type, light conditions, and soil color ranges.
 *
 * Used by the procedural spawning system to place ingredients
 * in appropriate world locations.
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FIngredientSpawnData
{
	GENERATED_BODY()

	/** Whether this item has spawn data configured */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bHasSpawnData;

	/** Biomes where this ingredient can spawn (e.g. "Laubwald", "Wiese", "Gebirge") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (EditCondition = "bHasSpawnData"))
	TArray<FName> Biomes;

	// ========================
	// Light Conditions (0-255 from sun mask / lightmap)
	// ========================

	/** Minimum light value (0 = full shade, 255 = full sun) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Light", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 LichtMin;

	/** Maximum light value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Light", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 LichtMax;

	// ========================
	// Soil Color Ranges (RGB from landscape vertex color / material)
	// ========================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenR_Min;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenR_Max;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenG_Min;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenG_Max;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenB_Min;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Boden", meta = (ClampMin = "0", ClampMax = "255", EditCondition = "bHasSpawnData"))
	uint8 BodenB_Max;

	FIngredientSpawnData()
		: bHasSpawnData(false)
		, LichtMin(0)
		, LichtMax(255)
		, BodenR_Min(0)
		, BodenR_Max(255)
		, BodenG_Min(0)
		, BodenG_Max(255)
		, BodenB_Min(0)
		, BodenB_Max(255)
	{
	}

	/** Check if a given light value falls within the valid range */
	bool IsLightValid(uint8 LightValue) const
	{
		return LightValue >= LichtMin && LightValue <= LichtMax;
	}

	/** Check if a soil color falls within the valid RGB ranges */
	bool IsSoilColorValid(uint8 R, uint8 G, uint8 B) const
	{
		return R >= BodenR_Min && R <= BodenR_Max
			&& G >= BodenG_Min && G <= BodenG_Max
			&& B >= BodenB_Min && B <= BodenB_Max;
	}

	/** Check all spawn conditions at once */
	bool AreConditionsMet(uint8 LightValue, uint8 SoilR, uint8 SoilG, uint8 SoilB, FName Biome) const
	{
		if (!bHasSpawnData) return true; // No restrictions
		if (!IsLightValid(LightValue)) return false;
		if (!IsSoilColorValid(SoilR, SoilG, SoilB)) return false;
		if (Biomes.Num() > 0 && !Biomes.Contains(Biome)) return false;
		return true;
	}
};
