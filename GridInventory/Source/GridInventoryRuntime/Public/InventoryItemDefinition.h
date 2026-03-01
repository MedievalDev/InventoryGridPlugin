// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemEffectValue.h"
#include "ItemClassMultiplier.h"
#include "ItemRequirement.h"
#include "IngredientSpawnData.h"
#include "InventoryItemDefinition.generated.h"

/**
 * Defines a type of inventory item (template).
 * Create as DataAsset: Right-click -> Miscellaneous -> Data Asset -> InventoryItemDefinition
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYRUNTIME_API UInventoryItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UInventoryItemDefinition();

	// ========================
	// Basic Info
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Item type for equipment slot matching and container filtering */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemType;

	// ========================
	// Grid
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1"))
	FIntPoint GridSize;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid")
	bool bCanRotate;

	// ========================
	// Stacking
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking")
	bool bCanStack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking", meta = (ClampMin = "1", EditCondition = "bCanStack"))
	int32 MaxStackSize;

	// ========================
	// Weight
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weight", meta = (ClampMin = "0.0"))
	float Weight;

	// ========================
	// Trade
	// ========================

	/** Base gold value of this item (used for buying/selling with NPCs) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0"))
	int32 BaseValue;

	// ========================
	// Requirements
	// ========================

	/**
	 * Requirements the player must meet to equip or use this item.
	 * Each entry is a stat name + minimum value.
	 * The EquipmentComponent checks these via the OnGetPlayerStat delegate.
	 *
	 * Examples:
	 *   "Level" >= 8, "Staerke" >= 10, "Geschicklichkeit" >= 5
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
	TArray<FItemRequirement> EquipRequirements;

	// ========================
	// Effect Values
	// ========================

	/**
	 * Base effect values for this item type.
	 * These are the defaults - instances can override or add to these.
	 * Examples: "Damage" 10, "Armor" 5, "HealAmount" 50
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<FItemEffectValue> BaseEffects;

	// ========================
	// Consumable
	// ========================

	/** Whether this item can be consumed / used directly from the inventory */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bIsConsumable;

	/** Whether the item is destroyed after use (true for potions, false for reusable tools) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (EditCondition = "bIsConsumable"))
	bool bDestroyOnConsume;

	// ========================
	// Merge / Upgrade System
	// ========================

	/**
	 * Whether identical items can be merged to increase class level.
	 * Drag one onto another -> sacrifice item is destroyed, target class level +1.
	 * Only works on non-stackable items (MaxStack = 1).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Merge Upgrade")
	bool bCanMergeUpgrade;

	/** Maximum class level reachable through merging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Merge Upgrade", meta = (ClampMin = "1", EditCondition = "bCanMergeUpgrade"))
	int32 MaxClassLevel;

	/**
	 * Multipliers per class level. Add entries in editor.
	 * All BaseEffects are multiplied by the matching class level's multiplier.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Merge Upgrade", meta = (EditCondition = "bCanMergeUpgrade"))
	TArray<FItemClassMultiplier> ClassMultipliers;

	// ========================
	// Loot / Drop Weights
	// ========================

	/**
	 * Drop weight per level. Used by the loot system.
	 * Add entries: Level 1 Weight 0.1, Level 5 Weight 2.0, etc.
	 * System picks the highest MinLevel <= current level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FLootWeightEntry> LootWeights;

	// ========================
	// World
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World")
	TSoftClassPtr<AActor> WorldActorClass;

	// ========================
	// Item Container
	// ========================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container")
	bool bIsContainer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "1", EditCondition = "bIsContainer"))
	FIntPoint ContainerGridSize;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "0.0", EditCondition = "bIsContainer"))
	float ContainerMaxWeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (EditCondition = "bIsContainer"))
	TArray<FName> ContainerAcceptedTypes;

	// ========================
	// Category / Classification
	// ========================

	/** Category tag for filtering (e.g. "PflanzenKraeuter", "Pilze", "Mineralien") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Classification")
	FName CategoryTag;

	// ========================
	// Ingredient Spawn Data
	// ========================

	/** Spawn conditions for procedural placement (biome, light, soil color) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
	FIngredientSpawnData SpawnData;

	// ========================
	// Alchemy Properties
	// ========================

	/** Primary effect type tag (e.g. "Heilung", "Gift", "Mana", "Stamina") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alchemy")
	FName WirkungTag;

	/** Protection string, parseable at runtime (e.g. "Physischem Schaden +10%") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alchemy")
	FString SchutzVor;

	/** Antidote string (e.g. "Spinnengift", "Schlangengift") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alchemy")
	FString Gegengift;

	/** Miscellaneous properties string (e.g. "Hochentzuendlich", "Brennbar") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alchemy")
	FString Sonstiges;

	/** Effect duration in minutes. 0 = permanent / instant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alchemy", meta = (ClampMin = "0.0"))
	float DurationMinutes;

	// ========================
	// Functions
	// ========================

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	FIntPoint GetEffectiveSize(bool bRotated) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	float GetStackWeight(int32 StackCount) const;

	/** Get a base effect value by ID */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	float GetBaseEffectValue(FName EffectID) const;

	/** Get the class multiplier for a specific class level */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	float GetClassMultiplier(int32 ClassLevel) const;

	/** Get the class info struct for a specific level */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	FItemClassMultiplier GetClassInfo(int32 ClassLevel) const;

	/** Get the loot weight for a specific player/area level */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	float GetLootWeightForLevel(int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Item")
	bool ContainerAcceptsItemType(FName InItemType) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
