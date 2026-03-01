// Copyright Marco. All Rights Reserved.

#include "InventoryContainer.h"
#include "GridInventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "LootTable.h"
#include "Net/UnrealNetwork.h"

AInventoryContainer::AInventoryContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InventoryComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("ContainerInventory"));

	bIsLocked = false;
	RequiredKeyItemID = NAME_None;
	bConsumeKey = false;
	bIsOpen = false;
	CurrentInteractor = nullptr;
	LootTable = nullptr;
	DropWeightMultiplier = 1.0f;
	LootLevel = 0;
	bLootGenerated = false;
}

void AInventoryContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInventoryContainer, bIsLocked);
	DOREPLIFETIME(AInventoryContainer, bIsOpen);
	DOREPLIFETIME(AInventoryContainer, bLootGenerated);
}

void AInventoryContainer::BeginPlay()
{
	Super::BeginPlay();

	// Pre-fill with editor-configured default items
	if (DefaultItems.Num() > 0 && InventoryComponent)
	{
		InventoryComponent->TryAddItemsBatch(DefaultItems);
	}
}

bool AInventoryContainer::TryOpen(AActor* Interactor)
{
	if (!Interactor) return false;

	// Already open
	if (bIsOpen)
	{
		// Same person -> close
		if (CurrentInteractor == Interactor)
		{
			Close();
			return false;
		}
		// Someone else has it open
		return false;
	}

	// Check lock
	if (bIsLocked)
	{
		if (!HasRequiredKey(Interactor))
		{
			OnContainerLockFailed.Broadcast(Interactor, this);
			return false;
		}

		// Consume key if configured
		if (bConsumeKey)
		{
			UGridInventoryComponent* InteractorInv = Interactor->FindComponentByClass<UGridInventoryComponent>();
			if (InteractorInv)
			{
				// Find and remove one key item
				for (const FInventoryItemInstance& Item : InteractorInv->GetAllItems())
				{
					if (Item.ItemDef && Item.ItemDef->ItemID == RequiredKeyItemID)
					{
						InteractorInv->RemoveItem(Item.UniqueID, 1);
						break;
					}
				}
			}
		}

		bIsLocked = false;
	}

	bIsOpen = true;
	CurrentInteractor = Interactor;

	// Auto-generate loot on first open (async to avoid hitches)
	if (!bLootGenerated && LootTable)
	{
		GenerateLootAsync(0);
	}

	OnContainerOpened.Broadcast(Interactor, this);
	OnOpenStateChanged(true);
	return true;
}

void AInventoryContainer::GenerateLoot(int32 OverrideLevel)
{
	if (!LootTable || !InventoryComponent) return;

	const int32 Level = (OverrideLevel > 0) ? OverrideLevel : FMath::Max(1, LootLevel);

	TArray<FLootResult> Loot = LootTable->GenerateLoot(Level, DropWeightMultiplier);

	for (const FLootResult& LR : Loot)
	{
		if (LR.ItemDef && LR.Count > 0)
		{
			InventoryComponent->TryAddItem(LR.ItemDef, LR.Count);
		}
	}

	bLootGenerated = true;
}

void AInventoryContainer::RegenerateLoot(int32 OverrideLevel)
{
	if (InventoryComponent)
	{
		InventoryComponent->ClearInventory();
	}
	bLootGenerated = false;
	GenerateLoot(OverrideLevel);
}

void AInventoryContainer::GenerateLootAsync(int32 OverrideLevel)
{
	if (!LootTable || !InventoryComponent) return;

	const int32 Level = (OverrideLevel > 0) ? OverrideLevel : FMath::Max(1, LootLevel);

	// Mark as generated immediately to prevent double-calls
	bLootGenerated = true;

	TWeakObjectPtr<AInventoryContainer> WeakThis(this);

	LootTable->GenerateLootAsync(Level, DropWeightMultiplier,
		[WeakThis](const TArray<FLootResult>& Loot)
		{
			if (!WeakThis.IsValid()) return;

			AInventoryContainer* Self = WeakThis.Get();
			if (!Self->InventoryComponent) return;

			for (const FLootResult& LR : Loot)
			{
				if (LR.ItemDef && LR.Count > 0)
				{
					Self->InventoryComponent->TryAddItem(LR.ItemDef, LR.Count);
				}
			}

			Self->OnLootGenerated.Broadcast(Self);
		}
	);
}

void AInventoryContainer::Close()
{
	if (!bIsOpen) return;

	AActor* PreviousInteractor = CurrentInteractor;
	bIsOpen = false;
	CurrentInteractor = nullptr;
	OnContainerClosed.Broadcast(PreviousInteractor, this);
	OnOpenStateChanged(false);
}

void AInventoryContainer::Unlock()
{
	bIsLocked = false;
}

void AInventoryContainer::Lock(FName NewKeyItemID)
{
	bIsLocked = true;
	if (NewKeyItemID != NAME_None)
	{
		RequiredKeyItemID = NewKeyItemID;
	}
}

bool AInventoryContainer::HasRequiredKey(AActor* Actor) const
{
	if (!Actor || RequiredKeyItemID == NAME_None)
	{
		return !bIsLocked;
	}

	UGridInventoryComponent* Inv = Actor->FindComponentByClass<UGridInventoryComponent>();
	if (!Inv) return false;

	for (const FInventoryItemInstance& Item : Inv->GetAllItems())
	{
		if (Item.ItemDef && Item.ItemDef->ItemID == RequiredKeyItemID)
		{
			return true;
		}
	}

	return false;
}

void AInventoryContainer::OnRep_IsOpen()
{
	OnOpenStateChanged(bIsOpen);
}

void AInventoryContainer::OnOpenStateChanged_Implementation(bool bOpened)
{
	// Override in Blueprint for animations
}
