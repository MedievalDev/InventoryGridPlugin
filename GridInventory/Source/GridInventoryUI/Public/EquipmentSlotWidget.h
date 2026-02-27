// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentSlotDefinition.h"
#include "InventoryItemInstance.h"
#include "EquipmentSlotWidget.generated.h"

class UEquipmentComponent;
class UGridInventoryComponent;
class UImage;
class UTextBlock;

/**
 * Widget for a single equipment slot.
 *
 * Usage in your HUD Widget Blueprint:
 * 1. Drag as many EquipmentSlotWidgets as you have slots
 * 2. On each, set SlotID to the matching slot name (e.g. "Head", "MainHand")
 * 3. Call SetEquipmentComponent to bind
 *
 * Supports drag-drop from/to the grid inventory.
 * Override the NativeEvents for custom visuals.
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UEquipmentSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Which equipment slot this widget represents.
	 * Set in the editor for each widget instance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot|Config")
	FName SlotID;

	/** Pixel size of this slot widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot|Config")
	float SlotSize;

	/** Bind to an equipment component */
	UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
	void SetEquipmentComponent(UEquipmentComponent* InEquipment);

	/** Also set linked inventory for drag-drop integration */
	UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
	void SetLinkedInventory(UGridInventoryComponent* InInventory);

	/** Refresh the display */
	UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
	void Refresh();

	/** Get the currently equipped item in this slot */
	UFUNCTION(BlueprintPure, Category = "Equipment Slot")
	FInventoryItemInstance GetEquippedItem() const;

	/** Check if this slot is empty */
	UFUNCTION(BlueprintPure, Category = "Equipment Slot")
	bool IsEmpty() const;

	/** Get the slot definition for this slot */
	UFUNCTION(BlueprintPure, Category = "Equipment Slot")
	FEquipmentSlotDefinition GetSlotDefinition() const;

	// ========================
	// Overridable Events
	// ========================

	/** Called when the slot content changes. Override for custom visuals. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment Slot")
	void OnSlotChanged(const FInventoryItemInstance& Item, bool bIsEmpty);

	/** Called when drag enters this slot. Override for highlight effects. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment Slot")
	void OnDragHighlight(bool bCanEquip);

	/** Called when drag leaves. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment Slot")
	void OnDragHighlightClear();

	/** Called on right-click (for unequip via context). */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment Slot")
	void OnSlotRightClicked();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment Slot")
	UEquipmentComponent* EquipmentComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment Slot")
	UGridInventoryComponent* LinkedInventory;

private:
	UFUNCTION()
	void OnEquipmentChanged();

	void UnbindFromEquipment();
};
