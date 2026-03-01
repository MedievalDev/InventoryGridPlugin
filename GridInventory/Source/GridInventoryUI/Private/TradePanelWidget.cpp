// Copyright Marco. All Rights Reserved.

#include "TradePanelWidget.h"
#include "GridInventoryWidget.h"
#include "GridInventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryItemInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UTradePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CellSize <= 0.0f) CellSize = 64.0f;
	if (BuyPriceMultiplier <= 0.0f) BuyPriceMultiplier = 1.0f;
	if (SellPriceMultiplier <= 0.0f) SellPriceMultiplier = 0.5f;

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UTradePanelWidget::OnCloseButtonClicked);
	}
}

void UTradePanelWidget::NativeDestruct()
{
	CloseTrade();

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UTradePanelWidget::OnCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UTradePanelWidget::OpenTrade(UGridInventoryComponent* PlayerInv, UGridInventoryComponent* MerchantInv, FText MerchantName)
{
	if (!PlayerInv || !MerchantInv) return;

	// Close previous trade if any
	if (PlayerInventory)
	{
		CloseTrade();
	}

	PlayerInventory = PlayerInv;
	MerchantInventory = MerchantInv;

	// Configure player grid
	if (PlayerGrid)
	{
		if (!SlotWidgetClass.IsNull())
		{
			PlayerGrid->SlotWidgetClass = SlotWidgetClass;
		}
		PlayerGrid->CellSize = CellSize;
		PlayerGrid->SetInventoryComponent(PlayerInv);
	}

	// Configure merchant grid
	if (MerchantGrid)
	{
		if (!SlotWidgetClass.IsNull())
		{
			MerchantGrid->SlotWidgetClass = SlotWidgetClass;
		}
		MerchantGrid->CellSize = CellSize;
		MerchantGrid->SetInventoryComponent(MerchantInv);
	}

	// Set merchant title
	if (MerchantTitle)
	{
		MerchantTitle->SetText(MerchantName.IsEmpty()
			? FText::FromString(TEXT("Merchant"))
			: MerchantName);
	}

	// Bind gold changed delegates
	PlayerInv->OnGoldChanged.AddDynamic(this, &UTradePanelWidget::OnPlayerGoldChanged);
	MerchantInv->OnGoldChanged.AddDynamic(this, &UTradePanelWidget::OnMerchantGoldChanged);

	// Bind inventory changed for price refresh
	PlayerInv->OnInventoryChanged.AddDynamic(this, &UTradePanelWidget::OnInventoryContentsChanged);
	MerchantInv->OnInventoryChanged.AddDynamic(this, &UTradePanelWidget::OnInventoryContentsChanged);

	RefreshGoldDisplay();

	SetVisibility(ESlateVisibility::Visible);

	OnTradeOpened();

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] TradePanel — Opened trade with '%s'"),
		*MerchantName.ToString());
}

void UTradePanelWidget::CloseTrade()
{
	if (!PlayerInventory && !MerchantInventory) return;

	// Unbind delegates
	if (PlayerInventory)
	{
		PlayerInventory->OnGoldChanged.RemoveDynamic(this, &UTradePanelWidget::OnPlayerGoldChanged);
		PlayerInventory->OnInventoryChanged.RemoveDynamic(this, &UTradePanelWidget::OnInventoryContentsChanged);
	}
	if (MerchantInventory)
	{
		MerchantInventory->OnGoldChanged.RemoveDynamic(this, &UTradePanelWidget::OnMerchantGoldChanged);
		MerchantInventory->OnInventoryChanged.RemoveDynamic(this, &UTradePanelWidget::OnInventoryContentsChanged);
	}

	// Clear grid bindings
	if (PlayerGrid)
	{
		PlayerGrid->SetInventoryComponent(nullptr);
	}
	if (MerchantGrid)
	{
		MerchantGrid->SetInventoryComponent(nullptr);
	}

	PlayerInventory = nullptr;
	MerchantInventory = nullptr;

	SetVisibility(ESlateVisibility::Collapsed);

	OnTradeClosed();
}

bool UTradePanelWidget::BuyItem(FGuid ItemID)
{
	if (!PlayerInventory || !MerchantInventory) return false;

	const float Price = GetBuyPrice(ItemID);
	if (Price <= 0.0f) return false;

	if (!PlayerInventory->CanAffordGold(Price))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Trade — Cannot afford %.0f gold (have %.0f)"),
			Price, PlayerInventory->GetGold());
		return false;
	}

	// Try to transfer item from merchant to player
	if (!MerchantInventory->TransferItem(ItemID, PlayerInventory))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Trade — No space in player inventory"));
		return false;
	}

	// Exchange gold
	PlayerInventory->AddGold(-Price);
	MerchantInventory->AddGold(Price);

	OnItemBought(ItemID, Price);

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] Trade — Bought item for %.0f gold"), Price);
	return true;
}

bool UTradePanelWidget::SellItem(FGuid ItemID)
{
	if (!PlayerInventory || !MerchantInventory) return false;

	const float Price = GetSellPrice(ItemID);
	if (Price <= 0.0f) return false;

	// Try to transfer item from player to merchant
	if (!PlayerInventory->TransferItem(ItemID, MerchantInventory))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Trade — No space in merchant inventory"));
		return false;
	}

	// Exchange gold
	PlayerInventory->AddGold(Price);
	MerchantInventory->AddGold(-Price);

	OnItemSold(ItemID, Price);

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] Trade — Sold item for %.0f gold"), Price);
	return true;
}

float UTradePanelWidget::GetBuyPrice(FGuid ItemID) const
{
	if (!MerchantInventory) return 0.0f;
	return MerchantInventory->GetItemSellPrice(ItemID, BuyPriceMultiplier);
}

float UTradePanelWidget::GetSellPrice(FGuid ItemID) const
{
	if (!PlayerInventory) return 0.0f;
	return PlayerInventory->GetItemSellPrice(ItemID, SellPriceMultiplier);
}

void UTradePanelWidget::RefreshGoldDisplay()
{
	if (PlayerGoldText && PlayerInventory)
	{
		PlayerGoldText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f"), PlayerInventory->GetGold())));
	}
	if (MerchantGoldText && MerchantInventory)
	{
		MerchantGoldText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f"), MerchantInventory->GetGold())));
	}
}

void UTradePanelWidget::OnTradeOpened_Implementation()
{
	// Override in Blueprint for animations, sounds, etc.
}

void UTradePanelWidget::OnTradeClosed_Implementation()
{
	// Override in Blueprint for animations, sounds, etc.
}

void UTradePanelWidget::OnItemBought_Implementation(const FGuid& ItemID, float Price)
{
	// Override in Blueprint for buy feedback
}

void UTradePanelWidget::OnItemSold_Implementation(const FGuid& ItemID, float Price)
{
	// Override in Blueprint for sell feedback
}

void UTradePanelWidget::OnCloseButtonClicked()
{
	CloseTrade();
}

void UTradePanelWidget::OnPlayerGoldChanged(float NewGold, float Delta)
{
	RefreshGoldDisplay();
}

void UTradePanelWidget::OnMerchantGoldChanged(float NewGold, float Delta)
{
	RefreshGoldDisplay();
}

void UTradePanelWidget::OnInventoryContentsChanged()
{
	RefreshGoldDisplay();
}
