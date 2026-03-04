// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryGrid.h"
#include "RandomItemEntry.h"
#include "InventoryContainer.generated.h"

class UGridInventoryComponent;
class ULootTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContainerInteraction, AActor*, Interactor, AInventoryContainer*, Container);

/**
 * Placeable inventory container (chest, barrel, sack, crate, etc.).
 * Has its own GridInventoryComponent with configurable size.
 *
 * Usage:
 * 1. Place in level or spawn at runtime
 * 2. Set GridWidth/GridHeight on the InventoryComponent
 * 3. Optionally pre-fill with items
 * 4. Player interacts -> fires OnContainerOpened -> show UI with two inventory grids
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYRUNTIME_API AInventoryContainer : public AActor
{
	GENERATED_BODY()

public:
	AInventoryContainer();

	/** The inventory this container holds */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	UGridInventoryComponent* InventoryComponent;

	/**
	 * Stable identifier for save/load — must be unique per container in the level.
	 * Set this in the editor for each placed container. Containers without an ID
	 * are skipped during save/load.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Save", SaveGame)
	FName ContainerID;

	// ========================
	// Lock System
	// ========================

	/** Whether this container is locked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Lock", Replicated)
	bool bIsLocked;

	/** If locked, which key item ID is required to open it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Lock", meta = (EditCondition = "bIsLocked"))
	FName RequiredKeyItemID;

	/** Whether the key is consumed when used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Lock", meta = (EditCondition = "bIsLocked"))
	bool bConsumeKey;

	// ========================
	// Loot System
	// ========================

	/** Loot table for random item generation on first open */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Loot")
	TSoftObjectPtr<ULootTable> LootTable;

	/**
	 * Weight multiplier for loot generation.
	 * 1.0 = normal, 3.0 = rare/golden chest, 0.5 = junk container
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Loot", meta = (ClampMin = "0.01"))
	float DropWeightMultiplier;

	/**
	 * Level for loot weight calculation.
	 * 0 = will try to get level from interacting player (you implement this in Blueprint).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Loot", meta = (ClampMin = "0"))
	int32 LootLevel;

	/** Whether loot has already been generated (prevents re-rolling) */
	UPROPERTY(BlueprintReadOnly, Category = "Container|Loot", SaveGame, Replicated)
	bool bLootGenerated;

	/**
	 * Generate loot and fill the container inventory.
	 * Called automatically on first open if LootTable is set.
	 * Can also be called manually (e.g. for respawning loot).
	 * @param OverrideLevel Level to use. 0 = use LootLevel property.
	 */
	UFUNCTION(BlueprintCallable, Category = "Container|Loot")
	void GenerateLoot(int32 OverrideLevel = 0);

	/**
	 * Async loot generation. Loads all item definitions in the background first.
	 * Fires OnLootGenerated when complete. Use this for gameplay to avoid hitches.
	 */
	UFUNCTION(BlueprintCallable, Category = "Container|Loot")
	void GenerateLootAsync(int32 OverrideLevel = 0);

	/** Fired when async loot generation is complete */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootGenerated, AInventoryContainer*, Container);

	UPROPERTY(BlueprintAssignable, Category = "Container|Events")
	FOnLootGenerated OnLootGenerated;

	/** Force re-generation of loot (clears existing and regenerates) */
	UFUNCTION(BlueprintCallable, Category = "Container|Loot")
	void RegenerateLoot(int32 OverrideLevel = 0);

	// ========================
	// Default Items (Editor Pre-Fill)
	// ========================

	/**
	 * Items that are always placed in this container at game start.
	 * Configure in the editor per container instance — select Item + Count.
	 * These are added BEFORE loot table generation.
	 * Can be combined with LootTable for fixed + random items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Default Items")
	TArray<FItemAddRequest> DefaultItems;

	// ========================
	// Random Default Items
	// ========================

	/**
	 * Items that may randomly appear in this container at game start.
	 * Each entry has a SpawnChance (0-1) and a MinCount/MaxCount range.
	 * SpawnChance is used as weight for selection when MinRandomItems/MaxRandomItems are set.
	 *
	 * Example: Heilkraut with SpawnChance 0.5, Min 1, Max 3
	 * -> 50% chance to appear, if it does: 1-3 items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Random Items")
	TArray<FRandomItemEntry> RandomDefaultItems;

	/**
	 * Minimum number of different items to pick from RandomDefaultItems.
	 * 0 = roll each entry individually (old behavior).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Random Items", meta = (ClampMin = "0"))
	int32 MinRandomItems;

	/**
	 * Maximum number of different items to pick from RandomDefaultItems.
	 * 0 = roll each entry individually (old behavior).
	 * If both Min and Max are > 0, a random count between Min-Max entries is selected
	 * using SpawnChance as weight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Random Items", meta = (ClampMin = "0"))
	int32 MaxRandomItems;

	/**
	 * Generate random items from the RandomDefaultItems list.
	 * Called automatically in BeginPlay. Can be called again to add more random items.
	 * Respects DropWeightMultiplier and MinRandomItems/MaxRandomItems.
	 */
	UFUNCTION(BlueprintCallable, Category = "Container|Random Items")
	void GenerateRandomDefaults();

	// ========================
	// State
	// ========================

	/** Whether the container is currently open (being interacted with) */
	UPROPERTY(BlueprintReadOnly, Category = "Container", ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen;

	/** Who currently has this container open (null if closed) */
	UPROPERTY(BlueprintReadOnly, Category = "Container")
	AActor* CurrentInteractor;

	// ========================
	// Events
	// ========================

	UPROPERTY(BlueprintAssignable, Category = "Container|Events")
	FOnContainerInteraction OnContainerOpened;

	UPROPERTY(BlueprintAssignable, Category = "Container|Events")
	FOnContainerInteraction OnContainerClosed;

	UPROPERTY(BlueprintAssignable, Category = "Container|Events")
	FOnContainerInteraction OnContainerLockFailed;

	// ========================
	// Functions
	// ========================

	/**
	 * Try to open this container.
	 * Checks lock state, consumes key if needed.
	 * @param Interactor The actor trying to open (usually the player)
	 * @return true if opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Container")
	bool TryOpen(AActor* Interactor);

	/** Close the container */
	UFUNCTION(BlueprintCallable, Category = "Container")
	void Close();

	/** Unlock the container (e.g. via script/trigger) */
	UFUNCTION(BlueprintCallable, Category = "Container")
	void Unlock();

	/** Lock the container */
	UFUNCTION(BlueprintCallable, Category = "Container")
	void Lock(FName NewKeyItemID = NAME_None);

	/** Check if an actor has the required key */
	UFUNCTION(BlueprintPure, Category = "Container")
	bool HasRequiredKey(AActor* Actor) const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsOpen();

	/** Override in Blueprint for custom open/close animations */
	UFUNCTION(BlueprintNativeEvent, Category = "Container")
	void OnOpenStateChanged(bool bOpened);
};
