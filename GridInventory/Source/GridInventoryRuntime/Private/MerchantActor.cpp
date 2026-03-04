// Copyright Marco. All Rights Reserved.

#include "MerchantActor.h"
#include "GridInventoryComponent.h"
#include "InventoryItemDefinition.h"

AMerchantActor::AMerchantActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MerchantInventory = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("MerchantInventory"));

	MerchantName = FText::FromString(TEXT("Merchant"));
	MerchantID = NAME_None;
	BuyPriceMultiplier = 1.0f;
	SellPriceMultiplier = 0.5f;
	StartingGold = 500.0f;
	MinRandomStock = 0;
	MaxRandomStock = 0;
}

void AMerchantActor::BeginPlay()
{
	Super::BeginPlay();

	if (!MerchantInventory) return;

	// Set starting gold
	MerchantInventory->SetGold(StartingGold);

	// Add fixed stock (always available)
	if (DefaultStock.Num() > 0)
	{
		MerchantInventory->TryAddItemsBatch(DefaultStock);
	}

	// Generate random stock
	if (RandomStock.Num() > 0)
	{
		GenerateRandomStock();
	}
}

void AMerchantActor::GenerateRandomStock()
{
	if (!MerchantInventory || RandomStock.Num() == 0) return;

	const int32 EffectiveMin = FMath::Max(0, MinRandomStock);
	const int32 EffectiveMax = FMath::Max(0, MaxRandomStock);

	if (EffectiveMin > 0 || EffectiveMax > 0)
	{
		// === Selection mode: pick Min..Max entries using SpawnChance as weight ===
		const int32 ClampedMax = FMath::Min(EffectiveMax, RandomStock.Num());
		const int32 ClampedMin = FMath::Min(EffectiveMin, ClampedMax);
		const int32 NumToPick = FMath::RandRange(ClampedMin, ClampedMax);

		if (NumToPick <= 0) return;

		TArray<int32> ValidIndices;
		TArray<float> Weights;
		float TotalWeight = 0.0f;

		for (int32 i = 0; i < RandomStock.Num(); ++i)
		{
			const FRandomItemEntry& Entry = RandomStock[i];
			if (Entry.ItemDef.IsNull() || Entry.SpawnChance <= 0.0f) continue;

			ValidIndices.Add(i);
			Weights.Add(Entry.SpawnChance);
			TotalWeight += Entry.SpawnChance;
		}

		if (ValidIndices.Num() == 0 || TotalWeight <= 0.0f) return;

		for (int32 Picked = 0; Picked < NumToPick && ValidIndices.Num() > 0; ++Picked)
		{
			float Roll = FMath::FRand() * TotalWeight;
			int32 ChosenPoolIdx = ValidIndices.Num() - 1;

			for (int32 j = 0; j < ValidIndices.Num(); ++j)
			{
				Roll -= Weights[j];
				if (Roll <= 0.0f)
				{
					ChosenPoolIdx = j;
					break;
				}
			}

			const FRandomItemEntry& Chosen = RandomStock[ValidIndices[ChosenPoolIdx]];
			const int32 Count = FMath::RandRange(Chosen.MinCount, FMath::Max(Chosen.MinCount, Chosen.MaxCount));
			UInventoryItemDefinition* LoadedDef = Chosen.ItemDef.LoadSynchronous();
			if (LoadedDef)
			{
				MerchantInventory->TryAddItem(LoadedDef, Count);
			}

			TotalWeight -= Weights[ChosenPoolIdx];
			ValidIndices.RemoveAtSwap(ChosenPoolIdx);
			Weights.RemoveAtSwap(ChosenPoolIdx);
		}
	}
	else
	{
		// === Individual roll mode ===
		for (const FRandomItemEntry& Entry : RandomStock)
		{
			if (Entry.ItemDef.IsNull() || Entry.SpawnChance <= 0.0f) continue;

			if (FMath::FRand() <= Entry.SpawnChance)
			{
				const int32 Count = FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
				UInventoryItemDefinition* LoadedDef = Entry.ItemDef.LoadSynchronous();
				if (LoadedDef)
				{
					MerchantInventory->TryAddItem(LoadedDef, Count);
				}
			}
		}
	}
}

void AMerchantActor::RestockMerchant()
{
	if (!MerchantInventory) return;

	// Clear existing inventory
	MerchantInventory->ClearInventory();

	// Reset gold
	MerchantInventory->SetGold(StartingGold);

	// Re-add fixed stock
	if (DefaultStock.Num() > 0)
	{
		MerchantInventory->TryAddItemsBatch(DefaultStock);
	}

	// Regenerate random stock
	if (RandomStock.Num() > 0)
	{
		GenerateRandomStock();
	}

	OnMerchantRestocked.Broadcast(this);
}
