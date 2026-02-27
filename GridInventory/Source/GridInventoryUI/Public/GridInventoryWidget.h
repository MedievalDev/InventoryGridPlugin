// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryItemInstance.h"
#include "GridInventoryWidget.generated.h"

class UGridInventoryComponent;
class UInventorySlotWidget;
class UCanvasPanel;
class USizeBox;

/**
 * VIRTUALIZED inventory grid widget. Optimized for large grids (60x80+).
 *
 * Only creates widgets for the visible viewport area (~200-300 slots).
 * Widgets are pooled and recycled on scroll. Full grid would be 4800 widgets,
 * this creates ~300 max.
 *
 * Nest inside a ScrollBox for scrolling. Call UpdateVisibleArea from the
 * ScrollBox's OnScroll event (or bind in Blueprint).
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UGridInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	float CellSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	/** Extra rows/columns rendered outside viewport as buffer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config", meta = (ClampMin = "1", ClampMax = "10"))
	int32 ViewportBuffer;

	// ========================
	// Grid Lines
	// ========================

	/** Whether to draw grid lines between cells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	bool bShowGridLines;

	/** Thickness of grid lines in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float GridLineThickness;

	/** Color of grid lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	FLinearColor GridLineColor;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Grid Inventory|Widgets")
	UCanvasPanel* GridCanvas;

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void SetInventoryComponent(UGridInventoryComponent* InInventory);

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Widget")
	UGridInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** Call when scroll position changes to recycle slots */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void UpdateVisibleArea(FVector2D ViewportOffset, FVector2D ViewportSize);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void RefreshGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void RefreshItems();

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Widget")
	FVector2D GetGridPixelSize() const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Widget")
	UInventorySlotWidget* GetSlotAt(int32 X, int32 Y) const;

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void HighlightArea(FIntPoint TopLeft, FIntPoint Size, bool bValid);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Widget")
	void ClearAllHighlights();

	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Widget")
	void OnItemClicked(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY);
	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Widget")
	void OnItemRightClicked(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY);
	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Widget")
	void OnItemHovered(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY);
	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Widget")
	void OnItemUnhovered();
	UFUNCTION(BlueprintNativeEvent, Category = "Grid Inventory|Widget")
	UWidget* CreateItemVisual(const FInventoryItemInstance& Item);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Grid-level mouse handling — all events are caught here, slots are visual-only
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, Category = "Grid Inventory|Widget")
	UGridInventoryComponent* InventoryComponent;

private:
	FIntPoint VisibleMin;
	FIntPoint VisibleMax;

	/** Active slots keyed by "Y * 10000 + X" */
	TMap<int64, UInventorySlotWidget*> ActiveSlots;
	TArray<UInventorySlotWidget*> SlotPool;
	TArray<UWidget*> ActiveItemVisuals;
	TArray<UWidget*> GridLineWidgets;

	// Grid-level mouse state
	FIntPoint PendingDragCell;
	FGuid PendingDragItemID;
	FIntPoint HoveredCell;
	FGuid HoveredItemID;

	int64 PosKey(int32 X, int32 Y) const { return (int64)Y * 10000 + X; }

	/** Convert absolute screen position to grid cell coordinates */
	FIntPoint ScreenToCell(const FVector2D& AbsolutePosition) const;

	/** Force all blueprint child widgets to SelfHitTestInvisible so events reach this widget */
	void SetupHitTestConfiguration();

	UInventorySlotWidget* AcquireSlot();
	void ReleaseSlot(UInventorySlotWidget* InvSlot);
	void RecycleSlots();
	void RefreshVisibleItems();
	void ClearItemVisuals();
	void CreateGridLines();
	void ClearGridLines();

	UFUNCTION()
	void OnInventoryChanged();
	void UnbindFromInventory();
};
