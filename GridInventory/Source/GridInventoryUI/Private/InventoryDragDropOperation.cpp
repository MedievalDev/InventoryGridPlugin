// Copyright Marco. All Rights Reserved.

#include "InventoryDragDropOperation.h"
#include "InventoryItemDefinition.h"

void UInventoryDragDropOperation::ToggleRotation()
{
	if (DraggedItem.GetItemDef() && DraggedItem.GetItemDef()->bCanRotate)
	{
		bDragRotated = !bDragRotated;
		GrabOffset = FIntPoint(GrabOffset.Y, GrabOffset.X);
	}
}

FIntPoint UInventoryDragDropOperation::GetEffectiveDragSize() const
{
	if (!DraggedItem.GetItemDef()) return FIntPoint(1, 1);
	const bool bEffective = DraggedItem.bIsRotated != bDragRotated;
	return DraggedItem.GetItemDef()->GetEffectiveSize(bEffective);
}
