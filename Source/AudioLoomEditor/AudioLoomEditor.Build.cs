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
			"AudioLoom", // runtime types: UAudioLoomComponent, enumerators, etc.
		});

		// Editor-only: Slate UI, details panel, tabs, workspace menu
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"PropertyEditor",      // IDetailCustomization, property handles
			"ContentBrowser",      // object picker widgets
			"DetailCustomizations",
			"EditorStyle",
			"LevelEditor",         // GEditor, selection, viewport focus
			"WorkspaceMenuStructure",
			"EditorSubsystem",
		});
	}
}
