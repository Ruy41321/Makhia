// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Spawn/MKHItemSpawnConditions.h"

#include "Character/CharacterBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/Spawn/MKHItemSpawnPoint.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "MKHItemSpawnConditions"

// ============================================================
// UMKHItemSpawnCondition
// ============================================================

bool UMKHItemSpawnCondition::Evaluate(const AMKHItemSpawnPoint& SpawnPoint) const
{
	return IsSatisfied(&SpawnPoint) != bNegate;
}

UWorld* UMKHItemSpawnCondition::GetWorld() const
{
	// The CDO has no world: returning nullptr there is how the engine detects that instances provide one.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* Outer = GetOuter();
	return IsValid(Outer) ? Outer->GetWorld() : nullptr;
}

bool UMKHItemSpawnCondition::IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const
{
	return true;
}

#if WITH_EDITOR
EDataValidationResult UMKHItemSpawnCondition::ValidateForSpawnPoint(const AMKHItemSpawnPoint& SpawnPoint, FDataValidationContext& Context) const
{
	return EDataValidationResult::Valid;
}
#endif

// ============================================================
// UMKHItemSpawnCondition_NoPawnNearby
// ============================================================

bool UMKHItemSpawnCondition_NoPawnNearby::IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const
{
	const UWorld* World = IsValid(SpawnPoint) ? SpawnPoint->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	const FVector SpawnLocation = SpawnPoint->GetActorLocation();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
		if (IsValid(Pawn) && FVector::DistSquared(Pawn->GetActorLocation(), SpawnLocation) <= FMath::Square(Radius))
		{
			return false;
		}
	}

	return true;
}

// ============================================================
// UMKHItemSpawnCondition_Score
// ============================================================

bool UMKHItemSpawnCondition_Score::IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const
{
	const UWorld* World = IsValid(SpawnPoint) ? SpawnPoint->GetWorld() : nullptr;
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(GameState) || GameState->GetPlayerDatas().Num() < 2)
	{
		return false;
	}

	const TArray<FMKHDuelPlayerData>& Participants = GameState->GetPlayerDatas();
	if (!IsOwnerRule())
	{
		const int32 Gap = FMath::Abs(Participants[0].Score - Participants[1].Score);
		return Rule == EMKHSpawnScoreRule::Tied ? Gap == 0 : Gap >= ScoreDifference;
	}

	const APlayerState* Owner = SpawnPoint->ResolveExclusiveCollector();
	const int32 OwnerIndex = Participants.IndexOfByPredicate([Owner](const FMKHDuelPlayerData& Data)
	{
		return IsValid(Owner) && IsValid(Data.Participant) && Data.Participant->GetPlayerState() == Owner;
	});
	if (OwnerIndex != 0 && OwnerIndex != 1)
	{
		return false;
	}

	const int32 OwnerLead = Participants[OwnerIndex].Score - Participants[1 - OwnerIndex].Score;
	return Rule == EMKHSpawnScoreRule::OwnerAheadAtLeast ? OwnerLead >= ScoreDifference : -OwnerLead >= ScoreDifference;
}

bool UMKHItemSpawnCondition_Score::IsOwnerRule() const
{
	return Rule == EMKHSpawnScoreRule::OwnerBehindAtLeast || Rule == EMKHSpawnScoreRule::OwnerAheadAtLeast;
}

#if WITH_EDITOR
EDataValidationResult UMKHItemSpawnCondition_Score::ValidateForSpawnPoint(const AMKHItemSpawnPoint& SpawnPoint, FDataValidationContext& Context) const
{
	if (IsOwnerRule() && !SpawnPoint.IsExclusive())
	{
		Context.AddError(FText::Format(LOCTEXT("OwnerRuleOnFreePoint", "{0}: the Score condition compares the owner's score, but the spawn point is not reserved to a player (CollectRestriction)."),
			FText::FromString(SpawnPoint.GetActorNameOrLabel())));
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}
#endif

// ============================================================
// UMKHItemSpawnCondition_MatchPhase
// ============================================================

bool UMKHItemSpawnCondition_MatchPhase::IsSatisfied_Implementation(const AMKHItemSpawnPoint* SpawnPoint) const
{
	const UWorld* World = IsValid(SpawnPoint) ? SpawnPoint->GetWorld() : nullptr;
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	return IsValid(GameState) && Phases.Contains(GameState->GetMatchPhase());
}

#undef LOCTEXT_NAMESPACE
