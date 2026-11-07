// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrouchSystemComponent.generated.h"

class ACharacterBase;
class UInputAction;
class UEnhancedInputComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API UCrouchSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCrouchSystemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sets up input bindings for this component */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Sets up crouch input bindings"))
	void SetupInput(UEnhancedInputComponent* EnhancedInputComponent);

	/** Handles crouch input - toggles between crouch and uncrouch */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Toggles crouch state"))
	void CrouchPressed(const FInputActionValue& Value);

	/** Start crouching */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Initiates crouch"))
	void Crouch();

	/** Server RPC for crouch */
	UFUNCTION(Server, Reliable)
	void ServerCrouch();

	/** Stop crouching */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Stops crouch"))
	void UnCrouch();

	/**
	 * Forces the character out of crouch regardless of input, used when an external
	 * system (e.g. a Stun/Stagger crowd-control effect) must cancel the crouch state.
	 * The uncrouch is still gated by CanUncrouchSafely() to avoid clipping into geometry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Forces uncrouch, bypassing input toggle"))
	void ForceUncrouch();

	/** Server RPC for uncrouch */
	UFUNCTION(Server, Reliable)
	void ServerUnCrouch();

	/** Checks if it's safe to uncrouch (no obstacles above) */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Check if uncrouch is safe"))
	bool CanUncrouchSafely() const;

	/**
	 * Stands up a server-driven character (AI) that has no headroom where it is, stepping it to the nearest free spot
	 * within a short distance: an AI has no player to walk it away from the wall. Same gates as the crouch key.
	 * See Documentation/CharacterSystemArchitecture.md, Dodge <-> Crouch.
	 *
	 * @return True if the character is standing when the call returns.
	 */
	bool TryStandUpWithNudge();

	/** Adjusts capsule height and mesh position during crouch transition */
	//void AdjustCapsuleHeight(float DeltaTime, float TargetCapsuleHeight, float TargetMeshHeight);

	/** Returns true if currently crouched */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Check if player is crouched"))
	bool IsCrouched() const { return bIsCrouched; }

	/** Returns true if crouch transition is in progress */
	//UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Check if crouch transition is active"))
	//bool IsCrouchingInProgress() const { return bIsCrouchingInProgress; }

	/** Gets crouch movement speed */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Returns crouch speed in cm/s"))
	float GetCrouchSpeed() const { return CrouchSpeed; }

	/** Gets crouch target height */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Returns crouch target height"))
	float GetCrouchTargetHeight() const { return CrouchTargetHeight; }

	/** Gets stand target height */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Returns stand target height"))
	float GetStandTargetHeight() const { return StandTargetHeight; }

	/** Gets crouch mesh height offset */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Returns crouch mesh height offset"))
	float GetCrouchMeshHeightOffset() const { return CrouchMeshHeightOffset; }

	/** Gets stand mesh height offset */
	UFUNCTION(BlueprintPure, Category = "Crouch System", meta = (ToolTip = "Returns stand mesh height offset"))
	float GetStandMeshHeightOffset() const { return StandMeshHeightOffset; }

	// Setters for external access
	void SetIsCrouched(bool NewIsCrouched) { bIsCrouched = NewIsCrouched; }
	//void SetIsCrouchingInProgress(bool NewIsCrouchingInProgress) { bIsCrouchingInProgress = NewIsCrouchingInProgress; }
	void SetCrouchSpeed(float NewCrouchSpeed) { CrouchSpeed = NewCrouchSpeed; }

	/** Returns the character owning this component. */
	UFUNCTION(BlueprintCallable, Category = "Crouch System", meta = (ToolTip = "Returns the character owning this component"))
	ACharacterBase* GetOwnerCharacter() const { return GetValidCharacter(); }

	/** Returns the owning character, re-casting the owner as a fallback. */
	ACharacterBase* GetValidCharacter() const;

	/**
	 * Checks whether the character is currently under a crowd-control effect
	 * (Stunned or Staggered) that must suppress movement actions such as crouching.
	 *
	 * @param Character  The owning character to query.
	 * @return True if a crowd-control tag is active on the character's ability system.
	 */
	bool IsCrowdControlled(const ACharacterBase* Character) const;

private:
	/**
	 * Tests whether the standing capsule fits at a location. Other characters never take the space.
	 *
	 * @param Character         The owning character.
	 * @param CrouchedLocation  Actor location of the crouched capsule to test from.
	 * @param OutBlockerName    Optional, receives the name of the blocking component.
	 * @return True if nothing blocks the standing capsule.
	 */
	bool IsStandingSpaceFree(const ACharacterBase& Character, const FVector& CrouchedLocation, FString* OutBlockerName = nullptr) const;

	/**
	 * Moves the character to the nearest spot where the standing capsule fits, sweeping so it never crosses geometry.
	 *
	 * @param Character  The owning character.
	 * @return True if the character was moved to a free spot.
	 */
	bool MoveToNearestStandingSpace(ACharacterBase& Character) const;

protected:
	/** Whether currently crouched */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crouch System",
		meta = (ToolTip = "True if player is crouched"))
	bool bIsCrouched = false;

	/** Whether crouch transition is in progress */
	//UPROPERTY(BlueprintReadOnly, Category = "Crouch System", 
	//	meta = (ToolTip = "True while crouch transition is active"))
	//bool bIsCrouchingInProgress = false;

	/** Movement speed during crouch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Crouch speed in cm/s", ClampMin = "50.0", ClampMax = "500.0"))
	float CrouchSpeed = 100.0f;

	/** Target capsule height when crouching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Target capsule height when crouched", ClampMin = "30.0", ClampMax = "120.0"))
	float CrouchTargetHeight = 65.0f;

	/** Target capsule height when standing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Target capsule height when standing", ClampMin = "60.0", ClampMax = "150.0"))
	float StandTargetHeight = 90.0f;

	/** Speed adjustment factor for capsule height changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Rate of height adjustment", ClampMin = "100.0", ClampMax = "1000.0"))
	float HeightAdjustmentRate = 300.0f;

	/** Mesh Z offset when crouching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Mesh vertical offset when crouched"))
	float CrouchMeshHeightOffset = -60.0f;

	/** Mesh Z offset when standing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Mesh vertical offset when standing"))
	float StandMeshHeightOffset = -90.0f;

	/** Speed adjustment factor for mesh position changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Rate of mesh position adjustment", ClampMin = "100.0", ClampMax = "1000.0"))
	float MeshOffsetAdjustmentRate = 300.0f;

	/** Extra safety margin when checking for obstacles above */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System", 
		meta = (ToolTip = "Extra safety margin (in cm) when checking for obstacles above", ClampMin = "0.0", ClampMax = "20.0"))
	float UncrouchSafetyMargin = 5.0f;

public:
	/** Input action for crouch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> CrouchPressedAction;

private:
	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;
};