// Copyright Marco. All Rights Reserved.

#include "GridInventoryEditor.h"
#include "InventoryJsonImporter.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGridInventoryEditorModule"

void FGridInventoryEditorModule::StartupModule()
{
	// Register console commands for the JSON importer
	UInventoryJsonImporter::RegisterConsoleCommands();

	UE_LOG(LogTemp, Log, TEXT("[GridInventoryEditor] Module loaded. Use 'Inv.ImportJSON <path>' in console."));
}

void FGridInventoryEditorModule::ShutdownModule()
{
	UInventoryJsonImporter::UnregisterConsoleCommands();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGridInventoryEditorModule, GridInventoryEditor)
