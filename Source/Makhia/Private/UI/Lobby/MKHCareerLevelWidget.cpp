// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHCareerLevelWidget.h"

#include "Components/TextBlock.h"
#include "UI/Button/MKHButton.h"
#include "UI/Button/MKHButtonStyle.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHCareerLevelWidget::SetLevel(const FText& State, const FText& Opponent, const FText& Reward, const bool bLocked)
{
	MKHLobbyUI::SetText(StateText, State);
	MKHLobbyUI::SetText(OpponentText, Opponent);
	MKHLobbyUI::SetText(RewardText, Reward);
	MKHLobbyUI::SetEnabled(LevelButton, !bLocked);
}

void UMKHCareerLevelWidget::SetSelected(const bool bSelected)
{
	// Instances placed in another widget can load without the class default: fall back to the class default object.
	const UMKHCareerLevelWidget* Defaults = GetClass()->GetDefaultObject<UMKHCareerLevelWidget>();
	UMKHButtonStyle* Normal = IsValid(NormalStyle) ? NormalStyle.Get() : Defaults->NormalStyle.Get();
	UMKHButtonStyle* Selected = IsValid(SelectedStyle) ? SelectedStyle.Get() : Defaults->SelectedStyle.Get();

	UMKHButtonStyle* Style = bSelected ? Selected : Normal;
	if (IsValid(LevelButton) && IsValid(Style))
	{
		LevelButton->SetButtonStyle(Style);
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHCareerLevelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(LevelButton))
	{
		LevelButton->OnClicked.AddUniqueDynamic(this, &UMKHCareerLevelWidget::HandleClicked);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHCareerLevelWidget::HandleClicked()
{
	OnLevelClicked.Broadcast(this);
}
