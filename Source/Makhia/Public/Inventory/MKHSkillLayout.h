// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FEquipmentAbilityDefinition;
struct FEquipmentEffectPackage;
struct FMKHSkillSlotPreference;

/**
 * Pure rules of a weapon's skill slots: which skill stands on which of the three skill inputs. The keys stay on the
 * slots, the skills move (SkillInputTag of each skill on the item). See Documentation/InventoryDashboard.md.
 */
struct MAKHIA_API FMKHSkillLayout
{
	/** The skill slots in display order: the input tags SkillSlot1..3. */
	static TConstArrayView<FGameplayTag> GetSkillSlots();

	/**
	 * Gets a skill slot by its display position.
	 *
	 * @param Index Position (0 = first).
	 * @return The slot's input tag, or an invalid tag out of range.
	 */
	static FGameplayTag GetSlotAt(int32 Index);

	/** Returns whether a tag is exactly one of the skill slots. */
	static bool IsSkillSlot(const FGameplayTag& Tag);

	/**
	 * Finds the skill standing on a slot.
	 *
	 * @param Package   Abilities of the weapon.
	 * @param SlotTag   Skill slot.
	 * @return The skill, or nullptr when the slot is empty. Do not keep it across changes of the package.
	 */
	static const FEquipmentAbilityDefinition* FindSkillInSlot(const FEquipmentEffectPackage& Package, const FGameplayTag& SlotTag);

	/**
	 * Moves the skill of FromSlot to ToSlot; a skill already there takes FromSlot.
	 *
	 * @param Package  Abilities of the weapon.
	 * @param FromSlot Slot holding the skill to move.
	 * @param ToSlot   Destination slot, empty or not.
	 * @return False (nothing changed) when the slots are not two different skill slots or FromSlot is empty.
	 */
	static bool SwapSlots(FEquipmentEffectPackage& Package, const FGameplayTag& FromSlot, const FGameplayTag& ToSlot);

	/**
	 * Puts the skills the player remembers on their slots; the skills they push away take the slots left free.
	 * When two skills of the weapon claim one slot the first in the weapon's order wins.
	 *
	 * @param Package     Abilities of the weapon.
	 * @param Preferences Remembered slot of each skill.
	 * @return True when a skill moved.
	 */
	static bool ApplyPreferences(FEquipmentEffectPackage& Package, TConstArrayView<FMKHSkillSlotPreference> Preferences);
};
