// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHAuthProvider_Steam.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "MKHLogChannels.h"
#include "MKHSteamShared.h"
#include "OnlineSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHAuthSteam"

namespace MKHAuthSteam
{
	/** Returns the identity interface of the Steam online subsystem, or null. */
	IOnlineIdentityPtr GetIdentity()
	{
		const IOnlineSubsystem* Subsystem = MKHSteam::GetSubsystem();
		return Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHAuthProvider_Steam::InitializeProvider()
{
	IOnlineSubsystem* Subsystem = MKHSteam::GetSubsystem();
	if (!Subsystem)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Steam provider: the Steam online subsystem is not running (Steam closed, -nosteam or editor)."));
		return;
	}

	bServersReachable = IsBackendReachable();
	ConnectionStatusHandle = Subsystem->AddOnConnectionStatusChangedDelegate_Handle(
		FOnConnectionStatusChangedDelegate::CreateUObject(this, &UMKHAuthProvider_Steam::HandleConnectionStatusChanged));
}

void UMKHAuthProvider_Steam::ShutdownProvider()
{
	if (IOnlineSubsystem* Subsystem = MKHSteam::GetSubsystem())
	{
		Subsystem->ClearOnConnectionStatusChangedDelegate_Handle(ConnectionStatusHandle);
	}

	ConnectionStatusHandle.Reset();
	bHasSignedInUser = false;
	bServersReachable = false;
	OnIdentityLost.Clear();
	OnIdentityRestored.Clear();
}

// ============================================================
// Public Interface
// ============================================================

bool UMKHAuthProvider_Steam::IsBackendAvailable() const
{
	return MKHAuthSteam::GetIdentity().IsValid();
}

bool UMKHAuthProvider_Steam::SupportsMethod(EMKHAuthMethod Method) const
{
	return Method == EMKHAuthMethod::PlatformAccount;
}

FText UMKHAuthProvider_Steam::GetMethodDisplayName(EMKHAuthMethod Method) const
{
	// Brand name: the same in every language.
	return Method == EMKHAuthMethod::PlatformAccount ? INVTEXT("Steam") : Super::GetMethodDisplayName(Method);
}

FText UMKHAuthProvider_Steam::GetUnavailableText() const
{
	return LOCTEXT("StartSteam", "Steam is not running. Start Steam, then restart the game to play online.");
}

bool UMKHAuthProvider_Steam::IsBackendReachable() const
{
	const IOnlineIdentityPtr Identity = MKHAuthSteam::GetIdentity();
	return Identity.IsValid() && Identity->GetLoginStatus(MKHSteam::LocalUserNum) == ELoginStatus::LoggedIn;
}

void UMKHAuthProvider_Steam::Login(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent, const FMKHAuthLoginCompleteDelegate& OnComplete)
{
	FMKHAuthLoginResult Result;
	const IOnlineIdentityPtr Identity = MKHAuthSteam::GetIdentity();
	if (!Identity.IsValid())
	{
		Result.Error = EMKHAuthError::BackendUnavailable;
		OnComplete.ExecuteIfBound(Result);
		return;
	}
	if (!SupportsMethod(Method))
	{
		Result.Error = EMKHAuthError::MethodNotSupported;
		OnComplete.ExecuteIfBound(Result);
		return;
	}

	// The Steam client owns the session: the identity only reports whether it reaches the Steam servers.
	const FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(MKHSteam::LocalUserNum);
	if (!UserId.IsValid() || Identity->GetLoginStatus(MKHSteam::LocalUserNum) != ELoginStatus::LoggedIn)
	{
		Result.Error = EMKHAuthError::NoNetwork;
		Result.RawError = TEXT("The Steam client is not connected to the Steam servers (offline mode?).");
		OnComplete.ExecuteIfBound(Result);
		return;
	}

	Result.bSuccess = true;
	Result.User.UserId = UserId->ToString();
	Result.User.DisplayName = Identity->GetPlayerNickname(MKHSteam::LocalUserNum);
	Result.User.Method = EMKHAuthMethod::PlatformAccount;
	Result.User.bHasSocialAccount = true;
	bHasSignedInUser = true;
	bServersReachable = true;
	UE_LOG(LogMKHAuth, Log, TEXT("Steam provider: signed in as %s (%s)."), *Result.User.DisplayName, *Result.User.UserId);
	OnComplete.ExecuteIfBound(Result);
}

void UMKHAuthProvider_Steam::Logout(const FSimpleDelegate& OnComplete)
{
	bHasSignedInUser = false;
	OnComplete.ExecuteIfBound();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHAuthProvider_Steam::HandleConnectionStatusChanged(const FString& ServiceName, EOnlineServerConnectionStatus::Type LastStatus,
	EOnlineServerConnectionStatus::Type NewStatus)
{
	UE_LOG(LogMKHAuth, Log, TEXT("Steam provider: Steam servers connection %s -> %s."), EOnlineServerConnectionStatus::ToString(LastStatus),
		EOnlineServerConnectionStatus::ToString(NewStatus));
	const bool bConnected = NewStatus == EOnlineServerConnectionStatus::Connected || NewStatus == EOnlineServerConnectionStatus::Normal;
	if (bConnected)
	{
		// Also after a start-up without network: the auth subsystem retries the login that failed.
		if (!bServersReachable)
		{
			bServersReachable = true;
			OnIdentityRestored.Broadcast();
		}
		return;
	}

	bServersReachable = false;
	if (bHasSignedInUser)
	{
		bHasSignedInUser = false;
		OnIdentityLost.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
