// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MatchOverWidget.generated.h"

class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FMatchOverDelegateSignature);

/**
 * End-of-match panel shown on every peer once the duel is decided.
 *
 * C++ owns the flow (show, actions, rematch ballot feedback, teardown); the widget Blueprint owns
 * the presentation and implements SetMatchResult to fill in the headline, the winner label and the
 * accent colours.
 *
 * Rematch is a ballot: pressing the button casts this player's vote and the match only restarts
 * once every participant has voted, which the game mode arbitrates. Return-to-lobby is unilateral.
 * After a won career level with another level to play, the same button opens that level instead.
 */
UCLASS()
class MAKHIA_API UMatchOverWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

protected:
	/** Drops the game state subscription so the panel cannot be ticked after it leaves the viewport. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Public Interface (Visibility)
	// ============================================================

public:
	/**
	 * Fills the panel with the match outcome, subscribes to the rematch ballot and makes it visible.
	 * Broadcasts FOnShowMatchOverDelegate so the HUD can switch the input focus to UI.
	 *
	 * @param bLocalPlayerWon  True when the owning player is the winner. Ignored on a draw.
	 * @param bIsDraw          True when the match ended without a winner.
	 */
	UFUNCTION(BlueprintCallable, Category = "MatchOver | Visibility")
	void ShowMatchResult(bool bLocalPlayerWon, bool bIsDraw);

	/** Blueprint-side presentation pass: headline text and accent colours. */
	UFUNCTION(BlueprintImplementableEvent, Category = "MatchOver | Visibility")
	void SetMatchResult(bool bLocalPlayerWon, bool bIsDraw);

	/** Broadcasts FOnDestroyMatchOverDelegate and removes the panel from the viewport. */
	void DestroyMatchOverPanel();

	/** Clears every listener bound to this widget's delegates. */
	void UnbindAllEventsFromDelegates();

	// ============================================================
	// Protected / Internal Logic (Button actions)
	// ============================================================

protected:
	/**
	 * Casts this player's rematch vote, or opens the next career level when the button offers it.
	 * The match restarts only once every participant has voted.
	 * Delegated to AMKHPlayerController::Server_RequestRematch because a UFUNCTION(Server) declared
	 * on a UUserWidget never leaves the client (UUserWidget has no RPC callspace resolution).
	 */
	UFUNCTION(BlueprintCallable, Category = "MatchOver | Actions")
	void RequestRematch();

	/**
	 * Leaves the match and travels the owning player back to a fresh lobby (UMKHGameInstance::LeaveToNewLobby):
	 * as host when logged in, offline otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "MatchOver | Actions")
	void ReturnToLobby();

	// ============================================================
	// Protected / Internal Logic (Rematch ballot)
	// ============================================================

protected:
	/**
	 * Updates the Rematch button label as votes come in: "REMATCH" while the ballot is empty,
	 * "REMATCH n/m" once someone has voted, "WAITING n/m" once this player has voted.
	 *
	 * @param CurrentVotes   Participants that have asked for a rematch.
	 * @param RequiredVotes  Votes needed before the match restarts.
	 */
	UFUNCTION()
	void HandleRematchVotesChanged(int32 CurrentVotes, int32 RequiredVotes);

private:
	/** Subscribes HandleRematchVotesChanged to the game state's ballot event. Safe to call twice. */
	void BindRematchBallot();

	/** Unsubscribes from the game state's ballot event. */
	void UnbindRematchBallot();

	// ============================================================
	// Properties
	// ============================================================

public:
	/** Fired when the panel becomes visible. */
	FMatchOverDelegateSignature FOnShowMatchOverDelegate;

	/** Fired right before the panel is removed from the viewport. */
	FMatchOverDelegateSignature FOnDestroyMatchOverDelegate;

protected:
	/**
	 * Rematch button caption, driven by the ballot.
	 * Optional so the panel keeps working if a designer renames or removes the label.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Rematch;

private:
	/** True once this player has voted; keeps the label on "WAITING" and skips redundant RPCs. */
	bool bLocalRematchVoteCast = false;

	/** True when the rematch button opens the next career level (won level, another one to play). */
	bool bOffersNextChallenge = false;
};
