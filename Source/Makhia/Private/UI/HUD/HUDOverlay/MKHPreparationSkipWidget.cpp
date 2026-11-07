// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/MKHPreparationSkipWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/MKHGameState.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "UserSettings/EnhancedInputUserSettings.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHPreparationSkipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Shown only by an open ballot.
	SetVisibility(ESlateVisibility::Collapsed);

	BindInputSettings();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (AGameStateBase* GameState = World->GetGameState())
	{
		BindGameState(GameState);
		return;
	}

	// On clients the game state has usually not replicated yet when the HUD is built.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UMKHPreparationSkipWidget::BindGameState);
}

void UMKHPreparationSkipWidget::NativeDestruct()
{
	UnbindGameState();
	UnbindInputSettings();

	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHPreparationSkipWidget::HandleSkipVotesChanged(int32 CurrentVotes, int32 RequiredVotes)
{
	// A closed ballot means the final countdown (or the duel) started: the prompt must go.
	if (RequiredVotes <= 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bLocalPlayerReady = IsLocalPlayerReady();
	const bool bOpponentReady = CurrentVotes > (bLocalPlayerReady ? 1 : 0);

	if (IsValid(Txt_SkipPrompt))
	{
		Txt_SkipPrompt->SetText(BuildPromptText(bLocalPlayerReady, bOpponentReady));
	}

	if (IsValid(Txt_ReadyCount))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(CurrentVotes));
		Args.Add(TEXT("Required"), FText::AsNumber(RequiredVotes));
		Txt_ReadyCount->SetText(FText::Format(ReadyCountFormat, Args));
	}

	// Never hit-testable: the prompt sits over the inventory during the equip phase.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BP_OnSkipVotesUpdated(CurrentVotes, RequiredVotes, bLocalPlayerReady, bOpponentReady);
}

void UMKHPreparationSkipWidget::HandleInputSettingsApplied()
{
	RefreshFromGameState();
}

// ============================================================
// Protected / Internal Logic (Helpers)
// ============================================================

void UMKHPreparationSkipWidget::BindGameState(AGameStateBase* GameState)
{
	AMKHGameState* MKHGameState = Cast<AMKHGameState>(GameState);
	if (!IsValid(MKHGameState))
	{
		return;
	}

	// GameStateSetHandle stays bound until NativeDestruct: removing it here would happen inside its own broadcast.
	if (AMKHGameState* PreviousGameState = BoundGameState.Get())
	{
		PreviousGameState->OnPreparationSkipVotesChanged.RemoveDynamic(this, &UMKHPreparationSkipWidget::HandleSkipVotesChanged);
	}

	BoundGameState = MKHGameState;
	MKHGameState->OnPreparationSkipVotesChanged.AddUniqueDynamic(this, &UMKHPreparationSkipWidget::HandleSkipVotesChanged);

	// The opponent may have readied up before this widget existed.
	RefreshFromGameState();
}

void UMKHPreparationSkipWidget::UnbindGameState()
{
	if (GameStateSetHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
		}
		GameStateSetHandle.Reset();
	}

	if (AMKHGameState* GameState = BoundGameState.Get())
	{
		GameState->OnPreparationSkipVotesChanged.RemoveDynamic(this, &UMKHPreparationSkipWidget::HandleSkipVotesChanged);
	}
	BoundGameState.Reset();
}

void UMKHPreparationSkipWidget::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	RefreshFromGameState();
}

void UMKHPreparationSkipWidget::BindInputSettings()
{
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = UMKHInputSettingsLibrary::GetActiveInputDeviceSubsystem(GetOwningPlayer()); IsValid(DeviceSubsystem))
	{
		BoundDeviceSubsystem = DeviceSubsystem;
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &UMKHPreparationSkipWidget::HandleActiveInputDeviceChanged);
	}

	UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(GetOwningPlayer());
	if (!IsValid(UserSettings))
	{
		return;
	}

	BoundInputSettings = UserSettings;
	UserSettings->OnSettingsApplied.AddUniqueDynamic(this, &UMKHPreparationSkipWidget::HandleInputSettingsApplied);
}

void UMKHPreparationSkipWidget::UnbindInputSettings()
{
	if (UEnhancedInputUserSettings* UserSettings = BoundInputSettings.Get())
	{
		UserSettings->OnSettingsApplied.RemoveDynamic(this, &UMKHPreparationSkipWidget::HandleInputSettingsApplied);
	}
	BoundInputSettings.Reset();

	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = BoundDeviceSubsystem.Get())
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &UMKHPreparationSkipWidget::HandleActiveInputDeviceChanged);
	}
	BoundDeviceSubsystem.Reset();
}

void UMKHPreparationSkipWidget::RefreshFromGameState()
{
	const AMKHGameState* GameState = BoundGameState.Get();
	if (!IsValid(GameState))
	{
		return;
	}

	const FMKHPreparationSkipVoteData& Votes = GameState->GetPreparationSkipVotes();
	HandleSkipVotesChanged(Votes.ReadyPlayers.Num(), Votes.RequiredVotes);
}

bool UMKHPreparationSkipWidget::IsLocalPlayerReady() const
{
	const AMKHGameState* GameState = BoundGameState.Get();
	const APlayerController* OwningPlayer = GetOwningPlayer();
	return IsValid(GameState) && IsValid(OwningPlayer) && GameState->IsPlayerReadyToSkip(OwningPlayer->PlayerState);
}

FText UMKHPreparationSkipWidget::BuildPromptText(bool bLocalPlayerReady, bool bOpponentReady) const
{
	const FText& Format = bLocalPlayerReady ? LocalReadyPromptFormat
		: (bOpponentReady ? OpponentReadyPromptFormat : IdlePromptFormat);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice(GetOwningPlayer(), SkipPreparationAction));
	return FText::Format(Format, Args);
}
