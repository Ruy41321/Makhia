// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Arena/MKHCrowdDirector.h"

#include "Audio/MKHCrowdAudioComponent.h"
#include "Character/CharacterBase.h"
#include "Engine/World.h"
#include "GameMode/MKHGameState.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "MKHLogChannels.h"
#include "Scalability.h"

namespace
{
	/** Prefix of the component tags carrying a crowd density tier (CrowdTier0, CrowdTier1, ...). */
	const FString CrowdTierTagPrefix = TEXT("CrowdTier");
}

// ============================================================
// Lifecycle
// ============================================================

AMKHCrowdDirector::AMKHCrowdDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicates(false);

	CrowdAudio = CreateDefaultSubobject<UMKHCrowdAudioComponent>(TEXT("CrowdAudio"));
}

void AMKHCrowdDirector::BeginPlay()
{
	Super::BeginPlay();

	ScalabilityHandle = Scalability::OnScalabilitySettingsChanged.AddUObject(
		this, &AMKHCrowdDirector::HandleScalabilityChanged);
	ApplyCrowdDensity(Scalability::GetQualityLevels().EffectsQuality);

	if (!IsValid(CrowdCollection))
	{
		UE_LOG(LogMKHArena, Warning, TEXT("CrowdDirector %s: no CrowdCollection set, the crowd will not react."), *GetName());
		return;
	}
	TargetExcitement = MinExcitement;
	DisplayedExcitement = MinExcitement;
	PushExcitement();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	if (AGameStateBase* GameState = World->GetGameState())
	{
		BindGameState(GameState);
		return;
	}
	// On clients the game state may replicate after the level's actors begin play.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &AMKHCrowdDirector::BindGameState);
}

void AMKHCrowdDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAll();
	Scalability::OnScalabilitySettingsChanged.Remove(ScalabilityHandle);
	ScalabilityHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AMKHCrowdDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateExcitement(DeltaSeconds);
	PushExcitement();

	if (TargetExcitement <= MinExcitement && DisplayedExcitement <= MinExcitement)
	{
		SetActorTickEnabled(false);
	}
	
	// Print on screen the lvl of excitement for debugging purposes
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Excitement: %.2f"), DisplayedExcitement));
	
}

// ============================================================
// Public Interface
// ============================================================

void AMKHCrowdDirector::AddExcitement(float Level)
{
	const float Clamped = FMath::Clamp(Level, 0.f, 1.f);
	if (Clamped <= 0.f)
	{
		return;
	}
	TargetExcitement = FMath::Max(TargetExcitement, Clamped);
	HoldRemaining = HoldSeconds;
	SetActorTickEnabled(true);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHCrowdDirector::BindGameState(AGameStateBase* GameState)
{
	AMKHGameState* MKHGameState = Cast<AMKHGameState>(GameState);
	if (!IsValid(MKHGameState) || BoundGameState.Get() == MKHGameState)
	{
		return;
	}
	if (UWorld* World = GetWorld(); IsValid(World) && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
		GameStateSetHandle.Reset();
	}

	BoundGameState = MKHGameState;
	MKHGameState->OnParticipantRegistered.AddDynamic(this, &AMKHCrowdDirector::HandleParticipantRegistered);
	MKHGameState->OnDuelStarted.AddDynamic(this, &AMKHCrowdDirector::HandleDuelStarted);
	MKHGameState->OnDuelTimeOver.AddDynamic(this, &AMKHCrowdDirector::HandleDuelTimeOver);
	MKHGameState->OnPlayerDied.AddDynamic(this, &AMKHCrowdDirector::HandlePlayerDied);
	MKHGameState->OnMatchOver.AddDynamic(this, &AMKHCrowdDirector::HandleMatchOver);

	// Participants registered before the director bound (late join, game state replicated late).
	for (const FMKHDuelPlayerData& Data : MKHGameState->GetPlayerDatas())
	{
		BindParticipant(Data.Participant);
	}
}

void AMKHCrowdDirector::UnbindAll()
{
	if (UWorld* World = GetWorld(); IsValid(World) && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
		GameStateSetHandle.Reset();
	}
	if (AMKHGameState* GameState = BoundGameState.Get())
	{
		GameState->OnParticipantRegistered.RemoveDynamic(this, &AMKHCrowdDirector::HandleParticipantRegistered);
		GameState->OnDuelStarted.RemoveDynamic(this, &AMKHCrowdDirector::HandleDuelStarted);
		GameState->OnDuelTimeOver.RemoveDynamic(this, &AMKHCrowdDirector::HandleDuelTimeOver);
		GameState->OnPlayerDied.RemoveDynamic(this, &AMKHCrowdDirector::HandlePlayerDied);
		GameState->OnMatchOver.RemoveDynamic(this, &AMKHCrowdDirector::HandleMatchOver);
	}
	BoundGameState.Reset();

	for (const TWeakObjectPtr<ACharacterBase>& Participant : BoundParticipants)
	{
		if (ACharacterBase* Character = Participant.Get())
		{
			Character->OnHealthChanged.RemoveDynamic(this, &AMKHCrowdDirector::HandleHealthChanged);
		}
	}
	BoundParticipants.Reset();
}

void AMKHCrowdDirector::BindParticipant(ACharacterBase* Participant)
{
	if (!IsValid(Participant) || BoundParticipants.Contains(Participant))
	{
		return;
	}
	BoundParticipants.Add(Participant);
	Participant->OnHealthChanged.AddDynamic(this, &AMKHCrowdDirector::HandleHealthChanged);
}

void AMKHCrowdDirector::HandleParticipantRegistered(int32 SlotIndex, ACharacterBase* Character)
{
	BindParticipant(Character);
}

void AMKHCrowdDirector::HandleHealthChanged(float OldHealth, float CurrentHealth, float MaxHealth)
{
	// Revives and heals raise health; round resets happen outside the duel.
	if (!IsDuelRunning() || CurrentHealth >= OldHealth)
	{
		return;
	}
	const float Excitement = ComputeHitExcitement(OldHealth - CurrentHealth, MaxHealth);
	AddExcitement(Excitement);
	if (Excitement > 0.f)
	{
		PlayCrowdReaction(EMKHCrowdReaction::Hit, Excitement);
	}
}

void AMKHCrowdDirector::HandleDuelStarted()
{
	AddExcitement(DuelStartExcitement);
	PlayCrowdReaction(EMKHCrowdReaction::DuelStart);
}

void AMKHCrowdDirector::HandleDuelTimeOver()
{
	AddExcitement(SuddenDeathExcitement);
	PlayCrowdReaction(EMKHCrowdReaction::SuddenDeath);
}

void AMKHCrowdDirector::HandlePlayerDied(ACharacterBase* DeadPlayer, ACharacterBase* ScoringPlayer, int32 ScoringScore)
{
	AddExcitement(DeathExcitement);
	PlayCrowdReaction(EMKHCrowdReaction::Death);
}

void AMKHCrowdDirector::HandleMatchOver(ACharacterBase* WinningPlayer, bool bIsDraw)
{
	AddExcitement(MatchOverExcitement);
	PlayCrowdReaction(EMKHCrowdReaction::MatchOver);
}

float AMKHCrowdDirector::ComputeHitExcitement(float Damage, float MaxHealth) const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	const float Fraction = Damage / MaxHealth;
	if (Fraction < MinHitHealthFraction)
	{
		return 0.f;
	}
	return FMath::Clamp(MinHitExcitement + Fraction * HitExcitementPerHealthFraction, 0.f, 1.f);
}

bool AMKHCrowdDirector::IsDuelRunning() const
{
	const AMKHGameState* GameState = BoundGameState.Get();
	if (!IsValid(GameState))
	{
		return false;
	}
	return GameState->IsDuelRunning();
}

void AMKHCrowdDirector::UpdateExcitement(float DeltaSeconds)
{
	if (HoldRemaining > 0.f)
	{
		HoldRemaining -= DeltaSeconds;
	}
	else
	{
		TargetExcitement = FMath::Max(MinExcitement, TargetExcitement - DecayPerSecond * DeltaSeconds);
	}

	// Ease up toward a new peak; follow the decay down directly (it is already gradual).
	DisplayedExcitement = DisplayedExcitement < TargetExcitement
		? FMath::Min(TargetExcitement, DisplayedExcitement + RisePerSecond * DeltaSeconds)
		: TargetExcitement;
}

void AMKHCrowdDirector::PushExcitement() const
{
	UWorld* World = GetWorld();
	if (!IsValid(CrowdCollection) || !IsValid(World))
	{
		return;
	}
	if (UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(CrowdCollection))
	{
		Instance->SetScalarParameterValue(ExcitementParameter, DisplayedExcitement);
	}
	if (IsValid(CrowdAudio))
	{
		CrowdAudio->SetExcitement(DisplayedExcitement);
	}
}

void AMKHCrowdDirector::HandleScalabilityChanged(const Scalability::FQualityLevels& QualityLevels)
{
	ApplyCrowdDensity(QualityLevels.EffectsQuality);
}

void AMKHCrowdDirector::ApplyCrowdDensity(int32 EffectsQuality) const
{
	if (!IsValid(CrowdActor))
	{
		UE_LOG(LogMKHArena, Warning, TEXT("CrowdDirector %s: no CrowdActor set, the crowd density does not follow Effects quality."), *GetName());
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Components(CrowdActor);
	for (UPrimitiveComponent* Component : Components)
	{
		const int32 Tier = GetDensityTier(Component);
		if (Tier != INDEX_NONE)
		{
			Component->SetVisibility(Tier <= EffectsQuality);
		}
	}
}

int32 AMKHCrowdDirector::GetDensityTier(const UActorComponent* Component)
{
	if (!IsValid(Component))
	{
		return INDEX_NONE;
	}
	for (const FName& Tag : Component->ComponentTags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(CrowdTierTagPrefix))
		{
			return FCString::Atoi(*TagString.RightChop(CrowdTierTagPrefix.Len()));
		}
	}
	return INDEX_NONE;
}

void AMKHCrowdDirector::PlayCrowdReaction(EMKHCrowdReaction Reaction, float Intensity) const
{
	if (IsValid(CrowdAudio))
	{
		CrowdAudio->PlayReaction(Reaction, Intensity);
	}
}
