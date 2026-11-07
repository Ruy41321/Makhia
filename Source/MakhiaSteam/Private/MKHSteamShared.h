// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class IOnlineSubsystem;

/** Shared helpers of the Steam backend. Private to MakhiaSteam: no Steam type leaves this module. */
namespace MKHSteam
{
	/** Name of the Steam online subsystem (STEAM_SUBSYSTEM). */
	extern const FName SubsystemName;

	/**
	 * Returns the Steam online subsystem, or null when Steam is not running, the game was started with -nosteam or
	 * this is an editor process (the subsystem only runs in game processes).
	 */
	IOnlineSubsystem* GetSubsystem();

	/** Local player index used for every online call. Makhia has a single local player. */
	static constexpr int32 LocalUserNum = 0;

	/**
	 * Parses a SteamID64 written in decimal (the NetId format of the Steam backend).
	 *
	 * @param UserId  Decimal SteamID64.
	 * @return The id, or 0 when the string is not a number.
	 */
	uint64 ParseSteamId64(const FString& UserId);
}
