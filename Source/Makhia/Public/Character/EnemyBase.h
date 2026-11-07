// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "EnemyBase.generated.h"

/**
 * AI-controlled character: owns its ability system component and attribute set (Minimal replication) instead of
 * reading them from a player state, and initializes them in BeginPlay on every machine. Movement components and the
 * movement state machine come from ACharacterBase, as for the player.
 *
 * Full model: see Documentation/CharacterSystemArchitecture.md (AEnemyBase).
 */
UCLASS()
class MAKHIA_API AEnemyBase : public ACharacterBase
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Creates the replicated ability system component (Minimal mode) and the attribute set. */
	AEnemyBase();

	/** Registers bInitAttributes for replication. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Binds the attribute callbacks, then initializes the ability actor info (after the components' BeginPlay). */
	virtual void BeginPlay() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Initializes the ability system with this actor as owner and avatar, syncs the movement state tag and, on the
	 * server, applies the class defaults and broadcasts the initial values.
	 */
	virtual void InitAbilityActorInfo() override;

	/** Binds the base callbacks and, on the server, flags bInitAttributes once the default attributes are given. */
	virtual void BindCallbacksToDependencies() override;

private:
	/** Replication callback: broadcasts the initial attribute values on clients once the server has set them. */
	UFUNCTION()
	void OnRep_InitAttributes();

	// ============================================================
	// Properties
	// ============================================================

	/** Set on the server when the default attributes are given; replicated so clients broadcast them too. */
	UPROPERTY(ReplicatedUsing = OnRep_InitAttributes)
	bool bInitAttributes = false;
};
