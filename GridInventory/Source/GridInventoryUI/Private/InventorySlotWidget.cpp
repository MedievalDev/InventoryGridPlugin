// Copyright Marco. All Rights Reserved.

#include "InventorySlotWidget.h"
#include "GridInventoryComponent.h"
#include "InventoryDragDropOperation.h"

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Slots are visual-only — all mouse handling is on the GridInventoryWidget
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInventorySlotWidget::InitSlot(int32 InX, int32 InY, UGridInventoryComponent* InInventory)
{
	SlotX = InX;
	SlotY = InY;
	OwningInventory = InInventory;
	bIsOccupied = false;
	bIsHighlighted = false;
	bIsValidPlacement = false;
}

void UInventorySlotWidget::SetOccupied(bool bNewOccupied)
{
	if (bIsOccupied != bNewOccupied)
	{
		bIsOccupied = bNewOccupied;
		OnOccupiedChanged(bNewOccupied);
	}
}

void UInventorySlotWidget::SetHighlight(bool bNewHighlighted, bool bValid)
{
	bIsHighlighted = bNewHighlighted;
	bIsValidPlacement = bValid;
	OnHighlightChanged(bNewHighlighted, bValid);
}

void UInventorySlotWidget::ClearHighlight()
{
	SetHighlight(false, false);
}

void UInventorySlotWidget::OnOccupiedChanged_Implementation(bool bOccupied) {}
void UInventorySlotWidget::OnHighlightChanged_Implementation(bool bHighlighted, bool bValid) {}
void UInventorySlotWidget::OnSlotRightClicked_Implementation(const FInventoryItemInstance& Item) {}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OwningInventory)
	{
		FInventoryItemInstance Item = OwningInventory->GetItemAtPosition(FIntPoint(SlotX, SlotY));
		
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (Item.IsValid())
			{
				return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
			}
		}
		else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			if (Item.IsValid())
			{
				OnSlotRightClicked(Item);
			}
		}
	}
	// Always handle to prevent click-through to game
	return FReply::Handled();
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !OwningInventory) return;

	const FIntPoint DropPos = FIntPoint(SlotX, SlotY) - DragOp->GrabOffset;
	const bool bEffRot = DragOp->DraggedItem.bIsRotated != DragOp->bDragRotated;
	const bool bCanPlace = OwningInventory->CanPlaceAt(DragOp->DraggedItem.GetItemDef(), DropPos, bEffRot);
	SetHighlight(true, bCanPlace);
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	ClearHighlight();
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearHighlight();

	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !OwningInventory) return false;

	const FIntPoint DropPos = FIntPoint(SlotX, SlotY) - DragOp->GrabOffset;
	const bool bEffRot = DragOp->DraggedItem.bIsRotated != DragOp->bDragRotated;

	if (DragOp->SourceInventory == OwningInventory)
	{
		return OwningInventory->MoveItem(DragOp->DraggedItem.UniqueID, DropPos, bEffRot);
	}

	if (DragOp->SourceInventory)
	{
		return DragOp->SourceInventory->TransferItemTo(
			DragOp->DraggedItem.UniqueID, OwningInventory, DropPos, bEffRot, DragOp->DragCount);
	}

	return false;
}
