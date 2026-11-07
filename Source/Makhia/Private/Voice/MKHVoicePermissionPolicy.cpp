// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Voice/MKHVoicePermissionPolicy.h"

EMKHVoiceBlockReason FMKHVoicePermissionPolicy::Evaluate(const FMKHVoicePeerView& Listener, const FMKHVoicePeerView& Other, bool bKnownPair)
{
	if (!Listener.bHasPreferences || !Listener.Preferences.bVoiceEnabled)
	{
		return EMKHVoiceBlockReason::VoiceDisabled;
	}
	if (!Other.bHasPreferences || !Other.Preferences.bVoiceEnabled)
	{
		return EMKHVoiceBlockReason::OtherVoiceDisabled;
	}
	if (HasMuted(Listener, Other.PlayerId))
	{
		return EMKHVoiceBlockReason::MutedByYou;
	}
	if (HasMuted(Other, Listener.PlayerId))
	{
		return EMKHVoiceBlockReason::Unavailable;
	}
	if (bKnownPair)
	{
		return EMKHVoiceBlockReason::None;
	}
	if (!Listener.Preferences.bAllowNonFriends)
	{
		return EMKHVoiceBlockReason::NonFriendsNotAllowed;
	}
	return Other.Preferences.bAllowNonFriends ? EMKHVoiceBlockReason::None : EMKHVoiceBlockReason::OtherNonFriendsNotAllowed;
}

bool FMKHVoicePermissionPolicy::HasMuted(const FMKHVoicePeerView& Muter, const FString& MutedId)
{
	return Muter.Preferences.MutedPlayerIds.Contains(MutedId);
}
