// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "MKHAbilitySystemComponent.generated.h"

class UEquipmentManagerComponent;
class UGameplayAbility;
class UGameplayEffect;
class UMKHGameplayAbility;
struct FEquipmentStatEffectDefinition;
struct FEquipmentAbilityDefinition;
struct FEquipmentEffectPackage;
struct FRPGEquipmentEntry;

/** Delegate fired after default attributes are granted to the owning avatar. */
DECLARE_MULTICAST_DELEGATE(FOnAttributesGiven);

/** Gameplay Ability System component with character setup, input routing, and equipment integration. */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	// =========================================================================================
	// Delegates & Events
	// =========================================================================================

	/** Delegate fired after default attributes are granted to the owning avatar. */
	FOnAttributesGiven OnAttributesGiven;


	// =========================================================================================
	// Initialization & Core Abilities
	// =========================================================================================

	virtual void BeginPlay() override;

	/**
	 * Called whenever an ability spec is given, both on the server (via GiveAbility) and on
	 * clients (via replication of ActivatableAbilities). Used to retry a queued input tag
	 * once the ability it depends on has actually become available (see HasGrantedAbilityForInputTag).
	 * @param AbilitySpec The ability spec that was just granted.
	 */
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	/**
	 * Grants active character abilities and binds their configured input tags.
	 * @param AbilitiesToGrant Ability classes to grant to this component.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant);

	/**
	 * Grants passive abilities and immediately activates them once.
	 * @param PassivesToGrant Passive ability classes to grant and auto-activate.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassivesToGrant);

	/**
	 * Applies the default attribute gameplay effect to self.
	 * @param AttributeEffect Gameplay effect class used to initialize attributes.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	void InitializeDefaultAttributes(const TSubclassOf<UGameplayEffect>& AttributeEffect);


	// =========================================================================================
	// Input Handling
	// =========================================================================================

	/**
	 * Handles pressed input tags by activating or forwarding input to matching abilities.
	 * @param InputTag Gameplay tag produced by the input layer.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Input")
	void AbilityInputPressed(const FGameplayTag& InputTag, bool bForceQueue);

	/**
	 * Handles released input tags for abilities that consume release events.
	 * @param InputTag Gameplay tag produced by the input layer.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Input")
	void AbilityInputReleased(const FGameplayTag& InputTag);


	// =========================================================================================
	// Equipment Integration
	// =========================================================================================

	/**
	 * Grants all gameplay effects of an equipment entry: the definition's base armor and the rolled stat effects.
	 * @param EquipmentEntry Mutable equipment entry owning granted effect handles.
	 */
	void AddEquipmentEffects(FRPGEquipmentEntry* EquipmentEntry);

	/**
	 * Removes every active gameplay effect previously granted by an equipment entry.
	 * @param EquipmentEntry Mutable equipment entry containing granted effect handles.
	 */
	void RemoveEquipmentEffects(FRPGEquipmentEntry* EquipmentEntry);

	/**
	 * Grants all abilities declared by an equipment entry.
	 * @param EquipmentEntry Mutable equipment entry owning granted ability handles.
	 */
	void AddEquipmentAbility(FRPGEquipmentEntry* EquipmentEntry);

	/**
	 * Removes all abilities previously granted by an equipment entry.
	 * @param EquipmentEntry Mutable equipment entry containing granted ability handles.
	 */
	void RemoveEquipmentAbility(FRPGEquipmentEntry* EquipmentEntry);

	// =========================================================================================
	// Utility
	// =========================================================================================

	/**
	 * Gets the remaining cooldown time and total cooldown duration for the Gameplay
	 * Effect that grants the given cooldown tag.
	 * @param CooldownTag The gameplay tag representing the cooldown to query.
	 * @param TimeRemaining Output parameter for the remaining cooldown time in seconds.
	 * @param CooldownDuration Output parameter for the total cooldown duration in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	void GetCooldownRemainingForTag(FGameplayTag CooldownTag, float& TimeRemaining, float& CooldownDuration) const;

	/**
	 * Returns true if an activatable ability spec is already granted for the given input tag.
	 * Used to tell whether an input should be queued (ability not granted yet, e.g. still waiting
	 * for a weapon quick-equip RPC round trip) or activated immediately.
	 * @param InputTag The input tag to look for among the granted ability specs.
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	bool HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const;

	/**
	 * Finds the granted spec an input tag drives, with the matching AbilityInputPressed uses: every quick slot input
	 * (Input.Ability.QuickSlot.*) drives the spec bound to Input.Ability.QuickSlot (GA_QuickSlot), the other inputs
	 * the spec bound to the tag itself. Lets AI tasks press a quick slot input and follow the ability it starts.
	 *
	 * @param InputTag The input tag to resolve.
	 * @return The spec, or null when no granted ability handles the input.
	 */
	const FGameplayAbilitySpec* FindAbilitySpecForInputTag(const FGameplayTag& InputTag) const;

	/**
	 * Finds the granted spec of an equipment ability by its AbilityTag (GrantEquipmentAbility puts the AbilityTag in
	 * the spec's dynamic source tags, next to the input tag).
	 *
	 * @param AbilityTag AbilityTag of the ability (e.g. GameplayAbility.Sword.Basic.Light).
	 * @return The spec, or null when no granted ability carries the tag (e.g. the weapon is not in hand).
	 */
	const FGameplayAbilitySpec* FindAbilitySpecForAbilityTag(const FGameplayTag& AbilityTag) const;

	/**
	 * Server: moves the granted skills of the weapon in hand to the inputs now on its item, in place: the specs stay,
	 * so cooldowns and the ability instances are untouched. See Documentation/InventoryDashboard.md.
	 *
	 * @param Package Abilities of the equipped weapon, with their new SkillInputTag.
	 */
	void RefreshEquippedSkillInputs(const FEquipmentEffectPackage& Package);

	/**
	 * Finds the input that activates a granted equipment ability, so AI tasks can press it like the player's key.
	 *
	 * @param AbilityTag AbilityTag of the ability.
	 * @return The Input.Ability.* tag of its spec, or an empty tag when the ability is not granted.
	 */
	FGameplayTag FindInputTagForAbilityTag(const FGameplayTag& AbilityTag) const;

	/**
	 * Input tag rule of GrantEquipmentAbility: a skill uses the input rolled on its item (SkillInputTag), any other
	 * ability the InputTag of its class. Lets the AI know the input of an ability that is not granted yet (the
	 * sheathed weapon's), which it gets once the weapon is in hand.
	 *
	 * @param AbilityDef  Definition of the ability, from the item's effect package.
	 * @param AbilityCDO  Class default object of AbilityDef.AbilityClass; null when the class is not loaded.
	 * @return The input tag the ability is (or will be) granted with; empty when unknown.
	 */
	static FGameplayTag ResolveEquipmentAbilityInputTag(const FEquipmentAbilityDefinition& AbilityDef, const UMKHGameplayAbility* AbilityCDO);

	/**
	 * Removes a loose ability-priority tag on the authority, on behalf of the owning client.
	 *
	 * The owning client is the authority on the cancellable window (anim-notify driven): the
	 * server's copy of the montage runs one latency behind, so its priority tag clears late.
	 * The client mirrors its local removal through this RPC right before sending any queued
	 * activation request; both travel on the same channel, so the reliable ordering guarantees
	 * the server clears the tag before validating that activation.
	 * Only the Priority1/Priority2 tags are accepted.
	 *
	 * @param PriorityTag The ability priority tag to remove.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RemovePriorityTag(FGameplayTag PriorityTag);


	// =========================================================================================
	// Weapon Swap Synchronization (client-side)
	// =========================================================================================

	/**
	 * Marks a weapon swap as in flight on a non-authoritative client, so queued attacks wait
	 * for the swap result instead of activating the previous weapon's stale specs (see
	 * OnGiveAbility). Arms a safety timeout in case the server rejects the swap. No-op on
	 * the authority, where the swap resolves synchronously.
	 */
	void NotifyWeaponSwapRequested();

	/** Clears the in-flight weapon swap state and cancels the safety timeout. */
	void NotifyWeaponSwapCompleted();

	/** True while a client-initiated weapon swap is still waiting for the server result. */
	UFUNCTION(BlueprintPure, Category = "GAS|Abilities")
	bool IsWeaponSwapInFlight() const { return bWeaponSwapInFlight; }

	
private:
	// =========================================================================================
	// Internal Input Logic
	// =========================================================================================

	/** Array of queued Input Tag to activate abilities after the end of previous ones. */
	TArray<FGameplayTag, TInlineAllocator<2>> QueuedAbilityTags;

	/** True while a client-initiated weapon swap is waiting for the server result to replicate. */
	bool bWeaponSwapInFlight = false;

	/** True while a next-tick flush of the queued input is already scheduled after an ability grant. */
	bool bQueuedFlushScheduled = false;

	/** Safety timeout handle armed when a weapon swap request is sent to the server. */
	FTimerHandle WeaponSwapTimeoutHandle;

	/** Maximum time (seconds) to wait for a weapon swap round trip before discarding stale queued attacks. */
	static constexpr float WeaponSwapTimeoutSeconds = 1.5f;

	/** Returns true when the input tag is an attack that must wait for an in-flight weapon swap. */
	bool IsAttackWaitingForWeaponSwap(const FGameplayTag& InputTag) const;

	/** Clears the in-flight swap state and drops stale queued attack inputs after a timeout. */
	void HandleWeaponSwapTimeout();

	/** Next-tick continuation from OnGiveAbility: clears the swap state and retries the queued input once stale specs are actually gone from the ability list. */
	void FlushQueuedAbilityAfterGrant();

	/** Returns true when the input belongs to the quick-slot input hierarchy. */
	bool IsQuickSlotInput(const FGameplayTag& InputTag) const;

	/** Returns true when an ability spec should react to the provided input tag. */
	bool DoesSpecMatchInput(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag, bool bIsQuickSlotInput) const;

	/** Sends the quick-slot gameplay event to the avatar actor. */
	void SendQuickSlotEvent(const FGameplayTag& InputTag) const;

	/** Activates a spec or forwards replicated pressed input to active instances. */
	void HandleAbilityInputPressedForSpec(const FGameplayAbilitySpec& Spec);

	/** Forwards replicated released input to active ability instances. */
	void HandleAbilityInputReleasedForSpec(const FGameplayAbilitySpec& Spec);

	/** Checks the condition for which the ability should be queued. */
	bool ShouldQueueAbility(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag) const;
	
	/** Tries to Queue the ability. Paying attention to place it in the right position of the stack. */
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	void QueueAbility(const FGameplayTag& InputTag);

	/** Returns true if the input tag it's an attack and it's already queued the ability to swap weapon. */
	bool IsOtherWeaponAtk(const FGameplayTag& InputTag) const;
	
	/** Register to the ActivateQueuedAbility event to handle queued activations. */
	void BindToActivateQueuedAbility();
	
	/** Handles the ActivateQueuedAbility event by activating the ability matching the queued input tag. */
	void OnActivateQueuedAbility(const FGameplayEventData* Payload);
	
	// =========================================================================================
	// Internal Equipment Logic
	// =========================================================================================

	/**
	 * Resolves the owning equipment manager: on the avatar first (the duel bot keeps it on its pawn), then through the
	 * player controller's IEquipmentInterface. Used by the async grants of equipment abilities and stats.
	 */
	TWeakObjectPtr<UEquipmentManagerComponent> GetWeakEquipmentManager() const;

	/** Finds a mutable equipment entry by item id and slot tag in the provided manager. */
	FRPGEquipmentEntry* FindEquipmentEntry(UEquipmentManagerComponent* EquipmentManager, int64 ItemId, const FGameplayTag& SlotTag) const;

	/**
	 * Applies the base Armor of the entry's definition (UMKHEquipmentArmorEffect) and tracks its handle on the entry.
	 * Does nothing for items without base armor.
	 *
	 * @param EquipmentEntry  Entry owning the granted handles.
	 * @param ContextHandle   Effect context shared by the entry's effects.
	 */
	void ApplyAndTrackBaseArmor(FRPGEquipmentEntry& EquipmentEntry, const FGameplayEffectContextHandle& ContextHandle);

	/** Applies one equipment stat effect and tracks its active handle on the entry. Used to simplify logic. */
	void ApplyAndTrackStatEffect(FRPGEquipmentEntry& EquipmentEntry, const FEquipmentStatEffectDefinition& StatEffect, const FGameplayEffectContextHandle& ContextHandle);

	/** Applies one equipment stat effect directly or asynchronously and tracks its active handle on the entry. */
	void GrantEquipmentStatEffect(FRPGEquipmentEntry& EquipmentEntry, const FEquipmentStatEffectDefinition& StatEffect, const FGameplayEffectContextHandle& ContextHandle);
	
	/** Extracted helper to just grant an equipment ability and track it, simplifying async callback. */
	void ApplyAndTrackEquipmentAbility(FRPGEquipmentEntry& EquipmentEntry, const FEquipmentAbilityDefinition& AbilityDef);

	/** Grants one equipment ability and tracks its granted handle on the entry. */
	void GrantEquipmentAbilityDefinition(FRPGEquipmentEntry& EquipmentEntry, const FEquipmentAbilityDefinition& AbilityDef);

	/** Builds and grants a single ability spec from an equipment ability definition. */
	FGameplayAbilitySpecHandle GrantEquipmentAbility(const FEquipmentAbilityDefinition& AbilityDef);
	
};
