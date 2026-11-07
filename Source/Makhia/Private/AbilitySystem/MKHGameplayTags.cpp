// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/MKHGameplayTags.h"

namespace MKHGameplayTags::Combat
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Combat.Data.Damage", "Set By Caller Data Tag For Combat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StaminaDamage, "Combat.Data.StaminaDamage", "Set By Caller Data Tag For Combat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StatusEffectDuration, "Combat.Data.StatusEffectDuration", "Set By Caller Data Tag For Combat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StatusEffectValue, "Combat.Data.StatusEffectValue", "Set By Caller Data Tag For Combat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_AbilityCooldownTime, "Combat.Data.AbilityCooldownTime", "Set By Caller Data Tag carrying the dynamic skill cooldown duration");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Armor, "Combat.Data.Armor", "Set By Caller Data Tag carrying the base Armor of an equipped armor piece");
	UE_DEFINE_GAMEPLAY_TAG(InputBufferWindow, "Combat.InputBufferWindow");
}

namespace MKHGameplayTags::State::Movement
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Idle,            "State.Movement.Idle",            "Character is idle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Walking,         "State.Movement.Walking",         "Character is walking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sprinting,       "State.Movement.Sprinting",       "Character is sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CrouchingIdle,   "State.Movement.CrouchingIdle",   "Character is crouching idle");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CrouchingMoving, "State.Movement.CrouchingMoving", "Character is crouching and moving");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Jumping,         "State.Movement.Jumping",         "Character is jumping");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Falling,         "State.Movement.Falling",         "Character is falling");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LandingInPlace,  "State.Movement.LandingInPlace",  "Character is landing in place");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LandingMoving,   "State.Movement.LandingMoving",   "Character is landing while moving");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dodging,         "State.Movement.Dodging",         "Character is dodging");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Blocking,        "State.Movement.Blocking",        "Character is blocking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attacking,       "State.Movement.Attacking",       "Character is attacking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dead,            "State.Movement.Dead",            "Character is dead");
}

namespace MKHGameplayTags::State
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(OutOfStamina, "State.General.Stamina.Out", "Character is out of stamina");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unkillable, "State.General.Unkillable", "Character survives at zero health (training dummy)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StaminaRecovery, "State.General.Stamina.Recovery", "Character's stamina is regenerating");
	UE_DEFINE_GAMEPLAY_TAG(Stunned, "State.Combat.CC.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(Staggered, "State.Combat.CC.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(QuickSlotUse, "State.General.QuickSlot.Use");
}

namespace MKHGameplayTags::Equip
{
	UE_DEFINE_GAMEPLAY_TAG(Category_Equipment, "Item.Equipment");
	UE_DEFINE_GAMEPLAY_TAG(Category_Weapon, "Item.Equipment.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Category_Armor, "Item.Equipment.Armor");
	UE_DEFINE_GAMEPLAY_TAG(Category_Consumable, "Item.Consumable");
	UE_DEFINE_GAMEPLAY_TAG(ArmorSlot, "Equipment.Slot.Armor");
	UE_DEFINE_GAMEPLAY_TAG(WeaponSlot, "Equipment.Slot.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(HatchetSlot, "Equipment.Slot.Weapon.Hatchet");
	UE_DEFINE_GAMEPLAY_TAG(SwordSlot, "Equipment.Slot.Weapon.Sword");
	UE_DEFINE_GAMEPLAY_TAG(HammerSlot, "Equipment.Slot.Weapon.Hammer");
	UE_DEFINE_GAMEPLAY_TAG(ConsumableQuickSlot1, "Input.Ability.QuickSlot.Consumable.First");
	UE_DEFINE_GAMEPLAY_TAG(ConsumableQuickSlot2, "Input.Ability.QuickSlot.Consumable.Second");
	UE_DEFINE_GAMEPLAY_TAG(ConsumableQuickSlot3, "Input.Ability.QuickSlot.Consumable.Third");
	UE_DEFINE_GAMEPLAY_TAG(WeaponQuickSlot1, "Input.Ability.QuickSlot.Weapon.Primary");
	UE_DEFINE_GAMEPLAY_TAG(WeaponQuickSlot2, "Input.Ability.QuickSlot.Weapon.Secondary");
}

namespace MKHGameplayTags::Input
{
	UE_DEFINE_GAMEPLAY_TAG(Inventory, "Input.Inventory");
	UE_DEFINE_GAMEPLAY_TAG(Ability, "Input.Ability");
	UE_DEFINE_GAMEPLAY_TAG(QuickSlot, "Input.Ability.QuickSlot");
	UE_DEFINE_GAMEPLAY_TAG(ConsumableQuickSlotCategory, "Input.Ability.QuickSlot.Consumable");
	UE_DEFINE_GAMEPLAY_TAG(WeaponQuickSlotCategory, "Input.Ability.QuickSlot.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(SheatheWeapon, "Input.Ability.SheatheWeapon");
	UE_DEFINE_GAMEPLAY_TAG(Attacks, "Input.Ability.Attacks");
	UE_DEFINE_GAMEPLAY_TAG(Dodge, "Input.Ability.Attacks.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Block, "Input.Ability.Attacks.Basics.Block");	
	UE_DEFINE_GAMEPLAY_TAG(LightAttack, "Input.Ability.Attacks.Basics.Light");
	UE_DEFINE_GAMEPLAY_TAG(Skill, "Input.Ability.Attacks.Skill");
	UE_DEFINE_GAMEPLAY_TAG(SkillSlot1, "Input.Ability.Attacks.Skill.First");
	UE_DEFINE_GAMEPLAY_TAG(SkillSlot2, "Input.Ability.Attacks.Skill.Second");
	UE_DEFINE_GAMEPLAY_TAG(SkillSlot3, "Input.Ability.Attacks.Skill.Third");
}

namespace MKHGameplayTags::Event
{
	UE_DEFINE_GAMEPLAY_TAG(Death, "Event.Combat.Death");
	UE_DEFINE_GAMEPLAY_TAG(HitScanStart, "Event.Animation.HitScan.Start");
	UE_DEFINE_GAMEPLAY_TAG(HitScanEnd, "Event.Animation.HitScan.End");
	UE_DEFINE_GAMEPLAY_TAG(ContinueComboStart, "Event.Animation.ContinueCombo.Start");
	UE_DEFINE_GAMEPLAY_TAG(ContinueComboEnd, "Event.Animation.ContinueCombo.End");
	UE_DEFINE_GAMEPLAY_TAG(ApplyDMT, "Event.Animation.ApplyDMT");
	UE_DEFINE_GAMEPLAY_TAG(SpawnProjectile, "Event.Animation.SpawnProjectile");
	UE_DEFINE_GAMEPLAY_TAG(UseQuickSlot, "Event.QuickSlot.Use");
	UE_DEFINE_GAMEPLAY_TAG(BlockSuccessful, "Event.Combat.BlockSuccessful");
	UE_DEFINE_GAMEPLAY_TAG(GuardBreak, "Event.Combat.GuardBreak");
	UE_DEFINE_GAMEPLAY_TAG(LethalHitPrevented, "Event.Combat.LethalHitPrevented");
	UE_DEFINE_GAMEPLAY_TAG(AttackDodged, "Event.Combat.AttackDodged");
	UE_DEFINE_GAMEPLAY_TAG(HitInflicted, "Event.Combat.HitInflicted");
	UE_DEFINE_GAMEPLAY_TAG(HitReceived, "Event.Combat.HitReceived");
	UE_DEFINE_GAMEPLAY_TAG(EnemyKilled, "Event.Combat.EnemyKilled");
	UE_DEFINE_GAMEPLAY_TAG(WeaponSheathed, "Event.Weapon.Sheathed");
	UE_DEFINE_GAMEPLAY_TAG(WeaponUnsheathed, "Event.Weapon.Unsheathed");
	UE_DEFINE_GAMEPLAY_TAG(MakeAbilityCancellable, "Event.Animation.MakeAbilityCancellable");
	UE_DEFINE_GAMEPLAY_TAG(ActivateQueuedAbility, "Event.Combat.ActivateQueuedAbility");
	UE_DEFINE_GAMEPLAY_TAG(AIReactToAttack, "Event.AI.ReactToAttack");
	UE_DEFINE_GAMEPLAY_TAG(AIDistanceBand, "Event.AI.DistanceBand");
	UE_DEFINE_GAMEPLAY_TAG(AIDistanceBandCombat, "Event.AI.DistanceBand.Combat");
	UE_DEFINE_GAMEPLAY_TAG(AIDistanceBandSafe, "Event.AI.DistanceBand.Safe");
	UE_DEFINE_GAMEPLAY_TAG(AIDistanceBandFar, "Event.AI.DistanceBand.Far");
}

namespace MKHGameplayTags::Ability
{
	UE_DEFINE_GAMEPLAY_TAG(All, "GameplayAbility")
	UE_DEFINE_GAMEPLAY_TAG(AbilityActive, "GameplayAbility.Active")
	UE_DEFINE_GAMEPLAY_TAG(Attacking, "GameplayAbility.Active.Attack")
	UE_DEFINE_GAMEPLAY_TAG(Priority1, "GameplayAbility.Active.Priority.First")
	UE_DEFINE_GAMEPLAY_TAG(Priority2, "GameplayAbility.Active.Priority.Second")

	UE_DEFINE_GAMEPLAY_TAG(Type_Attacks, "GameplayAbility.Type.Attacks")
	UE_DEFINE_GAMEPLAY_TAG(Type_Attacks_Basic, "GameplayAbility.Type.Attacks.Basic")
	UE_DEFINE_GAMEPLAY_TAG(Type_Attacks_Basic_Heavy, "GameplayAbility.Type.Attacks.Basic.Heavy")
	UE_DEFINE_GAMEPLAY_TAG(Type_Attacks_Basic_Light, "GameplayAbility.Type.Attacks.Basic.Light")
	UE_DEFINE_GAMEPLAY_TAG(Type_Attacks_Skill, "GameplayAbility.Type.Attacks.Skill")
	UE_DEFINE_GAMEPLAY_TAG(Type_Block, "GameplayAbility.Type.Block")
}

namespace MKHGameplayTags::AIState
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root, "AI.State", "Duel bot states that can be disabled at runtime");
	UE_DEFINE_GAMEPLAY_TAG(SafeApproach, "AI.State.SafeApproach");
	UE_DEFINE_GAMEPLAY_TAG(Attack, "AI.State.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Bait, "AI.State.Bait");
	UE_DEFINE_GAMEPLAY_TAG(PlayPassive, "AI.State.PlayPassive");
	UE_DEFINE_GAMEPLAY_TAG(React, "AI.State.React");
	UE_DEFINE_GAMEPLAY_TAG(ReactDodge, "AI.State.React.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(ReactBlock, "AI.State.React.Block");
	UE_DEFINE_GAMEPLAY_TAG(ReactActivateAbility, "AI.State.React.ActivateAbility");
	UE_DEFINE_GAMEPLAY_TAG(Idle, "AI.State.Idle");

	const FGameplayTagContainer& GetSelectableStates()
	{
		static const FGameplayTagContainer States = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
			SafeApproach, Attack, Bait, PlayPassive, React, ReactDodge, ReactBlock, ReactActivateAbility });
		return States;
	}
}

namespace MKHGameplayTags::Misc
{
	UE_DEFINE_GAMEPLAY_TAG(PerkDataTable, "StatEffect.Perk");
}

namespace MKHGameplayTags::Perk
{
	UE_DEFINE_GAMEPLAY_TAG(Empower, "StatEffect.Perk.Empower");
	UE_DEFINE_GAMEPLAY_TAG(Weaken, "StatEffect.Perk.Weaken");
	UE_DEFINE_GAMEPLAY_TAG(Exposed, "StatEffect.Perk.Exposed");
	UE_DEFINE_GAMEPLAY_TAG(Reinforced, "StatEffect.Perk.Reinforced");
	UE_DEFINE_GAMEPLAY_TAG(Stun, "StatEffect.Perk.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Stagger, "StatEffect.Perk.Stagger");
}

namespace MKHGameplayTags::SFX
{
	UE_DEFINE_GAMEPLAY_TAG(FootstepDefault, "SFX.Foley.Footstep");
	UE_DEFINE_GAMEPLAY_TAG(UIClick, "SFX.UI.Click");
	UE_DEFINE_GAMEPLAY_TAG(UIHover, "SFX.UI.Hover");
	UE_DEFINE_GAMEPLAY_TAG(UIEquip, "SFX.UI.Equip");
	UE_DEFINE_GAMEPLAY_TAG(UIUnequip, "SFX.UI.Unequip");
}
