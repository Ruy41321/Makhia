// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "UI/MKHSystemWidget.h"
#include "MKHPreparationSkipWidget.generated.h"

class AGameStateBase;
class AMKHGameState;
class UEnhancedInputUserSettings;
class UMKHActiveInputDeviceSubsystem;
class UInputAction;
class UTextBlock;

/**
 * Ready-up prompt of the initial preparation: tells the player which key skips the rest of the preparation
 * and how many participants are ready ("1/2 READY").
 *
 * C++ owns the flow: it follows the game state's preparation-skip ballot and is only shown while the ballot
 * is open, so it disappears on its own when the final countdown starts (on schedule or because both players
 * are ready). The widget Blueprint owns the look and can react to every refresh through BP_OnSkipVotesUpdated.
 * Full flow: see Documentation/MatchFlowArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHPreparationSkipWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

protected:
	/** Starts collapsed, subscribes to the ballot (now or once the game state replicates) and to key rebinds. */
	virtual void NativeConstruct() override;

	/** Drops every subscription. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Ballot callback: refreshes the texts, then shows the widget while the ballot is open and collapses it otherwise.
	 *
	 * @param CurrentVotes   Participants currently ready.
	 * @param RequiredVotes  Ready participants needed to skip; 0 once the ballot is closed.
	 */
	UFUNCTION()
	void HandleSkipVotesChanged(int32 CurrentVotes, int32 RequiredVotes);

	/** Key rebind callback: shows the newly bound key in the prompt. */
	UFUNCTION()
	void HandleInputSettingsApplied();

	/**
	 * Device switch callback: shows the key of the device now in use.
	 *
	 * @param NewDevice  Device family the player just switched to.
	 */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

	/**
	 * Blueprint presentation hook, called after every refresh while the ballot is open (e.g. to animate a vote).
	 *
	 * @param CurrentVotes       Participants currently ready.
	 * @param RequiredVotes      Ready participants needed to skip.
	 * @param bLocalPlayerReady  True when the owning player is ready.
	 * @param bOpponentReady     True when another participant is ready.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "PreparationSkip")
	void BP_OnSkipVotesUpdated(int32 CurrentVotes, int32 RequiredVotes, bool bLocalPlayerReady, bool bOpponentReady);

private:
	/**
	 * Subscribes to the ballot of the given game state and shows its current tally. Safe to call twice.
	 * Also the UWorld::GameStateSetEvent handler on clients.
	 *
	 * @param GameState  The world's game state; ignored when it is not an AMKHGameState.
	 */
	void BindGameState(AGameStateBase* GameState);

	/** Unsubscribes from the bound game state and from the world's game state event. */
	void UnbindGameState();

	/** Subscribes to the owning player's input settings, so a rebind updates the prompt. */
	void BindInputSettings();

	/** Unsubscribes from the input settings. */
	void UnbindInputSettings();

	/** Re-renders the widget from the bound game state's current ballot. */
	void RefreshFromGameState();

	/** True when the owning player is in the bound game state's ready list. */
	bool IsLocalPlayerReady() const;

	/**
	 * Builds the prompt for the current situation, with the skip key filled in.
	 *
	 * @param bLocalPlayerReady  True when the owning player is ready.
	 * @param bOpponentReady     True when another participant is ready.
	 */
	FText BuildPromptText(bool bLocalPlayerReady, bool bOpponentReady) const;

	// ============================================================
	// Properties
	// ============================================================

protected:
	/** Action whose key is shown in the prompt (IA_SkipInit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreparationSkip")
	TObjectPtr<UInputAction> SkipPreparationAction;

	/** Prompt while the owning player is not ready and nobody else is. {Key} is the skip key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreparationSkip|Text")
	FText IdlePromptFormat = NSLOCTEXT("MKHPreparationSkip", "IdlePrompt", "PRESS [{Key}] WHEN YOU'RE READY TO FIGHT");

	/** Prompt while the owning player is ready and waits for the opponent. {Key} is the skip key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreparationSkip|Text")
	FText LocalReadyPromptFormat = NSLOCTEXT("MKHPreparationSkip", "LocalReadyPrompt", "READY! WAITING FOR YOUR OPPONENT...   [{Key}] TO CANCEL");

	/** Prompt while only the opponent is ready. {Key} is the skip key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreparationSkip|Text")
	FText OpponentReadyPromptFormat = NSLOCTEXT("MKHPreparationSkip", "OpponentReadyPrompt", "YOUR OPPONENT IS READY!  PRESS [{Key}] TO FIGHT NOW");

	/** Ready counter. {Current} and {Required} are the ballot's numbers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PreparationSkip|Text")
	FText ReadyCountFormat = NSLOCTEXT("MKHPreparationSkip", "ReadyCount", "{Current}/{Required} READY");

	/** Prompt line. Optional so the widget keeps working if a designer renames or removes it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SkipPrompt;

	/** Ready counter line. Optional so the widget keeps working if a designer renames or removes it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ReadyCount;

private:
	/** Game state whose ballot this widget follows. */
	TWeakObjectPtr<AMKHGameState> BoundGameState;

	/** Input settings whose rebinds refresh the prompt. */
	TWeakObjectPtr<UEnhancedInputUserSettings> BoundInputSettings;

	/** Active device tracker whose switches refresh the prompt. */
	TWeakObjectPtr<UMKHActiveInputDeviceSubsystem> BoundDeviceSubsystem;

	/** Subscription to UWorld::GameStateSetEvent while waiting for the game state to replicate. */
	FDelegateHandle GameStateSetHandle;
};
