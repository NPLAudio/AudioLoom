// Copyright (c) 2026 AudioLoom Contributors.

using UnrealBuildTool;

public class AudioLoomEditor : ModuleRules
{
	public AudioLoomEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AudioLoom",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"PropertyEditor",
			"ContentBrowser",
			"DetailCustomizations",
			"EditorStyle",
			"LevelEditor",
			"WorkspaceMenuStructure",
			"EditorSubsystem",
		});
	}
}
