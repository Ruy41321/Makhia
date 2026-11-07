// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MKHAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Core attribute set containing character stats (Health, Stamina, Armor)
 * and combat related attributes.
 */
UCLASS()
class MAKHIA_API UMKHAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	/** Registers properties for network replication */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/** Clamps the transient CurrentValue before it is modified (duration/infinite modifiers, direct sets). */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Clamps the persistent BaseValue before it is modified (instant effects, ApplyModToAttribute). */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/** Adjusts base/current values when max values change */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	
	/** Handles execution of gameplay effects (used for damage processing) */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// -------------------------------------------------------------------
	// Health Attributes
	// -------------------------------------------------------------------
	
	/** Current Health of the character */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing=OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Health)

	/** Maximum Health of the character */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxHealth)

	// -------------------------------------------------------------------
	// Armor Attributes
	// -------------------------------------------------------------------

	/**
	 * Armor rating granted by the equipped armor pieces (base value + rolled bonuses). Never consumed by hits:
	 * it permanently reduces the damage reaching Health, see Documentation/ArmorMitigation.md.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Armor", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Armor)

	/**
	 * Fraction of an incoming hit the current Armor keeps away from Health, for HUD/tooltips ("Armor: 50%").
	 *
	 * @return Damage reduction in [0, 1).
	 */
	UFUNCTION(BlueprintPure, Category = "Attributes|Armor")
	float GetArmorDamageReduction() const;

	/**
	 * Damage reduction granted by an armor rating: Armor / (Armor + ArmorForHalfReduction).
	 * Single source of the formula, shared by UExecCalc_Damage and the UI.
	 *
	 * @param ArmorValue  Armor rating, negative values count as 0.
	 * @return            Damage reduction in [0, 1).
	 */
	UFUNCTION(BlueprintPure, Category = "Attributes|Armor")
	static float CalculateArmorDamageReduction(float ArmorValue);

	// -------------------------------------------------------------------
	// Stamina Attributes
	// -------------------------------------------------------------------

	/** Current Stamina of the character */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Stamina", ReplicatedUsing=OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Stamina)

	/** Maximum Stamina of the character */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxStamina)

	/** Cost of a dodge action */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Stamina", ReplicatedUsing = OnRep_DodgeStaminaCost)
	FGameplayAttributeData DodgeStaminaCost;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, DodgeStaminaCost)
	
	// -------------------------------------------------------------------
	// Combat Attributes
	// -------------------------------------------------------------------

	/** Chance to score a critical hit (0 - 1) */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat", ReplicatedUsing = OnRep_CritChance)
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, CritChance);

	/** Damage multiplier when scoring a critical hit */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat", ReplicatedUsing = OnRep_CritDamageMod)
	FGameplayAttributeData CritDamageMod;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, CritDamageMod);

	// -------------------------------------------------------------------
	// Buff / Debuff Attributes
	// Values are decimal fractions (0.3 = 30%). Applied dynamically by
	// gameplay effects coming from consumable items or skills, consumed by
	// UExecCalc_Damage when computing the final damage.
	// -------------------------------------------------------------------

	/** Bonus added to the attack's damage multiplier before scaling the weapon's base damage (offensive buff, on the Source). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_AdditiveBaseDamage)
	FGameplayAttributeData AdditiveBaseDamage;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, AdditiveBaseDamage)

	/** Cap for AdditiveBaseDamage. Defaults to 0.5 (50%). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_MaxAdditiveBaseDamage)
	FGameplayAttributeData MaxAdditiveBaseDamage = 0.5f;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxAdditiveBaseDamage)

	/** Fractional increase of the outgoing damage (offensive buff, on the Source). Balanced against Weaken. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_Empower)
	FGameplayAttributeData Empower;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Empower)

	/** Cap for Empower. Defaults to 0.5 (50%). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_MaxEmpower)
	FGameplayAttributeData MaxEmpower = 0.5f;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxEmpower)

	/** Fractional decrease of the outgoing damage (offensive debuff, on the Source). Balanced against Empower. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_Weaken)
	FGameplayAttributeData Weaken;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Weaken)

	/** Cap for Weaken. Defaults to 0.5 (50%). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_MaxWeaken)
	FGameplayAttributeData MaxWeaken = 0.5f;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxWeaken)

	/** Fractional decrease of the incoming damage (defensive buff, on the Target). Balanced against Exposed. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_Reinforced)
	FGameplayAttributeData Reinforced;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Reinforced)

	/** Cap for Reinforced. Defaults to 0.5 (50%). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_MaxReinforced)
	FGameplayAttributeData MaxReinforced = 0.5f;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxReinforced)

	/** Fractional increase of the incoming damage (defensive debuff, on the Target). Balanced against Reinforced. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_Exposed)
	FGameplayAttributeData Exposed;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, Exposed)

	/** Cap for Exposed. Defaults to 0.5 (50%). */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|BuffDebuff", ReplicatedUsing = OnRep_MaxExposed)
	FGameplayAttributeData MaxExposed = 0.5f;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, MaxExposed)

	// -------------------------------------------------------------------
	// Meta Attributes
	// -------------------------------------------------------------------

	/** Meta attribute for incoming damage computation. Not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, IncomingDamage);

	/** Meta attribute for incoming stamina damage computation. Not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingStaminaDamage;
	ATTRIBUTE_ACCESSORS(UMKHAttributeSet, IncomingStaminaDamage);

private:

	/**
	 * Latches once the death event has been sent for the current life, and is cleared when Health is
	 * restored above zero.
	 */
	bool bOutOfHealth = false;

	/** True once Health has been positive at least once, so the character can actually die. */
	bool bHasBeenAlive = false;

	/**
	 * Clamps clampable attributes (Health, Stamina, Armor, buffs) to their valid range.
	 * Shared by PreAttributeChange (CurrentValue) and PreAttributeBaseChange (BaseValue)
	 * so both the transient and the persistent value respect the same bounds.
	 *
	 * @param Attribute  The attribute being modified.
	 * @param NewValue   The incoming value, clamped in place if the attribute is clampable.
	 */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/**
	 * Processes the incoming damage attribute (already mitigated by Armor in UExecCalc_Damage) and applies it to Health.
	 * @param Data Gameplay Effect callback data containing the damage modification.
	 */
	void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);

	/**
	 * Processes the incoming stamina damage attribute, reducing stamina and handling guard breaks.
	 */
	void HandleIncomingStaminaDamage(const FGameplayEffectModCallbackData& Data);

	/** Builds the shared gameplay event payload (context, instigator, target, magnitude) used by hit/dodge/block/guard-break notifications. */
	static FGameplayEventData BuildCombatEventData(const FGameplayEffectContextHandle& ContextHandle, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float Magnitude);

	/** Sends a combat gameplay event to the recipient ASC, if valid. */
	static void SendCombatEvent(UAbilitySystemComponent* RecipientASC, const FGameplayTag& EventTag, const FGameplayEventData& EventData);

	/**
	 * Applies the on-hit status effects carried by the effect context: target effects to the
	 * hit target, self on-hit effects back to the attacker. Runs after the damage pipeline,
	 * so self on-hit buffs never affect the damage of the hit that triggered them.
	 * Lives here rather than in ExecCalc_Damage so executions stay pure computation.
	 */
	void ApplyStatusEffectsFromContext(const FGameplayEffectContextHandle& ContextHandle, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const;

	/**
	 * Sends Event::Death when Health reaches zero.
	 *
	 * @param ASC  The owning ability system component.
	 */
	void TrySendDeathEvent(UAbilitySystemComponent* ASC);

	/** Scales a current attribute proportionally when its associated max attribute changes. */
	void AdjustAttributeForMaxChange(
		const FGameplayAttribute& AffectedAttribute,
		float OldMaxValue,
		float NewMaxValue) const;

	/**
	 * Clamps both the base and current value of an attribute to [0, NewMaxValue].
	 * Used for buff/debuff attributes when their cap changes: unlike vital stats
	 * (Health/Stamina) they are not scaled proportionally, only re-clamped.
	 */
	static void ClampAttributeDataToMax(FGameplayAttributeData& AttributeData, float NewMaxValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldArmor);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	void OnRep_DodgeStaminaCost(const FGameplayAttributeData& OldDodgeStaminaCost);
	
	UFUNCTION()
	void OnRep_CritChance(const FGameplayAttributeData& OldCritChance);

	UFUNCTION()
	void OnRep_CritDamageMod(const FGameplayAttributeData& OldCritDamageMod);

	UFUNCTION()
	void OnRep_AdditiveBaseDamage(const FGameplayAttributeData& OldAdditiveBaseDamage);

	UFUNCTION()
	void OnRep_MaxAdditiveBaseDamage(const FGameplayAttributeData& OldMaxAdditiveBaseDamage);

	UFUNCTION()
	void OnRep_Empower(const FGameplayAttributeData& OldEmpower);

	UFUNCTION()
	void OnRep_MaxEmpower(const FGameplayAttributeData& OldMaxEmpower);

	UFUNCTION()
	void OnRep_Weaken(const FGameplayAttributeData& OldWeaken);

	UFUNCTION()
	void OnRep_MaxWeaken(const FGameplayAttributeData& OldMaxWeaken);

	UFUNCTION()
	void OnRep_Reinforced(const FGameplayAttributeData& OldReinforced);

	UFUNCTION()
	void OnRep_MaxReinforced(const FGameplayAttributeData& OldMaxReinforced);

	UFUNCTION()
	void OnRep_Exposed(const FGameplayAttributeData& OldExposed);

	UFUNCTION()
	void OnRep_MaxExposed(const FGameplayAttributeData& OldMaxExposed);
};
