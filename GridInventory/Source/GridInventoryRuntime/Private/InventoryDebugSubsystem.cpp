// Copyright Marco. All Rights Reserved.

#include "InventoryDebugSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "GridInventoryComponent.h"
#include "EquipmentComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryItemInstance.h"
#include "ItemContainerInventory.h"
#include "InventoryContainer.h"
#include "InventoryGrid.h"
#include "LootTable.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

#endif // !UE_BUILD_SHIPPING

// ============================================================================
// Lifecycle
// ============================================================================

bool UInventoryDebugSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if !UE_BUILD_SHIPPING
	return true;
#else
	return false;
#endif
}

void UInventoryDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
	bPerformanceOverlay = false;
	bDebugOverlay = false;
	bItemDBScanned = false;

	// --- Item Management ---
	RegisterCommand(TEXT("Inv.Add"),
		TEXT("Inv.Add <ItemID> [Count] - Add item to inventory"),
		&UInventoryDebugSubsystem::Cmd_Add);

	RegisterCommand(TEXT("Inv.AddAt"),
		TEXT("Inv.AddAt <ItemID> <X> <Y> [Count] [bRotated 0|1] - Add item at position"),
		&UInventoryDebugSubsystem::Cmd_AddAt);

	RegisterCommand(TEXT("Inv.Remove"),
		TEXT("Inv.Remove <ShortUID> [Count] - Remove item (0=full stack)"),
		&UInventoryDebugSubsystem::Cmd_Remove);

	RegisterCommand(TEXT("Inv.Consume"),
		TEXT("Inv.Consume <ShortUID> - Consume item if consumable"),
		&UInventoryDebugSubsystem::Cmd_Consume);

	RegisterCommand(TEXT("Inv.Drop"),
		TEXT("Inv.Drop <ShortUID> - Drop item into the world"),
		&UInventoryDebugSubsystem::Cmd_Drop);

	RegisterCommand(TEXT("Inv.Give"),
		TEXT("Inv.Give <ItemID> <Count> - Bulk add (ignores space, stress test)"),
		&UInventoryDebugSubsystem::Cmd_Give);

	// --- Equipment ---
	RegisterCommand(TEXT("Inv.Equip"),
		TEXT("Inv.Equip <SlotID> <ItemID> [Count] - Equip item in slot"),
		&UInventoryDebugSubsystem::Cmd_Equip);

	RegisterCommand(TEXT("Inv.Unequip"),
		TEXT("Inv.Unequip <SlotID> - Unequip slot back to inventory"),
		&UInventoryDebugSubsystem::Cmd_Unequip);

	RegisterCommand(TEXT("Inv.ListSlots"),
		TEXT("Inv.ListSlots - Show all equipment slots"),
		&UInventoryDebugSubsystem::Cmd_ListSlots);

	// --- Merge/Upgrade ---
	RegisterCommand(TEXT("Inv.Merge"),
		TEXT("Inv.Merge <TargetUID> <SacrificeUID> - Merge two items"),
		&UInventoryDebugSubsystem::Cmd_Merge);

	RegisterCommand(TEXT("Inv.SetClass"),
		TEXT("Inv.SetClass <ShortUID> <Level> - Set item class level"),
		&UInventoryDebugSubsystem::Cmd_SetClass);

	// --- Inventory Settings ---
	RegisterCommand(TEXT("Inv.SetMaxWeight"),
		TEXT("Inv.SetMaxWeight <Weight> - Set max weight (0=unlimited)"),
		&UInventoryDebugSubsystem::Cmd_SetMaxWeight);

	RegisterCommand(TEXT("Inv.SetGridSize"),
		TEXT("Inv.SetGridSize <Width> <Height> - Resize grid (preserves items, fails if they don't fit)"),
		&UInventoryDebugSubsystem::Cmd_SetGridSize);

	RegisterCommand(TEXT("Inv.Clear"),
		TEXT("Inv.Clear - Clear entire inventory"),
		&UInventoryDebugSubsystem::Cmd_Clear);

	RegisterCommand(TEXT("Inv.Sort"),
		TEXT("Inv.Sort - Sort inventory"),
		&UInventoryDebugSubsystem::Cmd_Sort);

	// --- Info & Search ---
	RegisterCommand(TEXT("Inv.List"),
		TEXT("Inv.List - List all items with UID, position, stack"),
		&UInventoryDebugSubsystem::Cmd_List);

	RegisterCommand(TEXT("Inv.ListByType"),
		TEXT("Inv.ListByType <ItemType> - Filter by item type"),
		&UInventoryDebugSubsystem::Cmd_ListByType);

	RegisterCommand(TEXT("Inv.Find"),
		TEXT("Inv.Find <SearchText> - Search items by name"),
		&UInventoryDebugSubsystem::Cmd_Find);

	RegisterCommand(TEXT("Inv.Info"),
		TEXT("Inv.Info <ShortUID> - Detailed item info"),
		&UInventoryDebugSubsystem::Cmd_Info);

	RegisterCommand(TEXT("Inv.ItemDB"),
		TEXT("Inv.ItemDB [Filter] - List all registered item definitions"),
		&UInventoryDebugSubsystem::Cmd_ItemDB);

	// --- Debug Visualization ---
	RegisterCommand(TEXT("Inv.Debug"),
		TEXT("Inv.Debug <on|off> - Toggle debug overlay"),
		&UInventoryDebugSubsystem::Cmd_Debug);

	RegisterCommand(TEXT("Inv.Debug.Bitmap"),
		TEXT("Inv.Debug.Bitmap - Print bitmap occupancy to log"),
		&UInventoryDebugSubsystem::Cmd_DebugBitmap);

	RegisterCommand(TEXT("Inv.Debug.FreeRects"),
		TEXT("Inv.Debug.FreeRects - Print free-rect cache to log"),
		&UInventoryDebugSubsystem::Cmd_DebugFreeRects);

	RegisterCommand(TEXT("Inv.Debug.Performance"),
		TEXT("Inv.Debug.Performance <on|off> - Toggle performance overlay"),
		&UInventoryDebugSubsystem::Cmd_DebugPerformance);

	RegisterCommand(TEXT("Inv.Debug.Memory"),
		TEXT("Inv.Debug.Memory - Print soft ref load status & memory"),
		&UInventoryDebugSubsystem::Cmd_DebugMemory);

	RegisterCommand(TEXT("Inv.Debug.Slots"),
		TEXT("Inv.Debug.Slots - Print widget virtualization stats"),
		&UInventoryDebugSubsystem::Cmd_DebugSlots);

	// --- Containers ---
	RegisterCommand(TEXT("Inv.Container.List"),
		TEXT("Inv.Container.List - List nearby containers"),
		&UInventoryDebugSubsystem::Cmd_ContainerList);

	RegisterCommand(TEXT("Inv.Container.Open"),
		TEXT("Inv.Container.Open <Index> - Force open container"),
		&UInventoryDebugSubsystem::Cmd_ContainerOpen);

	RegisterCommand(TEXT("Inv.Container.Loot"),
		TEXT("Inv.Container.Loot <Index> [Level] - Generate/regen loot"),
		&UInventoryDebugSubsystem::Cmd_ContainerLoot);

	// --- Gold ---
	RegisterCommand(TEXT("Inv.Gold.Add"),
		TEXT("Inv.Gold.Add <Amount> - Add/subtract gold (negative allowed)"),
		&UInventoryDebugSubsystem::Cmd_GoldAdd);

	RegisterCommand(TEXT("Inv.Gold.Set"),
		TEXT("Inv.Gold.Set <Amount> - Set gold to exact value"),
		&UInventoryDebugSubsystem::Cmd_GoldSet);

#endif // !UE_BUILD_SHIPPING
}

void UInventoryDebugSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	for (IConsoleObject* Cmd : RegisteredCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	RegisteredCommands.Empty();
#endif

	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING

// ============================================================================
// Registration Helper
// ============================================================================

void UInventoryDebugSubsystem::RegisterCommand(const TCHAR* Name, const TCHAR* Help,
	void (UInventoryDebugSubsystem::*Handler)(const TArray<FString>&, UWorld*))
{
	// Capture weak pointer to subsystem
	TWeakObjectPtr<UInventoryDebugSubsystem> WeakThis(this);
	auto Callback = Handler;

	IConsoleObject* Cmd = IConsoleManager::Get().RegisterConsoleCommand(
		Name, Help,
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[WeakThis, Callback](const TArray<FString>& Args, UWorld* World)
			{
				if (WeakThis.IsValid())
				{
					(WeakThis.Get()->*Callback)(Args, World);
				}
			}),
		ECVF_Default
	);

	if (Cmd)
	{
		RegisteredCommands.Add(Cmd);
	}
}

// ============================================================================
// Core Helpers
// ============================================================================

APawn* UInventoryDebugSubsystem::GetLocalPlayerPawn(UWorld* World) const
{
	if (!World) return nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

UGridInventoryComponent* UInventoryDebugSubsystem::GetPlayerInventory(UWorld* World) const
{
	APawn* Pawn = GetLocalPlayerPawn(World);
	return Pawn ? Pawn->FindComponentByClass<UGridInventoryComponent>() : nullptr;
}

UEquipmentComponent* UInventoryDebugSubsystem::GetPlayerEquipment(UWorld* World) const
{
	APawn* Pawn = GetLocalPlayerPawn(World);
	return Pawn ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;
}

void UInventoryDebugSubsystem::EnsureItemDBScanned()
{
	if (bItemDBScanned) return;
	bItemDBScanned = true;
	CachedItemDB.Empty();

	UAssetManager& AM = UAssetManager::Get();
	TArray<FAssetData> AssetDataList;

	// Scan for all UInventoryItemDefinition assets
	AM.GetPrimaryAssetDataList(FPrimaryAssetType("InventoryItem"), AssetDataList);

	// Fallback: scan asset registry directly
	if (AssetDataList.Num() == 0)
	{
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		ARM.Get().GetAssetsByClass(UInventoryItemDefinition::StaticClass()->GetFName(), AssetDataList, true);
	}

	for (const FAssetData& AD : AssetDataList)
	{
		TSoftObjectPtr<UInventoryItemDefinition> SoftPtr(AD.ToSoftObjectPath());
		UInventoryItemDefinition* Def = SoftPtr.LoadSynchronous();
		if (Def && Def->ItemID != NAME_None)
		{
			CachedItemDB.Add(Def->ItemID, SoftPtr);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Inv.Debug] ItemDB scanned: %d definitions found"), CachedItemDB.Num());
}

UInventoryItemDefinition* UInventoryDebugSubsystem::FindItemDefByID(FName ItemID) const
{
	// Mutable access for lazy scan
	UInventoryDebugSubsystem* MutableThis = const_cast<UInventoryDebugSubsystem*>(this);
	MutableThis->EnsureItemDBScanned();

	const TSoftObjectPtr<UInventoryItemDefinition>* Found = CachedItemDB.Find(ItemID);
	if (Found)
	{
		return Found->LoadSynchronous();
	}
	return nullptr;
}

FGuid UInventoryDebugSubsystem::ResolveShortUID(UGridInventoryComponent* Inv, const FString& ShortUID) const
{
	if (!Inv || ShortUID.IsEmpty()) return FGuid();

	const FString LowerUID = ShortUID.ToLower();

	for (const FInventoryItemInstance& Item : Inv->GetAllItems())
	{
		FString FullUID = Item.UniqueID.ToString();
		// Match first 8 chars (or full string)
		if (FullUID.Left(LowerUID.Len()).ToLower() == LowerUID)
		{
			return Item.UniqueID;
		}
	}
	return FGuid();
}

void UInventoryDebugSubsystem::DebugPrint(const FString& Message, FColor Color) const
{
	UE_LOG(LogTemp, Log, TEXT("[Inv] %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, Color, Message);
	}
}

void UInventoryDebugSubsystem::DebugPrintWarning(const FString& Message) const
{
	UE_LOG(LogTemp, Warning, TEXT("[Inv] %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Yellow, Message);
	}
}

void UInventoryDebugSubsystem::DebugPrintError(const FString& Message) const
{
	UE_LOG(LogTemp, Error, TEXT("[Inv] %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, Message);
	}
}

// ============================================================================
// Item Management Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_Add(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Add <ItemID> [Count]"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	FName ItemID = FName(*Args[0]);
	int32 Count = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
	Count = FMath::Max(1, Count);

	UInventoryItemDefinition* Def = FindItemDefByID(ItemID);
	if (!Def)
	{
		DebugPrintError(FString::Printf(TEXT("Item '%s' not found. Use Inv.ItemDB to list."), *Args[0]));
		return;
	}

	bool bSuccess = Inv->TryAddItem(Def, Count);
	if (bSuccess)
	{
		DebugPrint(FString::Printf(TEXT("Added %d x %s"), Count, *Def->DisplayName.ToString()));
	}
	else
	{
		DebugPrintWarning(FString::Printf(TEXT("Failed to add %s - no space or overweight"), *Args[0]));
	}
}

void UInventoryDebugSubsystem::Cmd_AddAt(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 3)
	{
		DebugPrint(TEXT("Usage: Inv.AddAt <ItemID> <X> <Y> [Count] [bRotated 0|1]"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	FName ItemID = FName(*Args[0]);
	int32 X = FCString::Atoi(*Args[1]);
	int32 Y = FCString::Atoi(*Args[2]);
	int32 Count = Args.Num() > 3 ? FCString::Atoi(*Args[3]) : 1;
	bool bRotated = Args.Num() > 4 ? FCString::Atoi(*Args[4]) != 0 : false;

	UInventoryItemDefinition* Def = FindItemDefByID(ItemID);
	if (!Def)
	{
		DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[0]));
		return;
	}

	bool bSuccess = Inv->TryAddItemAt(Def, FIntPoint(X, Y), bRotated, FMath::Max(1, Count));
	if (bSuccess)
	{
		DebugPrint(FString::Printf(TEXT("Placed %s at (%d,%d)%s"), *Def->DisplayName.ToString(),
			X, Y, bRotated ? TEXT(" [rotated]") : TEXT("")));
	}
	else
	{
		DebugPrintWarning(FString::Printf(TEXT("Cannot place %s at (%d,%d)"), *Args[0], X, Y));
	}
}

void UInventoryDebugSubsystem::Cmd_Remove(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Remove <ShortUID> [Count] (0=all)"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	FGuid UID = ResolveShortUID(Inv, Args[0]);
	if (!UID.IsValid()) { DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[0])); return; }

	int32 Count = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;

	bool bSuccess = Inv->RemoveItem(UID, Count);
	DebugPrint(bSuccess
		? FString::Printf(TEXT("Removed item %s (count=%d)"), *Args[0], Count)
		: FString::Printf(TEXT("Failed to remove %s"), *Args[0]));
}

void UInventoryDebugSubsystem::Cmd_Consume(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Consume <ShortUID>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	FGuid UID = ResolveShortUID(Inv, Args[0]);
	if (!UID.IsValid()) { DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[0])); return; }

	FInventoryItemInstance Item = Inv->GetItemByID(UID);
	if (!Item.IsValid() || !Item.ItemDef)
	{
		DebugPrintError(TEXT("Invalid item"));
		return;
	}

	if (!Item.ItemDef->bIsConsumable)
	{
		DebugPrintWarning(FString::Printf(TEXT("'%s' is not consumable"), *Item.ItemDef->DisplayName.ToString()));
		return;
	}

	bool bSuccess = Inv->ConsumeItem(UID);
	DebugPrint(bSuccess
		? FString::Printf(TEXT("Consumed %s"), *Item.ItemDef->DisplayName.ToString())
		: TEXT("Consume failed"));
}

void UInventoryDebugSubsystem::Cmd_Drop(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Drop <ShortUID>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	APawn* Pawn = GetLocalPlayerPawn(World);
	if (!Pawn) return;

	FGuid UID = ResolveShortUID(Inv, Args[0]);
	if (!UID.IsValid()) { DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[0])); return; }

	// Drop 150 units in front of pawn
	FVector DropLoc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.0f;
	DropLoc.Z += 50.0f;

	Inv->DropItem(UID, DropLoc, Pawn->GetActorRotation());
	DebugPrint(FString::Printf(TEXT("Dropped item at (%.0f, %.0f, %.0f)"), DropLoc.X, DropLoc.Y, DropLoc.Z));
}

void UInventoryDebugSubsystem::Cmd_Give(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		DebugPrint(TEXT("Usage: Inv.Give <ItemID> <Count> - Bulk add (stress test)"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	FName ItemID = FName(*Args[0]);
	int32 Count = FMath::Max(1, FCString::Atoi(*Args[1]));

	UInventoryItemDefinition* Def = FindItemDefByID(ItemID);
	if (!Def)
	{
		DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[0]));
		return;
	}

	double StartTime = FPlatformTime::Seconds();
	int32 Added = 0;

	for (int32 i = 0; i < Count; ++i)
	{
		if (Inv->TryAddItem(Def, 1))
		{
			Added++;
		}
		else
		{
			break;
		}
	}

	double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	DebugPrint(FString::Printf(TEXT("Give: %d/%d x %s added in %.2f ms"),
		Added, Count, *Def->DisplayName.ToString(), ElapsedMs));
}

// ============================================================================
// Equipment Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_Equip(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		DebugPrint(TEXT("Usage: Inv.Equip <SlotID> <ItemID> [Count]"));
		return;
	}

	UEquipmentComponent* Equip = GetPlayerEquipment(World);
	if (!Equip) { DebugPrintError(TEXT("No equipment component on player pawn")); return; }

	FName SlotID = FName(*Args[0]);
	FName ItemID = FName(*Args[1]);
	int32 Count = Args.Num() > 2 ? FMath::Max(1, FCString::Atoi(*Args[2])) : 1;

	UInventoryItemDefinition* Def = FindItemDefByID(ItemID);
	if (!Def)
	{
		DebugPrintError(FString::Printf(TEXT("Item '%s' not found"), *Args[1]));
		return;
	}

	if (!Equip->CanEquipInSlot(SlotID, Def))
	{
		DebugPrintWarning(FString::Printf(TEXT("'%s' cannot be equipped in slot '%s'"),
			*Args[1], *Args[0]));
		return;
	}

	bool bSuccess = Equip->EquipItem(SlotID, Def, Count);
	DebugPrint(bSuccess
		? FString::Printf(TEXT("Equipped %s in %s"), *Def->DisplayName.ToString(), *Args[0])
		: TEXT("Equip failed"));
}

void UInventoryDebugSubsystem::Cmd_Unequip(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Unequip <SlotID>"));
		return;
	}

	UEquipmentComponent* Equip = GetPlayerEquipment(World);
	if (!Equip) { DebugPrintError(TEXT("No equipment component")); return; }

	FName SlotID = FName(*Args[0]);
	bool bSuccess = Equip->UnequipItem(SlotID, 0);
	DebugPrint(bSuccess
		? FString::Printf(TEXT("Unequipped slot %s"), *Args[0])
		: FString::Printf(TEXT("Slot '%s' empty or not found"), *Args[0]));
}

void UInventoryDebugSubsystem::Cmd_ListSlots(const TArray<FString>& Args, UWorld* World)
{
	UEquipmentComponent* Equip = GetPlayerEquipment(World);
	if (!Equip) { DebugPrintError(TEXT("No equipment component")); return; }

	DebugPrint(TEXT("=== Equipment Slots ==="), FColor::Cyan);

	const TArray<FEquipmentSlotDefinition>& Slots = Equip->SlotDefinitions;
	for (const FEquipmentSlotDefinition& SlotDef : Slots)
	{
		FInventoryItemInstance Item = Equip->GetItemInSlot(SlotDef.SlotID);
		FString Status;

		if (Item.IsValid() && Item.ItemDef)
		{
			Status = FString::Printf(TEXT("[%s] %s: %s x%d"),
				*SlotDef.SlotID.ToString(),
				*SlotDef.DisplayName.ToString(),
				*Item.ItemDef->DisplayName.ToString(),
				Item.StackCount);
		}
		else
		{
			Status = FString::Printf(TEXT("[%s] %s: <empty> (accepts: %s)"),
				*SlotDef.SlotID.ToString(),
				*SlotDef.DisplayName.ToString(),
				*SlotDef.AcceptedItemType.ToString());
		}

		DebugPrint(Status);
	}
}

// ============================================================================
// Merge/Upgrade Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_Merge(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		DebugPrint(TEXT("Usage: Inv.Merge <TargetUID> <SacrificeUID>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	FGuid TargetUID = ResolveShortUID(Inv, Args[0]);
	FGuid SacrificeUID = ResolveShortUID(Inv, Args[1]);

	if (!TargetUID.IsValid()) { DebugPrintError(FString::Printf(TEXT("Target '%s' not found"), *Args[0])); return; }
	if (!SacrificeUID.IsValid()) { DebugPrintError(FString::Printf(TEXT("Sacrifice '%s' not found"), *Args[1])); return; }

	bool bSuccess = Inv->MergeItems(TargetUID, SacrificeUID);
	if (bSuccess)
	{
		FInventoryItemInstance Merged = Inv->GetItemByID(TargetUID);
		DebugPrint(FString::Printf(TEXT("Merged! %s now Class Level %d"),
			*Merged.GetDisplayName().ToString(), Merged.CurrentClassLevel));
	}
	else
	{
		DebugPrintWarning(TEXT("Merge failed - check same ItemDef, bCanMergeUpgrade, MaxClassLevel"));
	}
}

void UInventoryDebugSubsystem::Cmd_SetClass(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		DebugPrint(TEXT("Usage: Inv.SetClass <ShortUID> <Level>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	FGuid UID = ResolveShortUID(Inv, Args[0]);
	if (!UID.IsValid()) { DebugPrintError(TEXT("Item not found")); return; }

	int32 Level = FMath::Max(1, FCString::Atoi(*Args[1]));

	// Direct modification via Items array access
	for (FInventoryItemInstance& Item : const_cast<TArray<FInventoryItemInstance>&>(Inv->GetAllItems()))
	{
		if (Item.UniqueID == UID)
		{
			int32 MaxLevel = Item.ItemDef ? Item.ItemDef->MaxClassLevel : Level;
			Item.CurrentClassLevel = FMath::Min(Level, MaxLevel);
			DebugPrint(FString::Printf(TEXT("Set %s to Class Level %d (max %d)"),
				*Item.GetDisplayName().ToString(), Item.CurrentClassLevel, MaxLevel));
			return;
		}
	}

	DebugPrintError(TEXT("Item not found in array"));
}

// ============================================================================
// Inventory Settings Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_SetMaxWeight(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.SetMaxWeight <Weight> (0=unlimited)"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	float NewWeight = FCString::Atof(*Args[0]);
	Inv->MaxWeight = FMath::Max(0.0f, NewWeight);
	DebugPrint(FString::Printf(TEXT("MaxWeight set to %.1f (current: %.1f)"),
		Inv->MaxWeight, Inv->GetCurrentWeight()));
}

void UInventoryDebugSubsystem::Cmd_SetGridSize(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		DebugPrint(TEXT("Usage: Inv.SetGridSize <Width> <Height>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	const int32 W = FCString::Atoi(*Args[0]);
	const int32 H = FCString::Atoi(*Args[1]);

	if (Inv->ResizeGrid(W, H))
	{
		DebugPrint(FString::Printf(TEXT("Grid resized to %dx%d (%d items preserved)"),
			Inv->GridWidth, Inv->GridHeight, Inv->GetAllItems().Num()));
	}
	else
	{
		DebugPrintError(FString::Printf(TEXT("Cannot resize to %dx%d — items would not fit!"), W, H));
	}
}

void UInventoryDebugSubsystem::Cmd_Clear(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	int32 ItemCount = Inv->GetAllItems().Num();
	Inv->ClearInventory();
	DebugPrintWarning(FString::Printf(TEXT("Cleared %d items from inventory"), ItemCount));
}

void UInventoryDebugSubsystem::Cmd_Sort(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	double Start = FPlatformTime::Seconds();
	Inv->SortInventory();
	double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;

	DebugPrint(FString::Printf(TEXT("Inventory sorted in %.2f ms"), Ms));
}

// ============================================================================
// Info & Search Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_List(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	const TArray<FInventoryItemInstance>& Items = Inv->GetAllItems();

	DebugPrint(FString::Printf(TEXT("=== Inventory: %d items, %.1f/%.1f kg, Grid %dx%d ==="),
		Items.Num(), Inv->GetCurrentWeight(), Inv->MaxWeight,
		Inv->GridWidth, Inv->GridHeight), FColor::Cyan);

	for (const FInventoryItemInstance& Item : Items)
	{
		FString ShortUID = Item.UniqueID.ToString().Left(8);
		FString Name = Item.ItemDef ? Item.ItemDef->DisplayName.ToString() : TEXT("???");
		FString Rot = Item.bIsRotated ? TEXT(" [R]") : TEXT("");
		FString Container = Item.IsContainer() ? TEXT(" [C]") : TEXT("");
		FString ClassStr = (Item.CurrentClassLevel > 1) ?
			FString::Printf(TEXT(" Lv%d"), Item.CurrentClassLevel) : TEXT("");

		DebugPrint(FString::Printf(TEXT("  [%s] %s x%d @ (%d,%d)%s%s%s  %.1fkg"),
			*ShortUID, *Name, Item.StackCount,
			Item.GridPosition.X, Item.GridPosition.Y,
			*Rot, *ClassStr, *Container, Item.GetTotalWeight()));
	}
}

void UInventoryDebugSubsystem::Cmd_ListByType(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.ListByType <ItemType>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	FName TypeFilter = FName(*Args[0]);
	int32 Found = 0;

	DebugPrint(FString::Printf(TEXT("=== Items of type '%s' ==="), *Args[0]), FColor::Cyan);

	for (const FInventoryItemInstance& Item : Inv->GetAllItems())
	{
		if (Item.ItemDef && Item.ItemDef->ItemType == TypeFilter)
		{
			FString ShortUID = Item.UniqueID.ToString().Left(8);
			DebugPrint(FString::Printf(TEXT("  [%s] %s x%d @ (%d,%d)"),
				*ShortUID, *Item.ItemDef->DisplayName.ToString(),
				Item.StackCount, Item.GridPosition.X, Item.GridPosition.Y));
			Found++;
		}
	}

	DebugPrint(FString::Printf(TEXT("Found %d items"), Found));
}

void UInventoryDebugSubsystem::Cmd_Find(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Find <SearchText>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	FString Search = Args[0].ToLower();
	int32 Found = 0;

	DebugPrint(FString::Printf(TEXT("=== Search: '%s' ==="), *Args[0]), FColor::Cyan);

	for (const FInventoryItemInstance& Item : Inv->GetAllItems())
	{
		if (Item.ItemDef)
		{
			FString Name = Item.ItemDef->DisplayName.ToString();
			if (Name.ToLower().Contains(Search))
			{
				FString ShortUID = Item.UniqueID.ToString().Left(8);
				DebugPrint(FString::Printf(TEXT("  [%s] %s x%d @ (%d,%d)"),
					*ShortUID, *Name, Item.StackCount,
					Item.GridPosition.X, Item.GridPosition.Y));
				Found++;
			}
		}
	}

	// Also search sub-inventories
	for (const FInventoryItemInstance& ContainerItem : Inv->GetAllItems())
	{
		if (ContainerItem.SubInventory && ContainerItem.SubInventory->IsInitialized())
		{
			for (const FInventoryItemInstance& SubItem : ContainerItem.SubInventory->GetAllItems())
			{
				if (SubItem.ItemDef && SubItem.ItemDef->DisplayName.ToString().ToLower().Contains(Search))
				{
					FString ShortUID = SubItem.UniqueID.ToString().Left(8);
					FString ContainerName = ContainerItem.ItemDef ? ContainerItem.ItemDef->DisplayName.ToString() : TEXT("???");
					DebugPrint(FString::Printf(TEXT("  [%s] %s x%d (in %s)"),
						*ShortUID, *SubItem.ItemDef->DisplayName.ToString(),
						SubItem.StackCount, *ContainerName));
					Found++;
				}
			}
		}
	}

	DebugPrint(FString::Printf(TEXT("Found %d items"), Found));
}

void UInventoryDebugSubsystem::Cmd_Info(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Info <ShortUID>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	FGuid UID = ResolveShortUID(Inv, Args[0]);
	if (!UID.IsValid()) { DebugPrintError(TEXT("Item not found")); return; }

	FInventoryItemInstance Item = Inv->GetItemByID(UID);
	if (!Item.IsValid() || !Item.ItemDef) { DebugPrintError(TEXT("Invalid item")); return; }

	UInventoryItemDefinition* Def = Item.ItemDef;

	DebugPrint(TEXT("=== Item Info ==="), FColor::Cyan);
	DebugPrint(FString::Printf(TEXT("  UID:        %s"), *Item.UniqueID.ToString()));
	DebugPrint(FString::Printf(TEXT("  Name:       %s"), *Def->DisplayName.ToString()));
	DebugPrint(FString::Printf(TEXT("  ItemID:     %s"), *Def->ItemID.ToString()));
	DebugPrint(FString::Printf(TEXT("  ItemType:   %s"), *Def->ItemType.ToString()));
	DebugPrint(FString::Printf(TEXT("  GridSize:   %dx%d"), Def->GridSize.X, Def->GridSize.Y));
	DebugPrint(FString::Printf(TEXT("  Position:   (%d,%d)%s"), Item.GridPosition.X, Item.GridPosition.Y,
		Item.bIsRotated ? TEXT(" [Rotated]") : TEXT("")));
	DebugPrint(FString::Printf(TEXT("  Stack:      %d / %d"), Item.StackCount, Def->MaxStackSize));
	DebugPrint(FString::Printf(TEXT("  Weight:     %.2f (total: %.2f)"), Def->Weight, Item.GetTotalWeight()));
	DebugPrint(FString::Printf(TEXT("  ClassLevel: %d / %d"), Item.CurrentClassLevel, Def->MaxClassLevel));
	DebugPrint(FString::Printf(TEXT("  Consumable: %s  Stackable: %s  Rotatable: %s"),
		Def->bIsConsumable ? TEXT("YES") : TEXT("no"),
		Def->bCanStack ? TEXT("YES") : TEXT("no"),
		Def->bCanRotate ? TEXT("YES") : TEXT("no")));
	DebugPrint(FString::Printf(TEXT("  Container:  %s  MergeUpgrade: %s"),
		Def->bIsContainer ? TEXT("YES") : TEXT("no"),
		Def->bCanMergeUpgrade ? TEXT("YES") : TEXT("no")));

	// Icon soft ref status
	DebugPrint(FString::Printf(TEXT("  Icon:       %s (%s)"),
		Def->Icon.IsNull() ? TEXT("<none>") : *Def->Icon.GetAssetName(),
		Def->Icon.IsNull() ? TEXT("-") : (Def->Icon.IsValid() ? TEXT("LOADED") : TEXT("not loaded"))));

	// WorldActor soft ref status
	DebugPrint(FString::Printf(TEXT("  WorldActor: %s (%s)"),
		Def->WorldActorClass.IsNull() ? TEXT("<none>") : *Def->WorldActorClass.GetAssetName(),
		Def->WorldActorClass.IsNull() ? TEXT("-") : (Def->WorldActorClass.IsValid() ? TEXT("LOADED") : TEXT("not loaded"))));

	// Effects
	TArray<FItemEffectValue> Effects = Item.GetAllEffectValues();
	if (Effects.Num() > 0)
	{
		DebugPrint(TEXT("  Effects:"), FColor::Yellow);
		for (const FItemEffectValue& Eff : Effects)
		{
			// Show resolved value (with class multiplier applied)
			float Resolved = Item.GetEffectValue(Eff.EffectID);
			DebugPrint(FString::Printf(TEXT("    %s = %.2f (resolved: %.2f)"),
				*Eff.EffectID.ToString(), Eff.Value, Resolved));
		}
	}

	// Instance effects
	if (Item.InstanceEffects.Num() > 0)
	{
		DebugPrint(TEXT("  Instance Effects (overrides):"), FColor::Yellow);
		for (const FItemEffectValue& Eff : Item.InstanceEffects)
		{
			DebugPrint(FString::Printf(TEXT("    %s = %.2f"), *Eff.EffectID.ToString(), Eff.Value));
		}
	}

	// Sub-inventory
	if (Item.SubInventory && Item.SubInventory->IsInitialized())
	{
		DebugPrint(FString::Printf(TEXT("  SubInventory: %dx%d, %d items, %.1f kg"),
			Item.SubInventory->GetGridWidth(), Item.SubInventory->GetGridHeight(),
			Item.SubInventory->GetAllItems().Num(), Item.SubInventory->GetCurrentWeight()));
	}
}

void UInventoryDebugSubsystem::Cmd_ItemDB(const TArray<FString>& Args, UWorld* World)
{
	EnsureItemDBScanned();

	FString Filter = Args.Num() > 0 ? Args[0].ToLower() : TEXT("");

	DebugPrint(FString::Printf(TEXT("=== Item Database (%d entries) ==="), CachedItemDB.Num()), FColor::Cyan);

	int32 Shown = 0;
	for (auto& Pair : CachedItemDB)
	{
		FString IDStr = Pair.Key.ToString();
		if (!Filter.IsEmpty() && !IDStr.ToLower().Contains(Filter))
		{
			continue;
		}

		UInventoryItemDefinition* Def = Pair.Value.LoadSynchronous();
		if (Def)
		{
			DebugPrint(FString::Printf(TEXT("  %-30s | %s | %dx%d | %.1fkg | %s"),
				*Def->ItemID.ToString(),
				*Def->DisplayName.ToString(),
				Def->GridSize.X, Def->GridSize.Y,
				Def->Weight,
				*Def->ItemType.ToString()));
		}
		else
		{
			DebugPrint(FString::Printf(TEXT("  %-30s | <LOAD FAILED>"), *IDStr));
		}
		Shown++;
	}

	if (Shown == 0 && !Filter.IsEmpty())
	{
		DebugPrintWarning(FString::Printf(TEXT("No items matching '%s'"), *Filter));
	}
}

// ============================================================================
// Debug Visualization
// ============================================================================

void UInventoryDebugSubsystem::Cmd_Debug(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Debug <on|off>"));
		return;
	}

	bDebugOverlay = Args[0].ToLower() == TEXT("on") || Args[0] == TEXT("1");

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (Inv)
	{
		// Future: set a bShowDebug flag on the component
		DebugPrint(FString::Printf(TEXT("Debug overlay %s"), bDebugOverlay ? TEXT("ON") : TEXT("OFF")));
	}
	else
	{
		DebugPrint(FString::Printf(TEXT("Debug overlay %s (no inventory found)"),
			bDebugOverlay ? TEXT("ON") : TEXT("OFF")));
	}
}

void UInventoryDebugSubsystem::Cmd_DebugBitmap(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	const FInventoryGrid& Grid = Inv->GetGrid();
	const int32 W = Inv->GridWidth;
	const int32 H = Inv->GridHeight;

	UE_LOG(LogTemp, Log, TEXT("=== Bitmap %dx%d ==="), W, H);

	// Print Y header
	FString Header = TEXT("   ");
	for (int32 X = 0; X < W; X += 5)
	{
		Header += FString::Printf(TEXT("%-5d"), X);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *Header);

	for (int32 Y = 0; Y < H; ++Y)
	{
		FString Row = FString::Printf(TEXT("%2d "), Y);
		for (int32 X = 0; X < W; ++X)
		{
			bool bOccupied = !Grid.IsCellFree(FIntPoint(X, Y));
			Row += bOccupied ? TEXT("#") : TEXT(".");
		}
		UE_LOG(LogTemp, Log, TEXT("%s"), *Row);
	}

	DebugPrint(FString::Printf(TEXT("Bitmap printed to log (%dx%d)"), W, H));
}

void UInventoryDebugSubsystem::Cmd_DebugFreeRects(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	const FInventoryGrid& Grid = Inv->GetGrid();

	// Count free cells via bitmap
	int32 FreeCells = 0;
	int32 TotalCells = Inv->GridWidth * Inv->GridHeight;

	for (int32 Y = 0; Y < Inv->GridHeight; ++Y)
	{
		for (int32 X = 0; X < Inv->GridWidth; ++X)
		{
			if (Grid.IsCellFree(FIntPoint(X, Y))) FreeCells++;
		}
	}

	float Pct = TotalCells > 0 ? (float)FreeCells / TotalCells * 100.0f : 0.0f;

	DebugPrint(TEXT("=== Free-Rect Cache Info ==="), FColor::Cyan);
	DebugPrint(FString::Printf(TEXT("  Grid:       %dx%d = %d cells"),
		Inv->GridWidth, Inv->GridHeight, TotalCells));
	DebugPrint(FString::Printf(TEXT("  Free cells: %d / %d (%.1f%%)"), FreeCells, TotalCells, Pct));
	DebugPrint(FString::Printf(TEXT("  Items:      %d"), Inv->GetAllItems().Num()));

	// Test finding free slots for common sizes
	struct FSizeTest { int32 W; int32 H; const TCHAR* Label; };
	FSizeTest Tests[] = {
		{1, 1, TEXT("1x1")}, {2, 2, TEXT("2x2")}, {2, 6, TEXT("2x6 (Messer)")},
		{3, 3, TEXT("3x3")}, {4, 4, TEXT("4x4")}, {6, 6, TEXT("6x6")}
	};

	DebugPrint(TEXT("  FindFreeSlot speed tests:"), FColor::Yellow);
	for (const FSizeTest& Test : Tests)
	{
		double Start = FPlatformTime::Seconds();
		FGridPlacementResult Result = Grid.FindFirstFreeSlot(FIntPoint(Test.W, Test.H), true);
		double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;

		if (Result.bSuccess)
		{
			DebugPrint(FString::Printf(TEXT("    %s -> (%d,%d)%s in %.4f ms"),
				Test.Label, Result.Position.X, Result.Position.Y,
				Result.bRotated ? TEXT(" [R]") : TEXT(""), Ms));
		}
		else
		{
			DebugPrint(FString::Printf(TEXT("    %s -> NO SPACE in %.4f ms"),
				Test.Label, Ms));
		}
	}
}

void UInventoryDebugSubsystem::Cmd_DebugPerformance(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Debug.Performance <on|off>"));
		return;
	}

	bPerformanceOverlay = Args[0].ToLower() == TEXT("on") || Args[0] == TEXT("1");
	DebugPrint(FString::Printf(TEXT("Performance overlay %s"),
		bPerformanceOverlay ? TEXT("ON") : TEXT("OFF")));

	if (bPerformanceOverlay)
	{
		UGridInventoryComponent* Inv = GetPlayerInventory(World);
		if (!Inv) return;

		// Run benchmark
		DebugPrint(TEXT("=== Quick Benchmark ==="), FColor::Cyan);

		// GetAllItems (const ref)
		{
			double Start = FPlatformTime::Seconds();
			for (int32 i = 0; i < 10000; ++i)
			{
				volatile int32 N = Inv->GetAllItems().Num();
				(void)N;
			}
			double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
			DebugPrint(FString::Printf(TEXT("  GetAllItems x10000: %.3f ms (const ref)"), Ms));
		}

		// GetCurrentWeight
		{
			double Start = FPlatformTime::Seconds();
			for (int32 i = 0; i < 10000; ++i)
			{
				volatile float W = Inv->GetCurrentWeight();
				(void)W;
			}
			double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
			DebugPrint(FString::Printf(TEXT("  GetCurrentWeight x10000: %.3f ms"), Ms));
		}
	}
}

void UInventoryDebugSubsystem::Cmd_DebugMemory(const TArray<FString>& Args, UWorld* World)
{
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	DebugPrint(TEXT("=== Memory / Soft Ref Status ==="), FColor::Cyan);

	int32 TotalItems = 0;
	int32 IconsLoaded = 0;
	int32 IconsUnloaded = 0;
	int32 WorldActorsLoaded = 0;
	int32 WorldActorsUnloaded = 0;
	TSet<UInventoryItemDefinition*> UniqueDefs;

	for (const FInventoryItemInstance& Item : Inv->GetAllItems())
	{
		TotalItems++;
		if (!Item.ItemDef) continue;
		UniqueDefs.Add(Item.ItemDef);
	}

	for (UInventoryItemDefinition* Def : UniqueDefs)
	{
		if (!Def->Icon.IsNull())
		{
			if (Def->Icon.IsValid()) IconsLoaded++; else IconsUnloaded++;
		}
		if (!Def->WorldActorClass.IsNull())
		{
			if (Def->WorldActorClass.IsValid()) WorldActorsLoaded++; else WorldActorsUnloaded++;
		}
	}

	DebugPrint(FString::Printf(TEXT("  Items in inventory: %d"), TotalItems));
	DebugPrint(FString::Printf(TEXT("  Unique definitions: %d"), UniqueDefs.Num()));
	DebugPrint(FString::Printf(TEXT("  Icons:       %d loaded / %d not loaded"), IconsLoaded, IconsUnloaded));
	DebugPrint(FString::Printf(TEXT("  WorldActors: %d loaded / %d not loaded"), WorldActorsLoaded, WorldActorsUnloaded));

	// Grid memory estimate
	int32 GridCells = Inv->GridWidth * Inv->GridHeight;
	int32 BitmapBytes = (GridCells + 63) / 64 * 8; // uint64 per 64 cells
	int32 CellArrayBytes = GridCells * sizeof(FGuid);

	DebugPrint(FString::Printf(TEXT("  Grid: %dx%d = %d cells"), Inv->GridWidth, Inv->GridHeight, GridCells));
	DebugPrint(FString::Printf(TEXT("  Bitmap: %d bytes | CellArray: %d bytes"),
		BitmapBytes, CellArrayBytes));

	// ItemDB scan status
	DebugPrint(FString::Printf(TEXT("  ItemDB scanned: %s (%d entries)"),
		bItemDBScanned ? TEXT("yes") : TEXT("no"), CachedItemDB.Num()));
}

void UInventoryDebugSubsystem::Cmd_DebugSlots(const TArray<FString>& Args, UWorld* World)
{
	// This prints widget virtualization info
	// We can't directly access UI widgets from a subsystem, so we print what we know
	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory")); return; }

	int32 TotalCells = Inv->GridWidth * Inv->GridHeight;

	DebugPrint(TEXT("=== Widget Virtualization ==="), FColor::Cyan);
	DebugPrint(FString::Printf(TEXT("  Grid total cells: %d"), TotalCells));
	DebugPrint(FString::Printf(TEXT("  Without virtualization: %d widgets"), TotalCells));
	DebugPrint(FString::Printf(TEXT("  With virtualization: ~%d widgets (estimated visible area)"),
		FMath::Min(TotalCells, 300)));
	DebugPrint(FString::Printf(TEXT("  Memory saved: ~%d KB (est. 200 bytes/widget)"),
		FMath::Max(0, (TotalCells - 300)) * 200 / 1024));
	DebugPrint(TEXT("  Note: Exact counts available in GridInventoryWidget at runtime"));
}

// ============================================================================
// Container Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_ContainerList(const TArray<FString>& Args, UWorld* World)
{
	if (!World) return;

	APawn* Pawn = GetLocalPlayerPawn(World);
	FVector PlayerLoc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

	DebugPrint(TEXT("=== Nearby Containers (5000 units) ==="), FColor::Cyan);

	int32 Index = 0;
	for (TActorIterator<AInventoryContainer> It(World); It; ++It)
	{
		AInventoryContainer* Container = *It;
		float Dist = FVector::Dist(PlayerLoc, Container->GetActorLocation());

		if (Dist > 5000.0f) continue;

		FString LockStr = Container->bIsLocked ? TEXT("[LOCKED]") : TEXT("");
		FString LootStr = Container->bLootGenerated ? TEXT("[looted]") : TEXT("[fresh]");
		FString OpenStr = Container->bIsOpen ? TEXT("[OPEN]") : TEXT("");

		int32 ItemCount = Container->InventoryComponent ?
			Container->InventoryComponent->GetAllItems().Num() : 0;

		DebugPrint(FString::Printf(TEXT("  [%d] %s %.0fm away | %d items %s %s %s"),
			Index, *Container->GetName(), Dist / 100.0f,
			ItemCount, *LockStr, *LootStr, *OpenStr));

		Index++;
	}

	if (Index == 0) DebugPrint(TEXT("  No containers nearby"));
}

void UInventoryDebugSubsystem::Cmd_ContainerOpen(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Container.Open <Index>"));
		return;
	}

	if (!World) return;

	int32 TargetIndex = FCString::Atoi(*Args[0]);
	APawn* Pawn = GetLocalPlayerPawn(World);
	FVector PlayerLoc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

	int32 Index = 0;
	for (TActorIterator<AInventoryContainer> It(World); It; ++It)
	{
		AInventoryContainer* Container = *It;
		if (FVector::Dist(PlayerLoc, Container->GetActorLocation()) > 5000.0f) continue;

		if (Index == TargetIndex)
		{
			// Force open (bypass lock)
			Container->bIsLocked = false;
			bool bOpened = Container->TryOpen(Pawn);
			DebugPrint(bOpened
				? FString::Printf(TEXT("Opened %s (lock bypassed)"), *Container->GetName())
				: FString::Printf(TEXT("Failed to open %s"), *Container->GetName()));
			return;
		}
		Index++;
	}

	DebugPrintError(FString::Printf(TEXT("Container index %d not found"), TargetIndex));
}

void UInventoryDebugSubsystem::Cmd_ContainerLoot(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Container.Loot <Index> [Level]"));
		return;
	}

	if (!World) return;

	int32 TargetIndex = FCString::Atoi(*Args[0]);
	int32 Level = Args.Num() > 1 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 0;
	APawn* Pawn = GetLocalPlayerPawn(World);
	FVector PlayerLoc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;

	int32 Index = 0;
	for (TActorIterator<AInventoryContainer> It(World); It; ++It)
	{
		AInventoryContainer* Container = *It;
		if (FVector::Dist(PlayerLoc, Container->GetActorLocation()) > 5000.0f) continue;

		if (Index == TargetIndex)
		{
			if (!Container->LootTable)
			{
				DebugPrintWarning(FString::Printf(TEXT("%s has no LootTable"), *Container->GetName()));
				return;
			}

			Container->RegenerateLoot(Level);

			int32 ItemCount = Container->InventoryComponent ?
				Container->InventoryComponent->GetAllItems().Num() : 0;

			DebugPrint(FString::Printf(TEXT("Regenerated loot for %s (Level %d): %d items"),
				*Container->GetName(), Level > 0 ? Level : Container->LootLevel, ItemCount));
			return;
		}
		Index++;
	}

	DebugPrintError(FString::Printf(TEXT("Container index %d not found"), TargetIndex));
}

// ============================================================================
// Gold Commands
// ============================================================================

void UInventoryDebugSubsystem::Cmd_GoldAdd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Gold.Add <Amount>  (negative to subtract)"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	const float Amount = FCString::Atof(*Args[0]);
	const float Before = Inv->GetGold();
	Inv->AddGold(Amount);
	const float After = Inv->GetGold();

	DebugPrint(FString::Printf(TEXT("Gold: %.0f → %.0f (%+.0f)"), Before, After, After - Before),
		Amount >= 0.0f ? FColor::Yellow : FColor::Orange);
}

void UInventoryDebugSubsystem::Cmd_GoldSet(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		DebugPrint(TEXT("Usage: Inv.Gold.Set <Amount>"));
		return;
	}

	UGridInventoryComponent* Inv = GetPlayerInventory(World);
	if (!Inv) { DebugPrintError(TEXT("No inventory on player pawn")); return; }

	const float Amount = FCString::Atof(*Args[0]);
	Inv->SetGold(Amount);

	DebugPrint(FString::Printf(TEXT("Gold set to: %.0f"), Inv->GetGold()), FColor::Yellow);
}

#endif // !UE_BUILD_SHIPPING
