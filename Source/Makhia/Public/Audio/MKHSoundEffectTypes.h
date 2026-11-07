// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MKHSoundEffectTypes.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** Where a sound effect is heard from. */
UENUM(BlueprintType)
enum class EMKHSoundSpace : uint8
{
	/** Positioned in the world: attached to the character (at Socket) or played at the given location. */
	World,
	/** Not positioned: heard the same everywhere (interface, feedback for the local player). */
	TwoD UMETA(DisplayName = "2D")
};

/** Which machines play a sound effect. */
UENUM(BlueprintType)
enum class EMKHSoundAudience : uint8
{
	/** Every machine that sees the character. */
	Everyone,
	/** Only the machine where the character is the local player (e.g. the kill confirmation of the killer). */
	LocalPlayerOnly
};

/** Which character plays an event-triggered sound when the event reaches both sides of an exchange. */
UENUM(BlueprintType)
enum class EMKHSoundEventRole : uint8
{
	/** Every character whose ability system receives the event. */
	Any,
	/** Only the character that is the event's Target (e.g. the defender of Event.Combat.BlockSuccessful). */
	Target,
	/** Only the character that is the event's Instigator. */
	Instigator
};

/**
 * One sound effect of DT_SoundEffects. The row name is the sound's tag (SFX.*): code, anim notifies, gameplay cues and
 * the UI ask UMKHSoundEffectSubsystem for that tag, never for an asset. A row with no Variants is silent on purpose;
 * a tag with no row logs a warning. There is no fallback to the parent tag.
 *
 * Full model: see Documentation/AudioSystemArchitecture.md (Sound effects).
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHSoundEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Takes of the sound: one is picked at random each time, never the same one twice in a row. Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TArray<TSoftObjectPtr<USoundBase>> Variants;

	/** Volume multiplier of every variant (the SFX slider and submix still apply on top). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Volume = 1.f;

	/** Lowest random pitch multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float PitchMin = 0.95f;

	/** Highest random pitch multiplier (equal to PitchMin for a fixed pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float PitchMax = 1.05f;

	/** Positioned in the world or 2D. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	EMKHSoundSpace Space = EMKHSoundSpace::World;

	/** Which machines play it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	EMKHSoundAudience Audience = EMKHSoundAudience::Everyone;

	/** Socket of the character mesh the sound follows (e.g. "head" for voices); None = the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	FName Socket;

	/** Distance falloff; empty = DefaultSoundEffectAttenuation of Makhia Audio. Ignored by 2D sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	/** How many may play at once (e.g. one voice per character); empty = the engine default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	/**
	 * Gameplay event that plays the sound on the character receiving it (server decides, every machine plays).
	 * Leave empty when an anim notify, a gameplay cue or the UI plays it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (Categories = "Event"))
	FGameplayTag TriggerEvent;

	/** With a TriggerEvent sent to both characters of an exchange, which of them plays the sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	EMKHSoundEventRole EventRole = EMKHSoundEventRole::Any;

	/** What plays this sound and when (for whoever links the files). */
	UPROPERTY(EditAnywhere, Category = "Notes", meta = (MultiLine = true))
	FString Notes;
};
