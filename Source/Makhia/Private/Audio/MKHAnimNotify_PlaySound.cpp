// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Audio/MKHAnimNotify_PlaySound.h"

#include "Audio/MKHSoundEffectSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHAnimNotify_PlaySound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	const UWorld* World = IsValid(MeshComp) ? MeshComp->GetWorld() : nullptr;
	if (!SoundTag.IsValid() || !IsValid(World))
	{
		return;
	}

	if (World->WorldType == EWorldType::EditorPreview)
	{
		UMKHSoundEffectSubsystem::PlayPreview(SoundTag, MeshComp);
		return;
	}

	// No subsystem on a dedicated server: nothing to hear there.
	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(MeshComp))
	{
		SoundEffects->PlayOnComponent(SoundTag, MeshComp);
	}
}

FString UMKHAnimNotify_PlaySound::GetNotifyName_Implementation() const
{
	static const FString Prefix = TEXT("SFX.");
	FString Name = SoundTag.IsValid() ? SoundTag.ToString() : TEXT("Play Sound (no tag)");
	Name.RemoveFromStart(Prefix);
	return Name;
}
