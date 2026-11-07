// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "GameUserSettings/MKHGameUserSettings.h"

#include "Audio/MKHAudioSettings.h"
#include "AudioModulationStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "MKHLogChannels.h"
#include "SoundControlBus.h"

UMKHGameUserSettings::UMKHGameUserSettings()
{
	bEnableCameraShake = true;
}

void UMKHGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);
	ApplyAudioSettings();
}

void UMKHGameUserSettings::ApplyNonResolutionSettings()
{
	// Also run alone by the game engine at startup, so the saved brightness is live from the first frame.
	Super::ApplyNonResolutionSettings();
	ApplyBrightness();
}

void UMKHGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	FrameRateLimit = DefaultFrameRateLimit;
	MasterVolume = 1.f;
	MusicVolume = 1.f;
	SFXVolume = 1.f;
	CrowdVolume = 1.f;
	bEnableCameraShake = true;
	bHoldToEquipPickup = true;
	ScoreboardStyle = EMKHScoreboardStyle::Pediment;
	SetAutoQuickSlotLoadoutEnabled(true);
	MouseSensitivity = 1.f;
	bInvertYAxis = false;
	GamepadLookSensitivity = 1.f;
	bInvertGamepadYAxis = false;
	bEnableControllerVibration = true;
	Brightness = DefaultBrightness;
	bShowFPS = false;
	NetStatsMode = EMKHNetStatsMode::Off;
	GameOutputDeviceId.Reset();
	bVoiceChatEnabled = true;
	VoiceMicMode = EMKHVoiceMicMode::PushToTalk;
	bVoiceAllowNonFriends = false;
	VoiceInputVolume = 0.5f;
	VoiceActivationThreshold = 0.2f;
	VoiceOutputVolume = 1.f;
	VoiceOutputDeviceId.Reset();
}

bool UMKHGameUserSettings::SetVoicePlayerMuted(const FString& PlayerId, bool bMuted)
{
	if (PlayerId.IsEmpty())
	{
		return false;
	}
	if (!bMuted)
	{
		return MutedVoicePlayerIds.Remove(PlayerId) > 0;
	}
	if (MutedVoicePlayerIds.Contains(PlayerId))
	{
		return false;
	}
	MutedVoicePlayerIds.Add(PlayerId);
	return true;
}

UMKHGameUserSettings* UMKHGameUserSettings::GetMKHGameUserSettings()
{
	return Cast<UMKHGameUserSettings>(GetGameUserSettings());
}

void UMKHGameUserSettings::SetMasterVolume(float NewVolume)
{
	MasterVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	ApplyVolumeToControlBus(UMKHAudioSettings::Get()->MasterVolumeControlBus, MasterVolume);
}

void UMKHGameUserSettings::SetMusicVolume(float NewVolume)
{
	MusicVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	ApplyVolumeToControlBus(UMKHAudioSettings::Get()->MusicVolumeControlBus, MusicVolume);
}

void UMKHGameUserSettings::SetSFXVolume(float NewVolume)
{
	SFXVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	ApplyVolumeToControlBus(UMKHAudioSettings::Get()->SFXVolumeControlBus, SFXVolume);
}

void UMKHGameUserSettings::SetCrowdVolume(float NewVolume)
{
	CrowdVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	ApplyVolumeToControlBus(UMKHAudioSettings::Get()->CrowdVolumeControlBus, CrowdVolume);
}

void UMKHGameUserSettings::SetVoiceOutputVolume(float NewVolume)
{
	VoiceOutputVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	ApplyVolumeToControlBus(UMKHAudioSettings::Get()->VoiceVolumeControlBus, VoiceOutputVolume);
}

void UMKHGameUserSettings::ApplyAudioSettings()
{
	const UMKHAudioSettings* AudioSettings = UMKHAudioSettings::Get();
	if (!IsValid(AudioSettings))
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("UMKHGameUserSettings::ApplyAudioSettings: UMKHAudioSettings unavailable, cannot apply volumes."));
		return;
	}

	ApplyVolumeToControlBus(AudioSettings->MasterVolumeControlBus, MasterVolume);
	ApplyVolumeToControlBus(AudioSettings->MusicVolumeControlBus, MusicVolume);
	ApplyVolumeToControlBus(AudioSettings->SFXVolumeControlBus, SFXVolume);
	ApplyVolumeToControlBus(AudioSettings->CrowdVolumeControlBus, CrowdVolume);
	ApplyVolumeToControlBus(AudioSettings->VoiceVolumeControlBus, VoiceOutputVolume);
}

void UMKHGameUserSettings::ApplyVolumeToControlBus(const TSoftObjectPtr<USoundControlBus>& BusPtr, float LinearVolume) const
{
	// Modulation is per audio device, so a live play world is required. When none is
	// active yet, UMKHAudioWorldSubsystem re-applies the settings on world begin play.
	UWorld* World = IsValid(GEngine) ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!IsValid(World))
	{
		UE_LOG(LogMKHAudio, Verbose, TEXT("UMKHGameUserSettings::ApplyVolumeToControlBus: No active play world, deferring volume application."));
		return;
	}

	USoundControlBus* Bus = BusPtr.LoadSynchronous();
	if (!IsValid(Bus))
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("UMKHGameUserSettings::ApplyVolumeToControlBus: Control Bus '%s' is not assigned or failed to load. Check Project Settings -> Game -> Makhia Audio."), *BusPtr.ToString());
		return;
	}

	UAudioModulationStatics::SetGlobalBusMixValue(World, Bus, LinearVolume);
}

void UMKHGameUserSettings::RunFirstLaunchAutoDetect(bool bIsHandheld)
{
	if (bHasAutoDetectedQuality || GIsEditor || IsRunningDedicatedServer())
	{
		return;
	}

	StageAutoDetectedQuality(bIsHandheld);
	bHasAutoDetectedQuality = true;
	ApplySettings(false);

	UE_LOG(LogMKHInput, Log, TEXT("UMKHGameUserSettings::RunFirstLaunchAutoDetect: %s profile, overall quality %d, frame rate limit %.0f (CPU %.1f, GPU %.1f)."),
		bIsHandheld ? TEXT("Handheld") : TEXT("Benchmark"), GetOverallScalabilityLevel(), FrameRateLimit,
		LastCPUBenchmarkResult, LastGPUBenchmarkResult);
}

void UMKHGameUserSettings::StageAutoDetectedQuality(bool bIsHandheld)
{
	if (bIsHandheld)
	{
		StageHandheldProfile();
		return;
	}

	RunHardwareBenchmark();

	if (FrameRateLimit <= 0.f)
	{
		FrameRateLimit = DefaultFrameRateLimit;
	}
}

void UMKHGameUserSettings::StageHandheldProfile()
{
	SetOverallScalabilityLevel(HandheldQualityLevel);
	FrameRateLimit = HandheldFrameRateLimit;
	SetFullscreenMode(EWindowMode::WindowedFullscreen);
	SetScreenResolution(GetDesktopResolution());
}

void UMKHGameUserSettings::SetBrightness(float NewBrightness)
{
	Brightness = FMath::Clamp(NewBrightness, MinBrightness, MaxBrightness);
	ApplyBrightness();
}

void UMKHGameUserSettings::ApplyBrightness() const
{
	if (!IsValid(GEngine))
	{
		return;
	}

	// The tonemapper outputs pow(Color, 2.2 / DisplayGamma): a higher gamma lifts the midtones.
	GEngine->DisplayGamma = DefaultDisplayGamma * FMath::Clamp(Brightness, MinBrightness, MaxBrightness);
}

void UMKHGameUserSettings::SetAutoQuickSlotLoadoutEnabled(bool bEnable)
{
	if (bAutoQuickSlotLoadout == bEnable)
	{
		return;
	}

	bAutoQuickSlotLoadout = bEnable;
	OnAutoQuickSlotLoadoutChanged.Broadcast(bAutoQuickSlotLoadout);
}

void UMKHGameUserSettings::SetScoreboardStyle(EMKHScoreboardStyle NewStyle)
{
	if (ScoreboardStyle == NewStyle)
	{
		return;
	}

	ScoreboardStyle = NewStyle;
	OnScoreboardStyleChanged.Broadcast();
}

void UMKHGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 3.0f);
}

void UMKHGameUserSettings::SetGamepadLookSensitivity(float NewSensitivity)
{
	GamepadLookSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 3.0f);
}

void UMKHGameUserSettings::SetControllerVibrationEnabled(bool bEnable, const UObject* WorldContext)
{
	bEnableControllerVibration = bEnable;
	ApplyControllerVibration(WorldContext);
}

void UMKHGameUserSettings::ApplyControllerVibration(const UObject* WorldContext) const
{
	const UWorld* World = IsValid(GEngine) && IsValid(WorldContext)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get(); IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			PlayerController->bForceFeedbackEnabled = bEnableControllerVibration;
		}
	}
}

void UMKHGameUserSettings::SetShowFPS(bool bShow, const UObject* WorldContext)
{
	bShowFPS = bShow;

	if (!IsValid(GEngine) || !IsValid(WorldContext))
	{
		return;
	}

	// SetEngineStat takes an explicit on/off, unlike the "stat fps" console command which
	// only toggles and would drift out of sync with the saved value.
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull); IsValid(World))
	{
		GEngine->SetEngineStat(World, World->GetGameViewport(), TEXT("FPS"), bShowFPS);
	}
}
