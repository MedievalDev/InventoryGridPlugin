// Copyright Marco. All Rights Reserved.

#include "InventoryDragDropOperation.h"
#include "InventoryItemDefinition.h"

void UInventoryDragDropOperation::ToggleRotation()
{
	if (DraggedItem.ItemDef && DraggedItem.ItemDef->bCanRotate)
	{
		bDragRotated = !bDragRotated;
		GrabOffset = FIntPoint(GrabOffset.Y, GrabOffset.X);
	}
}

FIntPoint UInventoryDragDropOperation::GetEffectiveDragSize() const
{
	if (!DraggedItem.ItemDef) return FIntPoint(1, 1);
	const bool bEffective = DraggedItem.bIsRotated != bDragRotated;
	return DraggedItem.ItemDef->GetEffectiveSize(bEffective);
}
