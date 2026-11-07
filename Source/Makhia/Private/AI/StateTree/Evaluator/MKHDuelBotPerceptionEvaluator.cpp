// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotPerceptionEvaluator.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/MKHAbilityGrantPayload.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Abilities/MKHDamageAbility.h"
#include "AI/MKHAttackCandidateBuilder.h"
#include "AI/MKHDuelBot.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "AI/StateTree/Evaluator/MKHDuelBotContextEvaluator.h"
#include "Character/CharacterBase.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentTypes.h"
#include "GameMode/MKHGameState.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotPerceptionEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	// The scoring row is read every tick: DataTable edits and MKH.Bot.Tuning.Set apply at once.
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	Data.Params = IsValid(BotController) ? BotController->GetScoring().Perception : FMKHPerceptionParams();

	CheckBaitAwareness(Context, DeltaTime, Data);
	
	if (!IsValid(Data.BotCharacter) || !IsValid(Data.Opponent))
	{
		// Normal until the controller has found an opponent: Verbose, not to flood the log every tick.
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHDuelBotPerceptionEvaluator: no bot (%s) or opponent (%s) yet."),
			*GetNameSafe(Data.BotCharacter), *GetNameSafe(Data.Opponent));
		ResetOutputs(Data);
		return;
	}

	const UWorld* World = Context.GetWorld();
	const UAbilitySystemComponent* OpponentAbilitySystem = Data.Opponent->GetAbilitySystemComponent();
	if (!IsValid(World) || !IsValid(OpponentAbilitySystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotPerceptionEvaluator: missing world or ability system of %s."), *GetNameSafe(Data.Opponent));
		ResetOutputs(Data);
		return;
	}

	const float Now = World->GetTimeSeconds();
	UpdateAttackState(*OpponentAbilitySystem, Now, Data);
	UpdateAttackAbilityTag(*OpponentAbilitySystem, Data);
	UpdateShouldReact(Context, Data);
	SendReactionEventOnce(Context, Data);
	UpdateOpponentAbilitiesStatus(*OpponentAbilitySystem, Data);
	UpdateOpponentBlocking(*OpponentAbilitySystem, Now, Data);
	UpdateOpponentCrowdControl(*OpponentAbilitySystem, Now, Data);

	// Danger and windows read the ability data: without it they stay closed (no false punish).
	const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(Data.BotCharacter);
	if (IsValid(DataSubsystem))
	{
		UpdateOpponentAttackDanger(*DataSubsystem, Data);
		UpdateOpponentReadyDanger(*OpponentAbilitySystem, *DataSubsystem, Data);
		UpdatePunishWindows(*OpponentAbilitySystem, *DataSubsystem, Data);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotPerceptionEvaluator::ResetOutputs(FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	Data.bShouldReact = false;
	Data.bIsOpponentAttacking = false;
	Data.OpponentAttackAbilityTag = FGameplayTag();
	Data.SecondsSinceOpponentAttackStarted = 0.f;
	Data.SecondsSinceOpponentAttackEnded = NoRecentEventSeconds;
	Data.OpponentAbilitiesStatus = FMKHAbilitiesStatus();
	Data.bIsOpponentBlocking = false;
	Data.OpponentCrowdControl = EMKHCrowdControl::None;
	Data.OpponentCrowdControlTimeLeft = 0.f;
	Data.bKnowsCrowdControlTimeLeft = false;
	Data.OpponentAttackDanger = 0.f;
	Data.OpponentAttackStaminaDamage = 0.f;
	Data.OpponentReadyDanger = 0.f;
	Data.PunishWindowScore = 0.f;
	Data.PunishWindows = FMKHPunishWindows();
}

void FMKHDuelBotPerceptionEvaluator::UpdateAttackState(const UAbilitySystemComponent& OpponentAbilitySystem, const float Now,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	// Ability.Attacking is an activation owned tag of UMKHDamageAbility: present only while an attack runs.
	const bool bIsAttacking = OpponentAbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::Ability::Attacking);
	if (bIsAttacking != Data.bIsOpponentAttacking)
	{
		Data.bIsOpponentAttacking = bIsAttacking;
		if (bIsAttacking)
		{
			// Rising edge: one roll per attack, rolling every tick would make any chance a certainty.
			Data.OpponentAttackStartTime = Now;
			
			bool bIsBaiting = false;
			AMKHDuelBot* Bot = Cast<AMKHDuelBot>(Data.BotCharacter);
			if (IsValid(Bot))
			{
				bIsBaiting = Bot->IsBaiting();
			}
			
			Data.bReactionRollPassed = FMath::FRand() < (bIsBaiting ? Data.Tuning.BaitReactionChance : Data.Tuning.ReactionChance);
			const FFloatInterval ReactionTimeRange = bIsBaiting ? Data.Tuning.BaitReactionTimeRange : Data.Tuning.ReactionTimeRange;
			Data.CurrentReactionTime = FMath::RandRange(ReactionTimeRange.Min, ReactionTimeRange.Max);
			Data.bReactionEventSent = false;
		}
		else
		{
			Data.OpponentAttackEndTime = Now;
		}
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Opponent %s attack %s (reaction roll: %s, reaction time: %.2f s)."),
			*GetNameSafe(Data.Opponent), bIsAttacking ? TEXT("started") : TEXT("ended"),
			Data.bReactionRollPassed ? TEXT("react") : TEXT("ignore"), Data.CurrentReactionTime);
	}

	// Timings are refreshed every tick, not only on the edges.
	Data.SecondsSinceOpponentAttackStarted = bIsAttacking ? Now - Data.OpponentAttackStartTime : 0.f;
	Data.SecondsSinceOpponentAttackEnded = bIsAttacking ? NoRecentEventSeconds : Now - Data.OpponentAttackEndTime;
}

void FMKHDuelBotPerceptionEvaluator::UpdateAttackAbilityTag(const UAbilitySystemComponent& OpponentAbilitySystem,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	const FGameplayTag PreviousTag = Data.OpponentAttackAbilityTag;
	Data.OpponentAttackAbilityTag = FGameplayTag();

	const bool bCanRead = Data.Tuning.OpponentReadingFactor >= EOpponentReadingFactor::Medium;
	const bool bRecognised = Data.bIsOpponentAttacking && Data.SecondsSinceOpponentAttackStarted >= Data.CurrentReactionTime;
	const FMKHItemEntry* Weapon = bCanRead && bRecognised ? FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent) : nullptr;
	if (Weapon == nullptr)
	{
		return;
	}

	Data.OpponentAttackAbilityTag = FindRunningAttackTag(OpponentAbilitySystem, *Weapon);
	if (Data.OpponentAttackAbilityTag != PreviousTag)
	{
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Opponent attack recognised: %s."),
			*Data.OpponentAttackAbilityTag.ToString());
	}
}

FGameplayTag FMKHDuelBotPerceptionEvaluator::FindRunningAttackTag(const UAbilitySystemComponent& OpponentAbilitySystem,
	const FMKHItemEntry& Weapon)
{
	for (const FGameplayAbilitySpec& Spec : OpponentAbilitySystem.GetActivatableAbilities())
	{
		const UMKHAbilityGrantPayload* Payload = Cast<UMKHAbilityGrantPayload>(Spec.SourceObject.Get());
		if (!Spec.IsActive() || !IsValid(Payload) || !IsValid(Spec.Ability) || !Spec.Ability->IsA<UMKHDamageAbility>())
		{
			continue;
		}

		// Same input rule as UMKHAbilitySystemComponent::GrantEquipmentAbility: skills carry their rolled input tag.
		for (const FEquipmentAbilityDefinition& Ability : Weapon.EffectPackage.Abilities)
		{
			const bool bSameInput = Ability.bIsSkillAbility ? Ability.SkillInputTag == Payload->InputTag : !Payload->bIsSkillAbility;
			if (Ability.AbilityClass.Get() == Spec.Ability->GetClass() && bSameInput)
			{
				return Ability.AbilityTag;
			}
		}
	}
	return FGameplayTag();
}

void FMKHDuelBotPerceptionEvaluator::UpdateShouldReact(const FStateTreeExecutionContext& Context,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	const UWorld* World = Context.GetWorld();
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	const bool bCanFight = IsValid(GameState) && GameState->IsDuelRunning()
		&& !AMKHDuelBotController::IsCharacterDead(Data.BotCharacter)
		&& !AMKHDuelBotController::IsCharacterDead(Data.Opponent);

	Data.bShouldReact = bCanFight
		&& Data.bIsOpponentAttacking
		&& Data.bReactionRollPassed
		&& Data.SecondsSinceOpponentAttackStarted >= Data.CurrentReactionTime
		&& Data.DistanceBand == EMKHDuelDistanceBand::Combat;
}

void FMKHDuelBotPerceptionEvaluator::SendReactionEventOnce(FStateTreeExecutionContext& Context,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	if (!Data.bReactionEventSent && Data.bShouldReact)
	{
		// By active state, not IsBaiting: PlayPassive enables the bait awareness too, and it must stay defensive.
		if (AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController))
		{
			const bool bInBait = Context.GetActiveStateNames().Contains(FName(TEXT("Bait")));
			BotController->SetBaitWantsCounter(bInBait && FMath::FRand() < BotController->GetScoring().States.React.BaitCounterChance);
		}
		Context.SendEvent(MKHGameplayTags::Event::AIReactToAttack);
		Data.bReactionEventSent = true;
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Sent AIReactToAttack event."));
	}
}

void FMKHDuelBotPerceptionEvaluator::UpdateOpponentAbilitiesStatus(const UAbilitySystemComponent& OpponentAbilitySystem,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	if (Data.Tuning.OpponentReadingFactor != EOpponentReadingFactor::High)
	{
		Data.OpponentAbilitiesStatus = FMKHAbilitiesStatus();
		return;
	}

	FMKHDuelBotContextEvaluator::UpdateSheathedWeapon(*Data.Opponent, Data.OpponentSheathedWeapon);
	const FMKHItemEntry* SheathedWeapon = Data.OpponentSheathedWeapon.IsValid() ? &Data.OpponentSheathedWeapon : nullptr;
	FMKHDuelBotSelfCombatEvaluator::UpdateAbilitiesStatus(OpponentAbilitySystem,
		FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent), SheathedWeapon, false, Data.OpponentAbilitiesStatus);
}

void FMKHDuelBotPerceptionEvaluator::UpdateOpponentBlocking(const UAbilitySystemComponent& OpponentAbilitySystem, const float Now,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	const bool bBlocking = OpponentAbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::State::Movement::Blocking);
	if (bBlocking && !Data.bOpponentBlockSeen)
	{
		// Rising edge: one reaction delay per block, as for the attacks.
		Data.OpponentBlockStartTime = Now;
		Data.CurrentBlockReactionTime = FMath::RandRange(Data.Tuning.ReactionTimeRange.Min, Data.Tuning.ReactionTimeRange.Max);
	}
	Data.bOpponentBlockSeen = bBlocking;

	const bool bWasBlocking = Data.bIsOpponentBlocking;
	Data.bIsOpponentBlocking = bBlocking && Now - Data.OpponentBlockStartTime >= Data.CurrentBlockReactionTime;
	if (Data.bIsOpponentBlocking != bWasBlocking)
	{
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Opponent block %s."), Data.bIsOpponentBlocking ? TEXT("seen") : TEXT("ended"));
	}
}

void FMKHDuelBotPerceptionEvaluator::UpdateOpponentCrowdControl(const UAbilitySystemComponent& OpponentAbilitySystem,
	const float Now, FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	const EMKHCrowdControl CrowdControl = FMKHAttackCandidateBuilder::GetCrowdControl(OpponentAbilitySystem);
	if (CrowdControl != Data.OpponentCrowdControl)
	{
		Data.OpponentCrowdControlStartTime = Now;
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Opponent crowd control: %s."), *UEnum::GetValueAsString(CrowdControl));
	}
	Data.OpponentCrowdControl = CrowdControl;
	Data.OpponentCrowdControlTimeLeft = 0.f;
	Data.bKnowsCrowdControlTimeLeft = false;
	if (CrowdControl == EMKHCrowdControl::None)
	{
		return;
	}

	// Reading High: the exact time left of the effect granting the state (a very expert player counts it).
	if (Data.Tuning.OpponentReadingFactor == EOpponentReadingFactor::High)
	{
		const FGameplayTag StateTag = CrowdControl == EMKHCrowdControl::Stun ? MKHGameplayTags::State::Stunned : MKHGameplayTags::State::Staggered;
		const TArray<float> TimesLeft = OpponentAbilitySystem.GetActiveEffectsTimeRemaining(
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(StateTag.GetSingleTagContainer()));
		if (!TimesLeft.IsEmpty())
		{
			Data.OpponentCrowdControlTimeLeft = FMath::Max(FMath::Max(TimesLeft), 0.f);
			Data.bKnowsCrowdControlTimeLeft = true;
			return;
		}
	}

	// Lower readings only see that the opponent is stunned: estimate from the typical length.
	const float TypicalDuration = CrowdControl == EMKHCrowdControl::Stun ? Data.Params.TypicalStunDuration : Data.Params.TypicalStaggerDuration;
	Data.OpponentCrowdControlTimeLeft = FMath::Max(TypicalDuration - (Now - Data.OpponentCrowdControlStartTime), 0.f);
}

void FMKHDuelBotPerceptionEvaluator::UpdateOpponentAttackDanger(const UMKHDataSubsystem& DataSubsystem,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	Data.OpponentAttackDanger = 0.f;
	Data.OpponentAttackStaminaDamage = 0.f;
	PublishIncomingStrike(nullptr, Data);
	const FMKHItemEntry* Weapon = Data.OpponentAttackAbilityTag.IsValid()
		? FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent) : nullptr;
	if (Weapon == nullptr)
	{
		return;
	}

	const float WeaponBaseDamage = FMKHAttackCandidateBuilder::GetWeaponBaseDamage(DataSubsystem, *Weapon);
	for (const FEquipmentAbilityDefinition& Ability : Weapon->EffectPackage.Abilities)
	{
		FMKHAttackCandidateInput OpponentAttack;
		if (Ability.AbilityTag != Data.OpponentAttackAbilityTag
			|| !FMKHAttackCandidateBuilder::BuildCandidate(DataSubsystem, Ability, WeaponBaseDamage, OpponentAttack))
		{
			continue;
		}

		Data.OpponentAttackDanger = ComputeOpponentAbilityDanger(DataSubsystem, Ability, WeaponBaseDamage, Data);
		for (const FMKHAttackStrikeInput& Strike : OpponentAttack.Strikes)
		{
			Data.OpponentAttackStaminaDamage += Strike.StaminaDamage;
		}
		Data.LastSeenAttackDanger = Data.OpponentAttackDanger;
		PublishIncomingStrike(&OpponentAttack, Data);
		return;
	}
}

void FMKHDuelBotPerceptionEvaluator::PublishIncomingStrike(const FMKHAttackCandidateInput* OpponentAttack,
	const FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	if (!IsValid(BotController))
	{
		return;
	}

	// Strike times run from the activation: the next one is the first still ahead of the time elapsed.
	FMKHIncomingStrike IncomingStrike;
	if (OpponentAttack != nullptr)
	{
		const FMKHAttackStrikeInput* NextStrike = OpponentAttack->Strikes.FindByPredicate([&Data](const FMKHAttackStrikeInput& Strike)
		{
			return Strike.TimeToHit > Data.SecondsSinceOpponentAttackStarted;
		});
		if (NextStrike != nullptr)
		{
			IncomingStrike.CrowdControl = NextStrike->CrowdControl;
			IncomingStrike.SecondsToHit = NextStrike->TimeToHit - Data.SecondsSinceOpponentAttackStarted;
		}
	}
	BotController->SetIncomingStrike(IncomingStrike);
}

void FMKHDuelBotPerceptionEvaluator::UpdateOpponentReadyDanger(const UAbilitySystemComponent& OpponentAbilitySystem,
	const UMKHDataSubsystem& DataSubsystem, FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	Data.OpponentReadyDanger = 0.f;
	if (Data.Tuning.OpponentReadingFactor == EOpponentReadingFactor::Medium)
	{
		// Medium does not track cooldowns: it remembers how dangerous the last attack it recognised was.
		Data.OpponentReadyDanger = Data.LastSeenAttackDanger;
		return;
	}
	const FMKHItemEntry* Weapon = Data.Tuning.OpponentReadingFactor == EOpponentReadingFactor::High
		? FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent) : nullptr;
	if (Weapon == nullptr)
	{
		return;
	}

	// High: every ability of the weapon in hand that is off cooldown (a swap would be seen first).
	const float WeaponBaseDamage = FMKHAttackCandidateBuilder::GetWeaponBaseDamage(DataSubsystem, *Weapon);
	for (const FEquipmentAbilityDefinition& Ability : Weapon->EffectPackage.Abilities)
	{
		const bool bInCooldown = Ability.CooldownTag.IsValid() && OpponentAbilitySystem.HasMatchingGameplayTag(Ability.CooldownTag);
		if (!bInCooldown)
		{
			Data.OpponentReadyDanger = FMath::Max(Data.OpponentReadyDanger, ComputeOpponentAbilityDanger(DataSubsystem, Ability, WeaponBaseDamage, Data));
		}
	}
}

void FMKHDuelBotPerceptionEvaluator::UpdatePunishWindows(const UAbilitySystemComponent& OpponentAbilitySystem,
	const UMKHDataSubsystem& DataSubsystem, FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	FMKHPunishWindows Windows;
	float Score = 0.f;
	const EOpponentReadingFactor Reading = Data.Tuning.OpponentReadingFactor;

	// Low: what anyone sees (the attack just ended, the stamina bar is empty).
	if (!Data.bIsOpponentAttacking && Data.SecondsSinceOpponentAttackEnded < Data.Params.RecoveryWindowDuration)
	{
		Windows.bRecovery = true;
		Score = FMath::Max(Score, Data.Params.RecoveryWindowScore * (1.f - Data.SecondsSinceOpponentAttackEnded / Data.Params.RecoveryWindowDuration));
	}
	if (OpponentAbilitySystem.HasMatchingGameplayTag(MKHGameplayTags::State::OutOfStamina))
	{
		Windows.bOutOfStamina = true;
		Score = FMath::Max(Score, Data.Params.OutOfStaminaWindowScore);
	}

	// Medium: the opponent cannot pay for its last way out.
	if (Reading >= EOpponentReadingFactor::Medium && Data.OpponentAttributes.StaminaRatio < Data.OpponentAttributes.DodgeStaminaCostRatio)
	{
		Windows.bLastDodge = true;
		Score = FMath::Max(Score, Data.Params.LastDodgeWindowScore);
	}

	// High: the bot remembers the opponent's cooldowns.
	const float CooldownScore = Reading == EOpponentReadingFactor::High ? ComputeDangerousInCooldownScore(OpponentAbilitySystem, DataSubsystem, Data) : 0.f;
	if (CooldownScore > 0.f)
	{
		Windows.bDangerousInCooldown = true;
		Score = FMath::Max(Score, CooldownScore);
	}

	if (Windows != Data.PunishWindows)
	{
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Punish windows: recovery %d, out of stamina %d, last dodge %d, dangerous in cooldown %d -> %.2f."),
			Windows.bRecovery, Windows.bOutOfStamina, Windows.bLastDodge, Windows.bDangerousInCooldown, Score);
	}
	Data.PunishWindows = Windows;
	Data.PunishWindowScore = Score;
}

float FMKHDuelBotPerceptionEvaluator::ComputeDangerousInCooldownScore(const UAbilitySystemComponent& OpponentAbilitySystem,
	const UMKHDataSubsystem& DataSubsystem, const FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	const FMKHItemEntry* CurrentWeapon = FMKHDuelBotContextEvaluator::FindCurrentWeapon(*Data.Opponent);
	const FMKHItemEntry* SheathedWeapon = Data.OpponentSheathedWeapon.IsValid() ? &Data.OpponentSheathedWeapon : nullptr;

	bool bDangerousInCooldown = false;
	float MaxAvailableDanger = 0.f;
	for (const FMKHItemEntry* Weapon : { CurrentWeapon, SheathedWeapon })
	{
		if (Weapon == nullptr)
		{
			continue;
		}
		const float WeaponBaseDamage = FMKHAttackCandidateBuilder::GetWeaponBaseDamage(DataSubsystem, *Weapon);
		for (const FEquipmentAbilityDefinition& Ability : Weapon->EffectPackage.Abilities)
		{
			const float Danger = ComputeOpponentAbilityDanger(DataSubsystem, Ability, WeaponBaseDamage, Data);
			const bool bInCooldown = Ability.CooldownTag.IsValid() && OpponentAbilitySystem.HasMatchingGameplayTag(Ability.CooldownTag);
			bDangerousInCooldown |= bInCooldown && Danger >= Data.Params.DangerousAbilityThreshold;

			// Only the weapon in hand threatens now: the other one needs a swap the bot would see.
			MaxAvailableDanger = !bInCooldown && Weapon == CurrentWeapon ? FMath::Max(MaxAvailableDanger, Danger) : MaxAvailableDanger;
		}
	}
	return bDangerousInCooldown ? Data.Params.DangerousInCooldownWindowScore * (1.f - MaxAvailableDanger) : 0.f;
}

float FMKHDuelBotPerceptionEvaluator::ComputeOpponentAbilityDanger(const UMKHDataSubsystem& DataSubsystem,
	const FEquipmentAbilityDefinition& Ability, const float WeaponBaseDamage, const FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	FMKHAttackCandidateInput OpponentAbility;
	if (!FMKHAttackCandidateBuilder::BuildCandidate(DataSubsystem, Ability, WeaponBaseDamage, OpponentAbility))
	{
		return 0.f;
	}
	return FMKHAttackScoring::ComputeAbilityDanger(OpponentAbility, Data.OpponentAttributes.EffectiveOutputDamageMultiplier,
		Data.BotAttributes.EffectiveIncomingDamageMultiplier, Data.BotAttributes.CurrentArmorMitigation,
		Data.BotAttributes.CurrentHealth, Data.Params.Danger);
}

void FMKHDuelBotPerceptionEvaluator::CheckBaitAwareness(FStateTreeExecutionContext& Context, const float DeltaTime,
	FMKHDuelBotPerceptionEvaluatorInstanceData& Data)
{
	AMKHDuelBot* Bot = Cast<AMKHDuelBot>(Data.BotCharacter);
	if (!IsValid(Bot) || !Bot->IsBaiting())
	{
		return ;
	}
	
	Bot->SetBaitElapsedTime(Bot->GetBaitElapsedTime() + DeltaTime);
	if (Bot->GetBaitElapsedTime() >= Data.MaxBaitTime)
	{
		Bot->SetIsBaiting(false);
		UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Bait awareness disabled after %.2f seconds."), Data.MaxBaitTime);
	}
}
