// Copyright Marco. All Rights Reserved.

#include "InventoryContextMenuWidget.h"
#include "GridInventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"

void UInventoryContextMenuWidget::InitMenu(const FInventoryItemInstance& InItem, UGridInventoryComponent* InInventory)
{
	ContextItem = InItem;
	ContextInventory = InInventory;

	if (!WidgetTree || !ContextItem.GetItemDef()) return;

	// Build the visual tree before AddToViewport
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.95f));
	Background->SetPadding(FMargin(6.0f));
	WidgetTree->RootWidget = Background;

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->AddChild(VBox);

	// Title: item name
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(ContextItem.GetDisplayName());
	FSlateFontInfo TitleFont = Title->Font;
	TitleFont.Size = 12;
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f)));
	VBox->AddChild(Title);

	// Separator
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>();
	Sep->SetBrushColor(FLinearColor(0.4f, 0.4f, 0.4f, 0.6f));
	Sep->SetPadding(FMargin(0.0f, 0.5f));
	VBox->AddChild(Sep);

	// Use/Consume (only if consumable)
	if (ContextItem.GetItemDef()->bIsConsumable)
	{
		UButton* Btn = CreateMenuButton(VBox, FText::FromString(TEXT("Benutzen")));
		Btn->OnClicked.AddDynamic(this, &UInventoryContextMenuWidget::OnUseClicked);
	}

	// Rotate (only if rotatable)
	if (ContextItem.GetItemDef()->bCanRotate)
	{
		UButton* Btn = CreateMenuButton(VBox, FText::FromString(TEXT("Drehen")));
		Btn->OnClicked.AddDynamic(this, &UInventoryContextMenuWidget::OnRotateClicked);
	}

	// Drop (always)
	{
		UButton* Btn = CreateMenuButton(VBox, FText::FromString(TEXT("Wegwerfen")));
		Btn->OnClicked.AddDynamic(this, &UInventoryContextMenuWidget::OnDropClicked);
	}

	// Info (always)
	{
		UButton* Btn = CreateMenuButton(VBox, FText::FromString(TEXT("Info")));
		Btn->OnClicked.AddDynamic(this, &UInventoryContextMenuWidget::OnInfoClicked);
	}
}

UButton* UInventoryContextMenuWidget::CreateMenuButton(UVerticalBox* Parent, const FText& Label)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	Btn->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));

	UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>();
	BtnText->SetText(Label);
	FSlateFontInfo BtnFont = BtnText->Font;
	BtnFont.Size = 11;
	BtnText->SetFont(BtnFont);
	BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	Btn->AddChild(BtnText);
	Parent->AddChild(Btn);

	return Btn;
}

void UInventoryContextMenuWidget::CloseMenu()
{
	RemoveFromParent();
}

void UInventoryContextMenuWidget::OnUseClicked()
{
	if (ContextInventory && ContextItem.UniqueID.IsValid())
	{
		ContextInventory->ConsumeItem(ContextItem.UniqueID);
	}
	CloseMenu();
}

void UInventoryContextMenuWidget::OnDropClicked()
{
	if (ContextInventory && ContextItem.UniqueID.IsValid())
	{
		// Use async drop to avoid blocking on WorldActorClass loading
		APawn* Pawn = GetOwningPlayerPawn();
		if (Pawn)
		{
			FVector DropLoc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.0f;
			ContextInventory->DropItemAsync(ContextItem.UniqueID, DropLoc, FRotator::ZeroRotator);
		}
		else
		{
			ContextInventory->RemoveItem(ContextItem.UniqueID);
		}
	}
	CloseMenu();
}

void UInventoryContextMenuWidget::OnRotateClicked()
{
	if (ContextInventory && ContextItem.UniqueID.IsValid())
	{
		ContextInventory->RotateItem(ContextItem.UniqueID);
	}
	CloseMenu();
}

void UInventoryContextMenuWidget::OnInfoClicked()
{
	if (ContextItem.GetItemDef())
	{
		FString Info = FString::Printf(
			TEXT("[%s] %s\nTyp: %s | Gewicht: %.1f | Stack: %d/%d | Klasse: %d"),
			*ContextItem.UniqueID.ToString().Left(8),
			*ContextItem.GetItemDef()->DisplayName.ToString(),
			*ContextItem.GetItemDef()->ItemType.ToString(),
			ContextItem.GetOwnWeight(),
			ContextItem.StackCount,
			ContextItem.GetItemDef()->MaxStackSize,
			ContextItem.CurrentClassLevel);

		// Effects
		TArray<FItemEffectValue> Effects = ContextItem.GetAllEffectValues();
		if (Effects.Num() > 0)
		{
			Info += TEXT("\nEffekte:");
			for (const FItemEffectValue& Eff : Effects)
			{
				Info += FString::Printf(TEXT(" %s=%.0f"), *Eff.EffectID.ToString(), Eff.Value);
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] ItemInfo: %s"), *Info);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan, Info);
		}
	}
	CloseMenu();
}
