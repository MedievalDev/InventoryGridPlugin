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
10. [Gold & Währung](#10-gold-waehrung)
11. [NPC-Handel / Trade Panel](#11-npc-handel)
12. [Container Panel Widget](#12-container-panel)
13. [Equipment Panel Widget](#13-equipment-panel)
14. [Kontext-Menü](#14-kontext-menue)
15. [Debug-System & Konsolen-Commands](#15-debug-commands)
16. [Blueprint Function Library](#16-blueprint-functions)
17. [Performance-Tipps](#17-performance-tipps)
18. [Häufige Probleme & Lösungen](#18-troubleshooting)

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

## 8.4 Default Items (feste Items ohne Zufall)

Willst du dass eine Truhe **immer** bestimmte Items enthält (kein Zufall)?

1. Truhe im Level auswählen (oder BP_Truhe öffnen)
2. Im Details-Panel → **Container | Default Items** → **+** klicken
3. Pro Eintrag:

| Property | Wert | Beschreibung |
|----------|------|-------------|
| Item Def | DA_Heiltrank | Welches Item |
| Count | 3 | Wie viele |

Beispiel:

| # | Item Def | Count |
|---|----------|-------|
| 0 | DA_Heiltrank | 3 |
| 1 | DA_Eisenschwert | 1 |
| 2 | DA_Schluessel | 1 |

> **Tipp:** DefaultItems und LootTable können kombiniert werden! Die Truhe hat dann immer die festen Items PLUS zufällige Extras aus der LootTable.

**Reihenfolge:**
1. Bei Spielstart → Default Items werden ins Container-Inventar geladen
2. Bei Spielstart → Random Default Items werden gewürfelt und hinzugefügt
3. Beim ersten Öffnen → LootTable generiert zusätzliche Items
4. Bei Save/Load → Gespeicherter Zustand wird wiederhergestellt

## 8.5 Random Default Items (zufällige Items)

Zusätzlich zu den festen Default Items können Container auch **zufällige** Items enthalten.
Jedes Item hat eine Spawn-Chance und eine zufällige Menge.

1. Truhe im Level auswählen
2. Im Details-Panel → **Container | Random Items** → **RandomDefaultItems** → **+** klicken
3. Pro Eintrag:

| Property | Wert | Beschreibung |
|----------|------|-------------|
| Item Def | DA_Heiltrank | Welches Item |
| Min Count | 1 | Mindestmenge wenn es erscheint |
| Max Count | 5 | Maximalmenge wenn es erscheint |
| Spawn Chance | 0.5 | 0.0 = nie, 1.0 = immer, 0.5 = 50% |

Beispiel — Truhe mit gemischtem Loot:

| # | Item Def | Min | Max | Chance |
|---|----------|-----|-----|--------|
| 0 | DA_Heiltrank | 1 | 3 | 0.8 |
| 1 | DA_Goldmuenze | 5 | 20 | 0.6 |
| 2 | DA_SeltenesAmulett | 1 | 1 | 0.1 |

> **DropWeightMultiplier wirkt auf Random Items!**
> Eine Truhe mit `DropWeightMultiplier = 2.0` verdoppelt die Spawn-Chance.
> Ein Item mit SpawnChance 0.3 hat dann effektiv 0.6 (60% Chance).
> SpawnChance wird maximal auf 1.0 begrenzt.

> **Kombinierbar:** DefaultItems (immer) + RandomDefaultItems (zufällig) + LootTable (zufällig, auf erstem Öffnen).

**Blueprint-Aufruf (optional):**
Man kann `GenerateRandomDefaults()` auch manuell aufrufen, z.B. um Random Items neu zu würfeln:
```
Get Container Reference → Generate Random Defaults
```

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

## 9.4 Item-Voraussetzungen (Requirements)

Items können Voraussetzungen haben — z.B. "Schwert braucht Level 8 und Stärke 10".

### Requirements im Editor setzen

1. Öffne ein ItemDefinition DataAsset (z.B. `DA_Eisenschwert`)
2. Im Details-Panel → Kategorie **Requirements** → **Equip Requirements** → **+** klicken
3. Pro Eintrag:

| Requirement ID | Min Value | Bedeutung |
|---------------|-----------|-----------|
| Level | 8 | Spieler braucht Level 8 |
| Staerke | 10 | Spieler braucht 10 Stärke |
| Geschicklichkeit | 5 | Spieler braucht 5 Geschicklichkeit |

Die Namen sind frei wählbar — du entscheidest welche Attribute dein Spiel hat!

### Blueprint: Deine Variablen verbinden

Das EquipmentComponent fragt dein Blueprint nach den Spieler-Werten. Du musst einmal sagen "so liest man meine Variablen":

```
Event Begin Play
  │
  ├→ Get Equipment Component (Self)
  │
  └→ SET On Get Player Stat
       │
       └→ Create Event → "GetMeineStat" (Input: StatID, Output: float)
```

Dann die Funktion `GetMeineStat` implementieren:

```
Funktion: GetMeineStat (StatID: FName) → Return: float

  Select (StatID)
  │
  ├→ "Level"           → Return: MeinLevel        ← Deine Variable
  ├→ "Staerke"         → Return: MeineStaerke     ← Deine Variable
  ├→ "Geschicklichkeit" → Return: MeineGeschick    ← Deine Variable
  └→ Default           → Return: 0
```

### Ergebnis

- **Requirements erfüllt** → Item kann ausgerüstet werden
- **Requirements NICHT erfüllt** → `Equip Item` gibt false zurück
- **Kein Delegate gebunden** → Alle Items können ausgerüstet werden (keine Einschränkung)

### Unerfüllte Requirements anzeigen

Nutze den Node `Get Unmet Requirements` um dem Spieler zu zeigen was fehlt:

```
Get Unmet Requirements (ItemDef: DA_Eisenschwert)
  │
  └→ For Each (FItemRequirement)
       │
       ├→ Requirement ID: "Staerke"
       └→ Min Value: 10
           → Print: "Du brauchst 10 Stärke!"
```

---

# 10. Gold & Währung

## 10.1 Übersicht

Das Gold-System ist direkt im **GridInventoryComponent** integriert. Es gibt keine separate Gold-Komponente — du nutzt die gleiche Komponente die auch dein Inventar verwaltet.

**Blueprint-Nodes auf dem GridInventoryComponent:**

| Node | Typ | Beschreibung |
|------|-----|-------------|
| **Get Gold** | Pure | Aktuellen Goldstand abfragen |
| **Set Gold** | Callable | Gold auf einen bestimmten Wert setzen |
| **Add Gold** | Callable | Gold hinzufügen (positiv) oder abziehen (negativ) |
| **Can Afford Gold** | Pure | Prüfen ob genug Gold da ist |
| **Get Item Sell Price** | Pure | Verkaufspreis eines Items berechnen |
| **Get Inventory Total Value** | Pure | Gesamtwert aller Items im Inventar |

**Event:**

| Event | Parameter | Beschreibung |
|-------|-----------|-------------|
| **On Gold Changed** | NewGold (float), Delta (float) | Feuert bei jeder Goldänderung |

## 10.2 Gold mit deiner eigenen Variable syncen

Du hast schon eine `Gold`-Variable in deinem Character Blueprint? Kein Problem — du brauchst sie nicht zu ersetzen. Binde einfach das `On Gold Changed` Event:

```
Event Begin Play
  │
  ├→ Get Grid Inventory Component (Self)
  │
  └→ Bind Event to On Gold Changed
       │
       └→ Custom Event "OnMyGoldChanged" (NewGold: float, Delta: float)
            │
            └→ SET Gold = NewGold    ← Deine eigene Variable wird synchronisiert
```

Oder noch einfacher: **Verwende GetGold() direkt** statt einer eigenen Variable. Überall wo du bisher `Gold` gelesen hast, ziehst du stattdessen das GridInventoryComponent raus und rufst `Get Gold` auf.

## 10.3 BaseValue auf Items setzen

Jedes Item hat jetzt ein **Base Value** Feld (Kategorie "Trade" im Editor):

1. Öffne ein ItemDefinition DataAsset (z.B. `DA_Basilikum`)
2. Im Details-Panel → Kategorie **Trade** → **Base Value** = z.B. `5` (Gold)
3. Dieses BaseValue wird für Kauf/Verkauf bei NPC-Händlern verwendet

| Item | Base Value | Bedeutung |
|------|-----------|-----------|
| DA_Basilikum | 5 | Kostet 5 Gold beim Händler |
| DA_Eisenhut | 25 | Seltener, kostet mehr |
| DA_Mondstein | 100 | Wertvoll |

> **Tipp:** Stackbare Items: Preis = BaseValue × StackCount. Ein Stack von 10 Basilikum kostet also 50 Gold.

## 10.4 Gold per Konsole testen

```
Inv.Gold.Set 1000        → Setzt Gold auf 1000
Inv.Gold.Add 500          → Gibt 500 Gold dazu (= 1500)
Inv.Gold.Add -200         → Zieht 200 Gold ab (= 1300)
```

## 10.5 Gold-Anzeige im HUD

So zeigst du Gold im UI an:

1. In deinem HUD Widget Blueprint: füge einen **TextBlock** hinzu (z.B. `GoldDisplay`)
2. Im Event Graph:

```
Event Construct
  │
  ├→ Get Owning Player Pawn → Get Grid Inventory Component
  │
  └→ Bind Event to On Gold Changed
       │
       └→ Custom Event "UpdateGoldDisplay"
            │
            └→ GoldDisplay → Set Text
                  Text: Format Text "Gold: {0}" (NewGold als int)
```

## 10.6 Gold wird automatisch gespeichert

Gold wird zusammen mit dem Inventar gespeichert und geladen — kein extra Aufwand:
- `Save Inventory (Slot 0)` → speichert Items UND Gold
- `Load Inventory (Slot 0)` → stellt Items UND Gold wieder her

---

# 11. NPC-Handel / Trade Panel

## 11.1 Übersicht

Das Trade-System zeigt zwei Inventare nebeneinander: Spieler und Händler. Items können zwischen den Inventaren gekauft und verkauft werden. Gold wird dabei automatisch verrechnet.

**Preisberechnung:**
- **Kaufpreis** = BaseValue × StackCount × BuyPriceMultiplier (Standard: 1.0)
- **Verkaufspreis** = BaseValue × StackCount × SellPriceMultiplier (Standard: 0.5)

## 11.2 NPC-Händler einrichten (MerchantActor)

Das Plugin enthält einen fertigen **MerchantActor** — alles wird im Details-Panel konfiguriert, kein Blueprint-Scripting nötig:

1. Content Browser → Rechtsklick → Blueprint Class → Suche: **MerchantActor**
2. Name: `BP_Haendler`
3. Öffne den Blueprint, füge hinzu:
   - Add Component → **Static Mesh** (z.B. Charakter-Mesh)
   - Add Component → **Box Collision** (Interaktions-Reichweite)
4. Wähle den BP_Haendler aus → im **Details-Panel** konfigurieren:

| Kategorie | Property | Wert | Beschreibung |
|-----------|----------|------|-------------|
| Merchant | Merchant Name | "Kräuterhändler" | Name im Trade-Panel |
| Merchant\|Save | Merchant ID | "Haendler_01" | Eindeutig für Save/Load |
| Merchant\|Pricing | Buy Price Multiplier | 1.0 | Kaufpreis = BaseValue × 1.0 |
| Merchant\|Pricing | Sell Price Multiplier | 0.5 | Verkaufspreis = BaseValue × 0.5 |
| Merchant\|Gold | Starting Gold | 5000 | Wie viel Gold der Händler hat |

> **Preisbeispiel:** Ein Schwert mit BaseValue 100:
> - Kaufen: 100 × 1.0 = **100 Gold**
> - Verkaufen: 100 × 0.5 = **50 Gold**
> - Teurer Händler (BuyPriceMultiplier 1.5): Kaufen = **150 Gold**

## 11.3 Händler-Inventar befüllen

### Feste Items (immer verfügbar)

Im Details-Panel → **Merchant | Stock** → **DefaultStock** → **+** klicken:

| # | Item Def | Count |
|---|----------|-------|
| 0 | DA_Heiltrank | 10 |
| 1 | DA_Manatrank | 5 |
| 2 | DA_Eisenschwert | 1 |

Diese Items sind IMMER im Händler-Inventar.

### Zufällige Items (Random Stock)

Im Details-Panel → **Merchant | Stock** → **RandomStock** → **+** klicken:

| # | Item Def | Min | Max | Chance |
|---|----------|-----|-----|--------|
| 0 | DA_SeltenerTrank | 1 | 2 | 0.3 |
| 1 | DA_Magisches_Amulett | 1 | 1 | 0.1 |
| 2 | DA_Gift | 1 | 3 | 0.5 |

Diese Items erscheinen **zufällig** — SpawnChance 0.3 = 30% Wahrscheinlichkeit.

### Restock (Händler auffüllen)

Der Händler kann per Blueprint aufgefrischt werden:
```
Get BP_Haendler → Restock Merchant
```
Dies leert das Inventar, setzt Gold zurück und generiert Default + Random Stock neu.

**Event:** `OnMerchantRestocked` wird gefeuert wenn der Restock abgeschlossen ist.

## 11.4 WBP_TradePanel erstellen

1. Content Browser → Rechtsklick → User Interface → **Widget Blueprint**
2. Parent Class: **TradePanelWidget**
3. Name: `WBP_TradePanel`
4. Öffnen und folgende Struktur bauen:

```
[Root] Canvas Panel
 └─ Border (Hintergrund, z.B. dunkelgrau, Padding 10)
     └─ VerticalBox
         ├─ HorizontalBox (Titel-Zeile)
         │   ├─ TextBlock "Handel"
         │   └─ CloseButton (Button mit "X" Text)     ← Name: "CloseButton"
         │
         ├─ HorizontalBox (Haupt-Bereich)
         │   ├─ VerticalBox (Spieler-Seite, Padding Right 10)
         │   │   ├─ TextBlock "Dein Inventar"
         │   │   ├─ PlayerGrid (WBP_Inventory)         ← Name: "PlayerGrid"
         │   │   └─ HorizontalBox
         │   │       ├─ TextBlock "Gold: "
         │   │       └─ PlayerGoldText (TextBlock)     ← Name: "PlayerGoldText"
         │   │
         │   └─ VerticalBox (Händler-Seite)
         │       ├─ MerchantTitle (TextBlock)          ← Name: "MerchantTitle"
         │       ├─ MerchantGrid (WBP_Inventory)       ← Name: "MerchantGrid"
         │       └─ HorizontalBox
         │           ├─ TextBlock "Gold: "
         │           └─ MerchantGoldText (TextBlock)   ← Name: "MerchantGoldText"
         │
         └─ (optional: Info-Text unten)
```

**Wichtig:** Die Widget-Namen müssen EXAKT stimmen:
- `PlayerGrid`, `MerchantGrid` — müssen vom Typ **WBP_Inventory** sein
- `PlayerGoldText`, `MerchantGoldText`, `MerchantTitle` — TextBlocks
- `CloseButton` — Button

5. Im Details-Panel des WBP_TradePanel:

| Property | Wert | Beschreibung |
|----------|------|-------------|
| Cell Size | 64 | Pixelgröße pro Zelle |
| Slot Widget Class | WBP_InventorySlot | Dein Slot-Widget |
| Buy Price Multiplier | 1.0 | Kaufpreis = Itemwert × 1.0 |
| Sell Price Multiplier | 0.5 | Verkaufspreis = Itemwert × 0.5 |

## 11.5 Handel öffnen (Blueprint)

Im ThirdPersonCharacter (oder wo du Interaktion handhabst):

### Methode 1: Mit MerchantActor (empfohlen)

```
Keyboard F (Pressed)    ← oder dein Interaktions-Key
  │
  ├→ Line Trace / Sphere Trace
  │    Start: Camera Location
  │    End: Camera Location + Forward × 300
  │
  ├→ Hit Actor → Cast to BP_Haendler (MerchantActor)
  │
  └─ Cast Success:
      │
      ├→ Create Widget (Class: WBP_TradePanel)
      │    → Promote to Variable: "TradePanel"
      │
      ├→ TradePanel → Open Trade With Merchant
      │    Player Inv: Get Grid Inventory Component (Self)
      │    Merchant: BP_Haendler (Cast-Ergebnis)
      │
      ├→ TradePanel → Add to Viewport
      │
      └→ Set Input Mode Game And UI (Hide Cursor: false)
```

> **Vorteil:** `Open Trade With Merchant` übernimmt automatisch den MerchantName,
> BuyPriceMultiplier und SellPriceMultiplier vom MerchantActor.
> Kein manuelles Setzen der Preise nötig!

### Methode 2: Manuell mit beliebigem Inventar

```
TradePanel → Open Trade
    Player Inv: Get Grid Inventory Component (Self)
    Merchant Inv: Anderes GridInventoryComponent
    Merchant Name: "Kräuterhändler"
```

Diese Methode funktioniert mit jedem GridInventoryComponent, nicht nur MerchantActors.

## 11.6 Items kaufen und verkaufen

Das TradePanelWidget hat zwei Blueprint-Nodes:

| Node | Beschreibung |
|------|-------------|
| **Buy Item** (FGuid) | Kauft Item vom Händler. Zieht Gold vom Spieler ab, überträgt Item. |
| **Sell Item** (FGuid) | Verkauft Item an Händler. Gibt Gold dem Spieler, überträgt Item. |

Du kannst diese z.B. auf den Rechtsklick im Trade-Modus binden, oder über einen "Kaufen"/"Verkaufen" Button.

**Beispiel — Kauf per Rechtsklick auf Händler-Item:**
Im WBP_TradePanel Event Graph → Override `On Item Right Clicked` auf dem MerchantGrid:

```
Event On Item Right Clicked (MerchantGrid)
  │
  ├→ Item → Break → Unique ID
  │
  └→ Buy Item (Unique ID)
       │
       ├→ True: Print String "Gekauft!"
       └→ False: Print String "Nicht genug Gold oder Platz!"
```

**Verkauf per Rechtsklick auf eigenes Item:**
```
Event On Item Right Clicked (PlayerGrid)
  │
  ├→ Item → Break → Unique ID
  │
  └→ Sell Item (Unique ID)
```

## 11.7 Events überschreiben

Im WBP_TradePanel kannst du diese Events in Blueprint überschreiben:

| Event | Wann? | Nutzen |
|-------|-------|--------|
| On Trade Opened | Nach OpenTrade | Sound abspielen, Animation |
| On Trade Closed | Nach CloseTrade | Input zurücksetzen |
| On Item Bought | Nach erfolgreichem Kauf | "Ka-Ching!" Sound, Partikel |
| On Item Sold | Nach erfolgreichem Verkauf | Feedback |

---

# 12. Container Panel Widget

Das Container Panel zeigt zwei Inventare nebeneinander: dein Inventar und den Inhalt einer Truhe/Container.

## 12.1 WBP_ContainerPanel erstellen

1. Content Browser → Rechtsklick → User Interface → **Widget Blueprint**
2. Parent Class: **ContainerPanelWidget**
3. Name: `WBP_ContainerPanel`
4. Struktur:

```
[Root] Canvas Panel
 └─ Border (Hintergrund)
     └─ VerticalBox
         ├─ HorizontalBox (Titel-Zeile)
         │   ├─ ContainerTitle (TextBlock)          ← Name: "ContainerTitle"
         │   └─ CloseButton (Button mit "X")        ← Name: "CloseButton"
         │
         └─ HorizontalBox (Haupt-Bereich)
             ├─ VerticalBox
             │   ├─ TextBlock "Dein Inventar"
             │   └─ PlayerGrid (WBP_Inventory)      ← Name: "PlayerGrid"
             │
             └─ VerticalBox
                 ├─ TextBlock "Container"
                 └─ ContainerGrid (WBP_Inventory)   ← Name: "ContainerGrid"
```

5. Im Details-Panel:

| Property | Wert |
|----------|------|
| Cell Size | 64 |
| Slot Widget Class | WBP_InventorySlot |

## 12.2 Container öffnen (Blueprint)

Im ThirdPersonCharacter:

```
E gedrückt → Cast to InventoryContainer → Try Open (Self)
  │
  Branch (Return):
  │
  ├→ True:
  │   ├→ Create Widget (WBP_ContainerPanel) → Variable "ContainerPanel"
  │   ├→ ContainerPanel → Open Container
  │   │    Container: Cast Result (InventoryContainer)
  │   │    Player Inventory: Get Grid Inventory Component (Self)
  │   ├→ Add to Viewport
  │   └→ Set Input Mode Game And UI
  │
  └→ False: Print "Verschlossen!"
```

## 12.3 Drag & Drop zwischen Inventaren

Drag & Drop funktioniert automatisch! Items zwischen Player-Grid und Container-Grid ziehen "just works" — das GridInventoryWidget unterstützt Cross-Inventory-Transfers.

---

# 13. Equipment Panel Widget

Das Equipment Panel zeigt alle Equipment-Slots dynamisch an.

## 13.1 WBP_EquipmentPanel erstellen

1. Content Browser → Rechtsklick → User Interface → **Widget Blueprint**
2. Parent Class: **EquipmentPanelWidget**
3. Name: `WBP_EquipmentPanel`
4. Struktur:

```
[Root] Canvas Panel
 └─ Border (Hintergrund)
     └─ VerticalBox
         ├─ TextBlock "Ausrüstung"
         └─ SlotContainer (VerticalBox oder WrapBox)  ← Name: "SlotContainer"
```

> **Wichtig:** Der `SlotContainer` wird automatisch mit Equipment-Slot-Widgets befüllt. Du musst die Slots NICHT manuell hinzufügen — das macht der C++ Code basierend auf deinen Slot Definitions.

5. Im Details-Panel:

| Property | Wert |
|----------|------|
| Equipment Slot Size | 80 (Pixelgröße pro Slot) |
| Slot Widget Class | WBP_EquipmentSlot |

## 13.2 Equipment Panel einrichten (Blueprint)

Im ThirdPersonCharacter → Event Begin Play (nach dem Inventar-Widget):

```
Event Begin Play
  │
  ├→ Create Widget (WBP_EquipmentPanel) → Variable "EquipmentPanel"
  │
  ├→ EquipmentPanel → Set Equipment Component
  │    Equipment Comp: Get Equipment Component (Self)
  │
  ├→ EquipmentPanel → Set Linked Inventory
  │    Inventory: Get Grid Inventory Component (Self)
  │
  ├→ Add to Viewport
  │
  └→ EquipmentPanel → Refresh Slots
```

Die Slots werden automatisch aus deinen `Slot Definitions` (auf dem EquipmentComponent) erstellt.

---

# 14. Kontext-Menü

## 14.1 Automatisches Rechtsklick-Menü

Das GridInventoryWidget zeigt automatisch ein Kontext-Menü bei Rechtsklick auf ein Item. Du musst nichts extra einrichten — es funktioniert out-of-the-box.

**Standard-Aktionen:**

| Aktion | Bedingung | Was passiert |
|--------|-----------|-------------|
| **Benutzen** | Item ist `bIsConsumable` | `Consume Item` wird aufgerufen |
| **Drehen** | Item hat `bCanRotate` | Item um 90° drehen |
| **Wegwerfen** | Immer | Item wird in die Welt gedroppt |
| **Info** | Immer | Item-Details auf dem Bildschirm |

## 14.2 Eigenes Kontext-Menü gestalten

Willst du das Menü visuell anpassen?

1. Erstelle: `WBP_ContextMenu` (Parent: **InventoryContextMenuWidget**)
2. Gestalte es nach Belieben — die Logik (Buttons, Aktionen) wird vom C++ Code aufgebaut
3. Setze die Klasse in deinem GridInventoryWidget als Context Menu Class

> **Tipp:** Das Kontext-Menü wird über `Init Menu` vor dem `Add to Viewport` befüllt. Die Buttons werden programmatisch erzeugt.

---

# 15. Debug-System

Alle Commands in der Konsole (Tilde `~`). Nur in Non-Shipping Builds verfügbar.

## 15.1 Wichtigste Commands

| Command | Beschreibung |
|---------|-------------|
| `Inv.Add <ItemID> [Count]` | Item ins Inventar |
| `Inv.Remove <ShortUID> [Count]` | Item entfernen (UID aus Inv.List) |
| `Inv.List` | Alle Items anzeigen |
| `Inv.Find <Text>` | Nach Name suchen |
| `Inv.Info <ShortUID>` | Detail-Info zu einem Item |
| `Inv.Clear` | Inventar leeren |
| `Inv.Gold.Add <Amount>` | Gold hinzufügen (positiv) oder abziehen (negativ) |
| `Inv.Gold.Set <Amount>` | Gold auf exakten Wert setzen |
| `Inv.ImportJSON <Pfad>` | JSON-Datei importieren |

## 15.2 Gold-Commands Beispiele

```
Inv.Gold.Set 1000         → Gold = 1000
Inv.Gold.Add 500          → Gold = 1500
Inv.Gold.Add -200         → Gold = 1300
Inv.Gold.Set 0            → Gold = 0
```

## 15.3 Alle Commands

| Kategorie | Commands |
|-----------|----------|
| **Items** | Inv.Add, Inv.AddAt, Inv.Remove, Inv.Consume, Inv.Drop, Inv.Give |
| **Equipment** | Inv.Equip, Inv.Unequip, Inv.ListSlots |
| **Merge** | Inv.Merge, Inv.SetClass |
| **Gold** | Inv.Gold.Add, Inv.Gold.Set |
| **Einstellungen** | Inv.SetMaxWeight, Inv.SetGridSize, Inv.Clear, Inv.Sort |
| **Info** | Inv.List, Inv.ListByType, Inv.Find, Inv.Info, Inv.ItemDB |
| **Debug** | Inv.Debug, Inv.Debug.Bitmap, Inv.Debug.FreeRects, Inv.Debug.Performance, Inv.Debug.Memory, Inv.Debug.Slots |
| **Container** | Inv.Container.List, Inv.Container.Open, Inv.Container.Loot |

> **Tipp:** Die ShortUID ist der 8-stellige Hex-Code am Anfang jeder Zeile bei `Inv.List`. Du brauchst nicht die ganze UUID, nur die ersten 4-8 Zeichen.

---

# 16. Blueprint Function Library

20+ Blueprint-Nodes für Suche, Filter und Stats:

## 16.1 Suche & Filter

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

## 16.2 Stats & Utilities

| Node | Input | Output | Beschreibung |
|------|-------|--------|-------------|
| GetTotalItemCount | Inventory, ItemDef | int | Menge eines Items |
| GetUniqueItemTypeCount | Inventory | int | Verschiedene Item-Typen |
| GetItemEffectValue | Item, EffectID | float | Aufgelöster Effekt-Wert |
| GetAllItemEffects | Item | Array | Alle Effekte eines Items |
| GetItemDisplayName | Item | Text | Name mit Klassen-Suffix |
| SortInventory | Inventory | void | Nach Größe sortieren |

---

# 17. Performance-Tipps

## 17.1 Soft References

Alle Icons und Actor-Klassen sind Soft References. Das bedeutet: nur das was gerade sichtbar ist, wird geladen. Bei 160+ Zutaten spart das Gigabytes an RAM.

Du kannst den Status prüfen: `Inv.Debug.Memory`

## 17.2 Virtualisiertes Grid

Das UI erstellt nur Widgets für sichtbare Zellen. Bei einem 60×80 Grid sind das ~300 Widgets statt 4.800. Status prüfen: `Inv.Debug.Slots`

## 17.3 Grid-Größe wählen

| Verwendung | Grid-Größe | Zellen | Empfehlung |
|-----------|-----------|--------|-----------|
| Schnell testen | 10×10 | 100 | Zum Entwickeln |
| Spieler-Inventar | 20×15 | 300 | Guter Standard |
| Großes RPG | 40×30 | 1.200 | Für viele Items |
| Maximum (wie Alchimist) | 60×80 | 4.800 | Braucht Virtualisierung |

## 17.4 Batch-Operationen

Wenn du viele Items auf einmal hinzufügst (z.B. Loot), nutze `TryAddItemsBatch` statt einzelne `TryAddItem`-Calls. Das verhindert 10× UI-Refresh bei 10 Items.

## 17.5 Async Loot

Container generieren Loot automatisch async — kein Frame-Drop beim Öffnen einer Truhe.

---

# 18. Häufige Probleme & Lösungen

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
      BP_Haendler.uasset          (MerchantActor)
    UI/
      WBP_Inventory.uasset
      WBP_InventorySlot.uasset
      WBP_EquipmentSlot.uasset
      WBP_EquipmentPanel.uasset
      WBP_ContainerPanel.uasset
      WBP_TradePanel.uasset
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

*Alchemy Fox Studio — Grid Inventory Plugin v1.0 — März 2026*
