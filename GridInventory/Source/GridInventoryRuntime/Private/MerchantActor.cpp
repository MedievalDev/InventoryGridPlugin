// Copyright Marco. All Rights Reserved.

#include "MerchantActor.h"
#include "GridInventoryComponent.h"

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
	if (!MerchantInventory) return;

	for (const FRandomItemEntry& Entry : RandomStock)
	{
		if (!Entry.ItemDef || Entry.SpawnChance <= 0.0f) continue;

		if (FMath::FRand() <= Entry.SpawnChance)
		{
			const int32 Count = FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
			MerchantInventory->TryAddItem(Entry.ItemDef, Count);
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
