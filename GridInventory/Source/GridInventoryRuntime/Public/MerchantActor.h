// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryGrid.h"
#include "RandomItemEntry.h"
#include "MerchantActor.generated.h"

class UGridInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMerchantRestock, AMerchantActor*, Merchant);

/**
 * Merchant/vendor actor that sells and buys items from the player.
 * Place in the level, configure stock and pricing in the Details panel.
 *
 * Usage:
 * 1. Place in level or spawn at runtime
 * 2. Set DefaultStock (always available) and/or RandomStock (random availability)
 * 3. Set BuyPriceMultiplier / SellPriceMultiplier for pricing
 * 4. Set StartingGold for the merchant's gold reserve
 * 5. Player interacts -> open TradePanelWidget with OpenTradeWithMerchant()
 *
 * Works with TradePanelWidget: call OpenTradeWithMerchant() to automatically
 * use this merchant's inventory, name, and price multipliers.
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYRUNTIME_API AMerchantActor : public AActor
{
	GENERATED_BODY()

public:
	AMerchantActor();

	/** The merchant's inventory (holds their stock) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Merchant")
	UGridInventoryComponent* MerchantInventory;

	// ========================
	// Identity
	// ========================

	/** Display name shown in the trade UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant")
	FText MerchantName;

	/**
	 * Stable identifier for save/load — must be unique per merchant in the level.
	 * Merchants without an ID are skipped during save/load.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Save", SaveGame)
	FName MerchantID;

	// ========================
	// Stock — Fixed (Always Available)
	// ========================

	/**
	 * Items that are always in this merchant's stock at game start.
	 * Configure in the editor: select Item + Count.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Stock")
	TArray<FItemAddRequest> DefaultStock;

	// ========================
	// Stock — Random
	// ========================

	/**
	 * Items that may randomly appear in this merchant's stock.
	 * Each entry has a SpawnChance (0-1) and a MinCount/MaxCount range.
	 *
	 * Example: Seltener Trank with SpawnChance 0.3, Min 1, Max 2
	 * -> 30% chance to appear, if it does: 1-2 items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Stock")
	TArray<FRandomItemEntry> RandomStock;

	// ========================
	// Pricing
	// ========================

	/**
	 * Multiplier for buy prices (what the player pays).
	 * 1.0 = BaseValue, 1.5 = 150% markup, 0.8 = discount.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Pricing", meta = (ClampMin = "0.01"))
	float BuyPriceMultiplier;

	/**
	 * Multiplier for sell prices (what the player receives).
	 * 0.5 = half price, 1.0 = full BaseValue, 0.25 = pawnshop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Pricing", meta = (ClampMin = "0.0"))
	float SellPriceMultiplier;

	// ========================
	// Gold
	// ========================

	/**
	 * How much gold the merchant starts with.
	 * Limits how much the merchant can buy from the player.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Merchant|Gold", meta = (ClampMin = "0"))
	float StartingGold;

	// ========================
	// Functions
	// ========================

	/**
	 * Fully restock the merchant: clears inventory, re-adds all DefaultStock
	 * and regenerates RandomStock. Resets gold to StartingGold.
	 * Call this to refresh the merchant (e.g. daily restock timer).
	 */
	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void RestockMerchant();

	/**
	 * Generate random stock items from the RandomStock list.
	 * Called automatically in BeginPlay. Can be called again for additional random items.
	 */
	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void GenerateRandomStock();

	// ========================
	// Events
	// ========================

	/** Fired when the merchant is restocked (via RestockMerchant) */
	UPROPERTY(BlueprintAssignable, Category = "Merchant|Events")
	FOnMerchantRestock OnMerchantRestocked;

protected:
	virtual void BeginPlay() override;
};
