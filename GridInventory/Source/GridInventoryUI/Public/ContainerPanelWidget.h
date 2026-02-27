// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContainerPanelWidget.generated.h"

class AInventoryContainer;
class UGridInventoryComponent;
class UGridInventoryWidget;
class UTextBlock;
class UButton;

/**
 * Panel widget for interacting with world containers (chests, barrels, etc.).
 * Manages two inventory grids side by side: player + container.
 *
 * Blueprint setup:
 * 1. Create WBP_ContainerPanel (Parent: ContainerPanelWidget)
 * 2. Add layout: [PlayerGrid | ContainerGrid] with a title and close button
 * 3. Name widgets: "PlayerGrid", "ContainerGrid", "ContainerTitle", "CloseButton"
 * 4. In game: call OpenContainer(Container, PlayerInventory)
 *
 * Drag-drop between the two grids works automatically since
 * GridInventoryWidget supports cross-inventory transfers.
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UContainerPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================
	// Config
	// ========================

	/** Cell size for both grids */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container Panel|Config")
	float CellSize;

	/** Slot widget class for both grids */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container Panel|Config")
	TSubclassOf<class UInventorySlotWidget> SlotWidgetClass;

	// ========================
	// Bound Widgets
	// ========================

	/** Grid widget for the player's inventory */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Container Panel|Widgets")
	UGridInventoryWidget* PlayerGrid;

	/** Grid widget for the container's inventory */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Container Panel|Widgets")
	UGridInventoryWidget* ContainerGrid;

	/** Title text showing container name */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Container Panel|Widgets")
	UTextBlock* ContainerTitle;

	/** Close button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Container Panel|Widgets")
	UButton* CloseButton;

	// ========================
	// Functions
	// ========================

	/**
	 * Open a container and show both inventories.
	 * @param Container The world container to interact with
	 * @param PlayerInventory The player's inventory component
	 */
	UFUNCTION(BlueprintCallable, Category = "Container Panel")
	void OpenContainer(AInventoryContainer* Container, UGridInventoryComponent* PlayerInventory);

	/** Close the container panel and notify the container */
	UFUNCTION(BlueprintCallable, Category = "Container Panel")
	void CloseContainer();

	/** Check if a container is currently open */
	UFUNCTION(BlueprintPure, Category = "Container Panel")
	bool IsContainerOpen() const { return CurrentContainer != nullptr; }

	/** Get the currently opened container */
	UFUNCTION(BlueprintPure, Category = "Container Panel")
	AInventoryContainer* GetCurrentContainer() const { return CurrentContainer; }

	// ========================
	// Overridable Events
	// ========================

	UFUNCTION(BlueprintNativeEvent, Category = "Container Panel")
	void OnContainerOpened(AInventoryContainer* Container);

	UFUNCTION(BlueprintNativeEvent, Category = "Container Panel")
	void OnContainerClosed();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Container Panel")
	AInventoryContainer* CurrentContainer;

	UPROPERTY(BlueprintReadOnly, Category = "Container Panel")
	UGridInventoryComponent* PlayerInventoryComponent;

private:
	UFUNCTION()
	void OnCloseButtonClicked();
};
