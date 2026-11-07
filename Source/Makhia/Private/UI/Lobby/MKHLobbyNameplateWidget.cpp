// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHLobbyNameplateWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"
#include "Voice/MKHVoiceChatSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHLobbyNameplate"

// ============================================================
// Public Interface
// ============================================================

void UMKHLobbyNameplateWidget::SetPlayer(const FString& PlayerName, const bool bIsReady, const APlayerState* PlayerState)
{
	bPlayerReady = bIsReady;
	bLocal = IsLocalPlayerState(PlayerState);
	VoicePlayerId = IsValid(PlayerState) && PlayerState->GetUniqueId().IsValid() ? PlayerState->GetUniqueId().ToString() : FString();

	MKHLobbyUI::SetText(NameText, FText::AsCultureInvariant(PlayerName));
	RefreshShownReady();
	ApplyReadyLook();
	ApplyFrameLook();
	bHasPlayer = true;
}

void UMKHLobbyNameplateWidget::SetTitle(const FText& Title, const bool bHighestTitle)
{
	MKHLobbyUI::SetText(TitleText, Title);
	MKHLobbyUI::SetShown(TitleTab, !Title.IsEmpty(), ESlateVisibility::HitTestInvisible);

	if (IsValid(TitleTab))
	{
		TitleTab->SetBrushColor(bHighestTitle ? HighestTitleTabColor : TitleTabColor);
	}
	if (IsValid(TitleText))
	{
		TitleText->SetColorAndOpacity(bHighestTitle ? HighestTitleTextColor : TitleTextColor);
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHLobbyNameplateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The bot wait is local and has no replicated state: the subsystem tells when it changes, even while the
	// nameplate is hidden. A UObject binding drops itself when the widget goes away.
	if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
	{
		Lobby->OnOfflineMatchPendingChanged.AddUObject(this, &UMKHLobbyNameplateWidget::HandleOfflineMatchPendingChanged);
	}
}

void UMKHLobbyNameplateWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		bReady = bPreviewReady;
		bLocal = bPreviewLocal;
	}

	ApplyReadyLook();
	ApplyFrameLook();
}

void UMKHLobbyNameplateWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdatePulse(InDeltaTime);
	RefreshTalking();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLobbyNameplateWidget::RefreshTalking() const
{
	if (!IsValid(TalkingIcon))
	{
		return;
	}
	const UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this);
	FMKHVoiceTalkerInfo Talker;
	const bool bTalking = IsValid(VoiceChat) && !VoicePlayerId.IsEmpty() && VoiceChat->FindTalker(VoicePlayerId, Talker) && Talker.bIsTalking;
	TalkingIcon->SetVisibility(bTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UMKHLobbyNameplateWidget::ApplyReadyLook() const
{
	const FSlateColor& Colour = bReady ? ReadyColor : NotReadyColor;

	MKHLobbyUI::SetText(StatusText, bReady ? LOCTEXT("Ready", "Ready") : LOCTEXT("NotReady", "Not ready"));

	if (IsValid(StatusText))
	{
		StatusText->SetColorAndOpacity(Colour);
	}

	if (IsValid(StatusDot))
	{
		StatusDot->SetBrushTintColor(Colour);
	}
}

void UMKHLobbyNameplateWidget::ApplyFrameLook() const
{
	if (IsValid(Frame))
	{
		Frame->SetBrush(bLocal ? LocalFrameBrush : RemoteFrameBrush);
	}
}

void UMKHLobbyNameplateWidget::UpdatePulse(const float DeltaSeconds)
{
	if (PulseTimeLeft <= 0.f || !IsValid(StatusChip))
	{
		return;
	}

	PulseTimeLeft = FMath::Max(PulseTimeLeft - DeltaSeconds, 0.f);

	// Half sine: 1 -> PulseScale -> 1 over PulseDuration.
	const float Alpha = PulseDuration > 0.f ? 1.f - PulseTimeLeft / PulseDuration : 1.f;
	const float Scale = 1.f + (PulseScale - 1.f) * FMath::Sin(PI * Alpha);
	StatusChip->SetRenderScale(FVector2D(Scale));
}

bool UMKHLobbyNameplateWidget::ComputeShownReady() const
{
	if (bPlayerReady || !bLocal)
	{
		return bPlayerReady;
	}

	const UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	return IsValid(Lobby) && Lobby->IsOfflineMatchPending();
}

void UMKHLobbyNameplateWidget::RefreshShownReady()
{
	const bool bShownReady = ComputeShownReady();
	if (bShownReady == bReady)
	{
		return;
	}

	// The first state shown does not pulse.
	if (bHasPlayer)
	{
		PulseTimeLeft = PulseDuration;
	}
	bReady = bShownReady;
	ApplyReadyLook();
}

void UMKHLobbyNameplateWidget::HandleOfflineMatchPendingChanged(bool bPending)
{
	if (bHasPlayer)
	{
		RefreshShownReady();
	}
}

bool UMKHLobbyNameplateWidget::IsLocalPlayerState(const APlayerState* PlayerState) const
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	return IsValid(PlayerState) && IsValid(OwningPlayer) && OwningPlayer->PlayerState == PlayerState;
}

#undef LOCTEXT_NAMESPACE
