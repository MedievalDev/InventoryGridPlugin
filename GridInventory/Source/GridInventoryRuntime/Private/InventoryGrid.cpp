// Copyright Marco. All Rights Reserved.

#include "InventoryGrid.h"

FInventoryGrid::FInventoryGrid()
	: Width(0)
	, Height(0)
	, BitmapSize(0)
	, bFreeRectsDirty(true)
	, FreeCellCount(0)
{
}

void FInventoryGrid::Initialize(int32 InWidth, int32 InHeight)
{
	Width = FMath::Max(1, InWidth);
	Height = FMath::Max(1, InHeight);

	const int32 TotalCells = Width * Height;

	// Layer 1: Cell array
	Cells.Init(FGuid(), TotalCells);

	// Layer 2: Bitmap (64 cells per uint64)
	BitmapSize = (TotalCells + 63) / 64;
	OccupancyBitmap.Init(0, BitmapSize);

	// Layer 3: Free rects
	FreeRects.Empty();
	bFreeRectsDirty = true;

	FreeCellCount = TotalCells;
}

// ============================================================================
// Basic Queries
// ============================================================================

bool FInventoryGrid::IsValidPosition(FIntPoint Pos) const
{
	return Pos.X >= 0 && Pos.X < Width && Pos.Y >= 0 && Pos.Y < Height;
}

bool FInventoryGrid::IsAreaInBounds(FIntPoint Pos, FIntPoint Size) const
{
	return Pos.X >= 0 && Pos.Y >= 0
		&& (Pos.X + Size.X) <= Width
		&& (Pos.Y + Size.Y) <= Height;
}

bool FInventoryGrid::IsCellFree(FIntPoint Pos) const
{
	if (!IsValidPosition(Pos)) return false;
	return !GetBit(PosToIndex(Pos));
}

bool FInventoryGrid::IsCellFreeFast(int32 Index) const
{
	return !GetBit(Index);
}

FGuid FInventoryGrid::GetItemAt(FIntPoint Pos) const
{
	if (!IsValidPosition(Pos)) return FGuid();
	return Cells[PosToIndex(Pos)];
}

// ============================================================================
// Bitmap Operations - Core performance layer
// ============================================================================

void FInventoryGrid::SetBit(int32 Index)
{
	const int32 Word = Index >> 6;       // Index / 64
	const int32 Bit = Index & 63;        // Index % 64
	OccupancyBitmap[Word] |= (1ULL << Bit);
}

void FInventoryGrid::ClearBit(int32 Index)
{
	const int32 Word = Index >> 6;
	const int32 Bit = Index & 63;
	OccupancyBitmap[Word] &= ~(1ULL << Bit);
}

bool FInventoryGrid::GetBit(int32 Index) const
{
	const int32 Word = Index >> 6;
	const int32 Bit = Index & 63;
	return (OccupancyBitmap[Word] & (1ULL << Bit)) != 0;
}

void FInventoryGrid::SetAreaBits(FIntPoint Pos, FIntPoint Size, bool bOccupied)
{
	for (int32 Y = Pos.Y; Y < Pos.Y + Size.Y; ++Y)
	{
		for (int32 X = Pos.X; X < Pos.X + Size.X; ++X)
		{
			const int32 Idx = Y * Width + X;
			if (bOccupied)
			{
				SetBit(Idx);
			}
			else
			{
				ClearBit(Idx);
			}
		}
	}
}

bool FInventoryGrid::IsAreaFreeBitmap(FIntPoint Pos, FIntPoint Size) const
{
	// For small areas, check bit by bit
	// For rows that align to word boundaries, we could do word-level checks,
	// but for typical item sizes (< 10 wide) bit-by-bit on bitmap is already very fast.
	for (int32 Y = Pos.Y; Y < Pos.Y + Size.Y; ++Y)
	{
		for (int32 X = Pos.X; X < Pos.X + Size.X; ++X)
		{
			if (GetBit(Y * Width + X))
			{
				return false; // Cell occupied
			}
		}
	}
	return true;
}

bool FInventoryGrid::IsAreaFreeIgnoring(FIntPoint Pos, FIntPoint Size, const FGuid& IgnoreID) const
{
	for (int32 Y = Pos.Y; Y < Pos.Y + Size.Y; ++Y)
	{
		for (int32 X = Pos.X; X < Pos.X + Size.X; ++X)
		{
			const int32 Idx = Y * Width + X;
			if (GetBit(Idx) && Cells[Idx] != IgnoreID)
			{
				return false;
			}
		}
	}
	return true;
}

// ============================================================================
// Placement Checks
// ============================================================================

bool FInventoryGrid::CanPlaceAt(FIntPoint Pos, FIntPoint ItemSize) const
{
	if (!IsAreaInBounds(Pos, ItemSize)) return false;
	return IsAreaFreeBitmap(Pos, ItemSize);
}

bool FInventoryGrid::CanPlaceAtIgnoring(FIntPoint Pos, FIntPoint ItemSize, const FGuid& IgnoreID) const
{
	if (!IsAreaInBounds(Pos, ItemSize)) return false;
	return IsAreaFreeIgnoring(Pos, ItemSize, IgnoreID);
}

// ============================================================================
// Place / Remove
// ============================================================================

bool FInventoryGrid::PlaceItem(const FGuid& ItemID, FIntPoint Pos, FIntPoint ItemSize)
{
	if (!CanPlaceAt(Pos, ItemSize)) return false;

	const int32 CellCount = ItemSize.X * ItemSize.Y;

	for (int32 Y = Pos.Y; Y < Pos.Y + ItemSize.Y; ++Y)
	{
		for (int32 X = Pos.X; X < Pos.X + ItemSize.X; ++X)
		{
			const int32 Idx = Y * Width + X;
			Cells[Idx] = ItemID;
			SetBit(Idx);
		}
	}

	FreeCellCount -= CellCount;
	bFreeRectsDirty = true;
	return true;
}

bool FInventoryGrid::RemoveItem(const FGuid& ItemID)
{
	if (!ItemID.IsValid()) return false;

	bool bFound = false;
	int32 Removed = 0;

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i] == ItemID)
		{
			Cells[i].Invalidate();
			ClearBit(i);
			bFound = true;
			++Removed;
		}
	}

	if (bFound)
	{
		FreeCellCount += Removed;
		bFreeRectsDirty = true;
	}

	return bFound;
}

// ============================================================================
// Free-Rect Cache
// ============================================================================

void FInventoryGrid::EnsureFreeRectsValid() const
{
	if (bFreeRectsDirty)
	{
		BuildFreeRects();
	}
}

void FInventoryGrid::BuildFreeRects() const
{
	// Mutable because this is a lazy cache
	FInventoryGrid* MutableThis = const_cast<FInventoryGrid*>(this);
	MutableThis->FreeRects.Empty();

	// Maximal free rectangle algorithm using histogram approach
	// For each row, compute the height of consecutive free cells above (including current row)
	TArray<int32> Heights;
	Heights.Init(0, Width);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		// Update heights
		for (int32 X = 0; X < Width; ++X)
		{
			if (!GetBit(Y * Width + X))
			{
				Heights[X]++;
			}
			else
			{
				Heights[X] = 0;
			}
		}

		// Use a stack-based approach to find maximal rectangles in this histogram row
		// We extract rectangles that are at least 1x1 and useful for item placement
		TArray<int32> Stack; // Stack of X indices

		for (int32 X = 0; X <= Width; ++X)
		{
			const int32 CurrentHeight = (X < Width) ? Heights[X] : 0;

			while (Stack.Num() > 0 && Heights[Stack.Last()] > CurrentHeight)
			{
				const int32 H = Heights[Stack.Pop()];
				const int32 W = Stack.Num() > 0 ? (X - Stack.Last() - 1) : X;

				// Only add rects that could fit at least a 1x1 item
				if (W > 0 && H > 0)
				{
					FFreeRect Rect;
					Rect.Size = FIntPoint(W, H);
					Rect.Position = FIntPoint(Stack.Num() > 0 ? Stack.Last() + 1 : 0, Y - H + 1);

					// Avoid duplicates and tiny fragments - only add if area >= 1
					MutableThis->FreeRects.Add(Rect);
				}
			}

			Stack.Add(X);
		}
	}

	// Sort by position (top-left first) for predictable placement
	MutableThis->FreeRects.Sort([](const FFreeRect& A, const FFreeRect& B)
	{
		if (A.Position.Y != B.Position.Y) return A.Position.Y < B.Position.Y;
		return A.Position.X < B.Position.X;
	});

	MutableThis->bFreeRectsDirty = false;
}

FGridPlacementResult FInventoryGrid::FindInFreeRects(FIntPoint ItemSize, bool bAllowRotation) const
{
	EnsureFreeRectsValid();

	// Search free rects for a fit (normal orientation)
	for (const FFreeRect& Rect : FreeRects)
	{
		if (Rect.CanFit(ItemSize))
		{
			// Verify with bitmap (free rects may be stale in edge cases)
			if (IsAreaFreeBitmap(Rect.Position, ItemSize))
			{
				return FGridPlacementResult::Success(Rect.Position, false);
			}
		}
	}

	// Try rotated
	if (bAllowRotation && ItemSize.X != ItemSize.Y)
	{
		const FIntPoint Rotated(ItemSize.Y, ItemSize.X);
		for (const FFreeRect& Rect : FreeRects)
		{
			if (Rect.CanFit(Rotated))
			{
				if (IsAreaFreeBitmap(Rect.Position, Rotated))
				{
					return FGridPlacementResult::Success(Rect.Position, true);
				}
			}
		}
	}

	return FGridPlacementResult::Failed();
}

// ============================================================================
// Bitmap Scan Fallback
// ============================================================================

FGridPlacementResult FInventoryGrid::ScanBitmapForSlot(FIntPoint ItemSize, bool bAllowRotation) const
{
	// Brute-force scan using bitmap (still much faster than FGuid comparison)
	const int32 MaxX = Width - ItemSize.X;
	const int32 MaxY = Height - ItemSize.Y;

	for (int32 Y = 0; Y <= MaxY; ++Y)
	{
		for (int32 X = 0; X <= MaxX; ++X)
		{
			// Quick reject: check first cell via bitmap
			if (GetBit(Y * Width + X)) continue;

			if (IsAreaFreeBitmap(FIntPoint(X, Y), ItemSize))
			{
				return FGridPlacementResult::Success(FIntPoint(X, Y), false);
			}
		}
	}

	// Try rotated
	if (bAllowRotation && ItemSize.X != ItemSize.Y)
	{
		const FIntPoint Rotated(ItemSize.Y, ItemSize.X);
		const int32 MaxXR = Width - Rotated.X;
		const int32 MaxYR = Height - Rotated.Y;

		for (int32 Y = 0; Y <= MaxYR; ++Y)
		{
			for (int32 X = 0; X <= MaxXR; ++X)
			{
				if (GetBit(Y * Width + X)) continue;

				if (IsAreaFreeBitmap(FIntPoint(X, Y), Rotated))
				{
					return FGridPlacementResult::Success(FIntPoint(X, Y), true);
				}
			}
		}
	}

	return FGridPlacementResult::Failed();
}

// ============================================================================
// Public FindFirstFreeSlot - Two-phase approach
// ============================================================================

FGridPlacementResult FInventoryGrid::FindFirstFreeSlot(FIntPoint ItemSize, bool bAllowRotation) const
{
	// Quick exit: not enough free cells
	const int32 NeededCells = ItemSize.X * ItemSize.Y;
	if (FreeCellCount < NeededCells)
	{
		return FGridPlacementResult::Failed();
	}

	// Phase 1: Try free-rect cache (near instant)
	FGridPlacementResult Result = FindInFreeRects(ItemSize, bAllowRotation);
	if (Result.bSuccess)
	{
		return Result;
	}

	// Phase 2: Bitmap scan fallback (cache might be stale or fragmented)
	return ScanBitmapForSlot(ItemSize, bAllowRotation);
}

// ============================================================================
// Batch Operations
// ============================================================================

TArray<FGridPlacementResult> FInventoryGrid::FindSlotsForBatch(const TArray<FItemAddRequest>& Requests) const
{
	TArray<FGridPlacementResult> Results;
	Results.Reserve(Requests.Num());

	// Create a temporary copy of the grid for simulation
	FInventoryGrid TempGrid = *this;

	for (const FItemAddRequest& Req : Requests)
	{
		if (!Req.ItemDef)
		{
			Results.Add(FGridPlacementResult::Failed());
			continue;
		}

		FGridPlacementResult Slot = TempGrid.FindFirstFreeSlot(
			Req.ItemDef->GridSize,
			Req.ItemDef->bCanRotate
		);

		if (Slot.bSuccess)
		{
			// "Place" in temp grid so next items account for this placement
			const FIntPoint EffSize = Req.ItemDef->GetEffectiveSize(Slot.bRotated);
			TempGrid.PlaceItem(FGuid::NewGuid(), Slot.Position, EffSize);
		}

		Results.Add(Slot);
	}

	return Results;
}

// ============================================================================
// Utility
// ============================================================================

void FInventoryGrid::Clear()
{
	for (FGuid& Cell : Cells)
	{
		Cell.Invalidate();
	}
	OccupancyBitmap.Init(0, BitmapSize);
	FreeCellCount = Width * Height;
	bFreeRectsDirty = true;
}

TArray<FGuid> FInventoryGrid::GetAllItemIDs() const
{
	TSet<FGuid> UniqueIDs;
	for (const FGuid& Cell : Cells)
	{
		if (Cell.IsValid())
		{
			UniqueIDs.Add(Cell);
		}
	}
	return UniqueIDs.Array();
}

void FInventoryGrid::RebuildFreeRectCache()
{
	bFreeRectsDirty = true;
	EnsureFreeRectsValid();
}
