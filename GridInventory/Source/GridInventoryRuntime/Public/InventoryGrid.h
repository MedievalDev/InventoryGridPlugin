// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemInstance.h"
#include "InventoryGrid.generated.h"

/**
 * Cached free rectangle in the grid.
 * Used to speed up FindFirstFreeSlot - instead of scanning all cells,
 * we iterate over known free regions.
 */
struct FFreeRect
{
	FIntPoint Position;
	FIntPoint Size;

	int32 Area() const { return Size.X * Size.Y; }

	bool Contains(FIntPoint Pos) const
	{
		return Pos.X >= Position.X && Pos.X < Position.X + Size.X
			&& Pos.Y >= Position.Y && Pos.Y < Position.Y + Size.Y;
	}

	bool CanFit(FIntPoint ItemSize) const
	{
		return ItemSize.X <= Size.X && ItemSize.Y <= Size.Y;
	}

	bool CanFitRotated(FIntPoint ItemSize) const
	{
		return ItemSize.Y <= Size.X && ItemSize.X <= Size.Y;
	}
};

USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FGridPlacementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	bool bSuccess;

	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FIntPoint Position;

	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	bool bRotated;

	FGridPlacementResult() : bSuccess(false), Position(FIntPoint::ZeroValue), bRotated(false) {}

	static FGridPlacementResult Failed() { return FGridPlacementResult(); }
	static FGridPlacementResult Success(FIntPoint InPos, bool InRotated)
	{
		FGridPlacementResult R;
		R.bSuccess = true;
		R.Position = InPos;
		R.bRotated = InRotated;
		return R;
	}
};

/** Request for batch-adding items */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FItemAddRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	TSoftObjectPtr<UInventoryItemDefinition> ItemDef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 Count;

	FItemAddRequest() : Count(1) {}
	FItemAddRequest(UInventoryItemDefinition* InDef, int32 InCount) : ItemDef(InDef), Count(InCount) {}
};

/**
 * High-performance 2D grid data structure.
 *
 * Three-layer architecture for optimal performance on large grids (up to 60x80):
 * 1. Cell Array     - FGuid per cell, identifies which item occupies it
 * 2. Occupancy Bitmap - 1 bit per cell, ultra-fast "is free?" checks (64 cells per uint64)
 * 3. Free-Rect Cache  - Sorted list of free rectangles, makes FindFirstFreeSlot near-instant
 *
 * A 60x80 grid = 4800 cells:
 *   - Bitmap: 75 uint64s = 600 bytes (vs 76.8 KB for FGuid array alone)
 *   - Scanning bitmap for free cells: ~75 comparisons vs ~4800 with FGuid checks
 */
USTRUCT(BlueprintType)
struct GRIDINVENTORYRUNTIME_API FInventoryGrid
{
	GENERATED_BODY()

	FInventoryGrid();

	void Initialize(int32 InWidth, int32 InHeight);

	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }
	int32 GetTotalCells() const { return Width * Height; }
	int32 GetFreeCellCount() const { return FreeCellCount; }
	int32 GetOccupiedCellCount() const { return (Width * Height) - FreeCellCount; }

	bool IsValidPosition(FIntPoint Position) const;
	bool IsCellFree(FIntPoint Position) const;
	bool IsCellFreeFast(int32 Index) const;

	/** Check if an item can be placed at the given position */
	bool CanPlaceAt(FIntPoint Position, FIntPoint ItemSize) const;
	/** Check placement ignoring a specific item (for move operations) */
	bool CanPlaceAtIgnoring(FIntPoint Position, FIntPoint ItemSize, const FGuid& IgnoreID) const;

	/** Place an item on the grid */
	bool PlaceItem(const FGuid& ItemID, FIntPoint Position, FIntPoint ItemSize);
	/** Remove an item from the grid (scans all cells — use RemoveItemAt for O(1)) */
	bool RemoveItem(const FGuid& ItemID);
	/** Remove using known position+size — O(item_area) instead of O(total_cells) */
	bool RemoveItemAt(const FGuid& ItemID, FIntPoint Position, FIntPoint ItemSize);

	/** Get the item ID at a specific cell */
	FGuid GetItemAt(FIntPoint Position) const;

	/**
	 * Find the first free slot for an item.
	 * Uses Free-Rect Cache first (near instant), falls back to bitmap scan.
	 */
	FGridPlacementResult FindFirstFreeSlot(FIntPoint ItemSize, bool bAllowRotation) const;

	/**
	 * Find free slots for multiple items at once (batch operation).
	 * More efficient than calling FindFirstFreeSlot repeatedly because
	 * it updates a temporary grid state as it "places" items.
	 */
	TArray<FGridPlacementResult> FindSlotsForBatch(const TArray<FItemAddRequest>& Requests) const;

	void Clear();
	TArray<FGuid> GetAllItemIDs() const;

	/** Force rebuild of the free-rect cache (called automatically, but exposed for edge cases) */
	void RebuildFreeRectCache();

private:
	UPROPERTY()
	int32 Width;

	UPROPERTY()
	int32 Height;

	// Layer 1: Cell ownership (which item occupies each cell)
	UPROPERTY()
	TArray<FGuid> Cells;

	// Layer 2: Occupancy bitmap (1 bit per cell, 0 = free, 1 = occupied)
	TArray<uint64> OccupancyBitmap;
	int32 BitmapSize; // Number of uint64s

	// Layer 3: Free rectangle cache
	TArray<FFreeRect> FreeRects;
	bool bFreeRectsDirty;

	// Stats
	int32 FreeCellCount;

	// ---- Internal helpers ----
	int32 PosToIndex(FIntPoint Pos) const { return Pos.Y * Width + Pos.X; }
	bool IsAreaInBounds(FIntPoint Pos, FIntPoint Size) const;

	// Bitmap operations
	void SetBit(int32 Index);
	void ClearBit(int32 Index);
	bool GetBit(int32 Index) const;
	void SetAreaBits(FIntPoint Pos, FIntPoint Size, bool bOccupied);

	// Fast area check using bitmap
	bool IsAreaFreeBitmap(FIntPoint Pos, FIntPoint Size) const;
	bool IsAreaFreeIgnoring(FIntPoint Pos, FIntPoint Size, const FGuid& IgnoreID) const;

	// Free-rect cache management
	void EnsureFreeRectsValid() const;
	void BuildFreeRects() const;
	FGridPlacementResult FindInFreeRects(FIntPoint ItemSize, bool bAllowRotation) const;

	// Bitmap scan fallback
	FGridPlacementResult ScanBitmapForSlot(FIntPoint ItemSize, bool bAllowRotation) const;
};
