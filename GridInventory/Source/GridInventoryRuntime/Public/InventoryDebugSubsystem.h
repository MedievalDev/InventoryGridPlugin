// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "InventoryDebugSubsystem.generated.h"

class UGridInventoryComponent;
class UEquipmentComponent;
class UInventoryItemDefinition;

/**
 * Debug subsystem for the Grid Inventory Plugin.
 * Registers console commands for testing, spawning items, and visualizing
 * the inventory state at runtime.
 *
 * All commands are prefixed with "Inv." and only exist in non-shipping builds.
 *
 * Usage examples:
 *   Inv.ItemDB                       -- list all available item definitions
 *   Inv.Add Eisenschwert 1           -- add 1 Eisenschwert to inventory
 *   Inv.Equip MainHand Eisenschwert  -- equip to slot
 *   Inv.List                         -- show all items with UIDs
 *   Inv.Debug on                     -- enable screen debug overlay
 */
UCLASS()
class GRIDINVENTORYRUNTIME_API UInventoryDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
#if !UE_BUILD_SHIPPING

	// Item Management
	void Cmd_Add(const TArray<FString>& Args, UWorld* World);
	void Cmd_AddAt(const TArray<FString>& Args, UWorld* World);
	void Cmd_Remove(const TArray<FString>& Args, UWorld* World);
	void Cmd_Consume(const TArray<FString>& Args, UWorld* World);
	void Cmd_Drop(const TArray<FString>& Args, UWorld* World);
	void Cmd_Give(const TArray<FString>& Args, UWorld* World);

	// Equipment
	void Cmd_Equip(const TArray<FString>& Args, UWorld* World);
	void Cmd_Unequip(const TArray<FString>& Args, UWorld* World);
	void Cmd_ListSlots(const TArray<FString>& Args, UWorld* World);

	// Merge/Upgrade
	void Cmd_Merge(const TArray<FString>& Args, UWorld* World);
	void Cmd_SetClass(const TArray<FString>& Args, UWorld* World);

	// Inventory Settings
	void Cmd_SetMaxWeight(const TArray<FString>& Args, UWorld* World);
	void Cmd_SetGridSize(const TArray<FString>& Args, UWorld* World);
	void Cmd_Clear(const TArray<FString>& Args, UWorld* World);
	void Cmd_Sort(const TArray<FString>& Args, UWorld* World);

	// Info & Search
	void Cmd_List(const TArray<FString>& Args, UWorld* World);
	void Cmd_ListByType(const TArray<FString>& Args, UWorld* World);
	void Cmd_Find(const TArray<FString>& Args, UWorld* World);
	void Cmd_Info(const TArray<FString>& Args, UWorld* World);
	void Cmd_ItemDB(const TArray<FString>& Args, UWorld* World);

	// Debug Visualization
	void Cmd_Debug(const TArray<FString>& Args, UWorld* World);
	void Cmd_DebugBitmap(const TArray<FString>& Args, UWorld* World);
	void Cmd_DebugFreeRects(const TArray<FString>& Args, UWorld* World);
	void Cmd_DebugPerformance(const TArray<FString>& Args, UWorld* World);
	void Cmd_DebugMemory(const TArray<FString>& Args, UWorld* World);
	void Cmd_DebugSlots(const TArray<FString>& Args, UWorld* World);

	// Container
	void Cmd_ContainerList(const TArray<FString>& Args, UWorld* World);
	void Cmd_ContainerOpen(const TArray<FString>& Args, UWorld* World);
	void Cmd_ContainerLoot(const TArray<FString>& Args, UWorld* World);

	// Gold
	void Cmd_GoldAdd(const TArray<FString>& Args, UWorld* World);
	void Cmd_GoldSet(const TArray<FString>& Args, UWorld* World);

	// Helpers
	UGridInventoryComponent* GetPlayerInventory(UWorld* World) const;
	UEquipmentComponent* GetPlayerEquipment(UWorld* World) const;
	APawn* GetLocalPlayerPawn(UWorld* World) const;
	UInventoryItemDefinition* FindItemDefByID(FName ItemID) const;
	FGuid ResolveShortUID(UGridInventoryComponent* Inv, const FString& ShortUID) const;

	void DebugPrint(const FString& Message, FColor Color = FColor::Green) const;
	void DebugPrintWarning(const FString& Message) const;
	void DebugPrintError(const FString& Message) const;

	TArray<IConsoleObject*> RegisteredCommands;

	void RegisterCommand(const TCHAR* Name, const TCHAR* Help,
		void (UInventoryDebugSubsystem::*Handler)(const TArray<FString>&, UWorld*));

	bool bPerformanceOverlay;
	bool bDebugOverlay;

	TMap<FName, TSoftObjectPtr<UInventoryItemDefinition>> CachedItemDB;
	bool bItemDBScanned;
	void EnsureItemDBScanned();

#endif // !UE_BUILD_SHIPPING
};
