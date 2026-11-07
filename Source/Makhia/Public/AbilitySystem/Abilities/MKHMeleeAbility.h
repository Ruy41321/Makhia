// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/MKHDamageAbility.h"
#include "UObject/ObjectKey.h"
#include "MKHMeleeAbility.generated.h"

/**
 * Damage ability specialized for melee combat.
 *
 * Hit detection follows the client-predicted / server-validated pattern:
 * - The locally-controlled machine (owning client for players, server for AI) listens for the
 *   HitScanStart/End animation-notify events — the only machine where those notifies are
 *   reliable — and drives the weapon's native melee scan.
 * - Every detected hit is shipped to the server as replicated target data
 *   (FMKHGameplayAbilityTargetData_MeleeHit) inside a scoped prediction window.
 * - The server validates each hit for plausibility (range, cone, re-hit throttling, per-swing
 *   de-duplication) before applying damage, so authority is preserved without depending on
 *   the server's lag-shifted animation timeline.
 *
 * The server proxy of a remote player never self-terminates from its own montage/combo
 * timeline (which lags the client's); it ends through the client's replicated EndAbility,
 * bounded by a lifetime failsafe.
 *
 * Also drives the Directional Movement Technique (DMT) that softly homes the character
 * towards the best target while attacking, re-triggered on every combo strike.
 */
UCLASS()
class MAKHIA_API UMKHMeleeAbility : public UMKHDamageAbility
{
	GENERATED_BODY()

public:
	// ==========================================
	// Overrides
	// ==========================================

	/**
	 * Activates the melee ability. Binds the hit scan notify events on the locally-controlled
	 * machine and the replicated target data pipeline on the server proxy of remote players.
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * Ends the ability. Resolves any lingering hit scan window and unbinds the server target
	 * data delegate exactly once, guarded against the re-entrant EndAbility calls produced by
	 * montage interruption callbacks and replicated ends.
	 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * Natively triggers logic when the melee attack montage begins.
	 */
	virtual void OnMontageStarted_Implementation() override;

	/**
	 * Ends the ability when the montage finishes, except on the server proxy of a remote
	 * player: that timeline lags the owning client's, so the instance waits for the client's
	 * replicated EndAbility instead (bounded by the lifetime failsafe).
	 */
	virtual void OnMontageFinished_Implementation() override;

	// ==========================================
	// Combo Mechanics
	// ==========================================

	/**
	 * Hook exposed to Blueprint for adding specific logic during a combo attack.
	 * Allows implementation of dynamic damage multipliers.
	 * The base c++ implementation face the char where the player is aiming and tries to
	 * activate the DMT task if enabled.
	 *
	 * @param HitCounter The sequential index of the upcoming combo strike.
	 */
	virtual void OnComboTriggered_Implementation(int32 HitCounter) override;

protected:

	/**
	 * Opens the melee hit scan window: resolves the owning weapon and starts its native scan,
	 * routing detected hits into the client-predicted / server-validated pipeline.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee Ability | Hit Scan")
	void HitScanStart();
	virtual void HitScanStart_Implementation();

	/**
	 * Closes the melee hit scan window and stops the weapon's native scan.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee Ability | Hit Scan")
	void HitScanEnd();
	virtual void HitScanEnd_Implementation();

	/**
	 * Receives every unique actor detected by the hit scan. Applies damage directly when this
	 * machine is the authority (listen host, AI); otherwise ships the hit to the server as
	 * replicated target data. Exposed to subclasses so alternative scan shapes (e.g. AOE
	 * sphere traces) can route their hits through the same pipeline.
	 *
	 * @param HitResult  The detected hit.
	 */
	UFUNCTION()
	void HandleMeleeHitDetected(const FHitResult& HitResult);

	/**
	 * Maximum avatar-to-target distance accepted by server hit validation, in Unreal units.
	 * Subclasses with different scan shapes override this to keep validation coherent with
	 * their actual reach.
	 *
	 * @return The maximum accepted hit distance.
	 */
	virtual float GetHitValidationMaxRange() const { return MaxMeleeHitRange; }

	/**
	 * Maximum yaw angle (degrees) between the avatar's forward vector and the target accepted
	 * by server hit validation. Returning 180 disables the cone check entirely (360° scans).
	 *
	 * @return The maximum accepted hit cone angle, in degrees.
	 */
	virtual float GetHitValidationMaxConeAngleDegrees() const { return MaxMeleeHitConeAngleDegrees; }

private:
	// ==========================================
	// Component Configurations
	// ==========================================

	/** Seconds between two steps of the weapon's melee scan while the window is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "Melee Ability | Core Settings")
	float HitScanFrequency = 0.03f;

	// ==========================================
	// Server-Side Hit Validation
	// ==========================================

	/** Maximum avatar-to-target distance accepted by server hit validation, in Unreal units. Generous enough to absorb client/server position divergence under latency. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "0"), Category = "Melee Ability | Hit Validation")
	float MaxMeleeHitRange = 500.f;

	/** Maximum yaw angle (degrees) between the avatar's forward vector and the target accepted by server hit validation. Generous by design: wide swing arcs and rotation divergence under latency must not reject honest hits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "180"), Category = "Melee Ability | Hit Validation")
	float MaxMeleeHitConeAngleDegrees = 120.f;

	/** Minimum seconds between two accepted hits on the same target within one activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "0"), Category = "Melee Ability | Hit Validation")
	float MeleeRehitCooldown = 0.2f;

	/** Maximum lifetime of the server proxy instance for a remote player before it force-ends. Failsafe for a client that never replicates its EndAbility (disconnect, tampering). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "1"), Category = "Melee Ability | Hit Validation")
	float ServerLifetimeFailsafeDuration = 10.f;

	// ==========================================
	// Hit Scan Window (locally-controlled machine)
	// ==========================================

	/** Binds tasks waiting for start and end tags relative to weapon hit scanning. */
	void BindHitScanEvents();

	/** Caught when the HitScanStart gameplay event is received; opens the scan window. */
	UFUNCTION()
	virtual void OnHitScanStartReceived(FGameplayEventData Payload);

	/** Caught when the HitScanEnd gameplay event is received; closes the scan window. */
	UFUNCTION()
	virtual void OnHitScanEndReceived(FGameplayEventData Payload);

	/**
	 * Resolves the owning weapon, lazily initializing it on machines where activation did not
	 * (the owning client resolves it on first use).
	 *
	 * @return The owning weapon, or nullptr when unavailable.
	 */
	AMKHWeaponBase* ResolveOwningWeapon();

	/** True while the melee hit scan window is open on this machine. */
	bool bHitScanActive = false;

	// ==========================================
	// Replicated Target Data (client -> server)
	// ==========================================

	/**
	 * Packages a detected hit with the current combo index and ships it to the server through
	 * the GAS replicated target data pipeline inside a scoped prediction window.
	 *
	 * @param HitResult  The hit detected by the weapon scan on the owning client.
	 */
	void SendMeleeHitToServer(const FHitResult& HitResult);

	/** Subscribes the server proxy to replicated target data sent by the owning client. */
	void BindServerTargetDataDelegate();

	/** Unsubscribes the server proxy from replicated target data and consumes leftovers. */
	void RemoveServerTargetDataDelegate();

	/**
	 * Server-side entry point for replicated melee target data. Consumes the payload and
	 * routes every melee hit into authoritative processing.
	 *
	 * @param DataHandle      The replicated target data batch.
	 * @param ApplicationTag  Optional application tag forwarded by the sender (unused).
	 */
	void OnServerMeleeTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag);

	// ==========================================
	// Authoritative Hit Processing
	// ==========================================

	/**
	 * Validates and applies a melee hit on the authority: plausibility checks, combo counter
	 * synchronization, damage capture and gameplay effect application.
	 *
	 * @param HitResult   The reported hit.
	 * @param ComboIndex  Combo strike index active on the machine that detected the hit.
	 */
	void ProcessAuthoritativeMeleeHit(const FHitResult& HitResult, int32 ComboIndex);

	/**
	 * Checks whether a reported hit is plausible for this activation: valid ASC-bearing
	 * target within range and swing cone, not already hit this combo strike nor inside the
	 * re-hit cooldown.
	 *
	 * @param TargetActor  The actor reported as hit.
	 * @param ComboIndex   Combo strike index the hit belongs to.
	 * @return True when the hit passes every validation rule.
	 */
	bool IsMeleeHitPlausible(const AActor* TargetActor, int32 ComboIndex) const;

	/** Bookkeeping for an accepted hit, used by re-hit validation rules. */
	struct FAcceptedMeleeHit
	{
		/** Combo strike index the hit was accepted for. */
		int32 ComboIndex = INDEX_NONE;

		/** World time (seconds) the hit was accepted at. */
		double TimeSeconds = 0.0;
	};

	/** Accepted hits for the current activation, keyed by target actor. */
	TMap<FObjectKey, FAcceptedMeleeHit> AcceptedMeleeHits;

	// ==========================================
	// Server Lifetime Failsafe
	// ==========================================

	/** Schedules the failsafe that bounds the server proxy lifetime for remote players. */
	void ScheduleServerLifetimeFailsafe();

	/** Fired when the failsafe elapses with the ability still active; force-ends it. */
	UFUNCTION()
	void OnServerLifetimeFailsafeElapsed();

	// ==========================================
	// Directional Magnetic Targeting (DMT) Logic
	// ==========================================

	/** Attempts to activate the Directional Magnetic Targeting task based on current settings. */
	void TryActivateDMT();

	/** Helper bound to execute when the DMT task completes or expires. */
	UFUNCTION()
	virtual void OnDMTFinished();

	/** Binds tasks waiting for the ApplyDMT gameplay event to trigger DMT application logic. */
	void BindApplyDMTEvent();

	/** Caught when the ApplyDMT gameplay event is received. */
	UFUNCTION()
	void OnApplyDMTReceived(FGameplayEventData Payload);
};
