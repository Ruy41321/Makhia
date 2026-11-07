// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AbilitySystem/Cue/MKHGameplayCueNotify_Burst.h"

#include "Audio/MKHSoundEffectSubsystem.h"
#include "Components/SceneComponent.h"

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHGameplayCueNotify_Burst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const bool bResult = Super::OnExecute_Implementation(MyTarget, Parameters);
	PlayEffectSound(MyTarget, Parameters);
	return bResult;
}

void UMKHGameplayCueNotify_Burst::PlayEffectSound(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UMKHSoundEffectSubsystem* SoundEffects = SoundTag.IsValid() ? UMKHSoundEffectSubsystem::Get(MyTarget) : nullptr;
	if (!IsValid(SoundEffects))
	{
		return;
	}

	if (USceneComponent* AttachComponent = Parameters.TargetAttachComponent.Get(); IsValid(AttachComponent))
	{
		SoundEffects->PlayOnComponent(SoundTag, AttachComponent);
	}
	else if (!Parameters.Location.IsNearlyZero())
	{
		SoundEffects->PlayAtLocation(SoundTag, FVector(Parameters.Location), MyTarget);
	}
	else
	{
		SoundEffects->PlayOnActor(SoundTag, MyTarget);
	}
}
