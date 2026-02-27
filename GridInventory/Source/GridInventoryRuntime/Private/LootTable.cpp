// Copyright Marco. All Rights Reserved.

#include "LootTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

ULootTable::ULootTable()
	: MinItemDrops(1)
	, MaxItemDrops(3)
{
}

TArray<FLootResult> ULootTable::GenerateLoot(int32 Level, float WeightMultiplier) const
{
	TArray<FLootResult> Results;

	if (Entries.Num() == 0 || MaxItemDrops <= 0)
	{
		return Results;
	}

	// Build weighted pool
	struct FWeightedEntry
	{
		int32 EntryIndex;
		float EffectiveWeight;
	};

	TArray<FWeightedEntry> Pool;
	float TotalWeight = 0.0f;

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FLootTableEntry& Entry = Entries[i];
		if (Entry.ItemDef.IsNull()) continue;

		// Load soft reference synchronously for weight calculation
		UInventoryItemDefinition* LoadedDef = Entry.ItemDef.LoadSynchronous();
		if (!LoadedDef) continue;

		float W = 0.0f;
		if (Entry.FixedWeightOverride > 0.0f)
		{
			W = Entry.FixedWeightOverride * WeightMultiplier;
		}
		else
		{
			W = LoadedDef->GetLootWeightForLevel(Level) * WeightMultiplier;
		}

		if (W > 0.0f)
		{
			FWeightedEntry WE;
			WE.EntryIndex = i;
			WE.EffectiveWeight = W;
			Pool.Add(WE);
			TotalWeight += W;
		}
	}

	if (Pool.Num() == 0 || TotalWeight <= 0.0f)
	{
		return Results;
	}

	// Determine how many items to drop
	const int32 NumDrops = FMath::RandRange(
		FMath::Min(MinItemDrops, Pool.Num()),
		FMath::Min(MaxItemDrops, Pool.Num())
	);

	// Weighted random selection without replacement
	TArray<FWeightedEntry> RemainingPool = Pool;
	float RemainingWeight = TotalWeight;

	for (int32 Drop = 0; Drop < NumDrops && RemainingPool.Num() > 0; ++Drop)
	{
		// Pick random value in [0, RemainingWeight)
		float Roll = FMath::FRandRange(0.0f, RemainingWeight);
		float Cumulative = 0.0f;
		int32 PickedPoolIndex = RemainingPool.Num() - 1; // Fallback

		for (int32 j = 0; j < RemainingPool.Num(); ++j)
		{
			Cumulative += RemainingPool[j].EffectiveWeight;
			if (Roll < Cumulative)
			{
				PickedPoolIndex = j;
				break;
			}
		}

		// Get the entry and load the item def
		const FWeightedEntry& Picked = RemainingPool[PickedPoolIndex];
		const FLootTableEntry& Entry = Entries[Picked.EntryIndex];
		UInventoryItemDefinition* LoadedDef = Entry.ItemDef.LoadSynchronous();

		// Random count
		const int32 Count = FMath::RandRange(Entry.MinCount, Entry.MaxCount);

		if (LoadedDef)
		{
			Results.Add(FLootResult(LoadedDef, Count));
		}

		// Remove from pool (no replacement)
		RemainingWeight -= Picked.EffectiveWeight;
		RemainingPool.RemoveAt(PickedPoolIndex);
	}

	return Results;
}

TArray<float> ULootTable::PreviewWeightsForLevel(int32 Level, float WeightMultiplier) const
{
	TArray<float> Weights;
	Weights.Reserve(Entries.Num());

	for (const FLootTableEntry& Entry : Entries)
	{
		if (Entry.ItemDef.IsNull())
		{
			Weights.Add(0.0f);
			continue;
		}

		if (Entry.FixedWeightOverride > 0.0f)
		{
			Weights.Add(Entry.FixedWeightOverride * WeightMultiplier);
		}
		else
		{
			UInventoryItemDefinition* LoadedDef = Entry.ItemDef.LoadSynchronous();
			Weights.Add(LoadedDef ? LoadedDef->GetLootWeightForLevel(Level) * WeightMultiplier : 0.0f);
		}
	}

	return Weights;
}

FPrimaryAssetId ULootTable::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType("LootTable"), GetFName());
}

TArray<FSoftObjectPath> ULootTable::GetAllItemPaths() const
{
	TArray<FSoftObjectPath> Paths;
	for (const FLootTableEntry& Entry : Entries)
	{
		if (!Entry.ItemDef.IsNull())
		{
			Paths.AddUnique(Entry.ItemDef.ToSoftObjectPath());
		}
	}
	return Paths;
}

void ULootTable::GenerateLootAsync(int32 Level, float WeightMultiplier,
	TFunction<void(const TArray<FLootResult>&)> OnComplete) const
{
	if (!OnComplete || Entries.Num() == 0 || MaxItemDrops <= 0)
	{
		if (OnComplete) OnComplete(TArray<FLootResult>());
		return;
	}

	// Collect all soft object paths
	TArray<FSoftObjectPath> Paths = GetAllItemPaths();

	if (Paths.Num() == 0)
	{
		OnComplete(TArray<FLootResult>());
		return;
	}

	// Capture 'this' safely — ULootTable is a UObject so we need a weak ptr
	TWeakObjectPtr<const ULootTable> WeakThis(this);
	const int32 CapturedLevel = Level;
	const float CapturedMultiplier = WeightMultiplier;

	// Async load all item definitions
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	StreamableManager.RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateLambda([WeakThis, CapturedLevel, CapturedMultiplier, OnComplete]()
		{
			if (!WeakThis.IsValid())
			{
				OnComplete(TArray<FLootResult>());
				return;
			}

			// All assets are now loaded — GenerateLoot will just resolve the soft ptrs instantly
			TArray<FLootResult> Results = WeakThis->GenerateLoot(CapturedLevel, CapturedMultiplier);
			OnComplete(Results);
		})
	);
}
