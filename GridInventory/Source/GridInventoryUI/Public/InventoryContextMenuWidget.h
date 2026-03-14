// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryItemInstance.h"
#include "InventoryContextMenuWidget.generated.h"

class UGridInventoryComponent;
class UVerticalBox;
class UButton;

/**
 * Context menu shown on right-click in the inventory grid.
 * Created and managed by GridInventoryWidget.
 *
 * Actions:
 * - Benutzen (Use/Consume) — only if bIsConsumable
 * - Drehen (Rotate) — only if bCanRotate
 * - Wegwerfen (Drop) — always
 * - Info — always, prints item details to log/screen
 *
 * Override in Blueprint for custom styling or additional actions.
 */
UCLASS(BlueprintType, Blueprintable)
class GRIDINVENTORYUI_API UInventoryContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Build the menu content for the given item.
	 * Must be called BEFORE AddToViewport.
	 */
	UFUNCTION(BlueprintCallable, Category = "Context Menu")
	void InitMenu(const FInventoryItemInstance& InItem, UGridInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Context Menu")
	void CloseMenu();

	UPROPERTY(BlueprintReadOnly, Category = "Context Menu")
	FInventoryItemInstance ContextItem;

	UPROPERTY(BlueprintReadOnly, Category = "Context Menu")
	UGridInventoryComponent* ContextInventory;

private:
	UButton* CreateMenuButton(UVerticalBox* Parent, const FText& Label);

	UFUNCTION()
	void OnUseClicked();

	UFUNCTION()
	void OnDropClicked();

	UFUNCTION()
	void OnRotateClicked();

	UFUNCTION()
	void OnInfoClicked();
};
