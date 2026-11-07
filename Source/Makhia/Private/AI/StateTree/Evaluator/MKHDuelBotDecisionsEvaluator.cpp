// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotDecisionsEvaluator.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AI/MKHAttackCandidateBuilder.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/Engine.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotDecisionsEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	const UWorld* World = Context.GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// The scoring row is read every tick: DataTable edits and MKH.Bot.Tuning.Set apply at once.
	static const FMKHDuelBotScoringRow DefaultScoring;
	AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	const FMKHStateScoringParams& Params = IsValid(BotController) ? BotController->GetScoring().States : DefaultScoring.States;

	UpdateChase(World->GetTimeSeconds(), DeltaTime, Params.Bait, Data);
	const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(BuildScoringContext(Data), Data.AttackCandidates, Params);
	PublishScores(Scores, Data);

	UE_VLOG(Data.AIController.Get(), LogMKHAI, Verbose, TEXT("State scores: %s"), *FormatScores(Scores));

	if (IsValid(BotController) && BotController->ConsumeStateScoresDumpRequest())
	{
		const FString Line = FString::Printf(TEXT("State scores: %s | chase %.1fs x retreat %.2f, ready danger %.2f, counter %s"),
			*FormatScores(Scores), Data.UnansweredAttackSeconds, Data.RetreatShare, Data.OpponentReadyDanger,
			Data.bHasCounterAttack ? *Data.CounterAttack.AbilityTag.ToString() : TEXT("none"));
		UE_LOG(LogMKHAI, Display, TEXT("%s"), *Line);
		UE_VLOG(Data.AIController.Get(), LogMKHAI, Display, TEXT("%s"), *Line);
#if !UE_BUILD_SHIPPING
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, Line);
		}
#endif
	}
}

FString FMKHDuelBotDecisionsEvaluator::FormatScores(const FMKHStateScores& Scores)
{
	return FString::Printf(TEXT("Attack %.2f | Bait %.2f (chase %.2f, danger %.2f) | PlayPassive %.2f | React: Block %.2f, Dodge %.2f, Counter %.2f"),
		Scores.Attack, Scores.Bait, Scores.BaitChase, Scores.BaitDanger, Scores.PlayPassive, Scores.Block, Scores.Dodge, Scores.Counter);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotDecisionsEvaluator::UpdateChase(const float Now, const float DeltaTime, const FMKHBaitStateParams& Params,
	FMKHDuelBotDecisionsEvaluatorInstanceData& Data)
{
	// An attack that starts answers every commit before it.
	const UAbilitySystemComponent* AbilitySystem = IsValid(Data.BotCharacter) ? Data.BotCharacter->GetAbilitySystemComponent() : nullptr;
	if (IsValid(AbilitySystem) && AbilitySystem->HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking))
	{
		Data.LastAttackStartTime = Now;
	}

	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	const float LastCommitTime = IsValid(BotController) ? BotController->GetLastAttackCommitTime() : -1000.f;
	const bool bCommitUnanswered = LastCommitTime > Data.LastAttackStartTime && Now - LastCommitTime <= Params.ChaseMemory;
	if (!bCommitUnanswered)
	{
		Data.ChaseStartTime = -1.f;
	}
	else if (Data.ChaseStartTime < 0.f)
	{
		Data.ChaseStartTime = LastCommitTime;
	}
	Data.UnansweredAttackSeconds = Data.ChaseStartTime >= 0.f ? Now - Data.ChaseStartTime : 0.f;

	// Moving average of "the distance grows faster than the threshold": the share of recent time spent retreating.
	if (Data.PreviousDistance >= 0.f && DeltaTime > 0.f)
	{
		const bool bRetreating = (Data.Distance - Data.PreviousDistance) / DeltaTime > Params.RetreatSpeedThreshold;
		const float Alpha = FMath::Clamp(DeltaTime / FMath::Max(Params.RetreatAveragingTime, 0.05f), 0.f, 1.f);
		Data.RetreatShare = FMath::Lerp(Data.RetreatShare, bRetreating ? 1.f : 0.f, Alpha);
	}
	Data.PreviousDistance = Data.Distance;
}

FMKHStateScoringContext FMKHDuelBotDecisionsEvaluator::BuildScoringContext(const FMKHDuelBotDecisionsEvaluatorInstanceData& Data)
{
	FMKHStateScoringContext ScoringContext;
	ScoringContext.AttackUtility = Data.AttackUtility;
	ScoringContext.DamageAdvantage = Data.DamageAdvantage;
	ScoringContext.PunishWindowScore = Data.PunishWindowScore;
	ScoringContext.Aggression = Data.Tuning.Aggression;
	ScoringContext.BaitWilling = Data.Tuning.BaitWilling;
	ScoringContext.UnansweredAttackSeconds = Data.UnansweredAttackSeconds;
	ScoringContext.RetreatShare = Data.RetreatShare;
	ScoringContext.OpponentReadyDanger = Data.OpponentReadyDanger;
	ScoringContext.BotHealthRatio = Data.BotAttributes.HealthRatio;
	ScoringContext.OpponentHealthRatio = Data.OpponentAttributes.HealthRatio;
	ScoringContext.BotStaminaRatio = Data.BotAttributes.StaminaRatio;
	ScoringContext.BotStamina = Data.BotAttributes.CurrentStamina;
	ScoringContext.bCanAffordDodge = Data.BotAttributes.StaminaRatio > 0.f
		&& Data.BotAttributes.StaminaRatio >= Data.BotAttributes.DodgeStaminaCostRatio;
	ScoringContext.OpponentCrowdControlTimeLeft = Data.OpponentCrowdControlTimeLeft;
	ScoringContext.bKnowsIncomingAttack = Data.OpponentAttackAbilityTag.IsValid();
	ScoringContext.IncomingStaminaDamage = Data.OpponentAttackStaminaDamage;
	ScoringContext.Distance = Data.Distance;

	if (const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController))
	{
		ScoringContext.bBaitWantsCounter = BotController->DoesBaitWantCounter();
		ScoringContext.IncomingCrowdControl = BotController->GetIncomingStrike().CrowdControl;
		ScoringContext.IncomingSecondsToHit = BotController->GetIncomingStrike().SecondsToHit;
	}

	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(Data.BotCharacter);
	ScoringContext.BotBlockStability = IsValid(DataSubsystem) && Data.CurrentWeaponEntry.IsValid()
		? FMKHAttackCandidateBuilder::GetWeaponBlockStability(*DataSubsystem, Data.CurrentWeaponEntry) : 0.f;
	return ScoringContext;
}

void FMKHDuelBotDecisionsEvaluator::PublishScores(const FMKHStateScores& Scores, FMKHDuelBotDecisionsEvaluatorInstanceData& Data)
{
	Data.Scores = Scores;
	Data.bHasCounterAttack = Data.AttackCandidates.IsValidIndex(Scores.CounterIndex);
	Data.CounterAttack = Data.bHasCounterAttack ? Data.AttackCandidates[Scores.CounterIndex] : FMKHAttackCandidate();
	Data.CounterAttack.Probability = Data.bHasCounterAttack ? 1.f : 0.f;

	Data.CounterAttackCandidates.Reset();
	if (Data.bHasCounterAttack)
	{
		Data.CounterAttackCandidates.Add(Data.CounterAttack);
	}
}
