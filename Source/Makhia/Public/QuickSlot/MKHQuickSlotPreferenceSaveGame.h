// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/SaveGame.h"
#include "MKHQuickSlotPreferenceSaveGame.generated.h"

/** One remembered quick slot: the kind of item the player put in it (never a concrete item). */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHQuickSlotPreference
{
	GENERATED_BODY()

	/** Creates an empty preference. */
	FMKHQuickSlotPreference() = default;

	/** Creates a preference for a quick slot and an item kind. */
	FMKHQuickSlotPreference(const FGameplayTag& InQuickSlotTag, const FGameplayTag& InItemKindTag)
		: QuickSlotTag(InQuickSlotTag), ItemKindTag(InItemKindTag)
	{
	}

	/** Quick slot (Input.Ability.QuickSlot.Weapon.* or Input.Ability.QuickSlot.Consumable.*). */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot")
	FGameplayTag QuickSlotTag;

	/** Item kind (e.g. Item.Equipment.Weapon.Sword, Item.Consumable.HealthPotion), see UMKHQuickSlotPreferenceComponent::GetItemKindTag. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot")
	FGameplayTag ItemKindTag;

	/** Returns whether both tags are set. */
	bool IsSet() const
	{
		return QuickSlotTag.IsValid() && ItemKindTag.IsValid();
	}

	/** Returns whether both tags match exactly. */
	bool operator==(const FMKHQuickSlotPreference& Other) const
	{
		return QuickSlotTag == Other.QuickSlotTag && ItemKindTag == Other.ItemKindTag;
	}
};

/** One remembered skill: the skill input the player keeps it on, on every weapon that grants it. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHSkillSlotPreference
{
	GENERATED_BODY()

	/** Creates an empty preference. */
	FMKHSkillSlotPreference() = default;

	/** Creates a preference for a skill and a skill slot. */
	FMKHSkillSlotPreference(const FGameplayTag& InAbilityTag, const FGameplayTag& InSkillSlotTag)
		: AbilityTag(InAbilityTag), SkillSlotTag(InSkillSlotTag)
	{
	}

	/** AbilityTag of the skill. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot")
	FGameplayTag AbilityTag;

	/** Skill slot (FMKHSkillLayout::GetSkillSlots). */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Slot")
	FGameplayTag SkillSlotTag;
};

/**
 * Quick slot preferences of this machine, kept by UMKHQuickSlotPreferenceComponent (save slot "QuickSlotPreferences")
 * and applied to the starting loadout of the next match.
 */
UCLASS()
class MAKHIA_API UMKHQuickSlotPreferenceSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Item kind remembered for each quick slot, one entry per slot. */
	UPROPERTY()
	TArray<FMKHQuickSlotPreference> Preferences;

	/** Armor types the player took off by hand and did not put back on (FMKHDefaultQuickSlots::GetArmorType). */
	UPROPERTY()
	TArray<FGameplayTag> UnequippedArmorTypes;

	/** True once the player's choices exist: the first match's defaults never apply again. Saves from before it read false. */
	UPROPERTY()
	bool bDefaultsApplied = false;

	/** Skill slot the player chose for each skill moved by hand. */
	UPROPERTY()
	TArray<FMKHSkillSlotPreference> SkillSlotPreferences;
};
