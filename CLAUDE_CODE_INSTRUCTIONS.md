# GridInventory Plugin — Claude Code Anweisungen

## Projekt-Kontext

Dieses Repository enthält das **GridInventory** Plugin für Unreal Engine 4.27.
Es ist ein Grid-basiertes Inventarsystem für das Spiel "Alchimist: Secret of the Worlds" (Medieval Fantasy RPG).

Entwickler: Marco (Alchemy Fox Studio)
Engine: Unreal Engine 4.27 (NICHT UE5!)
Sprache: C++ mit Blueprint-Support
Test-Projekt: "Weapon showcase" (Third Person Template)

---

## Modul-Struktur

```
GridInventory/
  GridInventory.uplugin
  Source/
    GridInventoryRuntime/    ← Spiellogik (Grid, Items, Equipment, Container, Loot)
    GridInventoryUI/         ← Widgets (GridInventoryWidget, InventorySlotWidget, Drag&Drop)
    GridInventoryEditor/     ← JSON-Importer (Konsolen-Command, DataAsset-Erstellung)
```

### GridInventoryRuntime
- `InventoryGrid.h/cpp` — Kern-Grid mit Bitmap-Belegung, FindFreeSlot, Platzierung
- `GridInventoryComponent.h/cpp` — UActorComponent, hält Grid + Items, Replikation
- `InventoryItemDefinition.h` — UDataAsset für Item-Definitionen (Typ, Größe, Gewicht, Effects, Alchemy Properties, SpawnData)
- `InventoryItemInstance.h` — Einzelne Item-Instanz (UID, Stack, Position, Rotation)
- `ItemEffectValue.h` — FItemEffectValue Struct (EffectID + Value)
- `IngredientSpawnData.h` — FIngredientSpawnData Struct (Biome, Licht, Boden)
- `EquipmentComponent.h/cpp` — Equipment-Slots
- `InventoryContainer.h/cpp` — Truhen/Container mit Loot-Generation
- `LootTable.h/cpp` — Loot-Tabellen
- `InventoryBlueprintLibrary.h/cpp` — 20+ Blueprint-Nodes
- `InventoryDebugSubsystem.h/cpp` — Alle Inv.* Konsolen-Commands

### GridInventoryUI
- `GridInventoryWidget.h/cpp` — Virtualisiertes Grid-Widget, Mouse-Handling
- `InventorySlotWidget.h/cpp` — Einzelner Slot (visuell, HitTestInvisible)
- `InventoryDragDropOperation.h` — Drag&Drop Daten
- `EquipmentSlotWidget.h/cpp` — Equipment-Slot Widget

### GridInventoryEditor
- `InventoryJsonImporter.h/cpp` — JSON-Import für 138 Alchemie-Zutaten
- `GridInventoryEditor.h/cpp` — Editor-Modul Startup

---

## Aktueller Status & Offene Probleme

### PROBLEM 1: Maus-Klicks funktionieren nicht auf dem Inventar-UI
**Status:** Zuletzt wurde der Ansatz geändert — alle Maus-Events werden jetzt auf dem GridInventoryWidget abgefangen (nicht auf einzelnen Slots). Die Slot-Widgets sind HitTestInvisible. Es wurde ein Debug-Log eingebaut:

```cpp
UE_LOG(LogTemp, Warning, TEXT("[GridInventory] MouseDown at Cell (%d, %d)..."));
```

**Nächster Schritt:**
1. Kompilieren und testen ob der Log erscheint
2. Falls NICHT → Das Widget-Setup im Blueprint muss geprüft werden:
   - WBP_Inventory (Parent: GridInventoryWidget) enthält: SizeBox → Border → GridCanvas
   - Der Border darf die Events nicht schlucken
   - Mögliche Lösung: Border auf "Self Hit Test Invisible" setzen, oder Border entfernen und durch ein Image ersetzen
3. Falls der Log ERSCHEINT → Maus-Events kommen an, Drag&Drop und Rechtsklick funktionieren

### PROBLEM 2: Grid-Lines fehlen
**Lösung (noch nicht implementiert):**
Am einfachsten: 1px Padding auf dem Overlay im Slot-Widget, SlotBackground als dunkle Farbe → sieht aus wie Grid-Lines.

---

## UE 4.27 Kompatibilitäts-Regeln (WICHTIG!)

Bei JEDEM Code-Change diese Regeln beachten:

1. **Kein `GetFont()` auf UTextBlock** — nutze `TextBlock->Font` Property direkt
2. **Kein `FSavePackageArgs`** — nutze alte Signatur: `UPackage::SavePackage(Package, Asset, RF_Public | RF_Standalone, *Filename)`
3. **Kein `#include "UObject/SavePackage.h"`** — existiert nicht in 4.27
4. **`CreatePackage()` braucht `nullptr` als ersten Parameter** — `CreatePackage(nullptr, *Name)`
5. **`FAutoConsoleCommand` mit `FConsoleCommandWithArgsDelegate`** — NICHT `FConsoleCommandWithWorldAndArgsDelegate`
6. **`TryGetNumberField()` gibt `double` zurück** — nicht `int32`
7. **Kein `FReply::Handled().CaptureMouse()`** in UUserWidget — nutze `DetectDrag()` stattdessen
8. **`ESlateVisibility`** statt `EVisibility` für UMG
9. **Variable darf nicht `Slot` heißen** in UWidget-Subklassen — `UWidget::Slot` ist ein Member
10. **`IsCellFree()` nimmt `FIntPoint`** — nicht zwei separate ints

---

## Befehle zum Testen

Im UE4 Editor (Konsole mit ~ öffnen):

```
Inv.ImportJSON E:/data.json          # 138 Zutaten importieren
Inv.Add Basilikum                    # Item hinzufügen
Inv.Add Brennnessel 5               # 5 Stück
Inv.List                             # Alle Items anzeigen
Inv.Info 3A4B                        # Detail-Info (ShortUID)
Inv.Debug.Bitmap                     # Grid-Belegung als ASCII
Inv.Debug.Slots                      # Widget-Virtualisierung Status
Inv.Clear                            # Inventar leeren
```

---

## Blueprint-Setup im Test-Projekt

### ThirdPersonCharacter
- Hat `GridInventoryComponent` (10x10 Grid)
- Hat Variable `InventoryWidget` (Typ: WBP_Inventory)
- BeginPlay: Create Widget → Add to Viewport → Set Inventory Component → Set Visibility Hidden
- Taste I: Flip Flop → Visible/Hidden + Show Mouse Cursor + Set Input Mode Game And UI / Game Only
- BeginPlay fügt Test-Items hinzu: Basilikum, Brennnessel x5, Eisenhut x3

### WBP_Inventory (Parent: GridInventoryWidget)
- Hierarchy: SizeBox (640x640) → Border (dunkel) → GridCanvas (Canvas Panel)
- Slot Widget Class: WBP_InventorySlot
- Cell Size: 64
- Viewport Buffer: 3

### WBP_InventorySlot (Parent: InventorySlotWidget)
- Hierarchy: Overlay → SlotBackground (Image) + ItemIcon (Image)
- Alle auf Not Hit-Testable (wird jetzt in C++ erzwungen)

---

## Prioritäten für Weiterentwicklung

1. **Maus-Klick Problem lösen** — Debug-Log prüfen, ggf. Widget-Hierarchy anpassen
2. **Grid-Lines** — visuelles Feedback für Zellen
3. **Drag & Drop** — Items im Grid verschieben (Grundlage ist schon im Code)
4. **Rechtsklick-Menü** — Item-Aktionen (Benutzen, Wegwerfen, Info)
5. **Tooltip/Hover** — Item-Info bei Mouseover
6. **Item-Icons** — Soft References auf Texturen laden und anzeigen
7. **Equipment-UI** — Ausrüstungs-Slots Widget
8. **Container-UI** — Truhen öffnen, zweites Grid anzeigen

---

## TODO: Save/Load System

### Anforderung
Einfache Blueprint-Nodes zum Speichern und Laden des kompletten Inventar-Zustands.

### API-Design
```cpp
// Blueprint-Nodes auf dem GridInventoryComponent:
UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
bool SaveInventory(int32 SlotIndex);

UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Save")
bool LoadInventory(int32 SlotIndex);
```

### Was gespeichert werden muss
- **Spieler-Inventar:** Alle Items mit Position, Rotation, Stack, ClassLevel, UniqueID
- **Equipment:** Alle belegten Equipment-Slots mit Item-Daten
- **Container:** Alle InventoryContainer in der Welt mit ihrem Inhalt, Lock-Status, ob schon geöffnet
- **Dynamische Items:** Zur Laufzeit erstellte Items (siehe TODO unten) — diese haben kein DataAsset, also müssen ALLE Properties inline gespeichert werden

### Technische Umsetzung
- `USaveGame`-Subklasse: `UInventorySaveGame`
- Serialisierung über `FInventorySaveData` Struct mit `TArray<FItemSaveEntry>`
- Pro Item: ItemDefPath (SoftObjectPath zum DataAsset), GridPosition, bIsRotated, StackCount, ClassLevel, UniqueID
- Für dynamische Items (kein DataAsset): Komplette Property-Kopie inline (Name, Effects, Weight, Alchemy Properties etc.)
- Container: `TMap<FName, FContainerSaveData>` — ContainerID → Items + Zustand
- Equipment: `TMap<FName, FItemSaveEntry>` — SlotID → Item
- Slot-basiert: `UGameplayStatics::SaveGameToSlot` / `LoadGameFromSlot` mit SlotName = `"Inventory_" + FString::FromInt(SlotIndex)`
- Beim Laden: Container-Items wiederherstellen, Equipment re-equippen, dynamische Items neu erzeugen

### Wichtig
- SaveGame muss auch funktionieren wenn Items zur Laufzeit dynamisch erstellt wurden (kein DataAsset vorhanden)
- Container die in der Welt platziert sind brauchen eine stabile ID (FName oder FGuid) um sie beim Laden wiederzufinden
- Equipment-Slots müssen ihren Zustand inklusive Item-Effekte korrekt wiederherstellen

---

## TODO: Dynamische Item-Erstellung zur Laufzeit

### Anforderung
Im Alchemie-System werden zur Laufzeit neue Tränke gebraut. Diese brauchen vollwertige Items die exakt die gleiche Struktur wie normale DataAsset-Items haben — keine abgespeckte Version.

### Konzept
Dynamische Items sind `FInventoryItemInstance` Instanzen die ihre Definition NICHT aus einem UDataAsset beziehen, sondern eine zur Laufzeit erzeugte Definition verwenden.

### Lösungsansatz: Runtime ItemDefinition

```cpp
// Neue Klasse oder Struct für zur Laufzeit erzeugte Items:
UCLASS()
class URuntimeItemDefinition : public UInventoryItemDefinition
{
    GENERATED_BODY()
public:
    // Erbt alle Properties von UInventoryItemDefinition:
    // - DisplayName, Description, ItemType, GridSize
    // - Weight, MaxStackSize, bCanStack, bCanRotate
    // - BaseEffects (TArray<FItemEffectValue>)
    // - Alchemy Properties (WirkungTag, SchutzVor, Gegengift, Sonstiges, DurationMinutes)
    // - Icon, ActorClass etc.
    
    // Zusätzlich:
    UPROPERTY()
    bool bIsRuntimeCreated = true;
    
    // Rezept-Herkunft (optional, für UI/Tooltip)
    UPROPERTY()
    TArray<FName> SourceIngredients;
};
```

### Blueprint-Node zum Erstellen

```cpp
// Auf dem GridInventoryComponent oder als statische Library-Funktion:
UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Crafting")
static URuntimeItemDefinition* CreateRuntimeItem(
    FText DisplayName,
    FText Description,
    TArray<FItemEffectValue> Effects,
    float Weight,
    // ... alle relevanten Properties
);

// Oder Variante: Item aus Rezept-Kombination erstellen
UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Crafting")
static URuntimeItemDefinition* CreatePotionFromIngredients(
    FText PotionName,
    const TArray<FInventoryItemInstance>& Ingredients
    // → kombiniert die Effects der Zutaten nach Alchemie-Regeln
);
```

### Persistenz (Zusammenspiel mit Save System)
- Dynamische Items haben kein DataAsset auf Disk → können nicht über SoftObjectPath referenziert werden
- Beim Speichern: Alle Properties des URuntimeItemDefinition inline serialisieren (als JSON oder USTRUCT Serialisierung)
- Beim Laden: URuntimeItemDefinition neu erstellen (NewObject<>), Properties befüllen, Item wiederherstellen
- Format-Vorschlag: JSON-String pro dynamischem Item im SaveGame, gleiche Struktur wie die Import-JSON

### Speicherung als JSON
Ja, dynamische Items können als JSON gespeichert werden — gleiche Struktur wie die data.json für Zutaten. Das hat den Vorteil:
- Konsistentes Format (Import-JSON und Runtime-JSON identisch)
- Der JSON-Importer kann wiederverwendet werden (leicht modifiziert für Runtime statt Editor)
- Menschenlesbar für Debugging
- Einfach erweiterbar

### Ablauf im Spiel
1. Spieler braut Trank im Alchemie-System
2. Alchemie-System ruft `CreateRuntimeItem()` auf mit berechneten Werten
3. RuntimeItemDefinition wird erstellt (lebt im Speicher, kein Asset auf Disk)
4. `TryAddItem(RuntimeDef, 1)` fügt den Trank ins Inventar ein
5. Spieler kann den Trank normal benutzen, stacken, in Container legen etc.
6. Beim Speichern: RuntimeDef wird als JSON-Block inline im SaveGame gespeichert
7. Beim Laden: JSON-Block → neues RuntimeDef → Item wiederhergestellt

### Wichtig
- URuntimeItemDefinition muss als UObject im Speicher leben (Garbage Collection beachten!)
- Solange mindestens ein Item darauf referenziert, darf es nicht GC'd werden
- Beim Entfernen des letzten Items mit dieser Definition: Definition kann freigegeben werden
- GridInventoryComponent sollte eine `TArray<URuntimeItemDefinition*> RuntimeDefinitions` halten um GC zu verhindern

---

## Code-Style

- Unreal Engine Coding Standards (PascalCase, UPROPERTY/UFUNCTION Macros)
- Kommentare auf Deutsch oder Englisch OK
- Alle neuen Features müssen UE 4.27 kompatibel sein
- Soft References für alles was Texturen/Meshes/Klassen lädt
- Blueprint-Exposure wo sinnvoll (BlueprintCallable, BlueprintNativeEvent)

---

## Datei-Referenz

Wenn du eine Datei änderst, prüfe immer ob:
1. Der Header (.h) und die Implementation (.cpp) konsistent sind
2. Neue Includes in der .cpp stehen (nicht im Header wenn vermeidbar)
3. Die Build.cs die nötigen Module referenziert
4. Kein UE5-only API verwendet wird (siehe Kompatibilitäts-Regeln oben)
