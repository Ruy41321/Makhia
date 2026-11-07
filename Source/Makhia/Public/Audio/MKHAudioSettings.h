// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MKHAudioSettings.generated.h"

class UDataTable;
class USoundAttenuation;
class USoundBase;
class USoundControlBus;
class USoundSubmixBase;

/**
 * Project-wide audio configuration exposed in Project Settings -> Game -> Makhia Audio.
 * Holds references to the Audio Modulation Control Buses that drive user-facing
 * volume categories. Gameplay code reads these instead of hardcoding asset paths,
 * so audio designers can re-route or swap buses without touching C++.
 *
 * Values are stored in Config/DefaultGame.ini under [/Script/Makhia.MKHAudioSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Makhia Audio"))
class MAKHIA_API UMKHAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	UMKHAudioSettings();

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Convenience accessor for the project audio settings.
	 *
	 * @return The CDO of UMKHAudioSettings holding the configured values.
	 */
	static const UMKHAudioSettings* Get();

	// ============================================================
	// Properties
	// ============================================================

	/** Control Bus driving the master volume. Attached to the output volume modulation of every top-level submix. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Volume Control Buses")
	TSoftObjectPtr<USoundControlBus> MasterVolumeControlBus;

	/** Control Bus driving the music volume. Attached to the output volume modulation of the music submix. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Volume Control Buses")
	TSoftObjectPtr<USoundControlBus> MusicVolumeControlBus;

	/** Control Bus driving the sound effects volume. Attached to the output volume modulation of the SFX submix. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Volume Control Buses")
	TSoftObjectPtr<USoundControlBus> SFXVolumeControlBus;

	/** Control Bus driving the crowd volume. Attached to the output volume modulation of the crowd submix. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Volume Control Buses")
	TSoftObjectPtr<USoundControlBus> CrowdVolumeControlBus;

	/**
	 * Control Bus driving the volume of the other players' voice (CB_Voice), on the output volume modulation of the voice
	 * submix (SBM_Voice). The master volume does not reach the voice (Documentation/VoiceChat.md).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Volume Control Buses")
	TSoftObjectPtr<USoundControlBus> VoiceVolumeControlBus;

	/**
	 * Maps a level name (short name, without path or PIE prefix — e.g. "Makhion")
	 * to the music track that starts when that level begins play.
	 * Levels without an entry fade out any currently playing music.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music")
	TMap<FString, TSoftObjectPtr<USoundBase>> LevelMusicMap;

	/** Default fade duration (seconds) used for music fade-in, fade-out and crossfades. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float DefaultMusicFadeSeconds = 2.f;

	// ------------------------------------------------------------
	// Crowd (played by UMKHCrowdAudioComponent on the arena's crowd director; an empty slot stays silent)
	// ------------------------------------------------------------

	/** Crowd murmur, looping, from the start of the match to the end (set the sound to loop). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Loops")
	TSoftObjectPtr<USoundBase> CrowdAmbienceLoop;

	/** Cheering crowd, looping; its volume follows the crowd excitement (silent when the crowd is calm). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Loops")
	TSoftObjectPtr<USoundBase> CrowdExcitedLoop;

	/** One-shot when a duel starts (the gates open), every round. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Reactions")
	TSoftObjectPtr<USoundBase> CrowdDuelStartSound;

	/** One-shot when a fighter takes a hit that excites the crowd ("ooh"); louder for bigger hits. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Reactions")
	TSoftObjectPtr<USoundBase> CrowdHitSound;

	/** One-shot when the duel timer runs out and sudden death begins. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Reactions")
	TSoftObjectPtr<USoundBase> CrowdSuddenDeathSound;

	/** One-shot when a fighter dies (the round is decided). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Reactions")
	TSoftObjectPtr<USoundBase> CrowdDeathSound;

	/** One-shot when the match is decided. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Reactions")
	TSoftObjectPtr<USoundBase> CrowdMatchOverSound;

	/** Submix of the crowd (SBM_Crowd): every crowd sound sets it as its Base Submix; a sound routed elsewhere logs a warning. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix")
	TSoftObjectPtr<USoundSubmixBase> CrowdSubmix;

	/** Volume of the ambience loop (the crowd submix still applies on top). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CrowdAmbienceVolume = 0.6f;

	/** Volume of the excited loop at full excitement. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CrowdExcitedLoopMaxVolume = 1.f;

	/** Volume of the reaction one-shots (hits play between 60 % and 100 % of it, by hit size). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CrowdReactionVolume = 1.f;

	/** Seconds the loops take to fade in when the arena starts. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix", meta = (ClampMin = "0.0"))
	float CrowdLoopFadeInSeconds = 2.f;

	/** Minimum seconds between two hit reactions (a quick combo plays one "ooh", not one per hit). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Crowd|Mix", meta = (ClampMin = "0.0"))
	float CrowdHitSoundCooldownSeconds = 0.8f;

	// ------------------------------------------------------------
	// Arena gates (played by AMKHArenaGate at its grille, spatialized; an empty slot stays silent)
	// ------------------------------------------------------------

	/** Winch and chains while the grille rises (one-shot, about as long as the rise, 2.5 s); cut short if it drops. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates")
	TSoftObjectPtr<USoundBase> GateRaiseSound;

	/** Clank when the grille locks fully open. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates")
	TSoftObjectPtr<USoundBase> GateRaisedSound;

	/** Chains running free while the grille drops (one-shot, about 1.2 s); cut short if it rises again. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates")
	TSoftObjectPtr<USoundBase> GateLowerSound;

	/** Heavy iron slam when the grille hits the ground. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates")
	TSoftObjectPtr<USoundBase> GateSlamSound;

	/** Distance falloff of the gate sounds (empty = each sound's own; a sound without any plays 2D). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates")
	TSoftObjectPtr<USoundAttenuation> GateSoundAttenuation;

	/** Volume of every gate sound (the SFX slider and submix still apply on top). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Arena Gates", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GateSoundVolume = 1.f;

	// ------------------------------------------------------------
	// Sound effects (every gameplay and UI sound, played by tag through UMKHSoundEffectSubsystem)
	// ------------------------------------------------------------

	/** DT_SoundEffects: one row per sound effect, row name = its SFX.* tag (row struct FMKHSoundEffectRow). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound Effects", meta = (RequiredAssetDataTags = "RowStructure=/Script/Makhia.MKHSoundEffectRow"))
	TSoftObjectPtr<UDataTable> SoundEffectsTable;

	/** Distance falloff of the world sound effects whose row has no Attenuation. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound Effects")
	TSoftObjectPtr<USoundAttenuation> DefaultSoundEffectAttenuation;
};
