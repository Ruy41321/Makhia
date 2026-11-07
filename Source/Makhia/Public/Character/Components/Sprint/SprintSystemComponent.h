// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SprintSystemComponent.generated.h"

class ACharacterBase;
class UInputAction;
class UEnhancedInputComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MAKHIA_API USprintSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USprintSystemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Sets up input bindings for this component */
	UFUNCTION(BlueprintCallable, Category = "Sprint System", meta = (ToolTip = "Sets up sprint input bindings"))
	void SetupInput(UEnhancedInputComponent* EnhancedInputComponent);

	/** Handles sprint input - starts/stops sprinting */
	UFUNCTION(BlueprintCallable, Category = "Sprint System", meta = (ToolTip = "Handles sprint input and conditions"))
	void SprintPressed(const FInputActionValue& Value, const bool bOverrideSprint = false, const bool bValueToOverride = false);

	UFUNCTION(Server, Reliable)
	void ServerSprintPressed(bool bSprintValue);

	/** Returns true if character is currently sprinting */
	UFUNCTION(BlueprintPure, Category = "Sprint System", meta = (ToolTip = "Check if character is sprinting"))
	bool IsSprinting() const { return bIsSprinting; }

	/** Returns true if sprint is currently interrupted */
	UFUNCTION(BlueprintPure, Category = "Sprint System", meta = (ToolTip = "Check if sprint is interrupted"))
	bool IsSprintInterrupted() const { return bSprintInterrupted; }

	/** Gets run/sprint speed */
	UFUNCTION(BlueprintPure, Category = "Sprint System", meta = (ToolTip = "Returns run speed in cm/s"))
	float GetRunSpeed() const { return RunSpeed; }

	/** Returns the character owning this component. */
	UFUNCTION(BlueprintCallable, Category = "Sprint System", meta = (ToolTip = "Returns the character owning this component"))
	ACharacterBase* GetOwnerCharacter() const { return GetValidCharacter(); }

	// Setters for external control
	void SetIsSprinting(bool NewIsSprinting) { bIsSprinting = NewIsSprinting; }
	void SetSprintInterrupted(bool NewSprintInterrupted) { bSprintInterrupted = NewSprintInterrupted; }
	void SetRunSpeed(float NewRunSpeed) { RunSpeed = NewRunSpeed; }

	/** Returns the owning character, re-casting the owner as a fallback. */
	ACharacterBase* GetValidCharacter() const;

protected:
	/** Run/Sprint speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint System", 
		meta = (ToolTip = "Run speed in cm/s", ClampMin = "100.0", ClampMax = "1500.0"))
	float RunSpeed = 600.0f;

	/** Whether character is currently sprinting */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sprint System", 
		meta = (ToolTip = "True if character is sprinting"))
	bool bIsSprinting = false;

	/** Whether sprint is currently interrupted */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint System", 
		meta = (ToolTip = "True if sprint is interrupted"))
	bool bSprintInterrupted = true;
	
	/** Indicates if the sprint should be activated by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint System",
		meta = (ToolTip = "True if character should sprint as default"))
	bool bSprintAsDefault = false;

private:
	/** Cached reference to the character owning this component. */
	UPROPERTY()
	TObjectPtr<ACharacterBase> OwnerCharacter;

public:
	/** Input action for sprint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;
};
