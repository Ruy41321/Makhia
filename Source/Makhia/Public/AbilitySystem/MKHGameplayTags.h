// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/** Combat and damage-related gameplay tags. */
namespace MKHGameplayTags::Combat
{
	/** Data tags used to pass damage payload values through gameplay effects and execution calculations. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_StaminaDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_StatusEffectDuration);
	/** SetByCaller key carrying the dynamic value (%) of the status effect g.e. 15% for empower. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_StatusEffectValue);
	/** SetByCaller key carrying the dynamic cooldown duration of skill abilities. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_AbilityCooldownTime);
	/** SetByCaller key carrying the base Armor granted by an equipped armor piece (UMKHEquipmentArmorEffect). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Armor);
	/** Tag to determine if the next ability has to be queued or insta activated.*/
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputBufferWindow);
}

/** Movement state tags synchronized between movement systems, abilities, and animation logic. */
namespace MKHGameplayTags::State::Movement
{
	/** Character is standing still and not transitioning to another locomotion state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Idle);
	/** Character is moving at non-sprint speed while grounded. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Walking);
	/** Character is moving in sprint mode. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sprinting);
	/** Character is crouched and currently stationary. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CrouchingIdle);
	/** Character is crouched and currently moving. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CrouchingMoving);
	/** Character has started a jump action. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jumping);
	/** Character is airborne and descending or otherwise not grounded. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Falling);
	/** Character is landing from air while mostly stationary. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(LandingInPlace);
	/** Character is landing from air while preserving movement velocity. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(LandingMoving);
	/** Character is in dodging state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dodging);
	/** Character is in blocking state. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blocking);
	/** Character is attacking. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attacking);
	/** Character is dead. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
}

/** Generic gameplay state tags not tied to a single subsystem. */
namespace MKHGameplayTags::State
{
	/** Character cannot spend stamina and is considered exhausted. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(OutOfStamina);
	/** Character's stamina is regenerating (granted by GE_Status_StaminaRegen). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StaminaRecovery);
	/** Chatacter is Stunned. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
	/** Character is staggered. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Staggered);
	/** Character is using a quick slot item. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlotUse);
	/** Character survives at zero health: Event.Combat.LethalHitPrevented replaces its death (training dummy). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unkillable);
}

/** Equipment category and slot tags used by inventory and equipment systems. */
namespace MKHGameplayTags::Equip
{
	/** Generic equipment category root. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Category_Equipment);
	/** Weapon category root. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Category_Weapon);
	/** Armor category root. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Category_Armor);
	/** Consumable category root. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Category_Consumable);
	/** Armor slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ArmorSlot);
	/** Weapon slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSlot);
	/** Hatchet slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HatchetSlot);
	/** Sword slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SwordSlot);
	/** Hammer slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HammerSlot);
	/** First consumable quick-slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ConsumableQuickSlot1);
	/** Second consumable quick-slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ConsumableQuickSlot2);
	/** Third consumable quick-slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ConsumableQuickSlot3);
	/** First weapon quick-slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponQuickSlot1);
	/** Second weapon quick-slot tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponQuickSlot2);
}

/** Input routing tags used by Enhanced Input and ability activation logic. */
namespace MKHGameplayTags::Input
{
	/** Inventory toggle input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Inventory);
	/** Base ability input hierarchy tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability);
	/** Base quick-slot input hierarchy tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickSlot);
	/** Category tag used to filter weapon quick-slot entries. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponQuickSlotCategory);
	/** Category tag used to filter consumable quick-slot entries. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ConsumableQuickSlotCategory);
	/** Sheathe weapon input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SheatheWeapon);
	/** Attack input hierarchy tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attacks);
	/** Dodge input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dodge);
	/** Block input action tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block);
	/** Light attack input tag (the duel bot keeps its light attacks on the weapon in hand). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(LightAttack);
	/** Skill input hierarchy tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill);
	/** First skill slot input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillSlot1);
	/** Second skill slot input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillSlot2);
	/** Third skill slot input tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillSlot3);
}

/** Gameplay event tags sent through GAS event payloads and anim notifies. */
namespace MKHGameplayTags::Event
{
	/** Event fired when a player dies. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
	/** Event fired when a hitscan window starts. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitScanStart);
	/** Event fired when a hitscan window ends. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitScanEnd);
	/** Event fired when combo input buffering starts. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ContinueComboStart);
	/** Event fired when combo input buffering ends. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ContinueComboEnd);
	/** Event fired to apply the DMT in a specific frame e.g. Heavy Attack Charge finished. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ApplyDMT);
	/** Event fired to spawn projectile-based ability actors. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SpawnProjectile);
	/** Event fired to request quick-slot item usage. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UseQuickSlot);
	/** Event fired to notify that an attack was successfully blocked. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(BlockSuccessful);
	/** Event fired to notify that a blocking defender had their stamina broken. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GuardBreak);
	/** Event fired instead of the death when a character with State.General.Unkillable reaches zero health. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(LethalHitPrevented);
	/** Event fired to notify that an attack was dodged. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackDodged);
	/** Event fired to notify that a hit was inflicted. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitInflicted);
	/** Event fired to notify that a hit was received. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitReceived);
	/** Event fired to the scoring character when its opponent dies (AMKHGameMode). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyKilled);
	/** Event fired when the weapon in hand is put away (UEquipmentManagerComponent, server). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponSheathed);
	/** Event fired when a weapon is taken in hand (UEquipmentManagerComponent, server). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponUnsheathed);
	/** Event fired to allow the ability to be cancelled early, removing its priority tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(MakeAbilityCancellable);
	/** Event fired to try the activation of a previously queued ability. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateQueuedAbility);
	/** Event fired when the AI should react to an attack. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AIReactToAttack);
	/** Parent of the AI distance band events: a transition on it fires on any band change. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AIDistanceBand);
	/** Event fired when the AI enters the Combat distance band. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AIDistanceBandCombat);
	/** Event fired when the AI enters the Safe distance band. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AIDistanceBandSafe);
	/** Event fired when the AI enters the Far distance band. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AIDistanceBandFar);
}

/** Ability lifecycle and grouping tags. */
namespace MKHGameplayTags::Ability
{
	/** Root tag for every ability-specific tag in the project. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(All);
	/** Tag representing abilities that are currently active. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AbilityActive);
	/** Tag representing attack abilities that are currently active. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attacking);
	
	/** First priority for ability activation. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Priority1);
	/** Second priority for ability activation. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Priority2);

	/** Attack ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Attacks);
	/** Basic attack ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Attacks_Basic);
	/** Heavy basic attack ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Attacks_Basic_Heavy);
	/** Light basic attack ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Attacks_Basic_Light);
	/** Skill attack ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Attacks_Skill);
	/** Block ability type tag. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Block);
}

/** Duel bot states that can be disabled at runtime (MKH.Bot.State.*, condition MKH Bot State Enabled). */
namespace MKHGameplayTags::AIState
{
	/** Root of the bot state tags. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
	/** Combat > SafeApproach. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SafeApproach);
	/** Combat > Attack. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
	/** Combat > Bait. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bait);
	/** Combat > PlayPassive. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayPassive);
	/** React (the reaction to an opponent attack). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(React);
	/** React > Dodge. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReactDodge);
	/** React > Block. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReactBlock);
	/** React > ActivateAbility (counterattack). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReactActivateAbility);
	/** Combat > Idle: fallback enabled only while some state is disabled, so Combat always has a child to select. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Idle);

	/** @return Every selectable state tag above (Root excluded), in tree order. */
	MAKHIA_API const FGameplayTagContainer& GetSelectableStates();
}

namespace MKHGameplayTags::Misc
{
	/** Tag used to retrieve perk data table. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(PerkDataTable);
}

/** Families of status effects (perks, StatEffect.Perk.*): the AI reads what a hit does from its perk tags. */
namespace MKHGameplayTags::Perk
{
	/** Raises the output damage of who receives it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Empower);
	/** Lowers the output damage of who receives it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weaken);
	/** Raises the damage taken by who receives it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exposed);
	/** Lowers the damage taken by who receives it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reinforced);
	/** Stuns who receives it (State.Combat.CC.Stunned). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);
	/** Staggers who receives it (State.Combat.CC.Staggered). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stagger);
}

/**
 * Sound effect tags used by code (row names of DT_SoundEffects). The tags only used by assets (anim notifies, gameplay
 * cues) live in DefaultGameplayTags.ini. Documentation/AudioSystemArchitecture.md (Sound effects) lists them all.
 */
namespace MKHGameplayTags::SFX
{
	/** Footstep on a surface whose physical material names no footstep sound of its own. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(FootstepDefault);
	/** Click of an MKH Button (mouse or gamepad confirm). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UIClick);
	/** Hover (mouse) or focus (gamepad) of an MKH Button. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UIHover);
	/** An item equipped from the inventory dashboard. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UIEquip);
	/** An item unequipped from the inventory dashboard. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UIUnequip);
}
