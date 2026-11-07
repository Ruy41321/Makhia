// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Audio/MKHCrowdAudioComponent.h"

#include "Audio/MKHAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundSubmix.h"

namespace
{
	/** Floor of the excited loop volume: a loop started or held at 0 may be culled as silent and never come back. */
	constexpr float MinLoopVolume = 0.001f;
}

// ============================================================
// Lifecycle
// ============================================================

UMKHCrowdAudioComponent::UMKHCrowdAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UMKHCrowdAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	if (!IsValid(Settings) || IsRunningDedicatedServer())
	{
		return;
	}
	AmbienceAudio = StartLoop(Settings->CrowdAmbienceLoop, Settings->CrowdAmbienceVolume, Settings->CrowdLoopFadeInSeconds);
	// Starts (nearly) silent: SetExcitement raises it to the director's excitement floor.
	ExcitedAudio = StartLoop(Settings->CrowdExcitedLoop, MinLoopVolume, Settings->CrowdLoopFadeInSeconds);
}

void UMKHCrowdAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UAudioComponent* Audio : { AmbienceAudio.Get(), ExcitedAudio.Get() })
	{
		if (IsValid(Audio))
		{
			Audio->Stop();
		}
	}
	AmbienceAudio = nullptr;
	ExcitedAudio = nullptr;

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHCrowdAudioComponent::SetExcitement(float Excitement)
{
	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	if (!IsValid(ExcitedAudio) || !IsValid(Settings))
	{
		return;
	}
	const float Volume = FMath::Clamp(Excitement, 0.f, 1.f) * Settings->CrowdExcitedLoopMaxVolume;
	ExcitedAudio->SetVolumeMultiplier(FMath::Max(Volume, MinLoopVolume));
}

void UMKHCrowdAudioComponent::PlayReaction(EMKHCrowdReaction Reaction, float Intensity)
{
	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	UWorld* World = GetWorld();
	if (!IsValid(Settings) || !IsValid(World) || IsRunningDedicatedServer())
	{
		return;
	}

	float Volume = Settings->CrowdReactionVolume;
	if (Reaction == EMKHCrowdReaction::Hit)
	{
		const double Now = World->GetTimeSeconds();
		if (Now - LastHitSoundTime < Settings->CrowdHitSoundCooldownSeconds)
		{
			return;
		}
		LastHitSoundTime = Now;
		Volume *= FMath::Lerp(0.6f, 1.f, FMath::Clamp(Intensity, 0.f, 1.f));
	}

	// Loaded on demand: a missing slot (no sound yet) is simply silent.
	if (USoundBase* Sound = GetReactionSlot(Reaction).LoadSynchronous())
	{
		WarnIfNotCrowdRouted(Sound);
		UGameplayStatics::PlaySound2D(this, Sound, Volume);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UAudioComponent* UMKHCrowdAudioComponent::StartLoop(const TSoftObjectPtr<USoundBase>& Sound, float Volume,
	float FadeInSeconds)
{
	USoundBase* Loaded = Sound.LoadSynchronous();
	if (!IsValid(Loaded))
	{
		return nullptr;
	}
	if (!Loaded->IsLooping())
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("Crowd loop %s does not loop: set Looping on the sound."), *Loaded->GetName());
	}
	WarnIfNotCrowdRouted(Loaded);

	UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Loaded, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!IsValid(Audio))
	{
		return nullptr;
	}
	Audio->SetVolumeMultiplier(Volume);
	Audio->FadeIn(FadeInSeconds, 1.f);
	return Audio;
}

const TSoftObjectPtr<USoundBase>& UMKHCrowdAudioComponent::GetReactionSlot(EMKHCrowdReaction Reaction)
{
	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	switch (Reaction)
	{
	case EMKHCrowdReaction::DuelStart:   return Settings->CrowdDuelStartSound;
	case EMKHCrowdReaction::Hit:         return Settings->CrowdHitSound;
	case EMKHCrowdReaction::SuddenDeath: return Settings->CrowdSuddenDeathSound;
	case EMKHCrowdReaction::Death:       return Settings->CrowdDeathSound;
	default:                             return Settings->CrowdMatchOverSound;
	}
}

void UMKHCrowdAudioComponent::WarnIfNotCrowdRouted(const USoundBase* Sound)
{
	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	if (!IsValid(Sound) || !IsValid(Settings) || Settings->CrowdSubmix.IsNull())
	{
		return;
	}
	if (Sound->SoundSubmixObject != Settings->CrowdSubmix.LoadSynchronous())
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("Crowd sound %s is not routed to %s: set it as the sound's Base Submix."),
			*Sound->GetName(), *Settings->CrowdSubmix.GetAssetName());
	}
}
