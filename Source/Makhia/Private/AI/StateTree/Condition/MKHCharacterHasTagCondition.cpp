// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Condition/MKHCharacterHasTagCondition.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHCharacterHasTagCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Data.Character);
	if (!Data.Tag.IsValid() || !IsValid(AbilitySystem))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHCharacterHasTagCondition: missing tag or ability system on %s."),
			*GetNameSafe(Data.Character));
		return false;
	}

	// HasMatchingGameplayTag also accepts a parent of an owned tag; the exact check looks at the owned tags only.
	const bool bHasTag = Data.bExactMatch ? AbilitySystem->GetOwnedGameplayTags().HasTagExact(Data.Tag) : AbilitySystem->HasMatchingGameplayTag(Data.Tag);
	return bHasTag != Data.bInvert;
}
