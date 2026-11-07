// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/MovementStateMachine/MovementStateTypes.h"
#include "AbilitySystem/Abilities/MKHGameplayAbility.h"
#include "BlockingSystemComponent.generated.h"

class ACharacterBase;

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class MAKHIA_API UBlockingSystemComponent : public UActorComponent
{
	GENERATED_BODY()

// ============================================================
// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
// ============================================================
public:
	UBlockingSystemComponent();

protected:
	virtual void BeginPlay() override;

// ============================================================
// Public Interface  (BlueprintCallable / externally-facing API)
// ============================================================
public:
	/** Initiates block action */
	UFUNCTION(BlueprintCallable, Category = "Blocking System", meta = (ToolTip = "Initiates block maneuver"))
	bool StartBlocking();

	/** Stops block action */
	UFUNCTION(BlueprintCallable, Category = "Blocking System", meta = (ToolTip = "Stops block maneuver"))
	void StopBlocking();

	/** The block ability that we want to ignore when cancelling all other abilities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking System | Ability")
	TSubclassOf<class UGameplayAbility> BlockAbilityClass;

	/** Returns true if blocking is active or queued */
	UFUNCTION(BlueprintPure, Category = "Blocking System", meta = (ToolTip = "Check if player is blocking or intends to block"))
	bool IsBlocking() const { return bWantsToBlock; }

	/** Applies modifications to movement speed and orientation when blocking */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Blocking System")
	void ApplyBlockingStateModifiers();
	virtual void ApplyBlockingStateModifiers_Implementation();

	/** Removes modifications to movement speed and orientation */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Blocking System")
	void RemoveBlockingStateModifiers();
	virtual void RemoveBlockingStateModifiers_Implementation();

	UFUNCTION(BlueprintPure, Category = "Blocking System")
	float GetBlockingSpeed() const { return BlockingMaxWalkSpeed; }
	
// ============================================================
// Protected / Internal Logic
// ============================================================
protected:

	/** Returns the owning character, re-casting the owner as a fallback. */
	UFUNCTION(BlueprintCallable, Category = "Blocking System", meta = (ToolTip = "Returns the owning character, re-casting the owner as a fallback"))
	ACharacterBase* GetValidCharacter() const;

	bool EnterBlockingState();
	
	UFUNCTION()
	void OnRep_WantsToBlock(bool bOldValue);
	
	bool IsInBlockableState(EMovementStateValue CurrentState) const;
	

// ============================================================
// Properties
// ============================================================
protected:
	/** Speed applied to the character when blocking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocking System")
	float BlockingMaxWalkSpeed = 100.0f;

	/** State indicating the player is currently blocking or queuing block during dodge */
	UPROPERTY(ReplicatedUsing=OnRep_WantsToBlock, BlueprintReadOnly, Category = "Blocking System")
	bool bWantsToBlock = false;

	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
