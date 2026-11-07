// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHDevLoginPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "UI/Button/MKHButton.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"
#include "UI/Text/MKHTextBlock.h"

// ============================================================
// UMKHDevLoginButtonRelay
// ============================================================

void UMKHDevLoginButtonRelay::HandleClicked()
{
	if (UMKHDevLoginPanelWidget* PanelWidget = Panel.Get())
	{
		PanelWidget->LoginWithProfile(ProfileIndex);
	}
}

// ============================================================
// Public Interface
// ============================================================

void UMKHDevLoginPanelWidget::LoginWithProfile(int32 ProfileIndex)
{
	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth))
	{
		Auth->LoginWithDevProfile(ProfileIndex);
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHDevLoginPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Also hidden when the online backend has no developer login (e.g. Steam).
	const UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	if (!UMKHAuthSubsystem::ShouldShowDevLoginButtons() || !IsValid(Auth) || !Auth->SupportsMethod(EMKHAuthMethod::Developer))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	BuildButtons();
}

void UMKHDevLoginPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	const bool bCanLogin = IsValid(Auth) && Auth->CanStartLogin();
	for (UMKHButton* Button : ProfileButtons)
	{
		MKHLobbyUI::SetEnabled(Button, bCanLogin);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHDevLoginPanelWidget::BuildButtons()
{
	const TArray<FString> Names = UMKHAuthSubsystem::GetDevCredentialNames();
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		UMKHButton* Button = WidgetTree->ConstructWidget<UMKHButton>(UMKHButton::StaticClass());
		UMKHTextBlock* Label = WidgetTree->ConstructWidget<UMKHTextBlock>(UMKHTextBlock::StaticClass());
		if (!IsValid(Button) || !IsValid(Label))
		{
			continue;
		}

		// Developer UI: culture invariant on purpose.
		Label->SetTextStyle(LabelStyle);
		Label->SetText(FText::Format(INVTEXT("Dev login: {0}"), FText::AsCultureInvariant(Names[Index])));
		Button->SetButtonStyle(ButtonStyle);
		Button->AddChild(Label);

		UMKHDevLoginButtonRelay* Relay = NewObject<UMKHDevLoginButtonRelay>(this);
		Relay->Panel = this;
		Relay->ProfileIndex = Index;
		Button->OnClicked.AddUniqueDynamic(Relay, &UMKHDevLoginButtonRelay::HandleClicked);

		if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(ButtonBox->AddChild(Button)))
		{
			BoxSlot->SetPadding(FMargin(0.f, Index > 0 ? ButtonSpacing : 0.f, 0.f, 0.f));
		}

		ProfileButtons.Add(Button);
		ButtonRelays.Add(Relay);
	}
}
