// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Character/Animation/MKHAnimNotify_GameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHAnimNotify_GameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// Editor previews (montage editor, thumbnails) have no ability system: nothing to tell, no warning.
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!EventTag.IsValid() || UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner) == nullptr)
	{
		return;
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, FGameplayEventData());
}

FString UMKHAnimNotify_GameplayEvent::GetNotifyName_Implementation() const
{
	static const FString Prefix = TEXT("Event.Animation.");
	FString Name = EventTag.ToString();
	Name.RemoveFromStart(Prefix);
	return Name;
}
