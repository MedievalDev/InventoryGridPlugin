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
	MinRandomItems = 0;
	MaxRandomItems = 0;
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

	// 1. Pre-fill with editor-configured default items (always added)
	if (DefaultItems.Num() > 0 && InventoryComponent)
	{
		InventoryComponent->TryAddItemsBatch(DefaultItems);
	}

	// 2. Generate random default items (based on SpawnChance * DropWeightMultiplier)
	if (RandomDefaultItems.Num() > 0 && InventoryComponent)
	{
		GenerateRandomDefaults();
	}
}

void AInventoryContainer::GenerateRandomDefaults()
{
	if (!InventoryComponent || RandomDefaultItems.Num() == 0) return;

	const int32 EffectiveMin = FMath::Max(0, MinRandomItems);
	const int32 EffectiveMax = FMath::Max(0, MaxRandomItems);

	if (EffectiveMin > 0 || EffectiveMax > 0)
	{
		// === Selection mode: pick Min..Max entries using SpawnChance as weight ===
		const int32 ClampedMax = FMath::Min(EffectiveMax, RandomDefaultItems.Num());
		const int32 ClampedMin = FMath::Min(EffectiveMin, ClampedMax);
		const int32 NumToPick = FMath::RandRange(ClampedMin, ClampedMax);

		if (NumToPick <= 0) return;

		// Build weighted pool (indices + weights)
		TArray<int32> ValidIndices;
		TArray<float> Weights;
		float TotalWeight = 0.0f;

		for (int32 i = 0; i < RandomDefaultItems.Num(); ++i)
		{
			const FRandomItemEntry& Entry = RandomDefaultItems[i];
			if (!Entry.ItemDef || Entry.SpawnChance <= 0.0f) continue;

			const float W = Entry.SpawnChance * DropWeightMultiplier;
			if (W <= 0.0f) continue;

			ValidIndices.Add(i);
			Weights.Add(W);
			TotalWeight += W;
		}

		if (ValidIndices.Num() == 0 || TotalWeight <= 0.0f) return;

		// Weighted selection without replacement
		for (int32 Picked = 0; Picked < NumToPick && ValidIndices.Num() > 0; ++Picked)
		{
			float Roll = FMath::FRand() * TotalWeight;
			int32 ChosenPoolIdx = ValidIndices.Num() - 1; // fallback to last

			for (int32 j = 0; j < ValidIndices.Num(); ++j)
			{
				Roll -= Weights[j];
				if (Roll <= 0.0f)
				{
					ChosenPoolIdx = j;
					break;
				}
			}

			const FRandomItemEntry& Chosen = RandomDefaultItems[ValidIndices[ChosenPoolIdx]];
			const int32 Count = FMath::RandRange(Chosen.MinCount, FMath::Max(Chosen.MinCount, Chosen.MaxCount));
			InventoryComponent->TryAddItem(Chosen.ItemDef, Count);

			// Remove from pool (no duplicates)
			TotalWeight -= Weights[ChosenPoolIdx];
			ValidIndices.RemoveAtSwap(ChosenPoolIdx);
			Weights.RemoveAtSwap(ChosenPoolIdx);
		}
	}
	else
	{
		// === Individual roll mode: each entry rolls independently ===
		for (const FRandomItemEntry& Entry : RandomDefaultItems)
		{
			if (!Entry.ItemDef || Entry.SpawnChance <= 0.0f) continue;

			const float AdjustedChance = FMath::Clamp(Entry.SpawnChance * DropWeightMultiplier, 0.0f, 1.0f);

			if (FMath::FRand() <= AdjustedChance)
			{
				const int32 Count = FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
				InventoryComponent->TryAddItem(Entry.ItemDef, Count);
			}
		}
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
