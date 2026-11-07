// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "AudioDeviceNotificationSubsystem.h"
#include "Online/Backend/MKHVoiceProvider.h"
#include "Containers/Ticker.h"
#include "MKHVoiceProvider_OSS.generated.h"

class IOnlineVoice;
class UVoipListenerSynthComponent;
namespace Audio { class FPatchMixerSplitter; }

/**
 * Voice provider on the native voice interface of the engine online subsystems (IOnlineVoice of
 * OnlineSubsystemUtils): capture and codec of Unreal, packets on the voice channel of the game connection, relayed
 * by the listen server. The same implementation serves the Null (LAN) and the Steam backend: FVoiceEngineSteam
 * derives from the engine one and only adds the Steam "in game voice speaking" flag.
 *
 * It uses the voice interface of the default online subsystem, the same one the engine net code feeds, so it works
 * only while that subsystem has a session (the voice interface does not tick otherwise).
 * Full model: see Documentation/VoiceChat.md.
 */
UCLASS()
class MAKHIA_API UMKHVoiceProvider_OSS : public UMKHVoiceProvider
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Follows the world cleanups (ReleaseWorldPlaybacks). */
	virtual void InitializeProvider() override;

	/** Stops the transmission. */
	virtual void ShutdownProvider() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** True when the default online subsystem has a voice interface. */
	virtual bool IsVoiceAvailable() const override;
	/** Starts or stops the networked voice of the local talker (registered on first use). */
	virtual void SetTransmitting(bool bTransmit) override;
	/** Registers the remote talker with the voice interface. */
	virtual void RegisterRemoteTalker(const FUniqueNetIdRepl& PlayerId) override;
	/** Unregisters the remote talker. */
	virtual void UnregisterRemoteTalker(const FUniqueNetIdRepl& PlayerId) override;
	/** Local (not system wide) mute of the remote talker. */
	virtual void SetRemoteTalkerMuted(const FUniqueNetIdRepl& PlayerId, bool bMuted) override;
	/** True while transmitting and the voice interface reports the local talker as talking. */
	virtual bool IsLocalPlayerTalking() const override;
	/** Talking state of the remote talker reported by the voice interface. */
	virtual bool IsRemotePlayerTalking(const FUniqueNetIdRepl& PlayerId) const override;

	/**
	 * Plays the remote talkers' audio on a stream of its own (FMKHVoiceOutputEndpoint) and mutes them in the game
	 * mix; an empty name follows the Windows default output. Never the engine's PatchRemoteTalkerOutputToEndpoint:
	 * see Documentation/VoiceChat.md -> Voice output stream.
	 */
	virtual void SetVoiceOutputDevice(const FString& DeviceName) override;

	/** Gain of the voice stream: Volume x MaxOutputGain. */
	virtual void SetOutputVolume(float Volume) override;

	/** Peak of the voice stream since the previous call (FMKHVoiceOutputEndpoint::ConsumePeakLevel). */
	virtual float ConsumeOutputPeakLevel() override;

	/** Creates and starts a capture of the default microphone (IVoiceCapture of the Voice module). */
	virtual bool StartMicrophoneTest() override;

	/** Stops and releases the test capture. */
	virtual void StopMicrophoneTest() override;

	/** Drains the test capture and returns its current amplitude. */
	virtual float GetMicrophoneTestLevel() override;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns the voice interface of the default online subsystem of the game world (null without voice). */
	TSharedPtr<class IOnlineVoice, ESPMode::ThreadSafe> GetVoiceInterface() const;

	/** Registers the local talker once per voice interface (the capture device is created there, not opened). */
	void EnsureLocalTalker(const TSharedPtr<class IOnlineVoice, ESPMode::ThreadSafe>& Voice);

	/**
	 * Follows the talking notifications of the current voice interface (bound again when the interface changes).
	 * The engine's IsRemotePlayerTalking stays true while a talker buffer exists and IsLocalPlayerTalking never
	 * reports user 0, so the talking state comes from OnPlayerTalkingStateChanged instead.
	 *
	 * @param Voice  Current voice interface.
	 */
	void BindTalkingNotifications(const TSharedPtr<class IOnlineVoice, ESPMode::ThreadSafe>& Voice) const;

	/**
	 * Voice interface notification: a talker started or stopped talking.
	 *
	 * @param TalkerId    Local or remote talker.
	 * @param bIsTalking  New state.
	 */
	void HandleTalkingStateChanged(TSharedRef<const FUniqueNetId> TalkerId, bool bIsTalking) const;

	/** Returns the unique id of the local player on the default online subsystem, empty when unknown. */
	FString GetLocalPlayerId() const;

	/**
	 * Follows the session creations and joins of the default online subsystem: they register the local talker again,
	 * and the engine then starts its networked voice (FOnlineVoiceImpl::RegisterLocalTalker). The microphone is closed
	 * again unless the voice chat transmits.
	 */
	void BindSessionNotifications() const;

	/** Session created or joined: closes the networked voice the session code may have started. */
	void HandleSessionRegisteredLocalTalker(FName SessionName, bool bWasSuccessful) const;

	/** Join variant of HandleSessionRegisteredLocalTalker. */
	void HandleSessionJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result) const;

	/**
	 * voice.MuteAudioEngineOutput: silences the remote talkers in the game mix while they play on the separate device.
	 *
	 * @param bMuted  True while the separate output plays.
	 */
	static void SetGameMixVoiceMuted(bool bMuted);

	/**
	 * Opens the voice stream on AppliedOutputDevice, fed by the remote talkers' mix of the voice interface.
	 *
	 * @param Voice  Current voice interface.
	 */
	void OpenOutputEndpoint(IOnlineVoice& Voice);

	/** Follows the Windows default output changes (once). */
	void BindDefaultDeviceNotifications();

	/**
	 * Windows default output changed (game thread): reopens the stream when it follows the default.
	 *
	 * @param Role      Device role that changed.
	 * @param DeviceId  New default device.
	 */
	void HandleDefaultRenderDeviceChanged(EAudioDeviceChangedRole Role, FString DeviceId);

	/**
	 * Ticker while the voice stream is open: detaches the voice playbacks the engine has dropped (DetachDroppedPlaybacks).
	 *
	 * @param DeltaTime  Seconds since the previous tick.
	 * @return True to keep ticking.
	 */
	bool HandleStreamTick(float DeltaTime);

	/**
	 * The engine drops a talker's playback (UVoipListenerSynthComponent) after 0.4 s of silence and makes a new one on
	 * the next packet, but the dropped one stays an input of the remote talkers' mix until garbage collection: the mix
	 * forwards only what every input has pushed, so the voice stream went silent after the first pause. Each dropped
	 * (unregistered) playback is moved to a mix nobody reads, ConnectToSplitter being its only public hook.
	 * See Documentation/VoiceChat.md -> Engine pitfalls.
	 */
	void DetachDroppedPlaybacks();

	/**
	 * World cleanup (seamless travel lobby -> arena included): unregisters the voice playbacks still registered in
	 * that world. The engine stops a removed talker's playback without unregistering it and resets the others only
	 * after the next map load, so the old world was cleaned with them registered: ensure in ValidateComponentsRegistration,
	 * then an access violation (2-PC test, entering a PvP match). See Documentation/VoiceChat.md -> Engine pitfalls.
	 *
	 * @param World           World being cleaned up.
	 * @param bSessionEnded   Unused.
	 * @param bCleanupResources  Unused.
	 */
	void ReleaseWorldPlaybacks(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	/** Voice stream gain at a full volume slider: the engine voice is quiet next to the game mix. */
	static constexpr float MaxOutputGain = 4.f;

private:
	/** True while the microphone is open for the network. */
	bool bTransmitting = false;

	/** Voice interface the local talker was registered with (a new interface after a restart needs it again). */
	TWeakPtr<class IOnlineVoice, ESPMode::ThreadSafe> RegisteredVoice;

	/** Voice interface whose talking notifications are followed. */
	mutable TWeakPtr<class IOnlineVoice, ESPMode::ThreadSafe> NotifyingVoice;

	/** Handle of the talking notification on NotifyingVoice. */
	mutable FDelegateHandle TalkingHandle;

	/** Session interface whose creations and joins are followed. */
	mutable TWeakPtr<class IOnlineSession, ESPMode::ThreadSafe> WatchedSessions;

	/** Handles of the session notifications on WatchedSessions. */
	mutable FDelegateHandle CreateSessionHandle;
	mutable FDelegateHandle JoinSessionHandle;

	/** Ids (ToString) of the talkers currently talking, the local one included. */
	mutable TSet<FString> TalkingIds;

	/** Remote talkers registered through this provider, by id (ToString). */
	TMap<FString, FUniqueNetIdRepl> RemoteTalkers;

	/** Device the remote voice is patched to; empty for the game mix. */
	FString AppliedOutputDevice;

	/** Voice interface the endpoint was patched on (a new interface starts without endpoints). */
	TWeakPtr<class IOnlineVoice, ESPMode::ThreadSafe> EndpointVoice;

	/** Voice stream, null before the voice interface exists. */
	TSharedPtr<class FMKHVoiceOutputEndpoint> OutputEndpoint;

	/** Gain of the voice stream (SetOutputVolume). */
	float OutputGain = MaxOutputGain;

	/** Handle of the Windows default output notification. */
	FDelegateHandle DefaultDeviceHandle;

	/** Handle of the world cleanup notification. */
	FDelegateHandle WorldCleanupHandle;

	/** Handle of HandleStreamTick. */
	FTSTicker::FDelegateHandle StreamTickHandle;

	/** Mix the dropped playbacks are moved to (never read; processed only to release them). */
	TSharedPtr<Audio::FPatchMixerSplitter> DroppedPlaybackSink;

	/** Playbacks already moved to DroppedPlaybackSink. */
	TSet<TWeakObjectPtr<UVoipListenerSynthComponent>> DroppedPlaybacks;

	/** Capture of the microphone test, null when not testing. */
	TSharedPtr<class IVoiceCapture> TestCapture;

	/** Buffer the test capture is drained into (the audio is dropped). */
	TArray<uint8> TestBuffer;
};
