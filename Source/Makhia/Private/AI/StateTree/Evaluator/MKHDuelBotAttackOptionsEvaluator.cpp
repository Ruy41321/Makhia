// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotAttackOptionsEvaluator.h"

#include "AIController.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Abilities/MKHDamageAbility.h"
#include "AbilitySystem/Abilities/MKHGameplayAbility.h"
#include "AI/MKHAttackCandidateBuilder.h"
#include "AI/MKHDuelBot.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "AI/StateTree/Evaluator/MKHDuelBotContextEvaluator.h"
#include "Character/CharacterBase.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Equipment/EquipmentTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotAttackOptionsEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	const UMKHAbilitySystemComponent* BotAbilitySystem = IsValid(Data.BotCharacter)
		? Cast<UMKHAbilitySystemComponent>(Data.BotCharacter->GetAbilitySystemComponent()) : nullptr;
	const UAbilitySystemComponent* OpponentAbilitySystem = IsValid(Data.Opponent) ? Data.Opponent->GetAbilitySystemComponent() : nullptr;
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(Data.BotCharacter);
	if (!IsValid(BotAbilitySystem) || !IsValid(OpponentAbilitySystem) || !IsValid(DataSubsystem))
	{
		// Normal for a few ticks at round start: Verbose, not Warning.
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHDuelBotAttackOptionsEvaluator: no bot (%s), opponent (%s) or data yet."),
			*GetNameSafe(Data.BotCharacter), *GetNameSafe(Data.Opponent));
		ResetOutputs(Data);
		return;
	}

	UpdateLastUsedAbility(*BotAbilitySystem, *DataSubsystem, Data);

	// The scoring row is read every tick: DataTable edits and MKH.Bot.Tuning.Set apply at once.
	static const FMKHDuelBotScoringRow DefaultScoring;
	AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	const FMKHDuelBotScoringRow& Scoring = IsValid(BotController) ? BotController->GetScoring() : DefaultScoring;

	TArray<FMKHAttackCandidateInput> Inputs;
	TArray<FMKHAttackCandidate> Candidates;
	GatherCandidates(*BotAbilitySystem, *DataSubsystem, Scoring.EngageDistanceFactor, Data, Inputs, Candidates);

	const FMKHAttackScoringContext ScoringContext = BuildScoringContext(*BotAbilitySystem, *OpponentAbilitySystem, *DataSubsystem, Data);
	ScoreAndPublish(Inputs, ScoringContext, Scoring.AttackScoring, Candidates, Data);

	if (IsValid(BotController) && BotController->ConsumeAttackOptionsDumpRequest())
	{
		DumpAttackOptions(ScoringContext, Data);
	}
}

FString FMKHDuelBotAttackOptionsEvaluator::FormatCandidateTable(TConstArrayView<FMKHAttackCandidate> Candidates, const int32 ChosenIndex)
{
	FString Table = TEXT("  Ability                                       Dist  Dmg   Guard Stat  CC    CD    Var   Spd  | Score  p     TTH\n");
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FMKHAttackCandidate& Candidate = Candidates[Index];
		const FMKHAttackScoreBreakdown& Breakdown = Candidate.Breakdown;
		Table += FString::Printf(TEXT("%s %-45s %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f | %.3f  %.2f  %.2fs%s%s%s\n"),
			Index == ChosenIndex ? TEXT(">") : TEXT(" "), *Candidate.AbilityTag.ToString(), Breakdown.Distance, Breakdown.Damage,
			Breakdown.GuardBreak, Breakdown.StatusEffect, Breakdown.CrowdControlFit, Breakdown.CooldownEconomy, Breakdown.Variety,
			Breakdown.Speed, Candidate.Score, Candidate.Probability, Candidate.TimeToHit, Candidate.bNeedsSwap ? TEXT(" [swap]") : TEXT(""),
			Breakdown.bLethal ? TEXT(" [lethal]") : TEXT(""), Breakdown.bBreaksGuard ? TEXT(" [guard break]") : TEXT(""));
	}
	return Table;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotAttackOptionsEvaluator::ResetOutputs(FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	Data.AttackCandidates.Reset();
	Data.AttackUtility = 0.f;
	Data.bHasAttackCandidates = false;
}

void FMKHDuelBotAttackOptionsEvaluator::DumpAttackOptions(const FMKHAttackScoringContext& ScoringContext,
	const FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	const FString Header = FString::Printf(TEXT("Attack options: utility %.2f, k %.1f%s | distance %.0f, punish %.2f, blocking %d, CC %s %.2fs, opp health %.0f stamina %.0f"),
		Data.AttackUtility, Data.Tuning.GetDecisionSharpness(), Data.Tuning.IsExactChoice() ? TEXT(" (argmax)") : TEXT(""),
		ScoringContext.Distance, ScoringContext.PunishWindowScore, ScoringContext.bOpponentBlocking,
		*UEnum::GetValueAsString(ScoringContext.OpponentCrowdControl), ScoringContext.OpponentCrowdControlTimeLeft,
		ScoringContext.OpponentHealth, ScoringContext.OpponentStamina);
	const FString Table = FormatCandidateTable(Data.AttackCandidates);

	UE_LOG(LogMKHAI, Display, TEXT("%s\n%s"), *Header, *Table);
	UE_VLOG(Data.AIController.Get(), LogMKHAI, Display, TEXT("%s\n%s"), *Header, *Table);
#if !UE_BUILD_SHIPPING
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan, Header + TEXT("\n") + Table);
	}
#endif
}

void FMKHDuelBotAttackOptionsEvaluator::UpdateLastUsedAbility(const UMKHAbilitySystemComponent& AbilitySystem,
	const UMKHDataSubsystem& DataSubsystem, FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	// Ability.Attacking is an activation owned tag of UMKHDamageAbility: its rising edge is a new attack.
	const bool bIsAttacking = AbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking);
	const bool bStarted = bIsAttacking && !Data.bWasBotAttacking;
	Data.bWasBotAttacking = bIsAttacking;
	if (!bStarted)
	{
		return;
	}

	// The running spec's AbilityTag is the dynamic source tag that has a tuning row.
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		const FGameplayTag* AbilityTag = Spec.IsActive() ? Spec.GetDynamicSpecSourceTags().GetGameplayTagArray().FindByPredicate(
			[&DataSubsystem](const FGameplayTag& Tag) { return DataSubsystem.FindAbilityTuning(Tag) != nullptr; }) : nullptr;
		if (AbilityTag != nullptr)
		{
			Data.LastUsedAbilityTag = *AbilityTag;
			UE_VLOG(Data.AIController.Get(), LogMKHAI, Verbose, TEXT("Attack started: %s."), *AbilityTag->ToString());
			return;
		}
	}
}

FMKHOpenComboWindow FMKHDuelBotAttackOptionsEvaluator::FindOpenComboWindow(const UMKHAbilitySystemComponent& AbilitySystem,
	const UMKHDataSubsystem& DataSubsystem)
{
	FMKHOpenComboWindow ComboWindow;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		const UMKHDamageAbility* DamageAbility = Spec.IsActive() ? Cast<UMKHDamageAbility>(Spec.GetPrimaryInstance()) : nullptr;
		if (!IsValid(DamageAbility) || !DamageAbility->IsWithinComboWindow())
		{
			continue;
		}

		const FGameplayTag* AbilityTag = Spec.GetDynamicSpecSourceTags().GetGameplayTagArray().FindByPredicate(
			[&DataSubsystem](const FGameplayTag& Tag) { return DataSubsystem.FindAbilityTuning(Tag) != nullptr; });
		if (AbilityTag != nullptr)
		{
			ComboWindow.AbilityTag = *AbilityTag;
			ComboWindow.NextStrikeIndex = DamageAbility->GetComboHitCounter() + 1;
			break;
		}
	}
	return ComboWindow;
}

void FMKHDuelBotAttackOptionsEvaluator::GatherCandidates(const UMKHAbilitySystemComponent& AbilitySystem,
	const UMKHDataSubsystem& DataSubsystem, const float EngageDistanceFactor, FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data,
	TArray<FMKHAttackCandidateInput>& OutInputs, TArray<FMKHAttackCandidate>& OutCandidates)
{
	// The swap is instant: the sheathed weapon competes whenever the bot can swap.
	const AMKHDuelBot* Bot = Cast<AMKHDuelBot>(Data.BotCharacter);
	const FGameplayTag SwapInputTag = IsValid(Bot) && Bot->CanSwapWeapon() ? Bot->GetOtherWeaponQuickSlot() : FGameplayTag();
	const FMKHOpenComboWindow ComboWindow = FindOpenComboWindow(AbilitySystem, DataSubsystem);

	for (const bool bSheathed : { false, true })
	{
		const FMKHItemEntry& Weapon = bSheathed ? Data.SheathedWeaponEntry : Data.CurrentWeaponEntry;
		if (!Weapon.IsValid() || (bSheathed && !SwapInputTag.IsValid()))
		{
			continue;
		}

		const float WeaponBaseDamage = FMKHAttackCandidateBuilder::GetWeaponBaseDamage(DataSubsystem, Weapon);
		for (const FEquipmentAbilityDefinition& Ability : Weapon.EffectPackage.Abilities)
		{
			FMKHAttackCandidateInput Input;
			const FGameplayTag InputTag = bSheathed ? FindSheathedWeaponInputTag(AbilitySystem, Ability, Data) : FindHeldWeaponInputTag(AbilitySystem, Ability, ComboWindow);
			const int32 StrikeIndex = !bSheathed && Ability.AbilityTag == ComboWindow.AbilityTag ? ComboWindow.NextStrikeIndex : 0;
			if (!InputTag.IsValid() || !FMKHAttackCandidateBuilder::BuildStrikeCandidate(DataSubsystem, Ability, WeaponBaseDamage, StrikeIndex, Input))
			{
				continue;
			}
			Input.bIsLastUsed = Ability.AbilityTag == Data.LastUsedAbilityTag;
			Input.bNeedsSwap = bSheathed;
			Input.bIsLight = InputTag.MatchesTagExact(MKHGameplayTags::Input::LightAttack);

			FMKHAttackCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.AbilityTag = Ability.AbilityTag;
			Candidate.InputTag = InputTag;
			Candidate.EngageDistance = Input.Reach * EngageDistanceFactor;
			Candidate.Reach = Input.Reach;
			Candidate.bNeedsSwap = bSheathed;
			Candidate.SwapInputTag = bSheathed ? SwapInputTag : FGameplayTag();
			Candidate.CrowdControl = FMKHAttackScoring::GetStrongestCrowdControl(Input);
			Candidate.TimeToHit = Input.Strikes.IsEmpty() ? 0.f : Input.Strikes[0].TimeToHit;
			OutInputs.Add(MoveTemp(Input));
		}
	}
}

FGameplayTag FMKHDuelBotAttackOptionsEvaluator::FindHeldWeaponInputTag(const UMKHAbilitySystemComponent& AbilitySystem,
	const FEquipmentAbilityDefinition& Ability, const FMKHOpenComboWindow& ComboWindow)
{
	const FGameplayAbilitySpec* Spec = AbilitySystem.FindAbilitySpecForAbilityTag(Ability.AbilityTag);
	if (Spec == nullptr || !IsValid(Spec->Ability))
	{
		return FGameplayTag();
	}

	// Same checks as a key press: off cooldown, affordable, not blocked by the bot's state. In a combo window the state
	// only blocks until the window ends; the attack running already passed them.
	const FGameplayAbilityActorInfo* ActorInfo = AbilitySystem.AbilityActorInfo.Get();
	const bool bUsable = !ComboWindow.IsOpen() ? Spec->Ability->CanActivateAbility(Spec->Handle, ActorInfo)
		: Ability.AbilityTag == ComboWindow.AbilityTag
			|| (Spec->Ability->CheckCooldown(Spec->Handle, ActorInfo) && Spec->Ability->CheckCost(Spec->Handle, ActorInfo));
	return bUsable ? AbilitySystem.FindInputTagForAbilityTag(Ability.AbilityTag) : FGameplayTag();
}

FGameplayTag FMKHDuelBotAttackOptionsEvaluator::FindSheathedWeaponInputTag(const UMKHAbilitySystemComponent& AbilitySystem,
	const FEquipmentAbilityDefinition& Ability, FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	// The cooldown tag stays on the bot after a swap: a skill in cooldown stays vetoed in the sheath too.
	if (Ability.CooldownTag.IsValid() && AbilitySystem.HasMatchingGameplayTag(Ability.CooldownTag))
	{
		return FGameplayTag();
	}

	// Never LoadSynchronous in game: load the class in the background, the ability competes from the next ticks.
	const UClass* AbilityClass = Ability.AbilityClass.Get();
	if (!IsValid(AbilityClass))
	{
		const FSoftObjectPath ClassPath = Ability.AbilityClass.ToSoftObjectPath();
		bool bAlreadyRequested = false;
		Data.RequestedAbilityClasses.Add(ClassPath, &bAlreadyRequested);
		if (!bAlreadyRequested && ClassPath.IsValid())
		{
			Data.AbilityClassLoads.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(ClassPath));
		}
		return FGameplayTag();
	}
	return UMKHAbilitySystemComponent::ResolveEquipmentAbilityInputTag(Ability, Cast<UMKHGameplayAbility>(AbilityClass->GetDefaultObject()));
}

FMKHAttackScoringContext FMKHDuelBotAttackOptionsEvaluator::BuildScoringContext(const UAbilitySystemComponent& BotAbilitySystem,
	const UAbilitySystemComponent& OpponentAbilitySystem, const UMKHDataSubsystem& DataSubsystem,
	const FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	FMKHAttackScoringContext ScoringContext;
	ScoringContext.Distance = Data.Distance;
	const UCharacterMovementComponent* Movement = Data.BotCharacter->GetCharacterMovement();
	ScoringContext.ApproachSpeed = IsValid(Movement) ? Movement->GetMaxSpeed() : ScoringContext.ApproachSpeed;

	ScoringContext.BotOutputMultiplier = Data.BotAttributes.EffectiveOutputDamageMultiplier;
	ScoringContext.BotIncomingMultiplier = Data.BotAttributes.EffectiveIncomingDamageMultiplier;
	ScoringContext.OpponentOutputMultiplier = Data.OpponentAttributes.EffectiveOutputDamageMultiplier;
	ScoringContext.OpponentIncomingMultiplier = Data.OpponentAttributes.EffectiveIncomingDamageMultiplier;
	ScoringContext.ModifierRoom = FMKHAttackCandidateBuilder::ComputeModifierRoom(BotAbilitySystem, OpponentAbilitySystem);

	ScoringContext.OpponentHealth = Data.OpponentAttributes.CurrentHealth;
	ScoringContext.OpponentArmorMitigation = Data.OpponentAttributes.CurrentArmorMitigation;
	ScoringContext.OpponentStamina = Data.OpponentAttributes.CurrentStamina;
	const FMKHItemEntry* OpponentWeapon = FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent);
	ScoringContext.OpponentBlockStability = OpponentWeapon != nullptr ? FMKHAttackCandidateBuilder::GetWeaponBlockStability(DataSubsystem, *OpponentWeapon) : 0.f;

	ScoringContext.bOpponentBlocking = Data.bIsOpponentBlocking;
	ScoringContext.OpponentCrowdControl = Data.OpponentCrowdControl;
	ScoringContext.OpponentCrowdControlTimeLeft = Data.OpponentCrowdControlTimeLeft;
	ScoringContext.PunishWindowScore = Data.PunishWindowScore;
	return ScoringContext;
}

void FMKHDuelBotAttackOptionsEvaluator::ScoreAndPublish(TConstArrayView<FMKHAttackCandidateInput> Inputs,
	const FMKHAttackScoringContext& Context, const FMKHAttackScoringParams& Params, TArray<FMKHAttackCandidate>& Candidates,
	FMKHDuelBotAttackOptionsEvaluatorInstanceData& Data)
{
	const TArray<FMKHAttackScoreBreakdown> Breakdowns = FMKHAttackScoring::ScoreCandidates(Inputs, Context, Params);
	TArray<float> Scores;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Candidates[Index].Breakdown = Breakdowns[Index];
		Candidates[Index].Score = Breakdowns[Index].Total;
		Scores.Add(Breakdowns[Index].Total);
	}

	// Expertise only sharpens the choice: the same scores, a more or less certain pick.
	const TArray<float> Probabilities = FMKHAttackScoring::ComputeChoiceProbabilities(Scores, Data.Tuning.GetDecisionSharpness(),
		Data.Tuning.IsExactChoice());
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Candidates[Index].Probability = Probabilities[Index];
	}

	Data.AttackUtility = FMKHAttackScoring::ComputeExpectedUtility(Scores, Probabilities);
	Data.bHasAttackCandidates = !Candidates.IsEmpty();
	Data.AttackCandidates = MoveTemp(Candidates);

	// The table changes every tick (distance): log it only when the best attack or the candidate count changes.
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Scores.Num(); ++Index)
	{
		BestIndex = BestIndex == INDEX_NONE || Scores[Index] > Scores[BestIndex] ? Index : BestIndex;
	}
	const FString Ranking = FString::Printf(TEXT("%d:%s"), Scores.Num(),
		BestIndex != INDEX_NONE ? *Data.AttackCandidates[BestIndex].AbilityTag.ToString() : TEXT("none"));
	if (Ranking != Data.LastLoggedRanking)
	{
		Data.LastLoggedRanking = Ranking;
		UE_VLOG(Data.AIController.Get(), LogMKHAI, Verbose, TEXT("Attack options (utility %.2f):\n%s"), Data.AttackUtility,
			*FormatCandidateTable(Data.AttackCandidates));
	}
}
