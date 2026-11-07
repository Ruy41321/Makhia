// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHBootOverlayWidget.h"

#include "Components/TextBlock.h"
#include "GameInstance/MKHGameInstance.h"
#include "MKHLogChannels.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"

#define LOCTEXT_NAMESPACE "MKHBootOverlay"

// ============================================================
// Lifecycle
// ============================================================

void UMKHBootOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	if (!IsValid(GameInstance))
	{
		RemoveFromParent();
		return;
	}

	GameInstance->OnBootPhaseChanged.AddUObject(this, &UMKHBootOverlayWidget::HandleBootPhaseChanged);
	HandleBootPhaseChanged(GameInstance->GetBootPhase());

	// Focusable so the gamepad menu navigation treats the overlay as the screen on top and keeps the
	// focus off the lobby underneath while it covers it.
	SetIsFocusable(true);
}

void UMKHBootOverlayWidget::NativeDestruct()
{
	if (UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this); IsValid(GameInstance))
	{
		GameInstance->OnBootPhaseChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMKHBootOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bFadingOut)
	{
		return;
	}

	FadeElapsed += InDeltaTime;
	const float Alpha = FadeOutDuration > 0.f ? FMath::Clamp(1.f - FadeElapsed / FadeOutDuration, 0.f, 1.f) : 0.f;
	SetRenderOpacity(Alpha);
	if (Alpha <= 0.f)
	{
		UE_LOG(LogMKHUI, Log, TEXT("UMKHBootOverlayWidget: boot over, overlay removed."));
		RemoveFromParent();
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHBootOverlayWidget::HandleBootPhaseChanged(EMKHBootPhase NewPhase)
{
	MKHLobbyUI::SetText(StatusText, GetPhaseText(NewPhase));

	// The lobby is usable from now on: stop catching clicks at once, the fade is cosmetic.
	if (NewPhase == EMKHBootPhase::Done && !bFadingOut)
	{
		bFadingOut = true;
		FadeElapsed = 0.f;
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

FText UMKHBootOverlayWidget::GetPhaseText(EMKHBootPhase Phase)
{
	switch (Phase)
	{
	case EMKHBootPhase::Travelling:
		return LOCTEXT("OpeningLobby", "Opening your lobby...");
	case EMKHBootPhase::Done:
		return FText::GetEmpty();
	default:
		return LOCTEXT("Connecting", "Connecting...");
	}
}

#undef LOCTEXT_NAMESPACE
