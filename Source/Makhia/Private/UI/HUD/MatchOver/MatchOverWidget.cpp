// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/MatchOver/MatchOverWidget.h"

#include "Components/TextBlock.h"
#include "GameInstance/MKHGameInstance.h"
#include "GameMode/MKHGameState.h"
#include "MKHLogChannels.h"
#include "Player/PlayerController/MKHPlayerController.h"

#define LOCTEXT_NAMESPACE "MatchOverWidget"

// ============================================================
// Lifecycle
// ============================================================

void UMatchOverWidget::NativeDestruct()
{
	UnbindRematchBallot();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface (Visibility)
// ============================================================

void UMatchOverWidget::ShowMatchResult(bool bLocalPlayerWon, bool bIsDraw)
{
	SetMatchResult(bLocalPlayerWon, bIsDraw);

	const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	bOffersNextChallenge = bLocalPlayerWon && !bIsDraw && IsValid(GameInstance) && GameInstance->CanStartNextCareerLevel();

	// The ballot opens with the match-over phase, so subscribing here is early enough to catch
	// every vote, including one cast by the opponent before this panel was built.
	BindRematchBallot();

	SetVisibility(ESlateVisibility::Visible);
	FOnShowMatchOverDelegate.Broadcast();
}

void UMatchOverWidget::DestroyMatchOverPanel()
{
	UnbindRematchBallot();

	FOnDestroyMatchOverDelegate.Broadcast();
	RemoveFromParent();
}

void UMatchOverWidget::UnbindAllEventsFromDelegates()
{
	FOnShowMatchOverDelegate.Clear();
	FOnDestroyMatchOverDelegate.Clear();
}

// ============================================================
// Protected / Internal Logic (Button actions)
// ============================================================

void UMatchOverWidget::RequestRematch()
{
	if (bOffersNextChallenge)
	{
		if (UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this))
		{
			GameInstance->StartNextCareerLevel();
		}
		return;
	}

	// The game mode already ignores duplicate votes; skipping here just saves the round trip.
	if (bLocalRematchVoteCast)
	{
		return;
	}

	AMKHPlayerController* PC = Cast<AMKHPlayerController>(GetOwningPlayer());
	if (!IsValid(PC))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UMatchOverWidget::RequestRematch: no owning MKH player controller."));
		return;
	}

	bLocalRematchVoteCast = true;
	PC->Server_RequestRematch();
}

void UMatchOverWidget::ReturnToLobby()
{
	const APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UMatchOverWidget::ReturnToLobby: no owning player controller."));
		return;
	}

	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(PC);
	if (!IsValid(GameInstance))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UMatchOverWidget::ReturnToLobby: no MKH game instance."));
		return;
	}

	// Hosts the new lobby only when logged in: after an offline match the lobby reopens offline.
	GameInstance->LeaveToNewLobby();
}

// ============================================================
// Protected / Internal Logic (Rematch ballot)
// ============================================================

void UMatchOverWidget::HandleRematchVotesChanged(int32 CurrentVotes, int32 RequiredVotes)
{
	if (!IsValid(Txt_Rematch))
	{
		return;
	}

	if (bOffersNextChallenge)
	{
		Txt_Rematch->SetText(LOCTEXT("NextChallenge", "NEXT CHALLENGE"));
		return;
	}

	if (CurrentVotes <= 0)
	{
		Txt_Rematch->SetText(LOCTEXT("RematchIdle", "REMATCH"));
		return;
	}

	// Once this player has voted the button is spent, so the caption switches to the wait state.
	const FText Format = bLocalRematchVoteCast
		? LOCTEXT("RematchWaiting", "WAITING  {0}/{1}")
		: LOCTEXT("RematchTally", "REMATCH  {0}/{1}");

	Txt_Rematch->SetText(FText::Format(Format, FText::AsNumber(CurrentVotes), FText::AsNumber(RequiredVotes)));
}

void UMatchOverWidget::BindRematchBallot()
{
	AMKHGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	// AddUniqueDynamic keeps a second ShowMatchResult call from double-subscribing.
	GameState->OnRematchVotesChanged.AddUniqueDynamic(this, &UMatchOverWidget::HandleRematchVotesChanged);

	// Replay the current tally: the opponent may have voted before this panel existed.
	const FMKHRematchVoteData& Votes = GameState->GetRematchVotes();
	HandleRematchVotesChanged(Votes.CurrentVotes, Votes.RequiredVotes);
}

void UMatchOverWidget::UnbindRematchBallot()
{
	AMKHGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	GameState->OnRematchVotesChanged.RemoveDynamic(this, &UMatchOverWidget::HandleRematchVotesChanged);
}

#undef LOCTEXT_NAMESPACE
