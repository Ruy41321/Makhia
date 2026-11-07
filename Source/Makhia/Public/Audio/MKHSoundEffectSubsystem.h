// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHSoundEffectSubsystem.generated.h"

class AActor;
class UAudioComponent;
class UDataTable;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
struct FMKHSoundEffectRow;
struct FStreamableHandle;

/** A sound effect ready to play: the variant picked and the row's playback settings resolved. */
struct FMKHResolvedSoundEffect
{
	/** Row the sound comes from. */
	const FMKHSoundEffectRow* Row = nullptr;

	/** Variant to play. */
	USoundBase* Sound = nullptr;

	/** Row volume. */
	float Volume = 1.f;

	/** Random pitch within the row's range. */
	float Pitch = 1.f;

	/** Row attenuation, or the project default. */
	USoundAttenuation* Attenuation = nullptr;

	/** Row concurrency, or null for the engine default. */
	USoundConcurrency* Concurrency = nullptr;
};

/**
 * Plays the sound effects of DT_SoundEffects by tag (SFX.*): the single entry point of every gameplay and UI sound.
 * Callers never reference a sound asset: anim notifies (UMKHAnimNotify_PlaySound), VFX gameplay cues
 * (UMKHGameplayCueNotify_Burst), server events (UMKHSoundEventComponent) and UI code ask for a tag, and the table row
 * decides the files, volume, pitch range, space, audience, socket, attenuation and concurrency.
 * Loads the table and preloads every variant when the game instance starts, so the first use never hitches.
 * Not created on a dedicated server (nothing to hear there).
 *
 * Full model: see Documentation/AudioSystemArchitecture.md (Sound effects).
 */
UCLASS()
class MAKHIA_API UMKHSoundEffectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Skips dedicated servers. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Loads the table, checks its row names and preloads every variant. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Releases the preloaded sounds. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the subsystem of the world context's game instance.
	 *
	 * @param WorldContextObject  Any object of a game world.
	 * @return The subsystem, or null (dedicated server, editor preview world).
	 */
	static UMKHSoundEffectSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Loads DT_SoundEffects from Makhia Audio. Also used where the subsystem does not exist (server events, previews).
	 *
	 * @return The table, or null when it is not set.
	 */
	static const UDataTable* LoadSoundEffectsTable();

	/**
	 * Plays a sound effect on an actor: attached to its mesh (character) or root at the row's Socket, or 2D.
	 *
	 * @param SoundTag  Sound effect (row name of DT_SoundEffects).
	 * @param Actor     Actor the sound belongs to (position, audience and concurrency owner).
	 * @return The audio component of a world sound, or null (2D, silent row, other audience).
	 */
	UFUNCTION(BlueprintCallable, Category = "MKH|Audio")
	UAudioComponent* PlayOnActor(UPARAM(meta = (Categories = "SFX")) FGameplayTag SoundTag, AActor* Actor);

	/**
	 * Plays a sound effect attached to a component (e.g. the mesh playing an animation).
	 *
	 * @param SoundTag   Sound effect.
	 * @param Component  Component to follow; its owner is the audience and concurrency owner.
	 * @param Socket     Socket of the component; None = the row's Socket.
	 * @return The audio component of a world sound, or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "MKH|Audio")
	UAudioComponent* PlayOnComponent(UPARAM(meta = (Categories = "SFX")) FGameplayTag SoundTag, USceneComponent* Component, FName Socket = NAME_None);

	/**
	 * Plays a sound effect at a fixed location (it does not follow anything).
	 *
	 * @param SoundTag  Sound effect.
	 * @param Location  World location.
	 * @param Owner     Audience and concurrency owner; also gives the world.
	 */
	UFUNCTION(BlueprintCallable, Category = "MKH|Audio")
	void PlayAtLocation(UPARAM(meta = (Categories = "SFX")) FGameplayTag SoundTag, FVector Location, AActor* Owner);

	/**
	 * Plays an interface sound (2D, plays even while the game is paused).
	 *
	 * @param SoundTag            Sound effect.
	 * @param WorldContextObject  Widget or any object of the world.
	 */
	UFUNCTION(BlueprintCallable, Category = "MKH|Audio", meta = (WorldContext = "WorldContextObject"))
	void PlayUISound(UPARAM(meta = (Categories = "SFX")) FGameplayTag SoundTag, const UObject* WorldContextObject);

	/**
	 * Plays a sound effect in an editor preview world (animation editor): loads the table and the variant on the spot.
	 *
	 * @param SoundTag   Sound effect.
	 * @param Component  Previewed mesh.
	 */
	static void PlayPreview(const FGameplayTag& SoundTag, USceneComponent* Component);

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/**
	 * Finds the row of a tag, picks a variant and resolves the playback settings, when the owner is in the audience.
	 *
	 * @param SoundTag  Sound effect.
	 * @param Owner     Owner of the sound (audience check), may be null.
	 * @param OutSound  Filled when the sound must play.
	 * @return True when something must play.
	 */
	bool ResolveSound(const FGameplayTag& SoundTag, const AActor* Owner, FMKHResolvedSoundEffect& OutSound);

	/**
	 * Plays a resolved sound on a component, attached or 2D.
	 *
	 * @param Resolved   Sound to play.
	 * @param Component  Component to follow.
	 * @param Socket     Socket override; None = the row's Socket.
	 * @return The audio component of a world sound, or null.
	 */
	static UAudioComponent* PlayResolvedOnComponent(const FMKHResolvedSoundEffect& Resolved, USceneComponent* Component, FName Socket);

	/**
	 * Picks a variant of a row, never the previous one when there are several.
	 *
	 * @param Row             The row.
	 * @param InOutLastIndex  Variant played last time (INDEX_NONE at first); updated.
	 * @return The variant, loaded, or null when the row is empty.
	 */
	static USoundBase* PickVariant(const FMKHSoundEffectRow& Row, int32& InOutLastIndex);

	/**
	 * Fills the playback settings of a row (volume, random pitch, attenuation, concurrency).
	 *
	 * @param Row       The row.
	 * @param OutSound  Resolved sound to complete.
	 */
	static void ResolvePlaybackSettings(const FMKHSoundEffectRow& Row, FMKHResolvedSoundEffect& OutSound);

	/**
	 * Whether this machine plays a sound of the given row for the given owner.
	 *
	 * @param Row    The row.
	 * @param Owner  Owner of the sound, may be null.
	 * @return False for a LocalPlayerOnly sound of a character that is not the local player.
	 */
	static bool IsInAudience(const FMKHSoundEffectRow& Row, const AActor* Owner);

	/** @return The mesh of a character, or the root component of any other actor. */
	static USceneComponent* GetAttachComponent(AActor* Actor);

	/** Logs a warning for every row whose name is not a registered gameplay tag (the row could never play). */
	void ValidateRowNames() const;

	/** Starts the async load of every variant, attenuation and concurrency of the table. */
	void PreloadSounds();

	// ============================================================
	// Properties
	// ============================================================

	/** DT_SoundEffects. */
	UPROPERTY()
	TObjectPtr<const UDataTable> SoundEffectsTable;

	/** Keeps the preloaded sounds in memory. */
	TSharedPtr<FStreamableHandle> PreloadHandle;

	/** Variant played last for each row, so the same take never plays twice in a row. */
	TMap<FName, int32> LastVariantIndex;

	/** Tags already reported as missing from the table (one warning each). */
	TSet<FName> ReportedMissingTags;
};
