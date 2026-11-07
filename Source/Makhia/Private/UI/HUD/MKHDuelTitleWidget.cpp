// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/MKHDuelTitleWidget.h"

#include "Career/MKHCareerSubsystem.h"
#include "Character/CharacterBase.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameMode/MKHGameState.h"
#include "Interfaces/MKHDuelParticipantInterface.h"
#include "TimerManager.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHDuelTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ShownCareerLevels = INDEX_NONE;
	SetVisibility(ESlateVisibility::Collapsed);

	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UMKHDuelTitleWidget::RefreshTitle, RefreshInterval, true);
	}
	RefreshTitle();
}

void UMKHDuelTitleWidget::NativeDestruct()
{
	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}

	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHDuelTitleWidget::RefreshTitle()
{
	const UWorld* World = GetWorld();
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(GameState) || !GameState->GetPlayerDatas().IsValidIndex(SlotIndex))
	{
		return;
	}

	const ACharacterBase* Participant = GameState->GetPlayerDatas()[SlotIndex].Participant;
	const IMKHDuelParticipant* DuelParticipant = Cast<IMKHDuelParticipant>(Participant);
	const int32 CareerLevels = DuelParticipant != nullptr ? DuelParticipant->GetParticipantCareerLevels() : INDEX_NONE;
	if (CareerLevels == ShownCareerLevels)
	{
		return;
	}

	ShownCareerLevels = CareerLevels;
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	if (CareerLevels == INDEX_NONE || !IsValid(Career) || !IsValid(TitleText))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bHighestTitle = Career->GetLevelCount() > 0 && CareerLevels >= Career->GetLevelCount();
	TitleText->SetText(Career->GetTitleForCompletedLevels(CareerLevels));
	TitleText->SetColorAndOpacity(bHighestTitle ? HighestTitleColor : TitleColor);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
