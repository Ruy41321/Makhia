// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "MKHGameplayAbility.generated.h"

/**
 * Base class for every gameplay ability in Makhia.
 *
 * Adds on top of UGameplayAbility:
 * - Tag-based input binding (InputTag, matched via the spec's dynamic source tags).
 * - Automatic montage orchestration (MontageToPlay + OnMontageStarted/Finished hooks).
 * - Priority tags (Priority1/Priority2) governing which abilities may interrupt others.
 * - Integration with the input queue: EndAbility and the MakeAbilityCancellable event
 *   fire Event::ActivateQueuedAbility so buffered inputs resolve at the earliest frame
 *   (see Documentation/QueueAbilitySystem.md).
 */
UCLASS()
class MAKHIA_API UMKHGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	
	UMKHGameplayAbility();
	
	// ==========================================
	// Overrides
	// ==========================================

	/** 
	 * Activates the ability, binding input events and commuting the ability.
	 * @param Handle The ability spec handle.
	 * @param ActorInfo The actor info activating the ability.
	 * @param ActivationInfo The activation info.
	 * @param TriggerEventData Payload data from the event that triggered this ability.
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * Terminates the ability, unbinding input events and cleaning up any active effects.
	 * 
	 * @param Handle The ability spec handle.
	 * @param ActorInfo The actor info that owns the ability.
	 * @param ActivationInfo The activation info for the ability.
	 * @param bReplicateEndAbility If true, the ability end will be replicated to clients.
	 * @param bWasCancelled If true, the ability was cancelled rather than completed normally.
	 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	/**
	 * Called when the ability is being removed from the Ability System Component.
	 * @param ActorInfo The actor info removing the ability.
	 * @param Spec The ability spec being removed.
	 */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	/**
	 * Checks if the ability satisfies tag requirements.
	 * Extended to support custom priority logic.
	 */
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// ==========================================
	// Input & Events
	// ==========================================

	/**
	 * Design-time input tag binding this ability to an input action; read from the CDO at
	 * grant time and added to the spec's dynamic source tags for input matching. Skills
	 * override it dynamically via the grant payload instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MKH Gameplay Ability | Input")
	FGameplayTag InputTag;

	/**
	 * Called repeatedly when the input is pressed again while the ability is active.
	 * @param TimeWaited The time waited before the button was pressed again.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MKH Gameplay Ability | Input")
	void OnAbilityActivatedAgain(float TimeWaited);
	virtual void OnAbilityActivatedAgain_Implementation(float TimeWaited);
	
	/**
	 * Called when the input bound to this ability is released.
	 * @param TimeWaited The time the button was held before releasing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "MKH Gameplay Ability | Input")
	void OnAbilityReleased(float TimeWaited);
	virtual void OnAbilityReleased_Implementation(float TimeWaited);
	
	/**
	 * Blueprint event triggered when the ability is removed.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MKH Gameplay Ability | Events")
	void OnAbilityRemoved();

	// ==========================================
	// Animation Events
	// ==========================================

	/**
	 * Triggered when the ability animation montage begins playing.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MKH Gameplay Ability | Animation")
	void OnMontageStarted();
	virtual void OnMontageStarted_Implementation();
	
	/**
	 * Event fired natively when the ability's montage finishes playing.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MKH Gameplay Ability | Animation")
	void OnMontageFinished();
	virtual void OnMontageFinished_Implementation();

	/**
	 * Montage this ability plays on activation; read on the class default object to time its strikes without
	 * playing it (FMKHAbilityTiming).
	 *
	 * @return The montage, or null when the ability plays none.
	 */
	UAnimMontage* GetMontageToPlay() const { return MontageToPlay; }

protected:
	
	// ==========================================
	// Helpers
	// ==========================================

	/**
	 * Adjusts the avatar's orientation behavior towards the camera.
	 * @param bFollowCamera If true, the character will use the controller rotation yaw.
	 */
	UFUNCTION(BlueprintCallable, Category = "MKH Gameplay Ability | Movement")
	void SetCharacterOrientation(bool bFollowCamera);
	
private:
	
	// ==========================================
	// Internal Logic
	// ==========================================

	/** Binds the ability to listen for subsequent input press events. */
	void BindInputPressEvent();
	
	/** Binds the ability to listen for input release events. */
	void BindInputReleaseEvent();
	
	/** Binds the ability to listen for MakeAbilityCancellable events to clear its priority tag. */
	void BindMakeCancellable();

	/** Called when the MakeAbilityCancellable event is received.
	 *
	 * Removes the priority tag on local and server and tries to activate locally the queued ability.
	 */
	UFUNCTION()
	void OnMakeAbilityCancellable(FGameplayEventData Payload);

	/** Removes the priority tag (if any) from the owning ability system component. */
	void RemovePriorityTag();

	/** Only once per activation fires the event to notify the ASC to activate the queued ability. */
	void TryActivateQueuedAbility();
	
	/** Property to indicate if the method TryActivateQueuedAbility has already been called. */
	bool bTryActivateQueuedAbilityCalled = false;
	
	/** Priority tag used for ability priority logic and interruption handling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags", meta = (AllowPrivateAccess = "true", Categories = "GameplayAbility.Active.Priority",
		Tooltip = "Priority tag used for ability priority logic and interruption handling.\n   First cannot be interrupted by any.\n   Second only by First"))
	FGameplayTag PriorityTag;
	
	/** The montage to orchestrate via this ability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "MKH Gameplay Ability | Animation")
	TObjectPtr<UAnimMontage> MontageToPlay;
	
	/**
	 * Executes the internal logic to play the specified MontageToPlay.
	 * @return True if successful.
	 */
	bool PlayAnimation();
	
};
