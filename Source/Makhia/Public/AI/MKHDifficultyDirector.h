// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/MKHDifficultyAdjustment.h"
#include "UObject/Object.h"
#include "MKHDifficultyDirector.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
struct FGameplayEventData;

/**
 * Random Duel, server: measures the damage each side takes in a round and picks the difficulty of the next one
 * (FMKHDifficultyAdjustment). AMKHGameMode owns it and applies the result to the bot. Full model: see
 * Documentation/OfflineModes.md.
 */
UCLASS()
class MAKHIA_API UMKHDifficultyDirector : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Stops listening to the participants. */
	virtual void BeginDestroy() override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Starts measuring a match from the starting difficulty.
	 *
	 * @param Player       The human participant.
	 * @param Bot          The bot participant.
	 * @param InSettings   Thresholds and profile order.
	 */
	void StartTracking(const ACharacterBase* Player, const ACharacterBase* Bot, const FMKHDifficultyAdjustmentSettings& InSettings);

	/** Rematch: back to the starting difficulty, with nothing measured. */
	void ResetMatch();

	/**
	 * Closes the round: updates the difficulty from what was measured and starts measuring the next one.
	 *
	 * @return The profile of the next round, None without a profile order.
	 */
	FName EvaluateRound();

	/** @return The profile of the current round, None without a profile order. */
	FName GetCurrentProfileId() const;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Event.Combat.HitReceived callback: adds the damage of the hit to the total of the side that took it.
	 *
	 * @param Payload  The event; EventMagnitude is the damage after armor.
	 * @param bIsBot   True when the bot took the hit.
	 */
	void HandleHitReceived(const FGameplayEventData* Payload, bool bIsBot);

	/**
	 * Listens to the hits one side takes.
	 *
	 * @param ASC     Ability system of the side.
	 * @param bIsBot  True for the bot.
	 */
	void BindHits(UAbilitySystemComponent* ASC, bool bIsBot);

	/** Stops listening to both sides. */
	void UnbindHits();

	// ============================================================
	// Properties
	// ============================================================

	/** Thresholds and profile order. */
	FMKHDifficultyAdjustmentSettings Settings;

	/** Ability system of the player. */
	TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;

	/** Ability system of the bot. */
	TWeakObjectPtr<UAbilitySystemComponent> BotASC;

	/** Hit callback on PlayerASC. */
	FDelegateHandle PlayerHitHandle;

	/** Hit callback on BotASC. */
	FDelegateHandle BotHitHandle;

	/** Index in Settings.ProfileOrder of the current round. */
	int32 CurrentLevel = 0;

	/** Damage the player took this round. */
	float DamageTaken = 0.f;

	/** Damage the bot took this round, that is the damage the player dealt. */
	float DamageDealt = 0.f;
};
