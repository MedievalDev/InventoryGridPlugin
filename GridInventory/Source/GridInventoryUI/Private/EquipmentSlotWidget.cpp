// Copyright Marco. All Rights Reserved.

#include "EquipmentSlotWidget.h"
#include "EquipmentComponent.h"
#include "GridInventoryComponent.h"
#include "InventoryDragDropOperation.h"
#include "InventoryItemDefinition.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UEquipmentSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SlotSize <= 0.0f) SlotSize = 64.0f;
}

void UEquipmentSlotWidget::NativeDestruct()
{
	UnbindFromEquipment();
	Super::NativeDestruct();
}

void UEquipmentSlotWidget::SetEquipmentComponent(UEquipmentComponent* InEquipment)
{
	UnbindFromEquipment();
	EquipmentComponent = InEquipment;

	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &UEquipmentSlotWidget::OnEquipmentChanged);
		Refresh();
	}
}

void UEquipmentSlotWidget::SetLinkedInventory(UGridInventoryComponent* InInventory)
{
	LinkedInventory = InInventory;
}

void UEquipmentSlotWidget::Refresh()
{
	if (!EquipmentComponent) return;

	FInventoryItemInstance Item = EquipmentComponent->GetItemInSlot(SlotID);
	OnSlotChanged(Item, !Item.IsValid());
}

FInventoryItemInstance UEquipmentSlotWidget::GetEquippedItem() const
{
	if (!EquipmentComponent) return FInventoryItemInstance();
	return EquipmentComponent->GetItemInSlot(SlotID);
}

bool UEquipmentSlotWidget::IsEmpty() const
{
	return !EquipmentComponent || EquipmentComponent->IsSlotEmpty(SlotID);
}

FEquipmentSlotDefinition UEquipmentSlotWidget::GetSlotDefinition() const
{
	if (!EquipmentComponent) return FEquipmentSlotDefinition();
	return EquipmentComponent->GetSlotDefinition(SlotID);
}

// ========================
// Default Implementations
// ========================

void UEquipmentSlotWidget::OnSlotChanged_Implementation(const FInventoryItemInstance& Item, bool bIsEmpty)
{
	// Override in Widget Blueprint for custom visuals
}

void UEquipmentSlotWidget::OnDragHighlight_Implementation(bool bCanEquip)
{
	// Override for highlight
}

void UEquipmentSlotWidget::OnDragHighlightClear_Implementation()
{
	// Override to clear highlight
}

void UEquipmentSlotWidget::OnSlotRightClicked_Implementation()
{
	// Default: unequip on right-click
	if (EquipmentComponent && !IsEmpty())
	{
		EquipmentComponent->UnequipItem(SlotID, 0);
	}
}

// ========================
// Input & Drag-Drop
// ========================

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!IsEmpty())
		{
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
	}

	return FReply::Unhandled();
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnSlotRightClicked();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!EquipmentComponent || IsEmpty()) return;

	FInventoryItemInstance Item = GetEquippedItem();
	if (!Item.IsValid()) return;

	UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
	DragOp->DraggedItem = Item;
	DragOp->SourceInventory = LinkedInventory; // May be null, that's ok
	DragOp->bDragRotated = false;
	DragOp->GrabOffset = FIntPoint::ZeroValue;
	DragOp->DragCount = 0; // All
	DragOp->bFromEquipmentSlot = true;
	DragOp->SourceEquipmentSlotID = SlotID;

	// Create a visual for the drag
	// Users can override CreateItemVisual on the grid widget or handle this in Blueprint
	DragOp->Pivot = EDragPivot::CenterCenter;

	OutOperation = DragOp;
}

void UEquipmentSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !EquipmentComponent) return;

	const bool bCanEquip = EquipmentComponent->CanEquipInSlot(SlotID, DragOp->DraggedItem.ItemDef);
	OnDragHighlight(bCanEquip);
}

void UEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	OnDragHighlightClear();
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	OnDragHighlightClear();

	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !EquipmentComponent) return false;

	// Check if item is compatible
	if (!EquipmentComponent->CanEquipInSlot(SlotID, DragOp->DraggedItem.ItemDef))
	{
		return false;
	}

	// If dragged from another equipment slot
	if (DragOp->bFromEquipmentSlot)
	{
		return EquipmentComponent->SwapSlots(DragOp->SourceEquipmentSlotID, SlotID);
	}

	// If dragged from inventory
	if (DragOp->SourceInventory)
	{
		return EquipmentComponent->EquipFromInventory(SlotID, DragOp->DraggedItem.UniqueID);
	}

	// Direct equip (no source inventory)
	return EquipmentComponent->EquipItem(SlotID, DragOp->DraggedItem.ItemDef, DragOp->DraggedItem.StackCount);
}

// ============================================================================
// Internal
// ============================================================================

void UEquipmentSlotWidget::OnEquipmentChanged()
{
	Refresh();
}

void UEquipmentSlotWidget::UnbindFromEquipment()
{
	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquipmentChanged.RemoveDynamic(this, &UEquipmentSlotWidget::OnEquipmentChanged);
	}
}
