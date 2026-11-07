// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Effects/MKHEquipmentArmorEffect.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"

UMKHEquipmentArmorEffect::UMKHEquipmentArmorEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FSetByCallerFloat ArmorAmount;
	ArmorAmount.DataTag = MKHGameplayTags::Combat::Data_Armor;

	FGameplayModifierInfo ArmorModifier;
	ArmorModifier.Attribute = UMKHAttributeSet::GetArmorAttribute();
	ArmorModifier.ModifierOp = EGameplayModOp::Additive;
	ArmorModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ArmorAmount);

	Modifiers.Add(ArmorModifier);
}
