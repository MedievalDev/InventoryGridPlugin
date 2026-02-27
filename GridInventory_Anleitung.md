# Grid Inventory Plugin — Komplettanleitung

## Für Unreal Engine 4.27 | Alchemy Fox Studio | Version 1.0

---

# Inhaltsverzeichnis

1. [Quick Start (5 Minuten)](#1-quick-start)
2. [Plugin installieren](#2-plugin-installieren)
3. [JSON-Import der Alchemie-Zutaten](#3-json-import)
4. [Character einrichten](#4-character-einrichten)
5. [Inventar-UI erstellen](#5-inventar-ui-erstellen)
6. [Maus-Input & Interaktion](#6-maus-input)
7. [Sammelbare Items in der Welt](#7-sammelbare-items)
8. [Truhen & Container](#8-truhen-container)
9. [Equipment-System](#9-equipment-system)
10. [Debug-System & Konsolen-Commands](#10-debug-commands)
11. [Blueprint Function Library](#11-blueprint-functions)
12. [Performance-Tipps](#12-performance-tipps)
13. [Häufige Probleme & Lösungen](#13-troubleshooting)

---

# 1. Quick Start

Für Ungeduldige — in 5 Minuten ein funktionierendes Inventar:

1. `GridInventory`-Ordner in `Plugins/` kopieren
2. Editor starten → "Ja" bei Rebuild klicken
3. ThirdPersonCharacter öffnen → Add Component → **GridInventoryComponent**
4. Grid Width: 10, Grid Height: 10
5. Play drücken → Konsole (~) → `Inv.Add Basilikum` → fertig!

Für das volle Setup mit UI, Maus, Items in der Welt → weiterlesen.

---

# 2. Plugin installieren

## 2.1 Dateien kopieren

Den `GridInventory`-Ordner aus der ZIP in den `Plugins`-Ordner deines Projekts:

```
MeinProjekt/
  Content/
  Config/
  Plugins/
    GridInventory/           ← hierhin entpacken
      GridInventory.uplugin
      Source/
        GridInventoryRuntime/
        GridInventoryUI/
        GridInventoryEditor/
```

Falls der `Plugins`-Ordner nicht existiert: einfach erstellen.

## 2.2 Editor starten

Doppelklick auf deine `.uproject`-Datei. Der Editor erkennt das neue Plugin automatisch und fragt:

> "The following modules are missing or built with a different engine version: GridInventoryRuntime, GridInventoryUI. Would you like to rebuild them now?"

→ **Ja** klicken. Erster Build dauert 1-2 Minuten.

## 2.3 Überprüfen

Edit → Plugins → links "Gameplay" auswählen → **Grid Inventory** sollte da sein mit Häkchen bei "Enabled".

## 2.4 Voraussetzungen

- Unreal Engine 4.27
- Visual Studio 2019 oder 2022 mit "Game development with C++"-Workload
- Kein manuelles Editieren der .uproject-Datei nötig

> **Tipp:** Falls der Build fehlschlägt, prüfe ob Visual Studio mit C++ Workload installiert ist. Die Build-Fehler stehen in: `Projekt/Saved/Logs/Projektname.log`

---

# 3. JSON-Import

## 3.1 Zutaten importieren

Der JSON-Importer liest deine `data.json` mit allen 138 Alchemie-Zutaten und erstellt automatisch DataAssets.

1. Kopiere `data.json` an einen bekannten Ort, z.B. `E:\data.json`
2. Im Editor: Tilde-Taste `~` drücken (Konsole öffnet sich)
3. Eingeben:

```
Inv.ImportJSON E:/data.json
```

4. Im Output Log (Window → Developer Tools → Output Log) erscheint:

```
[JsonImporter] Kategorie: Basis Zutaten -> /Game/Items/AlchemieZutaten/BasisZutaten
[JsonImporter]   + Wasser (0 Effects, Wirkung=None, Dauer=0.0 Min)
[JsonImporter]   + Basilikum (1 Effects, Wirkung=None, Dauer=2.0 Min)
...
[JsonImporter] Import abgeschlossen: 138 / 138 Items
```

## 3.2 Import prüfen

Content Browser → navigiere zu `Content/Items/AlchemieZutaten/`:

```
AlchemieZutaten/
  BasisZutaten/         (5 Items: Wasser, Wein, Alkohol, Öl, Fett)
  PflanzenKraeuter/     (28 Items: Basilikum, Brennnessel, ...)
  Pilze/                (8 Items: Fliegenpilz, ...)
  BeerenFruechte/       (7 Items)
  MineralienGesteine/   (20 Items)
  Organisches/          (58 Items)
  MagischeZutaten/      (9 Items)
  Sonstiges/            (3 Items)
```

Doppelklick auf ein Asset zeigt alle Properties: Effects, SpawnData, Wirkung, Dauer etc.

## 3.3 Eigener Zielordner (optional)

```
Inv.ImportJSON E:/data.json /Game/Items/MeineZutaten
```

## 3.4 Re-Import

Einfach nochmal `Inv.ImportJSON` ausführen — bestehende Assets werden überschrieben.

> **Tipp:** Behalte deine data.json als "Single Source of Truth". Ändere Werte dort und importiere neu. Keine manuelle Pflege von 138 DataAssets nötig.

---

# 4. Character einrichten

## 4.1 InventoryComponent hinzufügen

1. Content Browser → **ThirdPersonCharacter** Blueprint öffnen
2. Links im Components-Panel: **Add Component**
3. Suche: "GridInventory" → **GridInventoryComponent** auswählen
4. Im Details-Panel rechts einstellen:

| Property | Wert | Beschreibung |
|----------|------|--------------|
| Grid Width | 10 | Breite des Inventar-Grids |
| Grid Height | 10 | Höhe des Inventar-Grids |
| Max Weight | 0 | 0 = unbegrenzt, sonst Gewichtslimit |

5. **Compile** & **Save**

## 4.2 Testen ohne UI

1. Play drücken
2. Tilde `~` für Konsole
3. `Inv.Add Basilikum` → Item wird im Grid platziert
4. `Inv.List` → zeigt alle Items mit Position und Gewicht
5. `Inv.Add Brennnessel 5` → Stack von 5

> **Tipp:** Die Debug-Commands funktionieren sofort ohne UI. Perfekt zum Testen der Spiellogik bevor du dich um Widgets kümmerst.

---

# 5. Inventar-UI erstellen

## 5.1 Slot-Widget (WBP_InventorySlot)

Das einzelne Feld im Grid:

1. Content Browser → Rechtsklick → User Interface → **Widget Blueprint**
2. Name: `WBP_InventorySlot`
3. Öffnen → oben bei **Parent Class**: ändern zu **InventorySlotWidget**
4. Im Designer folgende Struktur aufbauen:

```
[WBP_InventorySlot]
  └ [Overlay]
      ├ SlotBackground   (Image)
      └ ItemIcon          (Image)
```

So geht's:
- **Overlay** aus der Palette in den Root ziehen
- In das Overlay: **Image** reinziehen → umbenennen zu `SlotBackground`
- Nochmal **Image** ins Overlay → umbenennen zu `ItemIcon`

5. **WICHTIG — Hit-Test deaktivieren:**
   - Klick auf **Overlay** → Details → Visibility → **Not Hit-Testable (Self Only)**
   - Klick auf **SlotBackground** → Details → Visibility → **Not Hit-Testable (Self Only)**
   - Klick auf **ItemIcon** → Details → Visibility → **Not Hit-Testable (Self Only)**

   > Das sorgt dafür dass Maus-Klicks durch die Bilder zum C++ InventorySlotWidget durchkommen.

6. **Speichern**

## 5.2 Grid-Widget (WBP_Inventory)

Das gesamte Inventar-Fenster:

1. Content Browser → Rechtsklick → User Interface → **Widget Blueprint**
2. Name: `WBP_Inventory`
3. Öffnen → **Parent Class** ändern zu **GridInventoryWidget**
4. Im Designer:

```
[WBP_Inventory]
  └ [SizeBox]                  ← Größe festlegen
      └ [Border]               ← Klicks abfangen + Hintergrund
          └ [GridCanvas]       ← MUSS so heißen!
```

So geht's:
- **Size Box** aus der Palette reinziehen
  - Details: `Width Override` = **640**, `Height Override` = **640** (10 Zellen × 64px)
- In die Size Box: **Border** reinziehen
  - Details: Brush Color = dunkle Farbe (z.B. schwarz mit 80% Alpha)
  - Visibility = **Visible** (MUSS Hit-Testable bleiben! Das fängt Maus-Events ab)
- In den Border: **Canvas Panel** reinziehen
  - Umbenennen zu **GridCanvas** (exakt so, der C++ Code sucht nach diesem Namen)

5. Root-Widget [WBP_Inventory] anklicken, im Details-Panel:

| Property | Wert |
|----------|------|
| Slot Widget Class | WBP_InventorySlot |
| Cell Size | 64 |
| Viewport Buffer | 3 |

6. **Speichern**

> **Tipp:** Die Size Box-Größe berechnet sich: Grid Width × Cell Size = 10 × 64 = 640. Bei einem 12x8 Grid wäre es 768 × 512.

---

# 6. Maus-Input

## 6.1 Blueprint-Setup im ThirdPersonCharacter

Öffne den **ThirdPersonCharacter** Blueprint → Event Graph.

### BeginPlay — Widget erstellen

```
Event BeginPlay
  │
  ├→ Create Widget
  │    Class: WBP_Inventory
  │    Owning Player: Get Player Controller (Index 0)
  │    Return Value → Promote to Variable: "InventoryWidget"
  │
  ├→ InventoryWidget → Add to Viewport
  │
  ├→ Self → Get Component by Class: GridInventoryComponent
  │    Return Value → in Variable speichern: "InventoryComp"
  │
  ├→ InventoryWidget → Set Inventory Component
  │    Component: InventoryComp
  │
  ├→ InventoryWidget → Set Visibility (Hidden)
  │
  └→ (Optional) Test-Items hinzufügen:
      InventoryComp → Try Add Item (Item Def: DA_Basilikum, Count: 1)
      InventoryComp → Try Add Item (Item Def: DA_Brennnessel, Count: 5)
```

### Taste I — Inventar öffnen/schließen

```
Keyboard Event: I (Pressed)
  │
  ├→ InventoryWidget → Is Visible → Branch
  │
  ├─ True (Inventar SCHLIESSEN):
  │   ├→ InventoryWidget → Set Visibility (Hidden)
  │   ├→ Get Player Controller → Set Show Mouse Cursor: false
  │   └→ Get Player Controller → Set Input Mode Game Only
  │
  └─ False (Inventar ÖFFNEN):
      ├→ InventoryWidget → Set Visibility (Visible)
      ├→ Get Player Controller → Set Show Mouse Cursor: true
      └→ Get Player Controller → Set Input Mode Game And UI
           In Widget to Focus: InventoryWidget
           Lock Mouse to Viewport: false
           Hide Cursor During Capture: false   ← WICHTIG!
```

### Warum das Border im Widget wichtig ist

Ohne den Border fängt das Widget keine Maus-Events auf dem leeren Grid-Bereich ab. Die Folge: Klicks auf leere Stellen gehen "durch" ans Spiel → Kamera dreht sich. Der Border als solider Hintergrund blockiert alle Klicks innerhalb des Inventar-Bereichs.

> **Tipp:** `Hide Cursor During Capture` muss **false** sein! Sonst verschwindet der Cursor beim Klicken.

---

# 7. Sammelbare Items

## 7.1 BP_WorldPickup erstellen

1. Content Browser → Rechtsklick → Blueprint Class → **Actor**
2. Name: `BP_WorldPickup`
3. Öffnen und Components hinzufügen:
   - **Static Mesh Component** → z.B. eine Kugel oder Pflanze als Platzhalter
   - **Sphere Collision** → Radius: 100, Collision Preset: **OverlapAllDynamic**

4. Variables erstellen (jeweils "Instance Editable" anhaken = Auge offen):

| Variable | Typ | Default | Instance Editable |
|----------|-----|---------|-------------------|
| ItemDefinition | InventoryItemDefinition (Object Reference) | None | ✓ |
| StackCount | Integer | 1 | ✓ |

5. Event Graph:

```
Event ActorBeginOverlap
  │ Other Actor
  │
  ├→ Other Actor → Get Component by Class: GridInventoryComponent
  │    Return Value → Is Valid → Branch
  │
  └─ True:
      ├→ Pickup Item
      │    Item Def: ItemDefinition (Variable)
      │    Count: StackCount (Variable)
      │    World Actor: Self
      │
      └→ Branch (Return Value)
           True: → Print String "Aufgesammelt!"
           False: → Print String "Inventar voll!"
```

## 7.2 Im Level platzieren

1. `BP_WorldPickup` aus dem Content Browser ins Level ziehen
2. Im Details-Panel:
   - `Item Definition` → auf z.B. **DA_Basilikum** setzen (aus dem Dropdown oder aus Content Browser draufziehen)
   - `Stack Count` → z.B. 3
3. Play → mit Character drüberlaufen → Item wird eingesammelt

## 7.3 Variante: Taste E zum Aufheben (statt Overlap)

Falls du lieber mit Tastendruck aufheben willst:

1. Statt Sphere Collision: **Box Collision** (größer, nur für Nähe-Check)
2. Interface erstellen: `BPI_Interactable` mit Funktion `Interact(Actor Caller)`
3. Im ThirdPersonCharacter:

```
Keyboard E (Pressed)
  │
  ├→ Line Trace by Channel
  │    Start: Get Camera Location
  │    End: Get Camera Location + (Get Forward Vector × 300)
  │
  └→ Break Hit Result → Hit Actor → Does Implement Interface: BPI_Interactable
       True: → Interact (Caller: Self)
```

4. BP_WorldPickup implementiert das Interface und sammelt bei `Interact` ein

---

# 8. Truhen & Container

## 8.1 Truhe erstellen

1. Content Browser → Rechtsklick → Blueprint Class → **InventoryContainer**
2. Name: `BP_Truhe`
3. Öffnen und Components hinzufügen:
   - **Static Mesh Component** → Truhen-Mesh oder Box als Platzhalter
   - **Box Collision** → für Interaktions-Reichweite (Radius ~200)

4. Im Details-Panel des BP_Truhe:

| Property | Wert | Beschreibung |
|----------|------|--------------|
| Grid Width | 6 | Truhen-Inventar Breite |
| Grid Height | 4 | Truhen-Inventar Höhe |
| Loot Table | Dein LootTable Asset | Siehe 8.2 |
| Loot Level | 5 | Loot-Stufe |
| Drop Weight Multiplier | 1.0 | 1.0 = normal, 3.0 = Goldtruhe |
| bIsLocked | false | true = braucht Schlüssel |
| Required Key Item ID | (leer) | z.B. "Schluessel" |
| bConsumeKey | true | Schlüssel wird verbraucht |

## 8.2 LootTable erstellen

1. Content Browser → Rechtsklick → Miscellaneous → **Data Asset**
2. Klasse auswählen: **LootTable**
3. Name: `DA_LootTable_Kraeuter`
4. Öffnen und Entries hinzufügen:

| Property | Wert |
|----------|------|
| Min Item Drops | 2 |
| Max Item Drops | 5 |

Bei "Entries" auf + klicken und Items hinzufügen:

| Entry | Item Def | Min Count | Max Count | Fixed Weight Override |
|-------|----------|-----------|-----------|----------------------|
| 0 | DA_Basilikum | 1 | 3 | 0 (= nutzt Item-Weights) |
| 1 | DA_Brennnessel | 1 | 5 | 0 |
| 2 | DA_Eisenhut | 1 | 1 | 0 |

## 8.3 Truhe öffnen (Blueprint)

Im ThirdPersonCharacter Blueprint:

```
Keyboard E (Pressed)
  │
  ├→ Sphere Trace by Channel (oder Line Trace)
  │    Start: Actor Location
  │    End: Actor Location + Forward × 250
  │
  ├→ Hit Actor → Cast to InventoryContainer
  │
  └─ Cast Success:
      ├→ Try Open (Interactor: Self)
      └→ Branch (Return)
           True: → Truhen-UI öffnen (zweites Widget)
           False: → Print String "Verschlossen!"
```

> **Tipp:** Loot wird beim ersten Öffnen automatisch generiert (async, kein Frame-Drop). Beim zweiten Öffnen sind die gleichen Items noch da.

---

# 9. Equipment-System

## 9.1 EquipmentComponent hinzufügen

1. ThirdPersonCharacter Blueprint öffnen
2. Add Component → **EquipmentComponent**
3. Im Details-Panel: **Slot Definitions** Array erweitern:

| Slot ID | Display Name | Accepted Item Type | bAllow Stacking |
|---------|-------------|-------------------|-----------------|
| Head | Helm | Helmet | false |
| MainHand | Haupthand | Weapon_1H | false |
| OffHand | Nebenhand | Shield | false |
| Chest | Brustpanzer | ChestArmor | false |
| PotionSlot1 | Trank 1 | Consumable | true (Max 10) |

## 9.2 Equipment-Slot Widget

1. Neues Widget Blueprint: `WBP_EquipmentSlot`, Parent: **EquipmentSlotWidget**
2. Struktur wie beim Inventory Slot (Overlay → Background Image → ItemIcon)
3. Alle Images: Visibility = Not Hit-Testable
4. Im Details-Panel: `Slot ID` setzen (z.B. "MainHand")

## 9.3 In Blueprint ausrüsten

```
Rechtsklick auf Item → Equip Item
  │
  ├→ Get Equipment Component
  ├→ Equip Item (Slot ID: "MainHand", Item: Selected Item)
  └→ Refresh UI
```

Oder per Debug-Command: `Inv.Equip MainHand Eisenschwert`

---

# 10. Debug-System

Alle Commands in der Konsole (Tilde `~`). Nur in Non-Shipping Builds verfügbar.

## 10.1 Wichtigste Commands

| Command | Beschreibung |
|---------|-------------|
| `Inv.Add <ItemID> [Count]` | Item ins Inventar |
| `Inv.Remove <ShortUID> [Count]` | Item entfernen (UID aus Inv.List) |
| `Inv.List` | Alle Items anzeigen |
| `Inv.Find <Text>` | Nach Name suchen |
| `Inv.Info <ShortUID>` | Detail-Info zu einem Item |
| `Inv.Clear` | Inventar leeren |
| `Inv.ImportJSON <Pfad>` | JSON-Datei importieren |

## 10.2 Alle Commands

Siehe beiliegende Excel-Datei `GridInventory_Commands.xlsx` für die komplette Liste.

> **Tipp:** Die ShortUID ist der 8-stellige Hex-Code am Anfang jeder Zeile bei `Inv.List`. Du brauchst nicht die ganze UUID, nur die ersten 4-8 Zeichen.

---

# 11. Blueprint Function Library

20+ Blueprint-Nodes für Suche, Filter und Stats:

## 11.1 Suche & Filter

| Node | Input | Output | Beschreibung |
|------|-------|--------|-------------|
| FindItemsByType | Inventory, ItemType | Array | Alle Items eines Typs |
| FindItemsByName | Inventory, SearchText | Array | Textsuche im DisplayName |
| FindItemsByDef | Inventory, ItemDef | Array | Alle Instanzen einer Definition |
| FindStackableItems | Inventory | Array | Alle stackbaren Items |
| FindPartialStacks | Inventory | Array | Items mit Platz im Stack |
| FindContainerItems | Inventory | Array | Alle Container-Items |
| FindMergeCandidates | Inventory, TargetID | Array | Merge-Partner für ein Item |
| DeepSearchByName | Inventory, Text | Array | Suche inkl. Sub-Inventare |
| DeepSearchByType | Inventory, Type | Array | Typ-Suche inkl. Sub-Inventare |

## 11.2 Stats & Utilities

| Node | Input | Output | Beschreibung |
|------|-------|--------|-------------|
| GetTotalItemCount | Inventory, ItemDef | int | Menge eines Items |
| GetUniqueItemTypeCount | Inventory | int | Verschiedene Item-Typen |
| GetItemEffectValue | Item, EffectID | float | Aufgelöster Effekt-Wert |
| GetAllItemEffects | Item | Array | Alle Effekte eines Items |
| GetItemDisplayName | Item | Text | Name mit Klassen-Suffix |
| SortInventory | Inventory | void | Nach Größe sortieren |

---

# 12. Performance-Tipps

## 12.1 Soft References

Alle Icons und Actor-Klassen sind Soft References. Das bedeutet: nur das was gerade sichtbar ist, wird geladen. Bei 160+ Zutaten spart das Gigabytes an RAM.

Du kannst den Status prüfen: `Inv.Debug.Memory`

## 12.2 Virtualisiertes Grid

Das UI erstellt nur Widgets für sichtbare Zellen. Bei einem 60×80 Grid sind das ~300 Widgets statt 4.800. Status prüfen: `Inv.Debug.Slots`

## 12.3 Grid-Größe wählen

| Verwendung | Grid-Größe | Zellen | Empfehlung |
|-----------|-----------|--------|-----------|
| Schnell testen | 10×10 | 100 | Zum Entwickeln |
| Spieler-Inventar | 20×15 | 300 | Guter Standard |
| Großes RPG | 40×30 | 1.200 | Für viele Items |
| Maximum (wie Alchimist) | 60×80 | 4.800 | Braucht Virtualisierung |

## 12.4 Batch-Operationen

Wenn du viele Items auf einmal hinzufügst (z.B. Loot), nutze `TryAddItemsBatch` statt einzelne `TryAddItem`-Calls. Das verhindert 10× UI-Refresh bei 10 Items.

## 12.5 Async Loot

Container generieren Loot automatisch async — kein Frame-Drop beim Öffnen einer Truhe.

---

# 13. Häufige Probleme & Lösungen

## Build schlägt fehl

**Problem:** "Weapon showcase could not be compiled"
**Lösung:** Öffne `Projekt/Saved/Logs/Projektname.log`, suche nach `error`. Die häufigsten:
- Missing include → Header vergessen
- Undeclared identifier → Modul-Dependency fehlt in Build.cs
- Stelle sicher dass Visual Studio mit C++ Workload installiert ist

## Items werden nicht im UI angezeigt

**Problem:** `Inv.Add` funktioniert, aber UI zeigt nichts
**Prüfe:**
1. Wurde `Set Inventory Component` auf dem Widget aufgerufen?
2. Heißt das Canvas Panel exakt `GridCanvas`?
3. Ist `Slot Widget Class` auf `WBP_InventorySlot` gesetzt?

## Klicks gehen durchs Inventar

**Problem:** Kamera dreht sich beim Klicken aufs Inventar
**Lösung:**
1. Prüfe ob ein **Border** als Hintergrund im WBP_Inventory ist (Visibility = Visible)
2. Prüfe `Set Input Mode Game And UI` bei Inventar öffnen
3. Prüfe `Hide Cursor During Capture` = false

## Maus verschwindet beim Klicken

**Problem:** Cursor weg bei Mausklick
**Lösung:** Bei `Set Input Mode Game And UI`: `Hide Cursor During Capture` = **false**

## JSON-Import findet keine Items

**Problem:** `Inv.ImportJSON` meldet 0 Items
**Prüfe:**
1. Pfad korrekt? Vorwärts-Slashes verwenden: `E:/data.json`
2. JSON-Datei gültig? Keine Syntax-Fehler?
3. Gibt es ein `categories`-Array mit `supercat: "Alchemie Zutaten"`?

## Inv.Add findet Item nicht

**Problem:** `Inv.Add Basilikum` → "Item not found"
**Lösung:** Erst `Inv.ImportJSON` ausführen, dann `Inv.ItemDB` zum Prüfen welche Items registriert sind.

## Performance-Probleme bei vielen Items

**Symptome:** FPS-Drops beim Öffnen des Inventars
**Lösungen:**
1. `Inv.Debug.Slots` prüfen — sind zu viele Widgets aktiv?
2. `Inv.Debug.Memory` prüfen — sind unnötig viele Texturen geladen?
3. Viewport Buffer auf 3 setzen (nicht höher)
4. Grid-Größe auf das Nötige reduzieren

---

# Anhang: Ordnerstruktur nach vollständiger Einrichtung

```
MeinProjekt/
  Content/
    Items/
      AlchemieZutaten/
        BasisZutaten/
          DA_Wasser.uasset
          DA_Alkohol.uasset
          ...
        PflanzenKraeuter/
          DA_Basilikum.uasset
          DA_Brennnessel.uasset
          ...
        Pilze/
        BeerenFruechte/
        MineralienGesteine/
        Organisches/
        MagischeZutaten/
        Sonstiges/
    Blueprints/
      BP_WorldPickup.uasset
      BP_Truhe.uasset
    UI/
      WBP_Inventory.uasset
      WBP_InventorySlot.uasset
      WBP_EquipmentSlot.uasset
    DataAssets/
      DA_LootTable_Kraeuter.uasset
  Plugins/
    GridInventory/
      Source/
        GridInventoryRuntime/  (Spiellogik)
        GridInventoryUI/       (Widgets)
        GridInventoryEditor/   (JSON-Importer)
```

---

*Alchemy Fox Studio — Grid Inventory Plugin v1.0 — Februar 2026*
