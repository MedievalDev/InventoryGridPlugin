// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentPanelWidget.generated.h"

class UEquipmentComponent;
class UGridInventoryComponent;
class UEquipmentSlotWidget;
class UPanelWidget;

/**
 * Panel widget that creates equipment slot widgets dynamically
 * from an EquipmentComponent's slot definitions.
 *
 * Blueprint setup:
 * 1. Create WBP_EquipmentPanel (Parent: EquipmentPanelWidget)
 * 2. Add a container widget (VerticalBox, WrapBox, etc.) and name it "SlotContainer"
 * 3. Set SlotWidgetClass to your WBP_EquipmentSlot
 * 4. In Blueprint: call SetEquipmentComponent + SetLinkedInventory
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UEquipmentPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Pixel size for each equipment slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Panel|Config")
	float EquipmentSlotSize;

	/** Widget class for individual equipment slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Panel|Config")
	TSubclassOf<UEquipmentSlotWidget> SlotWidgetClass;

	/** Container widget for slot widgets (VerticalBox, WrapBox, etc.) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Equipment Panel|Widgets")
	UPanelWidget* SlotContainer;

	/** Bind to an equipment component — creates slot widgets for all defined slots */
	UFUNCTION(BlueprintCallable, Category = "Equipment Panel")
	void SetEquipmentComponent(UEquipmentComponent* InEquipment);

	/** Set the linked inventory for drag-drop integration */
	UFUNCTION(BlueprintCallable, Category = "Equipment Panel")
	void SetLinkedInventory(UGridInventoryComponent* InInventory);

	/** Rebuild all slot widgets */
	UFUNCTION(BlueprintCallable, Category = "Equipment Panel")
	void RefreshSlots();

	/** Get a slot widget by its ID */
	UFUNCTION(BlueprintPure, Category = "Equipment Panel")
	UEquipmentSlotWidget* GetSlotWidget(FName SlotID) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment Panel")
	UEquipmentComponent* EquipmentComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment Panel")
	UGridInventoryComponent* LinkedInventory;

private:
	UPROPERTY()
	TArray<UEquipmentSlotWidget*> SlotWidgets;

	void ClearSlotWidgets();
	void CreateSlotWidgets();
};
