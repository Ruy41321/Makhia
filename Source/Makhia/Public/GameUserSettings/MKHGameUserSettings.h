// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "Voice/MKHVoiceTypes.h"
#include "MKHGameUserSettings.generated.h"

class USoundControlBus;

/** Broadcasts the new value of the "automatically quick slot the items used in the last match" setting. */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHAutoQuickSlotLoadoutChangedSignature, bool /*bEnabled*/);

/** Broadcast when the player picks another match scoreboard look. */
DECLARE_MULTICAST_DELEGATE(FMKHScoreboardStyleChangedSignature);

/** Look of the match scoreboard; see "The scoreboard looks" in Documentation/MatchFlowArchitecture.md. */
UENUM(BlueprintType)
enum class EMKHScoreboardStyle : uint8
{
	/** Pediment hanging from the top edge of the screen (W_MatchInfo). */
	Pediment,
	/** Open text over the scene, without a panel (W_MatchInfo_Epigraph). */
	Epigraph
};

/**
 * Project-specific user settings persisted to GameUserSettings.ini.
 * Extends the engine settings with accessibility options and user-facing
 * audio volume controls (master / music / SFX / crowd).
 *
 * Volumes are stored as normalized sliders in [0, 1] and pushed to the
 * Audio Modulation Control Buses configured in UMKHAudioSettings. The buses
 * use a Volume modulation parameter, which maps the normalized value to
 * decibels internally — this gives a perceptually linear slider without
 * manual dB conversion in game code.
 */
UCLASS()
class MAKHIA_API UMKHGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	UMKHGameUserSettings();

	//~ Begin UGameUserSettings Interface
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void ApplyNonResolutionSettings() override;
	virtual void SetToDefaults() override;
	//~ End UGameUserSettings Interface

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the active game user settings.
	 *
	 * @return The engine's UGameUserSettings instance cast to the project type, or nullptr if unavailable.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings")
	static UMKHGameUserSettings* GetMKHGameUserSettings();

	// --- Audio ---

	/**
	 * Sets the master volume and applies it immediately to the master Control Bus.
	 * Call SaveSettings() (or ApplySettings) afterwards to persist the value to disk.
	 *
	 * @param NewVolume Normalized volume in [0, 1]; values outside the range are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMasterVolume(float NewVolume);

	/** Gets the normalized master volume in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	/**
	 * Sets the music volume and applies it immediately to the music Control Bus.
	 * Call SaveSettings() (or ApplySettings) afterwards to persist the value to disk.
	 *
	 * @param NewVolume Normalized volume in [0, 1]; values outside the range are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetMusicVolume(float NewVolume);

	/** Gets the normalized music volume in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetMusicVolume() const { return MusicVolume; }

	/**
	 * Sets the sound effects volume and applies it immediately to the SFX Control Bus.
	 * Call SaveSettings() (or ApplySettings) afterwards to persist the value to disk.
	 *
	 * @param NewVolume Normalized volume in [0, 1]; values outside the range are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetSFXVolume(float NewVolume);

	/** Gets the normalized sound effects volume in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetSFXVolume() const { return SFXVolume; }

	/**
	 * Sets the arena crowd volume and applies it immediately to the crowd Control Bus.
	 * Call SaveSettings() (or ApplySettings) afterwards to persist the value to disk.
	 *
	 * @param NewVolume Normalized volume in [0, 1]; values outside the range are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetCrowdVolume(float NewVolume);

	/** Gets the normalized arena crowd volume in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	float GetCrowdVolume() const { return CrowdVolume; }

	/**
	 * Pushes every stored volume to its Control Bus on the currently active play world.
	 * Safe to call when no play world exists (e.g. before the first level is up):
	 * in that case it is a no-op and UMKHAudioWorldSubsystem will re-apply the
	 * values when the world begins play.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void ApplyAudioSettings();

	/** Returns the id of the device the game audio plays on; empty for the system default. */
	UFUNCTION(BlueprintPure, Category = "Settings|Audio")
	const FString& GetGameOutputDeviceId() const { return GameOutputDeviceId; }

	/**
	 * Stores the device the game audio plays on (applied by UMKHAudioDeviceLibrary).
	 *
	 * @param NewDeviceId  Device id from the output device list; empty for the system default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
	void SetGameOutputDeviceId(const FString& NewDeviceId) { GameOutputDeviceId = NewDeviceId; }

	// --- Voice chat (stored here, applied by UMKHVoiceChatSubsystem; Documentation/VoiceChat.md) ---

	/** Returns the voice chat master switch. */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	bool IsVoiceChatEnabled() const { return bVoiceChatEnabled; }

	/** Stores the voice chat master switch. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceChatEnabled(bool bEnable) { bVoiceChatEnabled = bEnable; }

	/** Returns how the microphone transmits. */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	EMKHVoiceMicMode GetVoiceMicMode() const { return VoiceMicMode; }

	/** Stores how the microphone transmits. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceMicMode(EMKHVoiceMicMode NewMode) { VoiceMicMode = NewMode; }

	/** Returns true when voice with players who are neither friends nor party is accepted. */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	bool IsVoiceWithNonFriendsAllowed() const { return bVoiceAllowNonFriends; }

	/** Stores whether voice with players who are neither friends nor party is accepted. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceWithNonFriendsAllowed(bool bAllow) { bVoiceAllowNonFriends = bAllow; }

	/** Returns the normalized microphone volume in [0, 1] (0.5 = unchanged gain). */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	float GetVoiceInputVolume() const { return VoiceInputVolume; }

	/** Stores the normalized microphone volume, clamped to [0, 1]. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceInputVolume(float NewVolume) { VoiceInputVolume = FMath::Clamp(NewVolume, 0.f, 1.f); }

	/** Returns the normalized level the microphone must exceed to transmit in open mic, in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	float GetVoiceActivationThreshold() const { return VoiceActivationThreshold; }

	/** Stores the normalized open mic activation level, clamped to [0, 1]. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceActivationThreshold(float NewThreshold) { VoiceActivationThreshold = FMath::Clamp(NewThreshold, 0.f, 1.f); }

	/** Returns the normalized volume of the other players' voice in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	float GetVoiceOutputVolume() const { return VoiceOutputVolume; }

	/** Stores the normalized volume of the other players' voice, clamped to [0, 1], and applies it to the voice Control Bus. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceOutputVolume(float NewVolume);

	/** Returns the id of the device the voice of the others plays on; empty for "same as the game". */
	UFUNCTION(BlueprintPure, Category = "Settings|Voice")
	const FString& GetVoiceOutputDeviceId() const { return VoiceOutputDeviceId; }

	/** Stores the device the voice of the others plays on; empty for "same as the game". */
	UFUNCTION(BlueprintCallable, Category = "Settings|Voice")
	void SetVoiceOutputDeviceId(const FString& NewDeviceId) { VoiceOutputDeviceId = NewDeviceId; }

	/** Returns the NetIds of the players the local player muted (kept between sessions). */
	const TArray<FString>& GetMutedVoicePlayerIds() const { return MutedVoicePlayerIds; }

	/**
	 * Adds or removes a player from the saved voice mutes.
	 *
	 * @param PlayerId  NetId of the player.
	 * @param bMuted    True to mute.
	 * @return True when the list changed.
	 */
	bool SetVoicePlayerMuted(const FString& PlayerId, bool bMuted);

	// --- Accessibility ---

	/** Enables or disables gameplay camera shake effects. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Accessibility")
	void SetCameraShakeEnabled(bool bEnable) { bEnableCameraShake = bEnable; }

	/** Checks whether gameplay camera shake effects are enabled. */
	UFUNCTION(BlueprintPure, Category = "Settings|Accessibility")
	bool IsCameraShakeEnabled() const { return bEnableCameraShake; }

	// --- Gameplay ---

	/**
	 * Allows or forbids the hold-to-equip pickup: holding Interact on a pickup also equips the item or puts it in
	 * a quick slot. Read by AMKHPlayerController when Interact is pressed.
	 *
	 * @param bEnable True to let a held Interact equip / quick slot (default).
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetHoldToEquipPickupEnabled(bool bEnable) { bHoldToEquipPickup = bEnable; }

	/** Whether holding Interact on a pickup equips / quick slots the item. */
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool IsHoldToEquipPickupEnabled() const { return bHoldToEquipPickup; }

	/**
	 * Allows or forbids the starting loadout quick slot preferences (UMKHQuickSlotPreferenceComponent): when off,
	 * the quick slots start empty. Broadcasts OnAutoQuickSlotLoadoutChanged on change, so the server learns it
	 * before the next loadout.
	 *
	 * @param bEnable True to quick slot the items used in the last match at match start (default).
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetAutoQuickSlotLoadoutEnabled(bool bEnable);

	/** Whether the starting loadout is quick slotted like in the last match. */
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool IsAutoQuickSlotLoadoutEnabled() const { return bAutoQuickSlotLoadout; }

	/** Broadcast when SetAutoQuickSlotLoadoutEnabled changes the value. */
	FMKHAutoQuickSlotLoadoutChangedSignature OnAutoQuickSlotLoadoutChanged;

	/**
	 * Picks the match scoreboard look. Broadcasts OnScoreboardStyleChanged on change, so a running match switches at once.
	 *
	 * @param NewStyle Look to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetScoreboardStyle(EMKHScoreboardStyle NewStyle);

	/** Match scoreboard look picked by the player. */
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	EMKHScoreboardStyle GetScoreboardStyle() const { return ScoreboardStyle; }

	/** Broadcast when SetScoreboardStyle changes the value. */
	FMKHScoreboardStyleChangedSignature OnScoreboardStyleChanged;

	// --- Camera ---

	/**
	 * Sets the look sensitivity multiplier applied to mouse input.
	 * Read live by UBasicMovementComponent, so it takes effect on the next look input.
	 *
	 * @param NewSensitivity Multiplier; values outside [0.1, 3.0] are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Camera")
	void SetMouseSensitivity(float NewSensitivity);

	/** Look sensitivity multiplier. */
	UFUNCTION(BlueprintPure, Category = "Settings|Camera")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	/** Flips the vertical look axis. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Camera")
	void SetInvertYAxis(bool bInvert) { bInvertYAxis = bInvert; }

	/** Whether the vertical look axis is flipped. */
	UFUNCTION(BlueprintPure, Category = "Settings|Camera")
	bool IsYAxisInverted() const { return bInvertYAxis; }

	// --- Controller ---

	/**
	 * Sets the look sensitivity multiplier applied to the right stick (IA_LookGamepad).
	 * Read live by UBasicMovementComponent, like the mouse sensitivity.
	 *
	 * @param NewSensitivity Multiplier; values outside [0.1, 3.0] are clamped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Controller")
	void SetGamepadLookSensitivity(float NewSensitivity);

	/** Right stick look sensitivity multiplier. */
	UFUNCTION(BlueprintPure, Category = "Settings|Controller")
	float GetGamepadLookSensitivity() const { return GamepadLookSensitivity; }

	/** Flips the vertical axis of the right stick look. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Controller")
	void SetInvertGamepadYAxis(bool bInvert) { bInvertGamepadYAxis = bInvert; }

	/** Whether the vertical axis of the right stick look is flipped. */
	UFUNCTION(BlueprintPure, Category = "Settings|Controller")
	bool IsGamepadYAxisInverted() const { return bInvertGamepadYAxis; }

	/**
	 * Allows or forbids controller vibration, and applies it to the local player controllers.
	 *
	 * @param bEnable       True to let force feedback play.
	 * @param WorldContext  Any object of the world to update; nullptr only stores the value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Controller", meta = (WorldContext = "WorldContext"))
	void SetControllerVibrationEnabled(bool bEnable, const UObject* WorldContext);

	/** Whether controller vibration is allowed. */
	UFUNCTION(BlueprintPure, Category = "Settings|Controller")
	bool IsControllerVibrationEnabled() const { return bEnableControllerVibration; }

	/**
	 * Writes the vibration choice on every local player controller of a world (bForceFeedbackEnabled).
	 * Called by AMKHPlayerController::BeginPlay, so a new level starts with the saved value.
	 *
	 * @param WorldContext  Any object of the world to update.
	 */
	void ApplyControllerVibration(const UObject* WorldContext) const;

	// --- Interface ---

	/**
	 * Shows or hides the engine frame rate counter, and remembers the choice.
	 * Applied through GEngine->SetEngineStat, which is explicit on/off rather than the
	 * "stat fps" console command, which only toggles.
	 *
	 * @param bShow          True to display the counter.
	 * @param WorldContext   Object used to reach the world; without one the value is only stored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Interface", meta = (WorldContext = "WorldContext"))
	void SetShowFPS(bool bShow, const UObject* WorldContext);

	/** Whether the frame rate counter is on. */
	UFUNCTION(BlueprintPure, Category = "Settings|Interface")
	bool IsShowingFPS() const { return bShowFPS; }

	/**
	 * Chooses what the network stats overlay shows. Read live by UMKHNetStatsWidget.
	 *
	 * @param NewMode Off, Compact (ping and route) or Detailed (adds jitter, loss and NAT types).
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Interface")
	void SetNetStatsMode(EMKHNetStatsMode NewMode) { NetStatsMode = NewMode; }

	/** What the network stats overlay shows. */
	UFUNCTION(BlueprintPure, Category = "Settings|Interface")
	EMKHNetStatsMode GetNetStatsMode() const { return NetStatsMode; }

	// --- Video ---

	/**
	 * Sets the brightness and applies it at once through the engine display gamma.
	 * Only the 3D scene changes: the UI is drawn after tonemapping and keeps its colours.
	 * Call SaveSettings() afterwards to persist the value to disk.
	 *
	 * @param NewBrightness Multiplier of the shipped display gamma; clamped to [MinBrightness, MaxBrightness].
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void SetBrightness(float NewBrightness);

	/** Brightness multiplier; 1 is the shipped look. */
	UFUNCTION(BlueprintPure, Category = "Settings|Video")
	float GetBrightness() const { return Brightness; }

	/** Writes the stored brightness to GEngine->DisplayGamma. */
	void ApplyBrightness() const;

	/** Shipped brightness, and the range offered by the settings slider. */
	static constexpr float DefaultBrightness = 1.f;
	static constexpr float MinBrightness = 0.7f;
	static constexpr float MaxBrightness = 1.3f;

	/** Engine display gamma at brightness 1. */
	static constexpr float DefaultDisplayGamma = 2.2f;

	/**
	 * Picks the video settings once, on the first launch of a packaged game, then applies and saves
	 * them. Skipped in the editor (it would change the editor's own scalability) and on dedicated servers.
	 *
	 * @param bIsHandheld  True on a handheld PC (UMKHOnlineServicesSubsystem::IsRunningOnHandheld):
	 *                     the handheld profile replaces the hardware benchmark.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void RunFirstLaunchAutoDetect(bool bIsHandheld = false);

	/**
	 * Stores the video settings this machine should use, without applying them: the handheld profile
	 * on a handheld PC, otherwise the hardware benchmark (short hitch) with an unlimited frame rate
	 * replaced by DefaultFrameRateLimit.
	 *
	 * @param bIsHandheld  True on a handheld PC (UMKHOnlineServicesSubsystem::IsRunningOnHandheld).
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Video")
	void StageAutoDetectedQuality(bool bIsHandheld = false);

	/** Frame rate cap used for new players and after a reset; unlimited makes the GPU run flat out. */
	static constexpr float DefaultFrameRateLimit = 144.f;

	/** Frame rate cap of the handheld profile (Steam Deck: stable 40 FPS, its screen can run at 40 Hz). */
	static constexpr float HandheldFrameRateLimit = 40.f;

	/** Overall scalability level of the handheld profile (1 = Medium); tuned on the Deck, Documentation/SteamDeck.md. */
	static constexpr int32 HandheldQualityLevel = 1;

private:

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Applies a single normalized volume to a Control Bus through a global bus mix value.
	 * No-op (with a verbose log) when no play world is active; logs a warning when the
	 * bus asset is not assigned in UMKHAudioSettings.
	 *
	 * @param BusPtr       Soft reference to the target Control Bus (from UMKHAudioSettings).
	 * @param LinearVolume Normalized volume in [0, 1] to set on the bus.
	 */
	void ApplyVolumeToControlBus(const TSoftObjectPtr<USoundControlBus>& BusPtr, float LinearVolume) const;

	/**
	 * Stores the handheld profile: HandheldQualityLevel, HandheldFrameRateLimit, borderless fullscreen at the
	 * native resolution of the screen. Replaces the benchmark, whose score under Proton says little about the Deck.
	 */
	void StageHandheldProfile();

	// ============================================================
	// Properties
	// ============================================================

	/** Id of the output device of the game audio; empty for the system default. */
	UPROPERTY(Config)
	FString GameOutputDeviceId;

	/** Voice chat master switch (default on, Documentation/VoiceChat.md). */
	UPROPERTY(Config)
	bool bVoiceChatEnabled = true;

	/** How the microphone transmits (default push-to-talk). */
	UPROPERTY(Config)
	EMKHVoiceMicMode VoiceMicMode = EMKHVoiceMicMode::PushToTalk;

	/** Voice with players who are neither friends nor party (default refused). */
	UPROPERTY(Config)
	bool bVoiceAllowNonFriends = false;

	/** Normalized microphone volume in [0, 1]; 0.5 keeps the captured level. */
	UPROPERTY(Config)
	float VoiceInputVolume = 0.5f;

	/** Normalized open mic activation level in [0, 1]. */
	UPROPERTY(Config)
	float VoiceActivationThreshold = 0.2f;

	/** Normalized volume of the other players' voice in [0, 1], independent of the master volume. */
	UPROPERTY(Config)
	float VoiceOutputVolume = 1.f;

	/** Id of the output device of the voice; empty for "same as the game". */
	UPROPERTY(Config)
	FString VoiceOutputDeviceId;

	/** NetIds of the players muted by the local player. */
	UPROPERTY(Config)
	TArray<FString> MutedVoicePlayerIds;

	/** Normalized master volume in [0, 1]. Drives the master Control Bus (affects every submix). */
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	/** Normalized music volume in [0, 1]. Drives the music Control Bus. */
	UPROPERTY(Config)
	float MusicVolume = 1.f;

	/** Normalized sound effects volume in [0, 1]. Drives the SFX Control Bus. */
	UPROPERTY(Config)
	float SFXVolume = 1.f;

	/** Normalized arena crowd volume in [0, 1]. Drives the crowd Control Bus. */
	UPROPERTY(Config)
	float CrowdVolume = 1.f;

	/** Look sensitivity multiplier applied to mouse input. */
	UPROPERTY(config)
	float MouseSensitivity = 1.f;

	/** True when the vertical look axis is flipped. */
	UPROPERTY(config)
	bool bInvertYAxis = false;

	/** Look sensitivity multiplier applied to the right stick. */
	UPROPERTY(config)
	float GamepadLookSensitivity = 1.f;

	/** True when the vertical axis of the right stick look is flipped. */
	UPROPERTY(config)
	bool bInvertGamepadYAxis = false;

	/** True when controller vibration (force feedback) may play. */
	UPROPERTY(config)
	bool bEnableControllerVibration = true;

	/** Brightness multiplier of the display gamma; 1 is the shipped look. */
	UPROPERTY(Config)
	float Brightness = DefaultBrightness;

	/** True when the frame rate counter is shown. */
	UPROPERTY(config)
	bool bShowFPS = false;

	/** What the network stats overlay shows; hidden by default. */
	UPROPERTY(config)
	EMKHNetStatsMode NetStatsMode = EMKHNetStatsMode::Off;

	/** Whether gameplay camera shake effects are allowed to play. */
	UPROPERTY(Config)
	bool bEnableCameraShake;

	/** True when holding Interact on a pickup also equips / quick slots the item. */
	UPROPERTY(Config)
	bool bHoldToEquipPickup = true;

	/** True when the starting loadout is quick slotted like in the last match. */
	UPROPERTY(Config)
	bool bAutoQuickSlotLoadout = true;

	/** Match scoreboard look picked by the player. */
	UPROPERTY(Config)
	EMKHScoreboardStyle ScoreboardStyle = EMKHScoreboardStyle::Pediment;

	/** True once the first-launch hardware benchmark has run; never reset, so it runs only once. */
	UPROPERTY(Config)
	bool bHasAutoDetectedQuality = false;
};
