// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

using UnrealBuildTool;

/**
 * Steam online backend (OnlineSubsystemSteam, SteamSockets, Steamworks SDK of the engine). Win64 only: listed in
 * Makhia.uproject and in the targets for that platform. Full model: see Documentation/OnlineBackends.md.
 */
public class MakhiaSteam : ModuleRules
{
	public MakhiaSteam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "Makhia", "OnlineSubsystem", "OnlineSubsystemUtils"
		});

		// Steam online subsystem, shared Steam types (protocol names) and the Steamworks SDK (networking config).
		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemSteam", "SteamShared", "Sockets", "NetCore" });
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
