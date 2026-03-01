// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ItemEffectValue.h"
#include "InventorySaveGame.generated.h"

/**
 * Save data for a single item instance.
 * Used for inventory, equipment, and container items.
 */
USTRUCT()
struct FItemSaveEntry
{
	GENERATED_BODY()

	/** Path to the DataAsset (for normal items) */
	UPROPERTY()
	FSoftObjectPath ItemDefPath;

	/** True if this is a runtime-created item (no DataAsset on disk) */
	UPROPERTY()
	bool bIsRuntimeCreated;

	/** JSON data for runtime items — complete definition inline */
	UPROPERTY()
	FString RuntimeItemJSON;

	/** Grid position in the inventory */
	UPROPERTY()
	FIntPoint GridPosition;

	/** Whether the item is rotated 90 degrees */
	UPROPERTY()
	bool bIsRotated;

	/** Stack count */
	UPROPERTY()
	int32 StackCount;

	/** Current class level (1 = base, increases through merging) */
	UPROPERTY()
	int32 CurrentClassLevel;

	/** Unique ID for this instance */
	UPROPERTY()
	FGuid UniqueID;

	/** Instance-specific effect overrides */
	UPROPERTY()
	TArray<FItemEffectValue> InstanceEffects;

	FItemSaveEntry()
		: bIsRuntimeCreated(false)
		, bIsRotated(false)
		, StackCount(1)
		, CurrentClassLevel(1)
	{
	}
};

/**
 * Save data for a container item's sub-inventory (e.g. herb pouch contents).
 * Stored separately to avoid struct recursion (UE4.27 UHT limitation).
 */
USTRUCT()
struct FSubInventorySaveData
{
	GENERATED_BODY()

	/** UniqueID of the container item these sub-items belong to */
	UPROPERTY()
	FGuid ParentItemID;

	/** Items inside this container */
	UPROPERTY()
	TArray<FItemSaveEntry> Items;
};

/**
 * Save data for a world container (AInventoryContainer).
 */
USTRUCT()
struct FContainerSaveData
{
	GENERATED_BODY()

	/** Stable ID for re-finding the container actor after load */
	UPROPERTY()
	FName ContainerID;

	/** Lock state */
	UPROPERTY()
	bool bIsLocked;

	UPROPERTY()
	FName RequiredKeyItemID;

	UPROPERTY()
	bool bConsumeKey;

	/** Whether loot was already generated (prevents re-rolling) */
	UPROPERTY()
	bool bLootGenerated;

	/** Items in this container's inventory */
	UPROPERTY()
	TArray<FItemSaveEntry> Items;

	FContainerSaveData()
		: bIsLocked(false)
		, bConsumeKey(false)
		, bLootGenerated(false)
	{
	}
};

/**
 * Main save game class for the inventory system.
 * Stores complete state: player inventory, equipment, world containers.
 *
 * Slot-based: saved via UGameplayStatics::SaveGameToSlot with
 * SlotName = "Inventory_" + SlotIndex.
 */
UCLASS()
class GRIDINVENTORYRUNTIME_API UInventorySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UInventorySaveGame();

	/** Grid dimensions at save time */
	UPROPERTY()
	int32 GridWidth;

	UPROPERTY()
	int32 GridHeight;

	/** Saved gold amount */
	UPROPERTY()
	float SavedGold;

	/** All items in the player inventory */
	UPROPERTY()
	TArray<FItemSaveEntry> InventoryItems;

	/** Sub-inventory contents for container items (herb pouches, etc.) */
	UPROPERTY()
	TArray<FSubInventorySaveData> SubInventories;

	/** Equipment slot IDs (parallel array with EquipmentItems) */
	UPROPERTY()
	TArray<FName> EquipmentSlotIDs;

	/** Equipment items (parallel array with EquipmentSlotIDs) */
	UPROPERTY()
	TArray<FItemSaveEntry> EquipmentItems;

	/** World container save data */
	UPROPERTY()
	TArray<FContainerSaveData> Containers;
};
