// Copyright Marco. All Rights Reserved.

using UnrealBuildTool;

public class GridInventoryEditor : ModuleRules
{
	public GridInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GridInventoryRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Json",
			"JsonUtilities",
			"AssetTools",
			"AssetRegistry",
		});
	}
}
