// Copyright Marco. All Rights Reserved.

#include "EquipmentPanelWidget.h"
#include "EquipmentComponent.h"
#include "EquipmentSlotWidget.h"
#include "GridInventoryComponent.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

void UEquipmentPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (EquipmentSlotSize <= 0.0f) EquipmentSlotSize = 64.0f;
}

void UEquipmentPanelWidget::NativeDestruct()
{
	ClearSlotWidgets();
	Super::NativeDestruct();
}

void UEquipmentPanelWidget::SetEquipmentComponent(UEquipmentComponent* InEquipment)
{
	EquipmentComponent = InEquipment;
	CreateSlotWidgets();
}

void UEquipmentPanelWidget::SetLinkedInventory(UGridInventoryComponent* InInventory)
{
	LinkedInventory = InInventory;

	// Update all existing slot widgets
	for (UEquipmentSlotWidget* SlotW : SlotWidgets)
	{
		if (SlotW)
		{
			SlotW->SetLinkedInventory(LinkedInventory);
		}
	}
}

void UEquipmentPanelWidget::RefreshSlots()
{
	for (UEquipmentSlotWidget* SlotW : SlotWidgets)
	{
		if (SlotW)
		{
			SlotW->Refresh();
		}
	}
}

UEquipmentSlotWidget* UEquipmentPanelWidget::GetSlotWidget(FName SlotID) const
{
	for (UEquipmentSlotWidget* SlotW : SlotWidgets)
	{
		if (SlotW && SlotW->SlotID == SlotID)
		{
			return SlotW;
		}
	}
	return nullptr;
}

void UEquipmentPanelWidget::ClearSlotWidgets()
{
	for (UEquipmentSlotWidget* SlotW : SlotWidgets)
	{
		if (SlotW)
		{
			UWidget* Parent = SlotW->GetParent();
			if (Parent)
			{
				Parent->RemoveFromParent();
			}
			else
			{
				SlotW->RemoveFromParent();
			}
		}
	}
	SlotWidgets.Empty();
}

void UEquipmentPanelWidget::CreateSlotWidgets()
{
	ClearSlotWidgets();

	if (!EquipmentComponent || !WidgetTree) return;

	// If no SlotContainer is bound from Blueprint, create a VerticalBox
	if (!SlotContainer)
	{
		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
		WidgetTree->RootWidget = VBox;
		SlotContainer = VBox;
	}

	TArray<FEquipmentSlotDefinition> Defs = EquipmentComponent->GetAllSlotDefinitions();

	for (const FEquipmentSlotDefinition& Def : Defs)
	{
		UEquipmentSlotWidget* SlotW = SlotWidgetClass
			? CreateWidget<UEquipmentSlotWidget>(this, SlotWidgetClass)
			: CreateWidget<UEquipmentSlotWidget>(this);

		if (!SlotW) continue;

		SlotW->SlotID = Def.SlotID;
		SlotW->SlotSize = EquipmentSlotSize;
		SlotW->SetEquipmentComponent(EquipmentComponent);
		SlotW->SetLinkedInventory(LinkedInventory);

		// Wrap in SizeBox for consistent sizing
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(EquipmentSlotSize);
		SB->SetHeightOverride(EquipmentSlotSize);
		SB->AddChild(SlotW);

		SlotContainer->AddChild(SB);
		SlotWidgets.Add(SlotW);
	}

	UE_LOG(LogTemp, Log, TEXT("[GridInventory] EquipmentPanel — Created %d slot widgets"), SlotWidgets.Num());
}
