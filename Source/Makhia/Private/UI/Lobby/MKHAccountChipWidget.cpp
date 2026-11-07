// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHAccountChipWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "Online/MKHFriendsSubsystem.h"
#include "UI/HUD/MKHLobbyHUD.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"

#define LOCTEXT_NAMESPACE "MKHAccountChip"

#if !UE_BUILD_SHIPPING
namespace MKHAccountChip
{
	static TAutoConsoleVariable<bool> CVarForceMenuBadge(TEXT("mkh.UI.ForceMenuBadge"), false,
		TEXT("Development: always show the notification badge of the game menu button (layout checks)."));
}
#endif

// ============================================================
// Lifecycle
// ============================================================

void UMKHAccountChipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(SignInButton))
	{
		SignInButton->OnClicked.AddUniqueDynamic(this, &UMKHAccountChipWidget::HandleSignInClicked);
	}

	if (IsValid(SignOutButton))
	{
		SignOutButton->OnClicked.AddUniqueDynamic(this, &UMKHAccountChipWidget::HandleSignOutClicked);
	}

	if (IsValid(SwitchAccountButton))
	{
		SwitchAccountButton->OnClicked.AddUniqueDynamic(this, &UMKHAccountChipWidget::HandleSwitchAccountClicked);
	}

	if (IsValid(GameMenuButton))
	{
		GameMenuButton->OnClicked.AddUniqueDynamic(this, &UMKHAccountChipWidget::HandleGameMenuClicked);
	}
}

void UMKHAccountChipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Refresh();
}

void UMKHAccountChipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Refresh();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHAccountChipWidget::Refresh()
{
	const UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	const EMKHAuthState State = IsValid(Auth) ? Auth->GetAuthState() : EMKHAuthState::Offline;
	const FMKHAuthUser User = IsValid(Auth) ? Auth->GetUser() : FMKHAuthUser();

	FText Name;
	FText Method;
	switch (State)
	{
	case EMKHAuthState::LoggedIn:
		// Player names are data, never translated.
		Name = FText::AsCultureInvariant(User.DisplayName);
		Method = Auth->GetMethodDisplayName(User.Method);
		break;
	case EMKHAuthState::LoggingIn:
	case EMKHAuthState::Unknown:
		Name = LOCTEXT("SigningIn", "Signing in...");
		break;
	default:
		Name = LOCTEXT("Offline", "Offline");
		Method = GetOfflineReasonText(Auth);
		break;
	}

	MKHLobbyUI::SetText(NameText, Name);
	MKHLobbyUI::SetText(MethodText, Method);
	MKHLobbyUI::SetShown(MethodText, !Method.IsEmpty(), ESlateVisibility::HitTestInvisible);

	const bool bLoggedIn = State == EMKHAuthState::LoggedIn;
	const bool bCanSwitch = bLoggedIn && User.Method == EMKHAuthMethod::Guest && Auth->SupportsMethod(EMKHAuthMethod::PlatformAccount);
	MKHLobbyUI::SetShown(SignInButton, State == EMKHAuthState::Offline && IsValid(Auth) && Auth->CanStartLogin());
	MKHLobbyUI::SetShown(SignOutButton, IsValid(Auth) && Auth->CanSignOut());

	const UMKHFriendsSubsystem* Friends = UMKHFriendsSubsystem::Get(this);
	bool bShowBadge = IsValid(Friends) && Friends->GetPendingNotificationCount() > 0;
#if !UE_BUILD_SHIPPING
	bShowBadge |= MKHAccountChip::CVarForceMenuBadge.GetValueOnGameThread();
#endif
	MKHLobbyUI::SetShown(GameMenuBadge, bShowBadge, ESlateVisibility::HitTestInvisible);
	MKHLobbyUI::SetShown(SwitchAccountButton, bCanSwitch);
	if (bCanSwitch)
	{
		MKHLobbyUI::SetText(SwitchAccountText, FText::Format(LOCTEXT("SignInWithMethod", "Sign in with {Method}"),
			FFormatNamedArguments{ { TEXT("Method"), Auth->GetMethodDisplayName(EMKHAuthMethod::PlatformAccount) } }));
	}
}

FText UMKHAccountChipWidget::GetOfflineReasonText(const UMKHAuthSubsystem* Auth)
{
	if (!IsValid(Auth) || !Auth->IsOnlineAvailable())
	{
		return LOCTEXT("OnlineUnavailable", "Online services unavailable");
	}
	return Auth->IsOnlineReachable() ? LOCTEXT("NotSignedIn", "Not signed in") : LOCTEXT("NoInternet", "No internet connection");
}

void UMKHAccountChipWidget::HandleGameMenuClicked()
{
	if (AMKHLobbyHUD* LobbyHUD = AMKHLobbyHUD::GetForPlayer(GetOwningPlayer()); IsValid(LobbyHUD))
	{
		LobbyHUD->ToggleGameMenu();
	}
}

void UMKHAccountChipWidget::HandleSignInClicked()
{
	if (AMKHLobbyHUD* LobbyHUD = AMKHLobbyHUD::GetForPlayer(GetOwningPlayer()); IsValid(LobbyHUD))
	{
		LobbyHUD->OpenLoginPanel();
	}
}

void UMKHAccountChipWidget::HandleSignOutClicked()
{
	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth))
	{
		Auth->Logout();
	}
}

void UMKHAccountChipWidget::HandleSwitchAccountClicked()
{
	if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this); IsValid(Auth))
	{
		Auth->SwitchToPlatformAccount();
	}
}

#undef LOCTEXT_NAMESPACE
