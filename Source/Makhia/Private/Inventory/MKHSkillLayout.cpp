// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/MKHSkillLayout.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Equipment/EquipmentTypes.h"
#include "QuickSlot/MKHQuickSlotPreferenceSaveGame.h"

namespace
{
	/** Finds the skill standing on a slot, mutable. */
	FEquipmentAbilityDefinition* FindMutableSkillInSlot(FEquipmentEffectPackage& Package, const FGameplayTag& SlotTag)
	{
		return Package.Abilities.FindByPredicate([&SlotTag](const FEquipmentAbilityDefinition& Ability)
		{
			return Ability.bIsSkillAbility && Ability.SkillInputTag.MatchesTagExact(SlotTag);
		});
	}
}

TConstArrayView<FGameplayTag> FMKHSkillLayout::GetSkillSlots()
{
	static const TArray<FGameplayTag> Slots = {
		MKHGameplayTags::Input::SkillSlot1.GetTag(),
		MKHGameplayTags::Input::SkillSlot2.GetTag(),
		MKHGameplayTags::Input::SkillSlot3.GetTag()
	};
	return Slots;
}

FGameplayTag FMKHSkillLayout::GetSlotAt(int32 Index)
{
	const TConstArrayView<FGameplayTag> Slots = GetSkillSlots();
	return Slots.IsValidIndex(Index) ? Slots[Index] : FGameplayTag();
}

bool FMKHSkillLayout::IsSkillSlot(const FGameplayTag& Tag)
{
	return Tag.IsValid() && GetSkillSlots().Contains(Tag);
}

const FEquipmentAbilityDefinition* FMKHSkillLayout::FindSkillInSlot(const FEquipmentEffectPackage& Package, const FGameplayTag& SlotTag)
{
	return Package.Abilities.FindByPredicate([&SlotTag](const FEquipmentAbilityDefinition& Ability)
	{
		return Ability.bIsSkillAbility && Ability.SkillInputTag.MatchesTagExact(SlotTag);
	});
}

bool FMKHSkillLayout::SwapSlots(FEquipmentEffectPackage& Package, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot)
{
	if (!IsSkillSlot(FromSlot) || !IsSkillSlot(ToSlot) || FromSlot == ToSlot)
	{
		return false;
	}

	FEquipmentAbilityDefinition* Moved = FindMutableSkillInSlot(Package, FromSlot);
	if (Moved == nullptr)
	{
		return false;
	}

	if (FEquipmentAbilityDefinition* Displaced = FindMutableSkillInSlot(Package, ToSlot))
	{
		Displaced->SkillInputTag = FromSlot;
	}
	Moved->SkillInputTag = ToSlot;
	return true;
}

bool FMKHSkillLayout::ApplyPreferences(FEquipmentEffectPackage& Package, TConstArrayView<FMKHSkillSlotPreference> Preferences)
{
	bool bChanged = false;
	TArray<FGameplayTag, TInlineAllocator<3>> ClaimedSlots;
	for (FEquipmentAbilityDefinition& Skill : Package.Abilities)
	{
		// A skill beyond the third has no slot and stays out of the layout.
		if (!Skill.bIsSkillAbility || !IsSkillSlot(Skill.SkillInputTag))
		{
			continue;
		}

		const FMKHSkillSlotPreference* Preference = Preferences.FindByPredicate([&Skill](const FMKHSkillSlotPreference& Candidate)
		{
			return Candidate.AbilityTag.MatchesTagExact(Skill.AbilityTag);
		});
		if (Preference == nullptr || !IsSkillSlot(Preference->SkillSlotTag) || ClaimedSlots.Contains(Preference->SkillSlotTag))
		{
			continue;
		}

		ClaimedSlots.Add(Preference->SkillSlotTag);
		bChanged |= SwapSlots(Package, Skill.SkillInputTag, Preference->SkillSlotTag);
	}
	return bChanged;
}
