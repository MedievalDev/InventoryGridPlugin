// Copyright Marco. All Rights Reserved.

#include "GridInventoryWidget.h"
#include "GridInventoryComponent.h"
#include "InventorySlotWidget.h"
#include "InventoryContextMenuWidget.h"
#include "InventoryItemDefinition.h"
#include "InventoryDragDropOperation.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

void UGridInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CellSize <= 0.0f) CellSize = 64.0f;
	if (ViewportBuffer <= 0) ViewportBuffer = 3;
	if (GridLineThickness <= 0.0f) GridLineThickness = 1.0f;
	if (GridLineColor == FLinearColor(0, 0, 0, 0)) GridLineColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.6f);
	VisibleMin = FIntPoint::ZeroValue;
	VisibleMax = FIntPoint::ZeroValue;
	HoveredCell = FIntPoint(-1, -1);
	ResolvedSlotWidgetClass = nullptr;
	CachedGridLineWidth = 0;
	CachedGridLineHeight = 0;

	// Make this widget hittable and focusable — grid handles all mouse events
	bIsFocusable = true;
	SetVisibility(ESlateVisibility::Visible);

	// Force all blueprint child widgets (SizeBox, Border, GridCanvas etc.)
	// to SelfHitTestInvisible so mouse events bubble up to this widget
	SetupHitTestConfiguration();

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] NativeConstruct — Visibility=%d, GridCanvas=%s, bIsFocusable=%d"),
		(int32)GetVisibility(), GridCanvas ? TEXT("OK") : TEXT("NULL"), bIsFocusable ? 1 : 0);
}

void UGridInventoryWidget::NativeDestruct()
{
	CloseContextMenu();

	// Cancel any in-flight async icon loads
	if (IconStreamHandle.IsValid() && IconStreamHandle->IsActive())
	{
		IconStreamHandle->CancelHandle();
	}
	IconStreamHandle.Reset();
	IconCache.Empty();

	UnbindFromInventory();
	Super::NativeDestruct();
}

void UGridInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Guard: Blueprint may toggle visibility to SelfHitTestInvisible (the UE4 default
	// for "Set Visibility" nodes). That makes this widget non-hittable and breaks all
	// mouse handling. Upgrade to Visible whenever detected.
	if (GetVisibility() == ESlateVisibility::SelfHitTestInvisible)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Visibility was SelfHitTestInvisible — upgrading to Visible for hit testing"));
		SetVisibility(ESlateVisibility::Visible);
	}
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

		// Re-enforce hittable state — Blueprint calls may have changed visibility
		SetVisibility(ESlateVisibility::Visible);
		SetupHitTestConfiguration();

		// Preload icons asynchronously before first refresh
		PreloadVisibleIcons();
		RefreshGrid();

		UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SetInventoryComponent — Grid %dx%d, GridCanvas=%s"),
			InventoryComponent->GridWidth, InventoryComponent->GridHeight,
			GridCanvas ? TEXT("OK") : TEXT("NULL"));
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
	CreateGridLines();
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

void UGridInventoryWidget::OnItemRightClicked_Implementation(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY)
{
	CloseContextMenu();

	// Create fresh each time — WidgetTree::RootWidget can't be safely reassigned
	// without orphaning old Slate widgets. For a user-triggered action (right-click)
	// the allocation overhead is negligible.
	ActiveContextMenu = CreateWidget<UInventoryContextMenuWidget>(GetOwningPlayer());
	if (ActiveContextMenu)
	{
		ActiveContextMenu->InitMenu(Item, InventoryComponent);
		ActiveContextMenu->AddToViewport(100);

		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			float MX, MY;
			PC->GetMousePosition(MX, MY);
			ActiveContextMenu->SetPositionInViewport(FVector2D(MX, MY));
		}
	}
}

void UGridInventoryWidget::OnItemHovered_Implementation(const FInventoryItemInstance& Item, int32 SlotX, int32 SlotY)
{
	if (!Item.ItemDef) return;

	// Build tooltip text from item properties
	FString Tip = Item.GetDisplayName().ToString();
	Tip += TEXT("\n");

	if (!Item.ItemDef->ItemType.IsNone())
	{
		Tip += FString::Printf(TEXT("Typ: %s\n"), *Item.ItemDef->ItemType.ToString());
	}

	if (!Item.ItemDef->Description.IsEmpty())
	{
		Tip += Item.ItemDef->Description.ToString();
		Tip += TEXT("\n");
	}

	// Effects
	TArray<FItemEffectValue> AllEffects = Item.GetAllEffectValues();
	for (const FItemEffectValue& Eff : AllEffects)
	{
		Tip += FString::Printf(TEXT("%s: %.0f\n"), *Eff.EffectID.ToString(), Eff.Value);
	}

	// Weight + Stack
	Tip += FString::Printf(TEXT("Gewicht: %.1f"), Item.GetOwnWeight());
	if (Item.ItemDef->bCanStack && Item.StackCount > 1)
	{
		Tip += FString::Printf(TEXT("  [%d/%d]"), Item.StackCount, Item.ItemDef->MaxStackSize);
	}

	SetToolTipText(FText::FromString(Tip));
}

void UGridInventoryWidget::OnItemUnhovered_Implementation()
{
	SetToolTipText(FText::GetEmpty());
}

UWidget* UGridInventoryWidget::CreateItemVisual_Implementation(const FInventoryItemInstance& Item)
{
	if (!Item.ItemDef || !WidgetTree) return nullptr;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();

	UImage* IconImage = WidgetTree->ConstructWidget<UImage>();

	// Use cached icon (preloaded asynchronously) — no LoadSynchronous on UI thread
	if (!Item.ItemDef->Icon.IsNull())
	{
		UTexture2D* CachedTex = GetCachedIcon(Item.ItemDef->Icon);
		if (CachedTex)
		{
			IconImage->SetBrushFromTexture(CachedTex);
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
// Grid-Level Mouse Handling
// ============================================================================

FIntPoint UGridInventoryWidget::ScreenToCell(const FVector2D& AbsolutePosition) const
{
	if (!GridCanvas) return FIntPoint(-1, -1);

	const FGeometry CanvasGeo = GridCanvas->GetCachedGeometry();
	const FVector2D LocalPos = CanvasGeo.AbsoluteToLocal(AbsolutePosition);

	return FIntPoint(
		FMath::FloorToInt(LocalPos.X / CellSize),
		FMath::FloorToInt(LocalPos.Y / CellSize)
	);
}

void UGridInventoryWidget::SetupHitTestConfiguration()
{
	if (!WidgetTree) return;

	int32 ConfiguredCount = 0;

	// Set all existing child widgets (SizeBox, Border, GridCanvas etc.) to
	// SelfHitTestInvisible — they remain visible but don't intercept mouse events.
	// This ensures all events bubble up to GridInventoryWidget::NativeOnMouseButtonDown.
	WidgetTree->ForEachWidget([this, &ConfiguredCount](UWidget* Widget)
	{
		if (Widget && Widget != this)
		{
			Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			ConfiguredCount++;
			UE_LOG(LogTemp, Log, TEXT("[GridInventory] SetupHitTest: '%s' → SelfHitTestInvisible"),
				*Widget->GetName());
		}
	});

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] SetupHitTestConfiguration — %d child widgets configured"), ConfiguredCount);
}

FReply UGridInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Close any open context menu on left-click
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CloseContextMenu();
	}

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] MouseDown — InventoryComponent=%s, GridCanvas=%s"),
		InventoryComponent ? TEXT("OK") : TEXT("NULL"),
		GridCanvas ? TEXT("OK") : TEXT("NULL"));

	if (!InventoryComponent || !GridCanvas)
	{
		return FReply::Handled();
	}

	const FIntPoint Cell = ScreenToCell(InMouseEvent.GetScreenSpacePosition());
	const int32 CellX = Cell.X;
	const int32 CellY = Cell.Y;

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] MouseDown at Cell (%d, %d)"), CellX, CellY);

	if (CellX < 0 || CellY < 0 || CellX >= InventoryComponent->GridWidth || CellY >= InventoryComponent->GridHeight)
	{
		return FReply::Handled();
	}

	FInventoryItemInstance Item = InventoryComponent->GetItemAtPosition(FIntPoint(CellX, CellY));

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (Item.IsValid())
		{
			// Store drag info for NativeOnDragDetected
			PendingDragCell = Cell;
			PendingDragItemID = Item.UniqueID;
			OnItemClicked(Item, CellX, CellY);

			// UE 4.27: use DetectDrag instead of CaptureMouse
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
	}
	else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Item.IsValid())
		{
			OnItemRightClicked(Item, CellX, CellY);
		}
	}

	return FReply::Handled();
}

FReply UGridInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PendingDragItemID.Invalidate();
	return FReply::Handled();
}

FReply UGridInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InventoryComponent || !GridCanvas) return FReply::Unhandled();

	const FIntPoint Cell = ScreenToCell(InMouseEvent.GetScreenSpacePosition());

	if (Cell != HoveredCell)
	{
		HoveredCell = Cell;

		if (Cell.X >= 0 && Cell.Y >= 0
			&& Cell.X < InventoryComponent->GridWidth
			&& Cell.Y < InventoryComponent->GridHeight)
		{
			FInventoryItemInstance Item = InventoryComponent->GetItemAtPosition(Cell);
			if (Item.IsValid())
			{
				if (Item.UniqueID != HoveredItemID)
				{
					HoveredItemID = Item.UniqueID;
					OnItemHovered(Item, Cell.X, Cell.Y);
				}
			}
			else if (HoveredItemID.IsValid())
			{
				HoveredItemID.Invalidate();
				OnItemUnhovered();
			}
		}
		else if (HoveredItemID.IsValid())
		{
			HoveredItemID.Invalidate();
			OnItemUnhovered();
		}
	}

	return FReply::Unhandled();
}

void UGridInventoryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (HoveredItemID.IsValid())
	{
		HoveredItemID.Invalidate();
		OnItemUnhovered();
	}
	HoveredCell = FIntPoint(-1, -1);
}

void UGridInventoryWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!InventoryComponent || !PendingDragItemID.IsValid()) return;

	FInventoryItemInstance Item = InventoryComponent->GetItemByID(PendingDragItemID);
	if (!Item.IsValid()) return;

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] DragDetected — Item '%s' from Cell (%d, %d)"),
		*Item.ItemDef->DisplayName.ToString(), PendingDragCell.X, PendingDragCell.Y);

	UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
	DragOp->DraggedItem = Item;
	DragOp->SourceInventory = InventoryComponent;
	DragOp->bDragRotated = false;
	DragOp->GrabOffset = PendingDragCell - Item.GridPosition;
	DragOp->DragCount = 0; // entire stack

	// Create drag visual
	UWidget* Visual = CreateItemVisual(Item);
	if (Visual)
	{
		DragOp->DefaultDragVisual = Visual;
		DragOp->Pivot = EDragPivot::MouseDown;
	}

	// Remove the item from its current position so the grid shows it as free
	// (if the drop fails, NativeOnDragCancelled should restore it)
	OutOperation = DragOp;

	PendingDragItemID.Invalidate();
}

bool UGridInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !InventoryComponent || !GridCanvas) return false;

	ClearAllHighlights();

	const FIntPoint Cell = ScreenToCell(InDragDropEvent.GetScreenSpacePosition());
	const FIntPoint DropPos = Cell - DragOp->GrabOffset;
	const FIntPoint Size = DragOp->GetEffectiveDragSize();
	const bool bEffRot = DragOp->DraggedItem.bIsRotated != DragOp->bDragRotated;
	const bool bCanPlace = InventoryComponent->CanPlaceAt(DragOp->DraggedItem.ItemDef, DropPos, bEffRot);

	HighlightArea(DropPos, Size, bCanPlace);

	return true;
}

void UGridInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearAllHighlights();
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UGridInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearAllHighlights();

	UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
	if (!DragOp || !InventoryComponent || !GridCanvas) return false;

	const FIntPoint Cell = ScreenToCell(InDragDropEvent.GetScreenSpacePosition());
	const FIntPoint DropPos = Cell - DragOp->GrabOffset;
	const bool bEffRot = DragOp->DraggedItem.bIsRotated != DragOp->bDragRotated;

	UE_LOG(LogTemp, Warning, TEXT("[GridInventory] Drop at Cell (%d, %d) → DropPos (%d, %d)"),
		Cell.X, Cell.Y, DropPos.X, DropPos.Y);

	if (DragOp->SourceInventory == InventoryComponent)
	{
		return InventoryComponent->MoveItem(DragOp->DraggedItem.UniqueID, DropPos, bEffRot);
	}

	if (DragOp->SourceInventory)
	{
		return DragOp->SourceInventory->TransferItemTo(
			DragOp->DraggedItem.UniqueID, InventoryComponent, DropPos, bEffRot, DragOp->DragCount);
	}

	return false;
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
		InvSlot->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// Create new — use resolved soft class (avoids hard ref)
		UClass* SlotClass = GetSlotWidgetClass();
		InvSlot = SlotClass
			? CreateWidget<UInventorySlotWidget>(this, SlotClass)
			: CreateWidget<UInventorySlotWidget>(this);

		if (InvSlot && GridCanvas)
		{
			// Wrap in SizeBox for precise sizing
			USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
			SB->SetWidthOverride(CellSize);
			SB->SetHeightOverride(CellSize);
			// SizeBox passes through hit-tests; slot itself is HitTestInvisible
			SB->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
	if (!InventoryComponent || !GridCanvas) return;

	// Build set of currently visible items
	TSet<FGuid> VisibleItemIDs;

	for (const FInventoryItemInstance& Item : InventoryComponent->GetAllItems())
	{
		if (!Item.IsValid()) continue;

		const FIntPoint Pos = Item.GridPosition;
		const FIntPoint Size = Item.GetEffectiveSize();

		// Check if item overlaps visible area
		if (Pos.X + Size.X <= VisibleMin.X || Pos.X >= VisibleMax.X) continue;
		if (Pos.Y + Size.Y <= VisibleMin.Y || Pos.Y >= VisibleMax.Y) continue;

		VisibleItemIDs.Add(Item.UniqueID);

		// Check if visual already exists and is up-to-date
		FItemVisualEntry* Existing = ActiveItemVisuals.Find(Item.UniqueID);
		if (Existing)
		{
			// Only rebuild if something changed (position, rotation, stack, class level)
			if (Existing->Position == Pos && Existing->bRotated == Item.bIsRotated
				&& Existing->StackCount == Item.StackCount && Existing->ClassLevel == Item.CurrentClassLevel)
			{
				continue; // No change — skip
			}

			// Changed — remove old visual
			if (Existing->Visual)
			{
				Existing->Visual->RemoveFromParent();
			}
			ActiveItemVisuals.Remove(Item.UniqueID);
		}

		// Create new visual
		UWidget* Visual = CreateItemVisual(Item);
		if (!Visual) continue;

		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(Size.X * CellSize);
		SB->SetHeightOverride(Size.Y * CellSize);
		SB->SetVisibility(ESlateVisibility::HitTestInvisible);
		SB->AddChild(Visual);

		GridCanvas->AddChild(SB);

		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(SB->Slot))
		{
			CS->SetAutoSize(true);
			CS->SetPosition(FVector2D(Pos.X * CellSize, Pos.Y * CellSize));
		}

		FItemVisualEntry Entry;
		Entry.UniqueID = Item.UniqueID;
		Entry.Position = Pos;
		Entry.Size = Size;
		Entry.StackCount = Item.StackCount;
		Entry.ClassLevel = Item.CurrentClassLevel;
		Entry.bRotated = Item.bIsRotated;
		Entry.Visual = SB;
		ActiveItemVisuals.Add(Item.UniqueID, Entry);
	}

	// Remove visuals for items that are no longer visible or no longer exist
	TArray<FGuid> ToRemove;
	for (auto& Pair : ActiveItemVisuals)
	{
		if (!VisibleItemIDs.Contains(Pair.Key))
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGuid& ID : ToRemove)
	{
		FItemVisualEntry& Entry = ActiveItemVisuals[ID];
		if (Entry.Visual)
		{
			Entry.Visual->RemoveFromParent();
		}
		ActiveItemVisuals.Remove(ID);
	}
}

void UGridInventoryWidget::ClearItemVisuals()
{
	for (auto& Pair : ActiveItemVisuals)
	{
		if (Pair.Value.Visual) Pair.Value.Visual->RemoveFromParent();
	}
	ActiveItemVisuals.Empty();
}

void UGridInventoryWidget::CreateGridLines()
{
	if (!bShowGridLines || !InventoryComponent || !GridCanvas)
	{
		ClearGridLines();
		return;
	}

	const int32 GW = InventoryComponent->GridWidth;
	const int32 GH = InventoryComponent->GridHeight;

	// Skip rebuild if dimensions haven't changed — avoids destroying/recreating 160+ Image widgets
	if (GW == CachedGridLineWidth && GH == CachedGridLineHeight && GridLineWidgets.Num() > 0)
	{
		return;
	}

	ClearGridLines();
	CachedGridLineWidth = GW;
	CachedGridLineHeight = GH;
	const float TotalW = GW * CellSize;
	const float TotalH = GH * CellSize;

	// Vertical lines
	for (int32 X = 0; X <= GW; ++X)
	{
		UImage* Line = WidgetTree->ConstructWidget<UImage>();
		Line->SetBrushSize(FVector2D(GridLineThickness, TotalH));
		Line->SetBrushTintColor(FSlateColor(GridLineColor));
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		GridCanvas->AddChild(Line);

		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Line->Slot))
		{
			CS->SetAutoSize(true);
			CS->SetPosition(FVector2D(X * CellSize - GridLineThickness * 0.5f, 0.0f));
		}

		GridLineWidgets.Add(Line);
	}

	// Horizontal lines
	for (int32 Y = 0; Y <= GH; ++Y)
	{
		UImage* Line = WidgetTree->ConstructWidget<UImage>();
		Line->SetBrushSize(FVector2D(TotalW, GridLineThickness));
		Line->SetBrushTintColor(FSlateColor(GridLineColor));
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		GridCanvas->AddChild(Line);

		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Line->Slot))
		{
			CS->SetAutoSize(true);
			CS->SetPosition(FVector2D(0.0f, Y * CellSize - GridLineThickness * 0.5f));
		}

		GridLineWidgets.Add(Line);
	}
}

void UGridInventoryWidget::ClearGridLines()
{
	for (UWidget* W : GridLineWidgets)
	{
		if (W) W->RemoveFromParent();
	}
	GridLineWidgets.Empty();
}

void UGridInventoryWidget::CloseContextMenu()
{
	if (ActiveContextMenu)
	{
		ActiveContextMenu->RemoveFromParent();
		ActiveContextMenu = nullptr;
	}
}

// ============================================================================
// Async Icon Preloading
// ============================================================================

void UGridInventoryWidget::PreloadVisibleIcons()
{
	if (!InventoryComponent) return;

	TArray<FSoftObjectPath> PathsToLoad;

	for (const FInventoryItemInstance& Item : InventoryComponent->GetAllItems())
	{
		if (!Item.IsValid() || !Item.ItemDef || Item.ItemDef->Icon.IsNull()) continue;

		// Only preload icons for items overlapping the visible viewport
		// This avoids loading textures for hundreds of off-screen items
		const FIntPoint Pos = Item.GridPosition;
		const FIntPoint Size = Item.GetEffectiveSize();
		if (Pos.X + Size.X <= VisibleMin.X || Pos.X >= VisibleMax.X) continue;
		if (Pos.Y + Size.Y <= VisibleMin.Y || Pos.Y >= VisibleMax.Y) continue;

		const FSoftObjectPath Path = Item.ItemDef->Icon.ToSoftObjectPath();

		// Skip already cached — O(1) hash lookup
		if (IconCache.Contains(Path)) continue;

		// Check if already loaded in memory (from another source)
		UTexture2D* AlreadyLoaded = Item.ItemDef->Icon.Get();
		if (AlreadyLoaded)
		{
			IconCache.Add(Path, AlreadyLoaded);
			continue;
		}

		PathsToLoad.AddUnique(Path);
	}

	if (PathsToLoad.Num() == 0) return;

	// Cancel previous in-flight load if any
	if (IconStreamHandle.IsValid() && IconStreamHandle->IsActive())
	{
		IconStreamHandle->CancelHandle();
	}

	TWeakObjectPtr<UGridInventoryWidget> WeakThis(this);

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	IconStreamHandle = StreamableManager.RequestAsyncLoad(PathsToLoad,
		FStreamableDelegate::CreateLambda([WeakThis, PathsToLoad]()
		{
			if (!WeakThis.IsValid()) return;

			// Cache all newly loaded textures
			for (const FSoftObjectPath& Path : PathsToLoad)
			{
				UTexture2D* Loaded = Cast<UTexture2D>(Path.ResolveObject());
				if (Loaded)
				{
					WeakThis->IconCache.Add(Path, Loaded);
				}
			}

			// Trigger a visual refresh now that icons are available
			WeakThis->RefreshVisibleItems();
		})
	);
}

UTexture2D* UGridInventoryWidget::GetCachedIcon(const TSoftObjectPtr<UTexture2D>& SoftIcon) const
{
	if (SoftIcon.IsNull()) return nullptr;

	const FSoftObjectPath Path = SoftIcon.ToSoftObjectPath();

	// Check cache first
	const UTexture2D* const* Found = IconCache.Find(Path);
	if (Found && *Found)
	{
		return const_cast<UTexture2D*>(*Found);
	}

	// If already loaded in memory (from another source), use it
	UTexture2D* InMemory = SoftIcon.Get();
	if (InMemory)
	{
		// Cache for future use (const_cast safe here since we're populating a mutable cache)
		const_cast<UGridInventoryWidget*>(this)->IconCache.Add(Path, InMemory);
		return InMemory;
	}

	return nullptr;
}

UClass* UGridInventoryWidget::GetSlotWidgetClass()
{
	if (ResolvedSlotWidgetClass) return ResolvedSlotWidgetClass;

	if (!SlotWidgetClass.IsNull())
	{
		// Try to get already-loaded class first (no blocking)
		ResolvedSlotWidgetClass = SlotWidgetClass.Get();
		if (!ResolvedSlotWidgetClass)
		{
			// Synchronous load only on first access — subsequent calls use cached pointer
			ResolvedSlotWidgetClass = SlotWidgetClass.LoadSynchronous();
		}
	}
	return ResolvedSlotWidgetClass;
}

void UGridInventoryWidget::OnInventoryChanged()
{
	// Preload icons for any new items asynchronously
	PreloadVisibleIcons();
	RefreshItems();
}

void UGridInventoryWidget::UnbindFromInventory()
{
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UGridInventoryWidget::OnInventoryChanged);
	}
}
