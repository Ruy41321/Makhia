// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Voice/MKHVoiceTypes.h"

/** What the server knows about one player when it decides a voice pair. */
struct MAKHIA_API FMKHVoicePeerView
{
	/** NetId of the player. */
	FString PlayerId;

	/** Choices the player's machine sent; meaningless while bHasPreferences is false. */
	FMKHVoicePreferences Preferences;

	/** False until the player's machine sent its choices: the voice stays closed meanwhile. */
	bool bHasPreferences = false;
};

/**
 * The voice permission rule (pure computation, server side): two players hear each other only when both turned the
 * voice chat on, neither muted or blocked the other, and they are a known pair (party or platform friends) or both
 * allow voice with non-friends. Bots and matches against the bot never reach it (no voice component).
 * Full model: see Documentation/VoiceChat.md.
 */
struct MAKHIA_API FMKHVoicePermissionPolicy
{
	/**
	 * Decides the voice between two players, seen from the listener (the reason goes to the listener's panel).
	 * The decision is symmetric: Evaluate(A, B) is None exactly when Evaluate(B, A) is None.
	 *
	 * @param Listener     Player the reason is reported to.
	 * @param Other        The other player of the pair.
	 * @param bKnownPair   True when the two are in the same party or friends on the platform.
	 * @return None when the voice is open, otherwise the first reason that closes it.
	 */
	static EMKHVoiceBlockReason Evaluate(const FMKHVoicePeerView& Listener, const FMKHVoicePeerView& Other, bool bKnownPair);

private:
	/** Returns true when Muter muted (or blocked) the player MutedId. */
	static bool HasMuted(const FMKHVoicePeerView& Muter, const FString& MutedId);
};
