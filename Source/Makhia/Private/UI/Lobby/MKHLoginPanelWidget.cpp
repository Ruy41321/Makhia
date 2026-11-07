// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHLoginPanelWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#define LOCTEXT_NAMESPACE "MKHLoginPanel"

// ============================================================
// Lifecycle
// ============================================================

void UMKHLoginPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindWidgetEvents();
}

void UMKHLoginPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	if (IsValid(Auth))
	{
		Auth->OnLoginFailed.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleLoginFailed);
		Auth->OnAuthStateChanged.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleAuthStateChanged);
		GuestNameBox->SetText(FText::AsCultureInvariant(Auth->GetSavedGuestName()));
		MKHLobbyUI::SetText(PlatformButtonText, FText::Format(LOCTEXT("SignInWithMethod", "Sign in with {Method}"),
			FFormatNamedArguments{ { TEXT("Method"), Auth->GetMethodDisplayName(EMKHAuthMethod::PlatformAccount) } }));
	}

	MKHLobbyUI::SetShown(GuestSection, IsValid(Auth) && Auth->SupportsMethod(EMKHAuthMethod::Guest), ESlateVisibility::SelfHitTestInvisible);
	MKHLobbyUI::SetShown(GuestForm, false);
	LastErrorText = FText::GetEmpty();
	RefreshGuestValidation();
	RefreshState();
	SetKeyboardFocus();
}

void UMKHLoginPanelWidget::NativeDestruct()
{
	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth))
	{
		Auth->OnLoginFailed.RemoveDynamic(this, &UMKHLoginPanelWidget::HandleLoginFailed);
		Auth->OnAuthStateChanged.RemoveDynamic(this, &UMKHLoginPanelWidget::HandleAuthStateChanged);
	}

	Super::NativeDestruct();
}

void UMKHLoginPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshState();
}

FReply UMKHLoginPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Focus inside the panel (name field) keeps Esc from the menu input action: handle it here, with the pad's Back.
	if (InKeyEvent.GetKey() == EKeys::Escape || UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent))
	{
		HandleCloseClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLoginPanelWidget::BindWidgetEvents()
{
	for (UButton* Close : { BackdropButton.Get(), CloseButton.Get() })
	{
		if (IsValid(Close))
		{
			Close->OnClicked.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleCloseClicked);
		}
	}

	PlatformButton->OnClicked.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandlePlatformClicked);
	if (IsValid(GuestButton))
	{
		GuestButton->OnClicked.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleGuestClicked);
	}

	if (IsValid(ContinueButton))
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleContinueClicked);
	}

	GuestNameBox->OnTextChanged.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleGuestNameChanged);
	GuestNameBox->OnTextCommitted.AddUniqueDynamic(this, &UMKHLoginPanelWidget::HandleGuestNameCommitted);
}

void UMKHLoginPanelWidget::RefreshState()
{
	const UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	const bool bCanLogin = IsValid(Auth) && Auth->CanStartLogin();
	const bool bLoggingIn = IsValid(Auth) && Auth->GetAuthState() == EMKHAuthState::LoggingIn;

	MKHLobbyUI::SetEnabled(PlatformButton, bCanLogin);
	MKHLobbyUI::SetEnabled(GuestButton, bCanLogin);
	MKHLobbyUI::SetEnabled(GuestNameBox, bCanLogin);
	MKHLobbyUI::SetEnabled(ContinueButton, bCanLogin && !GetValidGuestName().IsEmpty());

	const FText Status = bLoggingIn ? LOCTEXT("SigningIn", "Signing in...")
		: !LastErrorText.IsEmpty() ? LastErrorText
		: IsValid(Auth) && !Auth->IsOnlineAvailable() ? Auth->GetOnlineUnavailableText()
		: FText::GetEmpty();
	MKHLobbyUI::SetText(StatusText, Status);
	MKHLobbyUI::SetShown(StatusText, !Status.IsEmpty(), ESlateVisibility::HitTestInvisible);
}

void UMKHLoginPanelWidget::RefreshGuestValidation()
{
	FString TrimmedName;
	const EMKHGuestNameError NameError = UMKHAuthSubsystem::ValidateGuestName(GuestNameBox->GetText().ToString(), TrimmedName);

	MKHLobbyUI::SetText(GuestNameCounter, FText::Format(LOCTEXT("NameCounter", "{Count}/{Max}"), FFormatNamedArguments{
		{ TEXT("Count"), FText::AsNumber(TrimmedName.Len()) },
		{ TEXT("Max"), FText::AsNumber(UMKHAuthSubsystem::GetGuestNameMaxLength()) } }));

	// An empty field is not an error yet: the Continue button is simply disabled.
	const FText Message = TrimmedName.IsEmpty() ? FText::GetEmpty() : UMKHAuthSubsystem::GetGuestNameErrorText(NameError);
	MKHLobbyUI::SetText(GuestNameError, Message);
	MKHLobbyUI::SetShown(GuestNameError, !Message.IsEmpty(), ESlateVisibility::HitTestInvisible);
}

void UMKHLoginPanelWidget::SubmitGuestName()
{
	const FString Name = GetValidGuestName();
	UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	if (Name.IsEmpty() || !IsValid(Auth) || !Auth->CanStartLogin())
	{
		return;
	}

	LastErrorText = FText::GetEmpty();
	Auth->LoginAsGuest(Name);
}

FString UMKHLoginPanelWidget::GetValidGuestName() const
{
	FString TrimmedName;
	const EMKHGuestNameError NameError = UMKHAuthSubsystem::ValidateGuestName(GuestNameBox->GetText().ToString(), TrimmedName);
	return NameError == EMKHGuestNameError::None ? TrimmedName : FString();
}

void UMKHLoginPanelWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UMKHLoginPanelWidget::HandlePlatformClicked()
{
	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth))
	{
		LastErrorText = FText::GetEmpty();
		Auth->LoginWithPlatformAccount();
	}
}

void UMKHLoginPanelWidget::HandleGuestClicked()
{
	MKHLobbyUI::SetShown(GuestForm, true, ESlateVisibility::SelfHitTestInvisible);
	GuestNameBox->SetKeyboardFocus();
}

void UMKHLoginPanelWidget::HandleContinueClicked()
{
	SubmitGuestName();
}

void UMKHLoginPanelWidget::HandleGuestNameChanged(const FText& Text)
{
	RefreshGuestValidation();
}

void UMKHLoginPanelWidget::HandleGuestNameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitGuestName();
	}
}

void UMKHLoginPanelWidget::HandleLoginFailed(EMKHAuthMethod Method, EMKHAuthError Error)
{
	LastErrorText = UMKHAuthSubsystem::GetAuthErrorText(Error);
	RefreshState();
}

void UMKHLoginPanelWidget::HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState)
{
	if (NewState == EMKHAuthState::LoggedIn)
	{
		OnCloseRequested.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
