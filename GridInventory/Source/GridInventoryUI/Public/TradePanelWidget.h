// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TradePanelWidget.generated.h"

class UGridInventoryComponent;
class UGridInventoryWidget;
class UInventorySlotWidget;
class UTextBlock;
class UButton;

/**
 * Panel widget for NPC trading.
 * Shows player and merchant inventories side by side with gold displays
 * and buy/sell functionality.
 *
 * Blueprint setup:
 * 1. Create WBP_TradePanel (Parent: TradePanelWidget)
 * 2. Add layout: [PlayerGrid | MerchantGrid] with gold displays and close button
 * 3. Name widgets: "PlayerGrid", "MerchantGrid", "PlayerGoldText",
 *    "MerchantGoldText", "MerchantTitle", "CloseButton"
 * 4. In game: call OpenTrade(PlayerInventory, MerchantInventory, MerchantName)
 *
 * The NPC merchant uses a regular GridInventoryComponent as its inventory.
 * Items are transferred between inventories on buy/sell.
 * Gold is managed via GridInventoryComponent::GetGold()/AddGold().
 *
 * Price calculation:
 * - Buy price  = ItemDef->BaseValue * StackCount * BuyPriceMultiplier
 * - Sell price = ItemDef->BaseValue * StackCount * SellPriceMultiplier
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UTradePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================
	// Config
	// ========================

	/** Cell size for both grids */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Panel|Config")
	float CellSize;

	/** Slot widget class for both grids (soft ref — no hard package dependency) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Panel|Config")
	TSoftClassPtr<UInventorySlotWidget> SlotWidgetClass;

	/** Multiplier applied to BaseValue when buying from merchant (default 1.0 = full price) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Panel|Config", meta = (ClampMin = "0.0"))
	float BuyPriceMultiplier;

	/** Multiplier applied to BaseValue when selling to merchant (default 0.5 = half price) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Panel|Config", meta = (ClampMin = "0.0"))
	float SellPriceMultiplier;

	// ========================
	// Bound Widgets
	// ========================

	/** Grid widget for the player's inventory */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UGridInventoryWidget* PlayerGrid;

	/** Grid widget for the merchant's inventory */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UGridInventoryWidget* MerchantGrid;

	/** Text block showing player's gold */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UTextBlock* PlayerGoldText;

	/** Text block showing merchant's gold */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UTextBlock* MerchantGoldText;

	/** Title text showing merchant name */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UTextBlock* MerchantTitle;

	/** Close button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Trade Panel|Widgets")
	UButton* CloseButton;

	// ========================
	// Functions
	// ========================

	/**
	 * Open trade with a merchant.
	 * @param PlayerInv The player's inventory component
	 * @param MerchantInv The merchant's inventory component (acts as their stock)
	 * @param MerchantName Display name for the merchant
	 */
	UFUNCTION(BlueprintCallable, Category = "Trade Panel")
	void OpenTrade(UGridInventoryComponent* PlayerInv, UGridInventoryComponent* MerchantInv, FText MerchantName);

	/** Close the trade panel */
	UFUNCTION(BlueprintCallable, Category = "Trade Panel")
	void CloseTrade();

	/** Check if trade is currently open */
	UFUNCTION(BlueprintPure, Category = "Trade Panel")
	bool IsTradeOpen() const { return PlayerInventory != nullptr && MerchantInventory != nullptr; }

	/**
	 * Buy an item from the merchant.
	 * Transfers from merchant→player inventory, deducts gold from player, adds to merchant.
	 * @param ItemID UniqueID of the item in the merchant's inventory
	 * @return true if purchase succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Trade Panel")
	bool BuyItem(FGuid ItemID);

	/**
	 * Sell an item to the merchant.
	 * Transfers from player→merchant inventory, adds gold to player, deducts from merchant.
	 * @param ItemID UniqueID of the item in the player's inventory
	 * @return true if sale succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Trade Panel")
	bool SellItem(FGuid ItemID);

	/**
	 * Get the buy price for an item in the merchant's inventory.
	 * @param ItemID UniqueID of the item in the merchant's inventory
	 * @return Price the player would pay
	 */
	UFUNCTION(BlueprintPure, Category = "Trade Panel")
	float GetBuyPrice(FGuid ItemID) const;

	/**
	 * Get the sell price for an item in the player's inventory.
	 * @param ItemID UniqueID of the item in the player's inventory
	 * @return Gold the player would receive
	 */
	UFUNCTION(BlueprintPure, Category = "Trade Panel")
	float GetSellPrice(FGuid ItemID) const;

	// ========================
	// Overridable Events
	// ========================

	UFUNCTION(BlueprintNativeEvent, Category = "Trade Panel")
	void OnTradeOpened();

	UFUNCTION(BlueprintNativeEvent, Category = "Trade Panel")
	void OnTradeClosed();

	UFUNCTION(BlueprintNativeEvent, Category = "Trade Panel")
	void OnItemBought(const FGuid& ItemID, float Price);

	UFUNCTION(BlueprintNativeEvent, Category = "Trade Panel")
	void OnItemSold(const FGuid& ItemID, float Price);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Trade Panel")
	UGridInventoryComponent* PlayerInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Trade Panel")
	UGridInventoryComponent* MerchantInventory;

private:
	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnPlayerGoldChanged(float NewGold, float Delta);

	UFUNCTION()
	void OnMerchantGoldChanged(float NewGold, float Delta);

	UFUNCTION()
	void OnInventoryContentsChanged();

	void RefreshGoldDisplay();
};
