// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Audio/MKHSoundEffectSubsystem.h"

#include "Audio/MKHAudioSettings.h"
#include "Audio/MKHSoundEffectTypes.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTagsManager.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

// ============================================================
// Lifecycle
// ============================================================

bool UMKHSoundEffectSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UGameInstance* GameInstance = Cast<UGameInstance>(Outer);
	return IsValid(GameInstance) && !GameInstance->IsDedicatedServerInstance() && Super::ShouldCreateSubsystem(Outer);
}

void UMKHSoundEffectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SoundEffectsTable = LoadSoundEffectsTable();
	if (!IsValid(SoundEffectsTable))
	{
		UE_LOG(LogMKHAudio, Warning, TEXT("UMKHSoundEffectSubsystem: no Sound Effects Table in Makhia Audio, every sound effect is silent."));
		return;
	}

	ValidateRowNames();
	PreloadSounds();
}

void UMKHSoundEffectSubsystem::Deinitialize()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHSoundEffectSubsystem* UMKHSoundEffectSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHSoundEffectSubsystem>() : nullptr;
}

const UDataTable* UMKHSoundEffectSubsystem::LoadSoundEffectsTable()
{
	const UMKHAudioSettings* AudioSettings = UMKHAudioSettings::Get();
	return IsValid(AudioSettings) ? AudioSettings->SoundEffectsTable.LoadSynchronous() : nullptr;
}

UAudioComponent* UMKHSoundEffectSubsystem::PlayOnActor(FGameplayTag SoundTag, AActor* Actor)
{
	USceneComponent* AttachComponent = GetAttachComponent(Actor);
	if (!IsValid(AttachComponent))
	{
		return nullptr;
	}

	return PlayOnComponent(SoundTag, AttachComponent);
}

UAudioComponent* UMKHSoundEffectSubsystem::PlayOnComponent(FGameplayTag SoundTag, USceneComponent* Component, FName Socket)
{
	FMKHResolvedSoundEffect Resolved;
	if (!IsValid(Component) || !ResolveSound(SoundTag, Component->GetOwner(), Resolved))
	{
		return nullptr;
	}

	return PlayResolvedOnComponent(Resolved, Component, Socket);
}

void UMKHSoundEffectSubsystem::PlayAtLocation(FGameplayTag SoundTag, FVector Location, AActor* Owner)
{
	FMKHResolvedSoundEffect Resolved;
	if (!IsValid(Owner) || !ResolveSound(SoundTag, Owner, Resolved))
	{
		return;
	}

	if (Resolved.Row->Space == EMKHSoundSpace::TwoD)
	{
		UGameplayStatics::PlaySound2D(Owner, Resolved.Sound, Resolved.Volume, Resolved.Pitch, 0.f, Resolved.Concurrency, Owner, /*bIsUISound*/ false);
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(Owner, Resolved.Sound, Location, FRotator::ZeroRotator, Resolved.Volume, Resolved.Pitch,
		0.f, Resolved.Attenuation, Resolved.Concurrency, Owner);
}

void UMKHSoundEffectSubsystem::PlayUISound(FGameplayTag SoundTag, const UObject* WorldContextObject)
{
	FMKHResolvedSoundEffect Resolved;
	if (!IsValid(WorldContextObject) || !ResolveSound(SoundTag, nullptr, Resolved))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(WorldContextObject, Resolved.Sound, Resolved.Volume, Resolved.Pitch, 0.f, Resolved.Concurrency, nullptr, /*bIsUISound*/ true);
}

void UMKHSoundEffectSubsystem::PlayPreview(const FGameplayTag& SoundTag, USceneComponent* Component)
{
	const UDataTable* Table = LoadSoundEffectsTable();
	const FMKHSoundEffectRow* Row = IsValid(Table) && IsValid(Component)
		? Table->FindRow<FMKHSoundEffectRow>(SoundTag.GetTagName(), TEXT("UMKHSoundEffectSubsystem::PlayPreview"), /*bWarnIfRowMissing*/ false)
		: nullptr;
	if (!Row)
	{
		return;
	}

	FMKHResolvedSoundEffect Resolved;
	Resolved.Row = Row;
	int32 LastIndex = INDEX_NONE;
	Resolved.Sound = PickVariant(*Row, LastIndex);
	if (IsValid(Resolved.Sound))
	{
		ResolvePlaybackSettings(*Row, Resolved);
		PlayResolvedOnComponent(Resolved, Component, NAME_None);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHSoundEffectSubsystem::ResolveSound(const FGameplayTag& SoundTag, const AActor* Owner, FMKHResolvedSoundEffect& OutSound)
{
	if (!SoundTag.IsValid() || !IsValid(SoundEffectsTable))
	{
		return false;
	}

	const FName RowName = SoundTag.GetTagName();
	const FMKHSoundEffectRow* Row = SoundEffectsTable->FindRow<FMKHSoundEffectRow>(RowName, TEXT("UMKHSoundEffectSubsystem"), /*bWarnIfRowMissing*/ false);
	if (!Row)
	{
		if (!ReportedMissingTags.Contains(RowName))
		{
			ReportedMissingTags.Add(RowName);
			UE_LOG(LogMKHAudio, Warning, TEXT("UMKHSoundEffectSubsystem: no row '%s' in %s, the sound is skipped."),
				*RowName.ToString(), *GetNameSafe(SoundEffectsTable));
		}
		return false;
	}

	// An empty row is silent on purpose (sound not delivered yet, or covered by another one).
	if (Row->Variants.IsEmpty() || !IsInAudience(*Row, Owner))
	{
		return false;
	}

	OutSound.Row = Row;
	OutSound.Sound = PickVariant(*Row, LastVariantIndex.FindOrAdd(RowName, INDEX_NONE));
	if (!IsValid(OutSound.Sound))
	{
		return false;
	}

	ResolvePlaybackSettings(*Row, OutSound);
	UE_LOG(LogMKHAudio, Verbose, TEXT("UMKHSoundEffectSubsystem: %s plays %s (owner %s)."),
		*RowName.ToString(), *GetNameSafe(OutSound.Sound), *GetNameSafe(Owner));
	return true;
}

UAudioComponent* UMKHSoundEffectSubsystem::PlayResolvedOnComponent(const FMKHResolvedSoundEffect& Resolved, USceneComponent* Component, FName Socket)
{
	if (Resolved.Row->Space == EMKHSoundSpace::TwoD)
	{
		UGameplayStatics::PlaySound2D(Component, Resolved.Sound, Resolved.Volume, Resolved.Pitch, 0.f, Resolved.Concurrency,
			Component->GetOwner(), /*bIsUISound*/ false);
		return nullptr;
	}

	const FName AttachPoint = Socket.IsNone() ? Resolved.Row->Socket : Socket;
	return UGameplayStatics::SpawnSoundAttached(Resolved.Sound, Component, AttachPoint, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, /*bStopWhenAttachedToDestroyed*/ false, Resolved.Volume, Resolved.Pitch, 0.f,
		Resolved.Attenuation, Resolved.Concurrency);
}

USoundBase* UMKHSoundEffectSubsystem::PickVariant(const FMKHSoundEffectRow& Row, int32& InOutLastIndex)
{
	const int32 NumVariants = Row.Variants.Num();
	if (NumVariants == 0)
	{
		return nullptr;
	}

	// With several takes, never the same one twice in a row: pick among the others.
	int32 Index = 0;
	if (NumVariants > 1)
	{
		Index = FMath::RandRange(0, NumVariants - 2);
		if (InOutLastIndex != INDEX_NONE && Index >= InOutLastIndex)
		{
			++Index;
		}
	}
	InOutLastIndex = Index;

	// Preloaded at startup; the synchronous load only covers a row edited while playing in the editor.
	return Row.Variants[Index].LoadSynchronous();
}

void UMKHSoundEffectSubsystem::ResolvePlaybackSettings(const FMKHSoundEffectRow& Row, FMKHResolvedSoundEffect& OutSound)
{
	OutSound.Volume = Row.Volume;
	OutSound.Pitch = FMath::FRandRange(FMath::Min(Row.PitchMin, Row.PitchMax), FMath::Max(Row.PitchMin, Row.PitchMax));
	OutSound.Concurrency = Row.Concurrency.LoadSynchronous();
	OutSound.Attenuation = Row.Attenuation.LoadSynchronous();
	if (!IsValid(OutSound.Attenuation))
	{
		const UMKHAudioSettings* AudioSettings = UMKHAudioSettings::Get();
		OutSound.Attenuation = IsValid(AudioSettings) ? AudioSettings->DefaultSoundEffectAttenuation.LoadSynchronous() : nullptr;
	}
}

bool UMKHSoundEffectSubsystem::IsInAudience(const FMKHSoundEffectRow& Row, const AActor* Owner)
{
	if (Row.Audience == EMKHSoundAudience::Everyone)
	{
		return true;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	return IsValid(Pawn) && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

USceneComponent* UMKHSoundEffectSubsystem::GetAttachComponent(AActor* Actor)
{
	if (const ACharacter* Character = Cast<ACharacter>(Actor); IsValid(Character) && IsValid(Character->GetMesh()))
	{
		return Character->GetMesh();
	}

	return IsValid(Actor) ? Actor->GetRootComponent() : nullptr;
}

void UMKHSoundEffectSubsystem::ValidateRowNames() const
{
	const UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	for (const FName& RowName : SoundEffectsTable->GetRowNames())
	{
		if (!TagsManager.RequestGameplayTag(RowName, /*ErrorIfNotFound*/ false).IsValid())
		{
			UE_LOG(LogMKHAudio, Warning, TEXT("UMKHSoundEffectSubsystem: row '%s' of %s is not a gameplay tag, nothing can play it. ")
				TEXT("Add the tag (Project Settings > GameplayTags) or rename the row."), *RowName.ToString(), *GetNameSafe(SoundEffectsTable));
		}
	}
}

void UMKHSoundEffectSubsystem::PreloadSounds()
{
	TArray<FSoftObjectPath> Paths;
	SoundEffectsTable->ForeachRow<FMKHSoundEffectRow>(TEXT("UMKHSoundEffectSubsystem::PreloadSounds"),
		[&Paths](const FName&, const FMKHSoundEffectRow& Row)
		{
			for (const TSoftObjectPtr<USoundBase>& Variant : Row.Variants)
			{
				if (!Variant.IsNull())
				{
					Paths.AddUnique(Variant.ToSoftObjectPath());
				}
			}
			if (!Row.Attenuation.IsNull())
			{
				Paths.AddUnique(Row.Attenuation.ToSoftObjectPath());
			}
			if (!Row.Concurrency.IsNull())
			{
				Paths.AddUnique(Row.Concurrency.ToSoftObjectPath());
			}
		});

	const UMKHAudioSettings* AudioSettings = UMKHAudioSettings::Get();
	if (IsValid(AudioSettings) && !AudioSettings->DefaultSoundEffectAttenuation.IsNull())
	{
		Paths.AddUnique(AudioSettings->DefaultSoundEffectAttenuation.ToSoftObjectPath());
	}

	if (!Paths.IsEmpty())
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}
	UE_LOG(LogMKHAudio, Log, TEXT("UMKHSoundEffectSubsystem: %d sound effect rows, %d assets preloading."),
		SoundEffectsTable->GetRowMap().Num(), Paths.Num());
}
