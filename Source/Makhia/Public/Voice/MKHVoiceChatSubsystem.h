// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Voice/MKHVoiceTypes.h"
#include "MKHVoiceChatSubsystem.generated.h"

class APlayerState;
class UMKHGameUserSettings;
class UMKHVoicePlayerComponent;
class UMKHVoiceProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHVoiceStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHVoiceTalkersChangedSignature);

/**
 * The single entry point of the voice chat for the UI and the gameplay code: local voice state (master switch,
 * microphone mode, session microphone mute, push-to-talk, saved mutes), the players of the current lobby or match
 * with who is talking, and the settings applied to the voice engine. Nothing else talks to the voice API: this
 * subsystem goes through the voice provider of the online backend (UMKHVoiceProvider).
 *
 * The choices that matter to others travel to the server through the voice component of the local player
 * controller (UMKHVoicePlayerComponent); the server answers with its decision for every other player. The
 * microphone only opens in a networked lobby or match where the server opened the voice with someone (never
 * against the bot). Full model: see Documentation/VoiceChat.md.
 */
UCLASS()
class MAKHIA_API UMKHVoiceChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	/** Creates the voice provider of the online backend, applies the saved settings and starts the voice tick. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Closes the microphone and releases the provider. */
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the channel, the master switch, the microphone mode or mute, or the transmission changed. */
	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FMKHVoiceStateChangedSignature OnVoiceStateChanged;

	/** Broadcast when the player list, a talking state, a mute or a server decision changed. */
	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FMKHVoiceTalkersChangedSignature OnTalkersChanged;

	/**
	 * Returns the voice chat subsystem of the game instance of an object.
	 *
	 * @param WorldContextObject  Any object living in a game world.
	 * @return The subsystem, or null outside a game instance.
	 */
	static UMKHVoiceChatSubsystem* Get(const UObject* WorldContextObject);

	/** Returns true when the online backend has voice chat (capability) and its voice interface is up. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsVoiceChatSupported() const;

	/** Returns where the voice chat is connected now. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	EMKHVoiceChannel GetChannel() const { return Channel; }

	/** Returns the master switch (saved setting). */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsVoiceChatEnabled() const;

	/**
	 * Turns the voice chat on or off and saves it (also the quick button of the game menu).
	 *
	 * @param bEnable  True to hear and speak, false for neither.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetVoiceChatEnabled(bool bEnable);

	/** Returns how the microphone transmits (saved setting). */
	UFUNCTION(BlueprintPure, Category = "Voice")
	EMKHVoiceMicMode GetMicMode() const;

	/**
	 * Changes how the microphone transmits and saves it.
	 *
	 * @param NewMode  Open mic, push-to-talk or disabled (listen only).
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetMicMode(EMKHVoiceMicMode NewMode);

	/** Returns true when voice with players who are neither friends nor party is accepted (saved setting). */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool AllowsVoiceWithNonFriends() const;

	/**
	 * Accepts or refuses voice with players who are neither friends nor party, and saves it.
	 *
	 * @param bAllow  True to accept (both players must accept).
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetAllowVoiceWithNonFriends(bool bAllow);

	/** Returns true while the local microphone is muted for this session. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsMicMuted() const { return bMicMuted; }

	/** Returns true when the microphone mute button has an effect (voice on, microphone mode not disabled). */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool CanMuteMic() const;

	/**
	 * Mutes or unmutes the local microphone for this session (the saved microphone mode does not change).
	 *
	 * @param bMuted  True to stop transmitting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetMicMuted(bool bMuted);

	/**
	 * Push-to-talk key state (the voice component binds the input action).
	 *
	 * @param bHeld  True while the key is held.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetPushToTalkHeld(bool bHeld);

	/** Returns true while the local microphone is open for the network. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsTransmitting() const { return bTransmitting; }

	/** Returns true while the local player's voice is being sent (transmitting and above the silence threshold). */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsLocalPlayerTalking() const;

	/** Returns the players of the current lobby or match, the local player first. */
	UFUNCTION(BlueprintPure, Category = "Voice")
	TArray<FMKHVoiceTalkerInfo> GetTalkers() const { return Talkers; }

	/**
	 * Returns the row of one player of the current lobby or match.
	 *
	 * @param PlayerId  NetId of the player.
	 * @param OutInfo   The row when found.
	 * @return True when the player is listed.
	 */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool FindTalker(const FString& PlayerId, FMKHVoiceTalkerInfo& OutInfo) const;

	/**
	 * Mutes or unmutes another player for the local player; saved by platform id between sessions.
	 *
	 * @param PlayerId  NetId of the other player.
	 * @param bMuted    True to mute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SetPlayerMuted(const FString& PlayerId, bool bMuted);

	/**
	 * Returns true when the local player muted (saved) or blocked (platform) another player.
	 *
	 * @param PlayerId  NetId of the other player.
	 */
	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsPlayerMuted(const FString& PlayerId) const;

	/**
	 * Opens the microphone for the level meter of the settings (nothing is sent). Calls are counted: every
	 * StartMicrophoneTest needs a StopMicrophoneTest.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice|Settings")
	void StartMicrophoneTest();

	/** Closes the microphone opened by StartMicrophoneTest once the last user stopped. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Settings")
	void StopMicrophoneTest();

	/** Returns the microphone level of the test in [0, 1], on the scale of the activation threshold setting. */
	UFUNCTION(BlueprintPure, Category = "Voice|Settings")
	float GetMicrophoneTestLevel() const;

	/** Pushes the saved microphone settings (gain, activation level) to the voice engine. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void ApplyVoiceSettings();

	/**
	 * The voice component of the local player controller began play.
	 *
	 * @param Component  Voice component of the local player controller.
	 */
	void RegisterLocalComponent(UMKHVoicePlayerComponent* Component);

	/**
	 * The voice component of the local player controller ended play (travel, return to the lobby).
	 *
	 * @param Component  Voice component of the local player controller.
	 */
	void UnregisterLocalComponent(UMKHVoicePlayerComponent* Component);

	/** Logs the whole local voice state (console MKH.Voice.Dump). */
	void DumpState() const;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Ticker: follows the world, sends the choices, opens or closes the microphone and refreshes the players. */
	bool HandleVoiceTick(float DeltaTime);

	/** Rebuilds the player rows from the game state, registers the remote talkers and applies the local mutes. */
	void RefreshTalkers();

	/**
	 * Builds the row of one player.
	 *
	 * @param PlayerState     Player state of the player.
	 * @param bIsLocalPlayer  True for the local player.
	 * @return The row.
	 */
	FMKHVoiceTalkerInfo MakeTalkerInfo(const APlayerState& PlayerState, bool bIsLocalPlayer) const;

	/**
	 * Registers a remote player with the voice provider once, and mutes their playback here when the server closed
	 * the voice or the local player muted them.
	 *
	 * @param PlayerState  Player state of the remote player.
	 * @param Info         Row of the player.
	 */
	void SyncRemoteTalker(const APlayerState& PlayerState, const FMKHVoiceTalkerInfo& Info);

	/**
	 * Unregisters the remote talkers that are not in the current world anymore.
	 *
	 * @param PresentIds  NetIds of the remote players present.
	 */
	void ForgetAbsentTalkers(const TSet<FString>& PresentIds);

	/**
	 * Keeps a talker "talking" for TalkingHoldSeconds after the voice engine reports a pause, so the talking icons do
	 * not flicker between words.
	 *
	 * @param InOutTalkers  Talkers just built; their bIsTalking is extended.
	 */
	void HoldTalkingStates(TArray<FMKHVoiceTalkerInfo>& InOutTalkers);

	/**
	 * Logs the remote players who started or stopped talking since the previous refresh.
	 *
	 * @param NewTalkers  Talkers just built (held states).
	 */
	void LogTalkingChanges(const TArray<FMKHVoiceTalkerInfo>& NewTalkers) const;

	/** Returns where the voice chat is connected, from the world and the players present. */
	EMKHVoiceChannel ComputeChannel() const;

	/** Builds the choices sent to the server (mutes limited to the players present, blocked users included). */
	FMKHVoicePreferences BuildPreferences() const;

	/** Returns true when the microphone must be open for the network now. */
	bool ShouldTransmit() const;

	/** Opens or closes the microphone to match ShouldTransmit. */
	void UpdateTransmission();

	/** Sends the voice of the others to the saved voice output device (or the game mix) with the saved volume. */
	void ApplyOutputRouting() const;

	/** Sets a new channel and broadcasts it. */
	void SetChannel(EMKHVoiceChannel NewChannel);

	/** Returns the saved settings object (null before the engine created it). */
	static UMKHGameUserSettings* GetSettings();

	/** Saves the settings to disk. */
	static void SaveSettings();

	// ============================================================
	// Properties
	// ============================================================

	/** Voice provider of the online backend. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHVoiceProvider> Provider;

	/** Voice component of the local player controller of the current world. */
	TWeakObjectPtr<UMKHVoicePlayerComponent> LocalComponent;

private:
	/** Players of the current lobby or match, the local player first. */
	TArray<FMKHVoiceTalkerInfo> Talkers;

	/** Remote players registered with the provider, by NetId. */
	TMap<FString, FUniqueNetIdRepl> RegisteredTalkers;

	/** Last time each player was reported talking (FPlatformTime::Seconds), by NetId. */
	TMap<FString, double> LastTalkingTimes;

	/** Local playback mute applied per remote player, by NetId. */
	TMap<FString, bool> AppliedPlaybackMutes;

	/** Handle of the voice ticker. */
	FTSTicker::FDelegateHandle TickHandle;

	/** Current channel. */
	EMKHVoiceChannel Channel = EMKHVoiceChannel::None;

	/** Session microphone mute (not saved). */
	bool bMicMuted = false;

	/** True while the push-to-talk key is held. */
	bool bPushToTalkHeld = false;

	/** True while the microphone is open for the network. */
	bool bTransmitting = false;

	/** Users of the microphone test (meters on screen). */
	int32 MicrophoneTestUsers = 0;

	/** Seconds between two voice ticks. */
	static constexpr float TickInterval = 0.1f;
};
