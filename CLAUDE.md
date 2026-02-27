# CLAUDE.md

Dieses Repository enthält das **GridInventory** Plugin für Unreal Engine 4.27.

## Kritische Regeln

- **UE 4.27 ONLY** — kein UE5 API verwenden
- Kein `GetFont()` auf UTextBlock → nutze `TextBlock->Font`
- Kein `FSavePackageArgs` → alte `SavePackage()` Signatur
- `CreatePackage(nullptr, *Name)` mit nullptr
- `FConsoleCommandWithArgsDelegate` statt `WithWorldAndArgs`
- Variable nie `Slot` nennen in UWidget-Subklassen
- `IsCellFree()` nimmt `FIntPoint`, nicht zwei ints
- `TryGetNumberField()` gibt `double` zurück

## Modul-Struktur

- `Source/GridInventoryRuntime/` — Spiellogik (Grid, Items, Equipment, Container, Loot)
- `Source/GridInventoryUI/` — Widgets (Grid-Widget, Slot-Widget, Drag&Drop)
- `Source/GridInventoryEditor/` — JSON-Importer für 138 Alchemie-Zutaten

## Aktuelles Problem

Maus-Klicks auf dem Inventar-UI funktionieren nicht. Der aktuelle Ansatz:
- GridInventoryWidget fängt ALLE Mouse-Events ab (NativeOnMouseButtonDown etc.)
- Slot-Widgets sind HitTestInvisible (nur visuell)
- Debug-Log ist eingebaut: `[GridInventory] MouseDown at Cell...`
- Nächster Schritt: Testen ob Log erscheint, dann Widget-Hierarchy debuggen

## TODOs (Priorität)

1. Maus-Klick Problem lösen (Debug-Log prüfen)
2. **Save/Load System** — `SaveInventory(int32 SlotIndex)` / `LoadInventory(int32 SlotIndex)` Blueprint-Nodes. Speichert Inventar + Equipment + Container + dynamische Items. Nutzt USaveGame + Slot-basiert.
3. **Dynamische Item-Erstellung** — `URuntimeItemDefinition` (erbt von UInventoryItemDefinition) für zur Laufzeit gebraute Tränke. Vollwertige Items, gleiche Struktur wie DataAsset-Items. Persistenz über JSON inline im SaveGame. GC-sichere Referenz-Haltung im Component.
4. Grid-Lines, Drag&Drop, Rechtsklick-Menü, Tooltip, Icons, Equipment-UI, Container-UI

Siehe `CLAUDE_CODE_INSTRUCTIONS.md` für vollständige Spezifikation der TODOs.

UE4 Editor Konsole (~):
```
Inv.Add Basilikum
Inv.List
Inv.Debug.Bitmap
```

Siehe `CLAUDE_CODE_INSTRUCTIONS.md` für vollständige Dokumentation.
