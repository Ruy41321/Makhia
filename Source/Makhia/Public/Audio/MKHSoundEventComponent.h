// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Audio/MKHSoundEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MKHSoundEventComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEventData;

/** Sound effect a gameplay event plays, read from a DT_SoundEffects row with a TriggerEvent. */
USTRUCT()
struct FMKHEventSound
{
	GENERATED_BODY()

	/** Sound effect (row name). */
	UPROPERTY()
	FGameplayTag SoundTag;

	/** Which character of the event plays it. */
	UPROPERTY()
	EMKHSoundEventRole EventRole = EMKHSoundEventRole::Any;
};

/**
 * The "event" route of the sound effects, on every ACharacterBase (players and duel bot): on the server it listens to
 * the gameplay events named by the TriggerEvent of DT_SoundEffects rows on the character's ability system, and
 * multicasts the row's sound so every machine plays it on this character. For facts the server decides that have no
 * animation or visual effect of their own (blocked hit, kill, weapon sheathed / unsheathed). Adding such a sound is
 * a table row: no code.
 *
 * Full model: see Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHSoundEventComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Replicated (multicast of the sounds), no tick. */
	UMKHSoundEventComponent();

	/** Stops listening to the ability system. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Server: listens to the trigger events of DT_SoundEffects on this ability system. Rebind-safe (drops the
	 * previous bindings). Does nothing on clients.
	 *
	 * @param AbilitySystem  Ability system of the owning character.
	 */
	void BindToAbilitySystem(UAbilitySystemComponent* AbilitySystem);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Gameplay event callback: multicasts every sound of the event whose role matches this character.
	 *
	 * @param Payload   Event data (Target / Instigator for the role check).
	 * @param EventTag  Event received.
	 */
	void HandleGameplayEvent(const FGameplayEventData* Payload, FGameplayTag EventTag);

	/**
	 * Whether this character plays a sound with the given role for this event.
	 *
	 * @param EventRole  Role of the row.
	 * @param Payload    Event data.
	 * @return True when the role matches.
	 */
	bool MatchesEventRole(EMKHSoundEventRole EventRole, const FGameplayEventData* Payload) const;

	/** Reads the rows with a TriggerEvent into EventSounds. */
	void CacheEventSounds();

	/** Removes every event binding made by BindToAbilitySystem. */
	void UnbindFromAbilitySystem();

	/**
	 * Plays a sound effect on this character on every machine.
	 *
	 * @param SoundTag  Sound effect (row name).
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySoundEffect(FGameplayTag SoundTag);

	// ============================================================
	// Properties
	// ============================================================

	/** Sounds per trigger event, read from the table once. */
	TMap<FGameplayTag, TArray<FMKHEventSound>> EventSounds;

	/** Ability system the events are bound on. */
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	/** Event bindings to remove on rebind or end play. */
	TArray<TPair<FGameplayTag, FDelegateHandle>> EventHandles;

	/** True once EventSounds was read. */
	bool bEventSoundsCached = false;
};
