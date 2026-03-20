// Copyright (c) 2026 AudioLoom Contributors.

using System;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class AudioLoom : ModuleRules
{
	public AudioLoom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Modules whose public headers this module includes from its own public headers
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",           // UWorld, UActorComponent, USoundWave, subsystems
			"OSC",              // UOSCManager, server/client (OscSubsystem)
			"Sockets",          // UDP port availability check
			"Networking",       // often pulled with Sockets for address types
			"DeveloperSettings", // UAudioLoomOscSettings
		});

		// Used only from .cpp; not exposed in AudioLoom public API
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// COM (CoCreateInstance) for WASAPI device enumeration / audio client
			PublicSystemLibraries.Add("Ole32.lib");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// Native frameworks for AudioObject* APIs and float output buffers
			PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioToolbox", "CoreFoundation" });
		}

	}
}
