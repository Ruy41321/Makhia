// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Audio/MKHSoundEventComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Audio/MKHSoundEffectSubsystem.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHSoundEventComponent::UMKHSoundEventComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMKHSoundEventComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHSoundEventComponent::BindToAbilitySystem(UAbilitySystemComponent* AbilitySystem)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(AbilitySystem) || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	UnbindFromAbilitySystem();
	CacheEventSounds();

	for (const TPair<FGameplayTag, TArray<FMKHEventSound>>& Entry : EventSounds)
	{
		const FDelegateHandle Handle = AbilitySystem->GenericGameplayEventCallbacks.FindOrAdd(Entry.Key)
			.AddUObject(this, &UMKHSoundEventComponent::HandleGameplayEvent, Entry.Key);
		EventHandles.Emplace(Entry.Key, Handle);
	}
	BoundAbilitySystem = AbilitySystem;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSoundEventComponent::HandleGameplayEvent(const FGameplayEventData* Payload, FGameplayTag EventTag)
{
	const TArray<FMKHEventSound>* Sounds = EventSounds.Find(EventTag);
	if (!Sounds)
	{
		return;
	}

	for (const FMKHEventSound& Sound : *Sounds)
	{
		if (MatchesEventRole(Sound.EventRole, Payload))
		{
			Multicast_PlaySoundEffect(Sound.SoundTag);
		}
	}
}

bool UMKHSoundEventComponent::MatchesEventRole(const EMKHSoundEventRole EventRole, const FGameplayEventData* Payload) const
{
	switch (EventRole)
	{
	case EMKHSoundEventRole::Target:
		return Payload && Payload->Target == GetOwner();
	case EMKHSoundEventRole::Instigator:
		return Payload && Payload->Instigator == GetOwner();
	default:
		return true;
	}
}

void UMKHSoundEventComponent::CacheEventSounds()
{
	if (bEventSoundsCached)
	{
		return;
	}
	bEventSoundsCached = true;

	// Read even on a dedicated server: the server decides, the clients play.
	const UDataTable* Table = UMKHSoundEffectSubsystem::LoadSoundEffectsTable();
	if (!IsValid(Table))
	{
		return;
	}

	const UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	Table->ForeachRow<FMKHSoundEffectRow>(TEXT("UMKHSoundEventComponent::CacheEventSounds"),
		[this, &TagsManager](const FName& RowName, const FMKHSoundEffectRow& Row)
		{
			const FGameplayTag SoundTag = TagsManager.RequestGameplayTag(RowName, /*ErrorIfNotFound*/ false);
			if (Row.TriggerEvent.IsValid() && SoundTag.IsValid())
			{
				EventSounds.FindOrAdd(Row.TriggerEvent).Add({ SoundTag, Row.EventRole });
			}
		});
}

void UMKHSoundEventComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : EventHandles)
		{
			if (FGameplayEventMulticastDelegate* Delegate = AbilitySystem->GenericGameplayEventCallbacks.Find(Entry.Key))
			{
				Delegate->Remove(Entry.Value);
			}
		}
	}
	EventHandles.Reset();
	BoundAbilitySystem.Reset();
}

void UMKHSoundEventComponent::Multicast_PlaySoundEffect_Implementation(FGameplayTag SoundTag)
{
	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(this))
	{
		SoundEffects->PlayOnActor(SoundTag, GetOwner());
	}
}
