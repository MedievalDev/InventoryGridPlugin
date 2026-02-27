// Copyright Marco. All Rights Reserved.

#include "ContainerPanelWidget.h"
#include "GridInventoryWidget.h"
#include "InventoryContainer.h"
#include "GridInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UContainerPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CellSize <= 0.0f) CellSize = 64.0f;

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UContainerPanelWidget::OnCloseButtonClicked);
	}
}

void UContainerPanelWidget::NativeDestruct()
{
	CloseContainer();

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UContainerPanelWidget::OnCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UContainerPanelWidget::OpenContainer(AInventoryContainer* Container, UGridInventoryComponent* PlayerInventory)
{
	if (!Container || !PlayerInventory) return;

	// Close previous container if any
	if (CurrentContainer)
	{
		CloseContainer();
	}

	CurrentContainer = Container;
	PlayerInventoryComponent = PlayerInventory;

	// Configure player grid
	if (PlayerGrid)
	{
		if (SlotWidgetClass)
		{
			PlayerGrid->SlotWidgetClass = SlotWidgetClass;
		}
		PlayerGrid->CellSize = CellSize;
		PlayerGrid->SetInventoryComponent(PlayerInventory);
	}

	// Configure container grid
	if (ContainerGrid)
	{
		if (SlotWidgetClass)
		{
			ContainerGrid->SlotWidgetClass = SlotWidgetClass;
		}
		ContainerGrid->CellSize = CellSize;
		ContainerGrid->SetInventoryComponent(Container->InventoryComponent);
	}

	// Set title
	if (ContainerTitle)
	{
		FText Title = FText::FromName(Container->ContainerID);
		if (Container->ContainerID == NAME_None)
		{
			Title = FText::FromString(TEXT("Container"));
		}
		ContainerTitle->SetText(Title);
	}

	// Show the widget
	SetVisibility(ESlateVisibility::Visible);

	OnContainerOpened(Container);

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] ContainerPanel — Opened container '%s'"),
		*Container->ContainerID.ToString());
}

void UContainerPanelWidget::CloseContainer()
{
	if (!CurrentContainer) return;

	// Tell the container to close
	CurrentContainer->Close();

	// Clear grid bindings
	if (PlayerGrid)
	{
		PlayerGrid->SetInventoryComponent(nullptr);
	}
	if (ContainerGrid)
	{
		ContainerGrid->SetInventoryComponent(nullptr);
	}

	CurrentContainer = nullptr;
	PlayerInventoryComponent = nullptr;

	SetVisibility(ESlateVisibility::Collapsed);

	OnContainerClosed();
}

void UContainerPanelWidget::OnContainerOpened_Implementation(AInventoryContainer* Container)
{
	// Override in Blueprint for animations, sounds, etc.
}

void UContainerPanelWidget::OnContainerClosed_Implementation()
{
	// Override in Blueprint for animations, sounds, etc.
}

void UContainerPanelWidget::OnCloseButtonClicked()
{
	CloseContainer();
}
