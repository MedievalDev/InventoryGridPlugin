// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryItemInstance.h"
#include "InventoryDragDropOperation.generated.h"

class UGridInventoryComponent;

/**
 * Custom drag-drop operation for inventory items.
 * Carries item info, source inventory, rotation state, and grab offset.
 */
UCLASS(BlueprintType)
class GRIDINVENTORYUI_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	FInventoryItemInstance DraggedItem;

	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	UGridInventoryComponent* SourceInventory;

	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	bool bDragRotated;

	/** Cell offset from item top-left where user grabbed */
	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	FIntPoint GrabOffset;

	/** How many from stack (0 = all) */
	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	int32 DragCount;

	/** Whether this drag is from an equipment slot */
	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	bool bFromEquipmentSlot;

	/** If from equipment, which slot */
	UPROPERTY(BlueprintReadWrite, Category = "Grid Inventory|Drag")
	FName SourceEquipmentSlotID;

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Drag")
	void ToggleRotation();

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Drag")
	FIntPoint GetEffectiveDragSize() const;
};
