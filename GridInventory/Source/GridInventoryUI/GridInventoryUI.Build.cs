using UnrealBuildTool;

public class GridInventoryUI : ModuleRules
{
	public GridInventoryUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"GridInventoryRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore"
		});
	}
}
