// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryItemInstance.h"
#include "InventorySlotWidget.generated.h"

class UGridInventoryComponent;

/**
 * Widget for a single grid cell.
 * Override OnOccupiedChanged / OnHighlightChanged in your Widget Blueprint
 * to customize the look.
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Slot")
	void InitSlot(int32 InX, int32 InY, UGridInventoryComponent* InInventory);

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	int32 SlotX;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	int32 SlotY;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	UGridInventoryComponent* OwningInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	bool bIsOccupied;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	bool bIsHighlighted;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Slot")
	bool bIsValidPlacement;

	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Slot")
	void OnOccupiedChanged(bool bOccupied);

	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Slot")
	void OnHighlightChanged(bool bHighlighted, bool bValid);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Slot")
	void SetOccupied(bool bNewOccupied);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Slot")
	void SetHighlight(bool bNewHighlighted, bool bValid);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Slot")
	void ClearHighlight();

	/** Called when this slot is right-clicked and contains an item. Override in Blueprint for context menu etc. */
	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Slot")
	void OnSlotRightClicked(const FInventoryItemInstance& Item);

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};
