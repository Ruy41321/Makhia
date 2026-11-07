// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

using UnrealBuildTool;

/**
 * Discord integration (Discord Social SDK, plugin DiscordPartnerSDK): rich presence of the local player. Win64 only:
 * listed in Makhia.uproject and in the targets for that platform. Full model: see Documentation/DiscordIntegration.md.
 */
public class MakhiaDiscord : ModuleRules
{
	public MakhiaDiscord(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "Makhia" });

		// Discord Social SDK and its Unreal wrapper (UDiscordClient, UDiscordActivity...).
		PrivateDependencyModuleNames.AddRange(new string[] { "DiscordPartnerSDK" });
	}
}
