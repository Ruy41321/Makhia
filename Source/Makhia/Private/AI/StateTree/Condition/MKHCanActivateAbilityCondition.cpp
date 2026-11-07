// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Condition/MKHCanActivateAbilityCondition.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AI/MKHDuelBot.h"
#include "AIController.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHCanActivateAbilityCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	APawn* Pawn = IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr;
	const UMKHAbilitySystemComponent* AbilitySystem = Cast<UMKHAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn));
	if (!Data.InputTag.IsValid() || !IsValid(AbilitySystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHCanActivateAbilityCondition: missing input tag or ability system on %s."),
			*GetNameSafe(Pawn));
		return false;
	}

	// A quick slot is usable only with an item in it, which GA_QuickSlot's CanActivateAbility does not check.
	if (const AMKHDuelBot* Bot = Cast<AMKHDuelBot>(Pawn); IsValid(Bot) && Data.InputTag.MatchesTag(MKHGameplayTags::Input::QuickSlot))
	{
		return Bot->CanUseQuickSlot(Data.InputTag);
	}

	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecForInputTag(Data.InputTag);
	return Spec != nullptr && IsValid(Spec->Ability)
		&& Spec->Ability->CanActivateAbility(Spec->Handle, AbilitySystem->AbilityActorInfo.Get());
}
