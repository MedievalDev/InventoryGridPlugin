// Copyright Marco. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemDefinition.h"
#include "RuntimeItemDefinition.generated.h"

/**
 * Runtime-created item definition for dynamically crafted items (potions, etc.).
 *
 * Inherits ALL properties from UInventoryItemDefinition — same structure,
 * same effects, same grid behavior. The only difference: this object lives
 * in memory only (no DataAsset on disk).
 *
 * Lifecycle:
 * 1. Created via CreateRuntimeItem() on the GridInventoryComponent
 * 2. Stored in RuntimeDefinitions array on the component (GC-safe)
 * 3. Used like any normal ItemDef: TryAddItem, stacking, equipment, etc.
 * 4. On save: serialized to JSON inline in the SaveGame
 * 5. On load: deserialized from JSON, new URuntimeItemDefinition created
 *
 * GC safety: As long as the GridInventoryComponent's RuntimeDefinitions array
 * holds a reference, the definition won't be garbage collected.
 */
UCLASS(BlueprintType)
class GRIDINVENTORYRUNTIME_API URuntimeItemDefinition : public UInventoryItemDefinition
{
	GENERATED_BODY()

public:
	URuntimeItemDefinition();

	/** Marker — always true for runtime items */
	UPROPERTY(BlueprintReadOnly, Category = "Runtime Item")
	bool bIsRuntimeCreated;

	/** Which ingredients were used to create this (for tooltip/recipe tracking) */
	UPROPERTY(BlueprintReadWrite, Category = "Runtime Item")
	TArray<FName> SourceIngredients;

	/**
	 * Serialize all properties to a JSON string.
	 * Used by the save system to persist runtime items inline.
	 */
	UFUNCTION(BlueprintPure, Category = "Runtime Item")
	FString ToJSON() const;

	/**
	 * Create a new URuntimeItemDefinition from a JSON string.
	 * Used by the load system to restore runtime items.
	 * @param Outer UObject to own the new definition (typically the GridInventoryComponent)
	 * @param JSON The JSON string from ToJSON()
	 * @return New definition, or nullptr on parse failure
	 */
	static URuntimeItemDefinition* FromJSON(UObject* Outer, const FString& JSON);
};
