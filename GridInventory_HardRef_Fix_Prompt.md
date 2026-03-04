# GridInventory Hard Reference Fix — Claude Code Aufgabe

## Kontext

Das GridInventory Plugin für UE 4.27 hat kritische Hard References auf `UInventoryItemDefinition*` die dazu führen, dass beim Level-Load ALLE Item-DataAssets in den RAM gezogen werden. Bei 160+ Items verursacht das unnötigen Speicherverbrauch.

**Ziel:** Alle gespeicherten (UPROPERTY) Hard References auf `UInventoryItemDefinition*` durch `TSoftObjectPtr<UInventoryItemDefinition>` ersetzen. Funktions-Parameter bleiben als Raw-Pointer (sind nur temporär auf dem Stack).

---

## UE 4.27 Kompatibilitäts-Regeln (IMMER beachten!)

- Kein `GetFont()` auf UTextBlock → nutze `TextBlock->Font`
- Kein `FSavePackageArgs` → alte `SavePackage()` Signatur
- `CreatePackage(nullptr, *Name)` mit nullptr
- `FConsoleCommandWithArgsDelegate` statt `WithWorldAndArgs`
- Variable nie `Slot` nennen in UWidget-Subklassen
- `TryGetNumberField()` gibt `double` zurück
- Kein `#include "UObject/SavePackage.h"`

---

## Aufgaben (in dieser Reihenfolge!)

### Aufgabe 1: FRandomItemEntry::ItemDef → Soft (HÖCHSTE PRIORITÄT)

**Datei:** `Source/GridInventoryRuntime/Public/RandomItemEntry.h`

**Änderung:**
```cpp
// VORHER (Zeile 28):
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item")
UInventoryItemDefinition* ItemDef;

// NACHHER:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Item")
TSoftObjectPtr<UInventoryItemDefinition> ItemDef;
```

**Konstruktoren anpassen:**
```cpp
// Default-Konstruktor: ItemDef wird automatisch null-initialisiert (TSoftObjectPtr default ist null)

// Parametrisierter Konstruktor:
FRandomItemEntry(UInventoryItemDefinition* InDef, int32 InMin, int32 InMax, float InChance)
    : ItemDef(InDef)  // TSoftObjectPtr kann aus Raw-Pointer konstruiert werden
    , MinCount(InMin)
    , MaxCount(InMax)
    , SpawnChance(InChance)
{
}
```

**Betroffener Code der angepasst werden muss:**

1. `Source/GridInventoryRuntime/Private/InventoryContainer.cpp` — `BeginPlay()` und `GenerateRandomDefaults()`: Überall wo `Entry.ItemDef` direkt als Pointer verwendet wird, muss `Entry.ItemDef.LoadSynchronous()` oder ein Null-Check mit `Entry.ItemDef.IsNull()` verwendet werden.

2. `Source/GridInventoryRuntime/Private/MerchantActor.cpp` — `GenerateRandomStock()`: Gleiche Anpassung.

**Muster für Anpassung:**
```cpp
// VORHER:
if (Entry.ItemDef)
{
    Inventory->TryAddItem(Entry.ItemDef, Count);
}

// NACHHER:
if (!Entry.ItemDef.IsNull())
{
    UInventoryItemDefinition* LoadedDef = Entry.ItemDef.LoadSynchronous();
    if (LoadedDef)
    {
        Inventory->TryAddItem(LoadedDef, Count);
    }
}
```

---

### Aufgabe 2: FItemAddRequest::ItemDef → Soft

**Datei:** `Source/GridInventoryRuntime/Public/InventoryGrid.h`

**Finde die Struct `FItemAddRequest`** (Zeile 67-78) und ändere:

```cpp
// VORHER:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Add Request")
UInventoryItemDefinition* ItemDef;

// NACHHER:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Add Request")
TSoftObjectPtr<UInventoryItemDefinition> ItemDef;
```

**Konstruktoren anpassen:**
```cpp
// Default: TSoftObjectPtr ist standardmäßig null, also:
FItemAddRequest() : Count(1) {}  // ItemDef braucht kein nullptr mehr

// Parametrisiert — TSoftObjectPtr kann aus Raw-Pointer konstruiert werden:
FItemAddRequest(UInventoryItemDefinition* InDef, int32 InCount)
    : ItemDef(InDef), Count(InCount) {}
```

**Betroffener Code:**

1. `GridInventoryComponent.cpp` — `TryAddItemsBatch()` (~Zeile 150-210): Iteriert über `TArray<FItemAddRequest>`. Jeder Zugriff auf `Req.ItemDef` muss zu `Req.ItemDef.LoadSynchronous()` werden. **WICHTIG:** Diese Funktion hat ~15 Zugriffe auf `Req.ItemDef`. Am besten am Schleifenanfang einmalig laden:

```cpp
// Am Anfang der Schleife:
UInventoryItemDefinition* Def = Req.ItemDef.LoadSynchronous();
if (!Def || Req.Count <= 0) continue;

// Dann überall Def statt Req.ItemDef verwenden:
if (Def->bIsCurrency) { ... }
if (Def->bCanStack) { ... }
```

2. `InventoryContainer.cpp` — `BeginPlay()` wo `DefaultItems` iteriert wird.

3. `MerchantActor.cpp` — `RestockMerchant()` und `BeginPlay()` wo `DefaultStock` iteriert wird.

**WICHTIG:** `FItemAddRequest` wird auch intern erstellt (z.B. in `TryAddItemsBatch` Zeile ~180: `NeedPlacement.Add(FItemAddRequest(Req.ItemDef, Chunk))`). Da wird ein bereits geladener Def-Pointer übergeben. Das muss zu `FItemAddRequest(Def, Chunk)` werden (Def ist der lokale Raw-Pointer vom `LoadSynchronous()` oben).

---

### Aufgabe 3: AInventoryContainer::LootTable → Soft

**Datei:** `Source/GridInventoryRuntime/Public/InventoryContainer.h`

```cpp
// VORHER:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Loot")
ULootTable* LootTable;

// NACHHER:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container|Loot")
TSoftObjectPtr<ULootTable> LootTable;
```

**Betroffener Code in `InventoryContainer.cpp`:**

Überall wo `LootTable` verwendet wird (z.B. in `GenerateLoot()`, `GenerateLootAsync()`, `TryOpen()`):

```cpp
// VORHER:
if (LootTable)
{
    TArray<FLootResult> Results = LootTable->GenerateLoot(Level, DropWeightMultiplier);
}

// NACHHER:
if (!LootTable.IsNull())
{
    ULootTable* LoadedTable = LootTable.LoadSynchronous();
    if (LoadedTable)
    {
        TArray<FLootResult> Results = LoadedTable->GenerateLoot(Level, DropWeightMultiplier);
    }
}
```

---

### Aufgabe 4: FInventoryItemInstance::ItemDef → Soft mit Cache

**Datei:** `Source/GridInventoryRuntime/Public/InventoryItemInstance.h`

Das ist die komplexeste Änderung weil `ItemDef` überall im Codebase verwendet wird.

**Änderung im Header:**
```cpp
// VORHER:
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Instance")
UInventoryItemDefinition* ItemDef;

// NACHHER:
UPROPERTY(BlueprintReadOnly, Category = "Item Instance")
TSoftObjectPtr<UInventoryItemDefinition> ItemDefSoft;

// Getter — gibt gecachten oder frisch geladenen Pointer zurück
UInventoryItemDefinition* GetItemDef() const;

// Für Blueprint-Kompatibilität
UFUNCTION(BlueprintPure, Category = "Item Instance", meta = (DisplayName = "Get Item Definition"))
UInventoryItemDefinition* BP_GetItemDef() const { return GetItemDef(); }
```

**Implementierung in `InventoryItemInstance.cpp`:**
```cpp
UInventoryItemDefinition* FInventoryItemInstance::GetItemDef() const
{
    if (ItemDefSoft.IsNull())
    {
        return nullptr;
    }
    // LoadSynchronous() returned cached wenn bereits geladen
    return ItemDefSoft.LoadSynchronous();
}
```

**KRITISCH — Alle Stellen wo `Item.ItemDef` oder `Item.ItemDef->` verwendet wird müssen zu `Item.GetItemDef()` geändert werden!**

**Scope der Änderungen (`.ItemDef` Vorkommen pro Datei):**

```
GridInventoryComponent.cpp      58 Stellen
GridInventoryWidget.cpp         35 Stellen
InventoryFunctionLibrary.cpp    24 Stellen
InventoryDebugSubsystem.cpp     19 Stellen
InventoryContainer.cpp          10 Stellen
ItemContainerInventory.cpp      10 Stellen
EquipmentComponent.cpp           9 Stellen
MerchantActor.cpp                4 Stellen
InventoryItemInstance.cpp        2 Stellen
                               ───
                         TOTAL ~171 Stellen
```

Das betrifft VIELE Dateien. Suche mit grep nach `\.ItemDef` und `->ItemDef` in allen .h und .cpp Dateien.

- `GridInventoryComponent.cpp` (häufigste Verwendung)
- `InventoryItemInstance.cpp` (eigene Member-Funktionen wie GetEffectiveSize, GetTotalWeight etc.)
- `EquipmentComponent.cpp`
- `InventoryContainer.cpp`
- `ItemContainerInventory.cpp`
- `LootTable.cpp`
- `InventoryDebugSubsystem.cpp`
- `InventoryFunctionLibrary.cpp`
- `GridInventoryWidget.cpp`
- `InventorySlotWidget.cpp`
- `TradePanelWidget.cpp`
- `ContainerPanelWidget.cpp`
- `EquipmentPanelWidget.cpp`
- `InventoryJsonImporter.cpp`
- `InventorySaveGame.cpp` / `InventorySaveGame.h`

**Muster für Anpassung:**
```cpp
// VORHER:
if (Item.ItemDef)
{
    FIntPoint Size = Item.ItemDef->GetEffectiveSize(Item.bIsRotated);
}

// NACHHER:
UInventoryItemDefinition* Def = Item.GetItemDef();
if (Def)
{
    FIntPoint Size = Def->GetEffectiveSize(Item.bIsRotated);
}
```

**CreateNew() anpassen:**
```cpp
// VORHER:
FInventoryItemInstance FInventoryItemInstance::CreateNew(UInventoryItemDefinition* InItemDef, int32 InStackCount)
{
    FInventoryItemInstance Inst;
    Inst.ItemDef = InItemDef;
    // ...
}

// NACHHER:
FInventoryItemInstance FInventoryItemInstance::CreateNew(UInventoryItemDefinition* InItemDef, int32 InStackCount)
{
    FInventoryItemInstance Inst;
    Inst.ItemDefSoft = InItemDef;  // TSoftObjectPtr kann aus Raw-Pointer zugewiesen werden
    // ...
}
```

---

### Aufgabe 5: Save/Load System anpassen

**InventorySaveGame.h** — `FItemSaveEntry` verwendet bereits `FSoftObjectPath ItemDefPath` ✅ — das ist korrekt und muss NICHT geändert werden.

**GridInventoryComponent.cpp** — Die Funktionen `CreateSaveEntry()` (Zeile ~1010) und `RestoreItemFromEntry()` (Zeile ~1039) müssen angepasst werden:

**CreateSaveEntry() anpassen:**
```cpp
// VORHER (Zeile ~1014):
if (Item.ItemDef)
{
    URuntimeItemDefinition* RuntimeDef = Cast<URuntimeItemDefinition>(Item.ItemDef);

// NACHHER:
UInventoryItemDefinition* Def = Item.GetItemDef();
if (Def)
{
    URuntimeItemDefinition* RuntimeDef = Cast<URuntimeItemDefinition>(Def);
```

```cpp
// VORHER (Zeile ~1024):
Entry.ItemDefPath = FSoftObjectPath(Item.ItemDef);

// NACHHER:
Entry.ItemDefPath = Item.ItemDefSoft.ToSoftObjectPath();
```

**RestoreItemFromEntry() anpassen:**
Nach dem Laden des ItemDef über `Entry.ItemDefPath.TryLoad()` muss das Item korrekt erstellt werden. Suche die Stelle wo `Item.ItemDef = ...` gesetzt wird und ändere zu `Item.ItemDefSoft = ...`:

```cpp
// Statt den Raw-Pointer direkt zuzuweisen, den Soft-Pointer setzen:
NewItem.ItemDefSoft = TSoftObjectPtr<UInventoryItemDefinition>(Entry.ItemDefPath);
// oder wenn der geladene Pointer bereits verfügbar ist:
NewItem.ItemDefSoft = ItemDef;  // TSoftObjectPtr kann aus Raw-Pointer zugewiesen werden
```

**Prüfe auch** die Equipment- und Container-Restore Abschnitte (~Zeile 1200-1280) — dort wird ebenfalls `Entry.ItemDefPath.TryLoad()` und `Item.ItemDef = ...` verwendet.

---

## Prüfschritte nach jeder Aufgabe

Nach JEDER Aufgabe:

1. **Kompilier-Check:** Stelle sicher dass es keine Compile-Fehler gibt. Suche nach verbleibenden Referenzen auf das alte Member:
   - Nach Aufgabe 1: `grep -rn "\.ItemDef[^S]" --include="*.cpp" --include="*.h"` in Dateien die FRandomItemEntry verwenden
   - Nach Aufgabe 4: `grep -rn "\.ItemDef[^S]" --include="*.cpp" --include="*.h"` — es darf KEIN `.ItemDef` mehr geben (nur `.ItemDefSoft` und `.GetItemDef()`)

2. **Null-Safety:** Jeder `GetItemDef()` / `LoadSynchronous()` Aufruf MUSS einen Null-Check haben bevor auf den Pointer zugegriffen wird.

3. **Keine neuen Hard-Refs einführen:** Nirgendwo eine neue `UPROPERTY() UInventoryItemDefinition*` erstellen.

---

## Was NICHT geändert werden soll

- **Funktions-Parameter** die `UInventoryItemDefinition*` annehmen (z.B. `TryAddItem(UInventoryItemDefinition* ItemDef, ...)`) bleiben als Raw-Pointer. Das sind temporäre Stack-Variablen, keine gespeicherten Referenzen.
- **FLootResult::ItemDef** bleibt Raw-Pointer — FLootResult ist ein temporäres Return-Struct das nie als UPROPERTY auf einem Actor gespeichert wird.
- **Delegate-Signaturen** die `UInventoryItemDefinition*` übergeben (z.B. `FOnCurrencyCollected`) bleiben — Delegates halten keine persistenten Referenzen.
- **GridInventoryWidget::IconCache** bleibt — das ist ein gewollter Runtime-Cache.

---

## Zusammenfassung der Reihenfolge

1. ✅ `FRandomItemEntry::ItemDef` → `TSoftObjectPtr` + alle Verwendungen
2. ✅ `FItemAddRequest::ItemDef` → `TSoftObjectPtr` + alle Verwendungen
3. ✅ `AInventoryContainer::LootTable` → `TSoftObjectPtr` + alle Verwendungen
4. ✅ `FInventoryItemInstance::ItemDef` → `ItemDefSoft` + `GetItemDef()` + ALLE Verwendungen im gesamten Plugin
5. ✅ Save/Load System → Pfad-basiert statt Pointer

**Nach Abschluss aller 5 Aufgaben:** Finaler grep über das gesamte Plugin nach `UInventoryItemDefinition*` in UPROPERTY-Zeilen. Es darf keine mehr geben (außer in Funktions-Parametern und temporären Variablen).
