// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

using UnrealBuildTool;

public class Makhia : ModuleRules
{
	public Makhia(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayAbilities", "GameplayTasks", "GameplayTags", "NetCore",
			"OnlineSubsystem", "OnlineSubsystemUtils", "DeveloperSettings", 
			"AIModule", "StateTreeModule", "GameplayStateTreeModule", "PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AnimGraphRuntime", "AudioModulation", "UMG", "Slate", "SlateCore",
			"MoviePlayer", "NavigationSystem", "LevelSequence", "MovieScene", 
			"CoreOnline", "AudioMixer", "AudioMixerCore", "SignalProcessing", "AudioCaptureCore", "Voice"
		});
	}
}
