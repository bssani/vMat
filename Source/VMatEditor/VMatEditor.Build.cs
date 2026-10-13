using UnrealBuildTool;

public class VMatEditor : ModuleRules
{
	public VMatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ContentBrowser",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"EditorWidgets",
			"Engine",
			"InputCore",
			"Json",
			"JsonUtilities",
			"Projects",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
