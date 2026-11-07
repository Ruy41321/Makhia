// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"
#include "UObject/Object.h"
#include "MKHVoiceProvider.generated.h"

/**
 * Voice side of the online backend, used only by UMKHVoiceChatSubsystem: opens and closes the microphone, registers
 * the remote talkers, reports who is talking and routes the voice of the others to an output device.
 * This base class is a backend without voice chat (every call is a no-op); UMKHVoiceProvider_OSS implements it on
 * the native voice interface of the engine online subsystems. Nothing else in the game talks to the voice API.
 * Full model: see Documentation/VoiceChat.md.
 */
UCLASS()
class MAKHIA_API UMKHVoiceProvider : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Called once right after creation. */
	virtual void InitializeProvider() {}

	/** Called once before release: closes the microphone and forgets the talkers. */
	virtual void ShutdownProvider() {}

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns true when the backend voice interface exists right now (voice enabled in config, platform running). */
	virtual bool IsVoiceAvailable() const { return false; }

	/**
	 * Opens or closes the transmission of the local microphone to the other players.
	 *
	 * @param bTransmit  True to capture and send, false to stop capturing.
	 */
	virtual void SetTransmitting(bool bTransmit) {}

	/**
	 * Makes a remote player known to the voice interface, so their talking state is reported and they can be muted.
	 * Registering twice is harmless.
	 *
	 * @param PlayerId  Unique net id of the remote player.
	 */
	virtual void RegisterRemoteTalker(const FUniqueNetIdRepl& PlayerId) {}

	/**
	 * Forgets a remote player registered with RegisterRemoteTalker.
	 *
	 * @param PlayerId  Unique net id of the remote player.
	 */
	virtual void UnregisterRemoteTalker(const FUniqueNetIdRepl& PlayerId) {}

	/**
	 * Mutes or unmutes the playback of a remote player on this machine only (the server rule is applied apart).
	 *
	 * @param PlayerId  Unique net id of a registered remote player.
	 * @param bMuted    True to stop playing their voice here.
	 */
	virtual void SetRemoteTalkerMuted(const FUniqueNetIdRepl& PlayerId, bool bMuted) {}

	/** Returns true while the local microphone sends voice (above the silence threshold). */
	virtual bool IsLocalPlayerTalking() const { return false; }

	/**
	 * Returns true while voice of a remote player is being played.
	 *
	 * @param PlayerId  Unique net id of a registered remote player.
	 */
	virtual bool IsRemotePlayerTalking(const FUniqueNetIdRepl& PlayerId) const { return false; }

	/** Returns the level of the local microphone in [0, 1] while transmitting, 0 otherwise. */
	virtual float GetMicrophoneAmplitude() const { return 0.f; }

	/**
	 * Plays the voice of the other players on an output device: a separate one when
	 * FMKHOnlineCapabilities::bSupportsVoiceOutputDevice, else the game's. The same name again does nothing.
	 *
	 * @param DeviceName  Name of the output device as the system lists it; empty for the system default output.
	 */
	virtual void SetVoiceOutputDevice(const FString& DeviceName) {}

	/**
	 * Volume of the other players' voice.
	 *
	 * @param Volume  Slider value in [0, 1].
	 */
	virtual void SetOutputVolume(float Volume) {}

	/** Diagnostics: loudest voice sample played since the previous call, -1 without a voice stream. */
	virtual float ConsumeOutputPeakLevel() { return -1.f; }

	/**
	 * Opens the microphone for the level meter of the settings, apart from the network voice (nothing is sent).
	 *
	 * @return True when a capture started.
	 */
	virtual bool StartMicrophoneTest() { return false; }

	/** Closes the microphone opened by StartMicrophoneTest. */
	virtual void StopMicrophoneTest() {}

	/** Returns the level of the microphone test in [0, 1] (input gain applied), 0 when not testing. */
	virtual float GetMicrophoneTestLevel() { return 0.f; }
};
