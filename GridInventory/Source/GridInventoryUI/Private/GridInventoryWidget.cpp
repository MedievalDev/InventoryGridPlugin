// Copyright Marco. All Rights Reserved.

#include "GridInventoryWidget.h"
#include "GridInventoryComponent.h"
#include "InventorySlotWidget.h"
#include "InventoryItemDefinition.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/StreamableManager.h"

void UGridInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CellSize <= 0.0f) CellSize = 64.0f;
	if (ViewportBuffer <= 0) ViewportBuffer = 3;
	VisibleMin = FIntPoint::ZeroValue;
	VisibleMax = FIntPoint::ZeroValue;
}

void UGridInventoryWidget::NativeDestruct()
{
	UnbindFromInventory();
	Super::NativeDestruct();
}

void UGridInventoryWidget::SetInventoryComponent(UGridInventoryComponent* InInventory)
{
	UnbindFromInventory();
	InventoryComponent = InInventory;

	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UGridInventoryWidget::OnInventoryChanged);

		// Initialize with full visible area (will be narrowed by UpdateVisibleArea)
		VisibleMin = FIntPoint(0, 0);
		VisibleMax = FIntPoint(InventoryComponent->GridWidth, InventoryComponent->GridHeight);

		RefreshGrid();
	}
}

void UGridInventoryWidget::UpdateVisibleArea(FVector2D ViewportOffset, FVector2D ViewportSize)
{
	if (!InventoryComponent) return;

	// Calculate visible cell range from pixel coordinates
	const int32 MinX = FMath::Max(0, FMath::FloorToInt(ViewportOffset.X / CellSize) - ViewportBuffer);
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(ViewportOffset.Y / CellSize) - ViewportBuffer);
	const int32 MaxX = FMath::Min(InventoryComponent->GridWidth,
		FMath::CeilToInt((ViewportOffset.X + ViewportSize.X) / CellSize) + ViewportBuffer);
	const int32 MaxY = FMath::Min(InventoryComponent->GridHeight,
		FMath::CeilToInt((ViewportOffset.Y + ViewportSize.Y) / CellSize) + ViewportBuffer);

	const FIntPoint NewMin(MinX, MinY);
	const FIntPoint NewMax(MaxX, MaxY);

	// Only recycle if visible area actually changed
	if (NewMin != VisibleMin || NewMax != VisibleMax)
	{
		VisibleMin = NewMin;
		VisibleMax = NewMax;
		RecycleSlots();
		RefreshVisibleItems();
	}
}

void UGridInventoryWidget::RefreshGrid()
{
	RecycleSlots();
	RefreshVisibleItems();
}

void UGridInventoryWidget::RefreshItems()
{
	// Update occupied state for active slots
	if (!InventoryComponent) return;

	for (auto& Pair : ActiveSlots)
	{
		if (Pair.Value)
		{
			const int32 X = Pair.Key % 10000;
			const int32 Y = Pair.Key / 10000;
			Pair.Value->SetOccupied(!InventoryComponent->IsPositionFree(FIntPoint(X, Y)));
		}
	}

	RefreshVisibleItems();
}

FVector2D UGridInventoryWidget::GetGridPixelSize() const
{
	if (!InventoryComponent) return FVector2D::ZeroVector;
	return FVector2D(InventoryComponent->GridWidth * CellSize, InventoryComponent->GridHeight * CellSize);
}

UInventorySlotWidget* UGridInventoryWidget::GetSlotAt(int32 X, int32 Y) const
{
	const int64 Key = PosKey(X, Y);
	const auto* Found = ActiveSlots.Find(Key);
	return Found ? *Found : nullptr;
}

void UGridInventoryWidget::HighlightArea(FIntPoint TopLeft, FIntPoint Size, bool bValid)
{
	for (int32 Y = TopLeft.Y; Y < TopLeft.Y + Size.Y; ++Y)
	{
		for (int32 X = TopLeft.X; X < TopLeft.X + Size.X; ++X)
		{
			if (UInventorySlotWidget* InvSlot = GetSlotAt(X, Y))
			{
				InvSlot->SetHighlight(true, bValid);
			}
		}
	}
}

void UGridInventoryWidget::ClearAllHighlights()
{
	for (auto& Pair : ActiveSlots)
	{
		if (Pair.Value) Pair.Value->ClearHighlight();
	}
}

// Default implementations
void UGridInventoryWidget::OnItemClicked_Implementation(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY) {}
void UGridInventoryWidget::OnItemRightClicked_Implementation(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY) {}
void UGridInventoryWidget::OnItemHovered_Implementation(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY) {}
void UGridInventoryWidget::OnItemUnhovered_Implementation() {}

UWidget* UGridInventoryWidget::CreateItemVisual_Implementation(const FInventoryItemInstance& Item)
{
	if (!Item.ItemDef || !WidgetTree) return nullptr;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();

	UImage* IconImage = WidgetTree->ConstructWidget<UImage>();

	// Load icon from soft reference
	if (!Item.ItemDef->Icon.IsNull())
	{
		UTexture2D* LoadedIcon = Item.ItemDef->Icon.LoadSynchronous();
		if (LoadedIcon)
		{
			IconImage->SetBrushFromTexture(LoadedIcon);
		}
	}

	const FIntPoint EffSize = Item.GetEffectiveSize();
	IconImage->SetBrushSize(FVector2D(EffSize.X * CellSize, EffSize.Y * CellSize));
	Overlay->AddChild(IconImage);

	if (Item.ItemDef->bCanStack && Item.StackCount > 1)
	{
		UTextBlock* Count = WidgetTree->ConstructWidget<UTextBlock>();
		Count->SetText(FText::AsNumber(Item.StackCount));
		FSlateFontInfo CountFont = Count->Font;
		CountFont.Size = 14;
		Count->SetFont(CountFont);
		Count->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Count->SetShadowOffset(FVector2D(1, 1));
		Count->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
		Overlay->AddChild(Count);
	}

	// Show class level indicator
	if (Item.ItemDef->bCanMergeUpgrade && Item.CurrentClassLevel > 1)
	{
		UTextBlock* ClassText = WidgetTree->ConstructWidget<UTextBlock>();
		ClassText->SetText(Item.GetDisplayName());
		FSlateFontInfo ClassFont = ClassText->Font;
		ClassFont.Size = 10;
		ClassText->SetFont(ClassFont);
		ClassText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f)));
		Overlay->AddChild(ClassText);
	}

	return Overlay;
}

// ============================================================================
// Virtualization - Slot Pool Management
// ============================================================================

UInventorySlotWidget* UGridInventoryWidget::AcquireSlot()
{
	UInventorySlotWidget* InvSlot = nullptr;

	if (SlotPool.Num() > 0)
	{
		// Reuse from pool
		InvSlot = SlotPool.Pop();
		InvSlot->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// Create new
		InvSlot = SlotWidgetClass
			? CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass)
			: CreateWidget<UInventorySlotWidget>(this);

		if (InvSlot && GridCanvas)
		{
			// Wrap in SizeBox for precise sizing
			USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
			SB->SetWidthOverride(CellSize);
			SB->SetHeightOverride(CellSize);
			SB->AddChild(InvSlot);
			GridCanvas->AddChild(SB);
		}
	}

	return InvSlot;
}

void UGridInventoryWidget::ReleaseSlot(UInventorySlotWidget* InvSlot)
{
	if (InvSlot)
	{
		InvSlot->ClearHighlight();
		InvSlot->SetVisibility(ESlateVisibility::Collapsed);
		SlotPool.Add(InvSlot);
	}
}

void UGridInventoryWidget::RecycleSlots()
{
	if (!InventoryComponent || !GridCanvas) return;

	// 1. Release slots that are now outside visible range
	TArray<int64> ToRelease;
	for (auto& Pair : ActiveSlots)
	{
		const int32 X = Pair.Key % 10000;
		const int32 Y = Pair.Key / 10000;

		if (X < VisibleMin.X || X >= VisibleMax.X ||
			Y < VisibleMin.Y || Y >= VisibleMax.Y)
		{
			ToRelease.Add(Pair.Key);
		}
	}

	for (int64 Key : ToRelease)
	{
		ReleaseSlot(ActiveSlots[Key]);
		ActiveSlots.Remove(Key);
	}

	// 2. Create slots for newly visible cells
	for (int32 Y = VisibleMin.Y; Y < VisibleMax.Y; ++Y)
	{
		for (int32 X = VisibleMin.X; X < VisibleMax.X; ++X)
		{
			const int64 Key = PosKey(X, Y);
			if (ActiveSlots.Contains(Key)) continue;

			UInventorySlotWidget* InvSlot = AcquireSlot();
			if (!InvSlot) continue;

			InvSlot->InitSlot(X, Y, InventoryComponent);

			// Position on canvas
			UWidget* Parent = InvSlot->GetParent();
			if (Parent)
			{
				UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Parent->Slot);
				if (CanvasSlot)
				{
					CanvasSlot->SetAutoSize(true);
					CanvasSlot->SetPosition(FVector2D(X * CellSize, Y * CellSize));
				}
			}

			// Set occupied state
			InvSlot->SetOccupied(!InventoryComponent->IsPositionFree(FIntPoint(X, Y)));

			ActiveSlots.Add(Key, InvSlot);
		}
	}
}

void UGridInventoryWidget::RefreshVisibleItems()
{
	ClearItemVisuals();

	if (!InventoryComponent || !GridCanvas) return;

	// Only create visuals for items that overlap the visible area
	for (const FInventoryItemInstance& Item : InventoryComponent->GetAllItems())
	{
		if (!Item.IsValid()) continue;

		const FIntPoint Pos = Item.GridPosition;
		const FIntPoint Size = Item.GetEffectiveSize();

		// Check if item overlaps visible area
		if (Pos.X + Size.X <= VisibleMin.X || Pos.X >= VisibleMax.X) continue;
		if (Pos.Y + Size.Y <= VisibleMin.Y || Pos.Y >= VisibleMax.Y) continue;

		UWidget* Visual = CreateItemVisual(Item);
		if (!Visual) continue;

		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(Size.X * CellSize);
		SB->SetHeightOverride(Size.Y * CellSize);
		SB->AddChild(Visual);

		GridCanvas->AddChild(SB);

		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(SB->Slot))
		{
			CS->SetAutoSize(true);
			CS->SetPosition(FVector2D(Pos.X * CellSize, Pos.Y * CellSize));
		}

		ActiveItemVisuals.Add(SB);
	}
}

void UGridInventoryWidget::ClearItemVisuals()
{
	for (UWidget* W : ActiveItemVisuals)
	{
		if (W) W->RemoveFromParent();
	}
	ActiveItemVisuals.Empty();
}

void UGridInventoryWidget::OnInventoryChanged()
{
	RefreshItems();
}

void UGridInventoryWidget::UnbindFromInventory()
{
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UGridInventoryWidget::OnInventoryChanged);
	}
}
