// Copyright (c) 2026 AudioLoom Contributors.

using UnrealBuildTool;

public class AudioLoom : ModuleRules
{
	public AudioLoom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// WASAPI requires Windows multimedia
			PublicSystemLibraries.Add("Ole32.lib");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// CoreAudio for device enumeration and output
			PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioToolbox", "CoreFoundation" });
		}
	}
}
