// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MKHEquipmentArmorEffect.generated.h"

/**
 * Infinite effect granting the base Armor of an equipped armor piece (UEquipmentDefinition::BaseArmorGiven).
 * The amount is passed as SetByCaller (Combat.Data.Armor) and the active handle is tracked on the equipment
 * entry, so unequipping removes it. See Documentation/ArmorMitigation.md.
 */
UCLASS()
class MAKHIA_API UMKHEquipmentArmorEffect : public UGameplayEffect
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================
public:

	/** Builds the infinite additive modifier on UMKHAttributeSet::Armor driven by the Combat.Data.Armor SetByCaller. */
	UMKHEquipmentArmorEffect();
};
