// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Voice/MKHVoiceTypes.h"

namespace MKHVoiceLimits
{
	/** Muted players a machine may send (saved mutes of the players present, blocked users included). */
	static constexpr int32 MaxMutedPlayers = 64;

	/** Longest NetId accepted (SteamID64 and Null ids are far shorter). */
	static constexpr int32 MaxPlayerIdLength = 128;
}

bool FMKHVoicePreferences::IsWithinLimits() const
{
	if (MutedPlayerIds.Num() > MKHVoiceLimits::MaxMutedPlayers)
	{
		return false;
	}
	return !MutedPlayerIds.ContainsByPredicate([](const FString& PlayerId)
	{
		return PlayerId.Len() > MKHVoiceLimits::MaxPlayerIdLength;
	});
}
