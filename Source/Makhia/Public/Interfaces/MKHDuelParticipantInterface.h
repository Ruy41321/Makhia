// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "MKHDuelParticipantInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMKHDuelParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 * A character that can take a slot of the arena duel: the human player (AMKHPlayerCharacter) or the offline
 * opponent (AMKHDuelBot). AMKHGameMode tracks participants as ACharacterBase and goes through this interface for
 * everything that differs between the two (round reset, displayed name, vote counting).
 *
 * Native only: Blueprints read it through UMKHDuelParticipantLibrary.
 */
class MAKHIA_API IMKHDuelParticipant
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Server: brings the participant back for a new round (between rounds and on a rematch): snaps it back to its
	 * spawn and revives it if it died. Attributes are restored separately by the game mode (RoundResetEffect).
	 *
	 * @param SpawnTransform  Transform captured when the participant was registered, at its first spawn.
	 */
	virtual void ResetForNewRound(const FTransform& SpawnTransform) = 0;

	/** Name shown for this participant in the match UI (scoreboard, kill feed). */
	virtual FText GetParticipantDisplayName() const = 0;

	/** True for a participant driven by a player: only humans vote in the preparation-skip ballot. */
	virtual bool IsHumanParticipant() const = 0;

	/**
	 * Career levels that give this participant its title (UMKHCareerSubsystem::GetTitleForCompletedLevels).
	 *
	 * @return The levels, INDEX_NONE for a participant without a title (the training dummy, a player not reported yet).
	 */
	virtual int32 GetParticipantCareerLevels() const = 0;
};

/**
 * Null-safe accessors to IMKHDuelParticipant, usable from Blueprints (the interface itself is native only) and from
 * C++ code holding a plain actor pointer.
 */
UCLASS()
class MAKHIA_API UMKHDuelParticipantLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Gets the name shown for a duel participant.
	 *
	 * @param Participant  A duel participant, usually from AMKHGameState's OnParticipantRegistered.
	 * @return The participant's display name, or empty text when the actor is not a duel participant.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Participant")
	static FText GetParticipantDisplayName(const AActor* Participant);

	/**
	 * Tells whether a duel participant is driven by a player.
	 *
	 * @param Participant  A duel participant.
	 * @return True for a human participant; false for a bot or an actor that is not a duel participant.
	 */
	UFUNCTION(BlueprintPure, Category = "Duel|Participant")
	static bool IsHumanParticipant(const AActor* Participant);
};
