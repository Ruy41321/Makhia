// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/Null/MKHAuthProvider_Null.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "MKHLogChannels.h"
#include "OnlineSubsystem.h"

namespace MKHAuthNull
{
	/** Name of the Null online subsystem. */
	static const FName SubsystemName(TEXT("NULL"));

	/** Characters of the id kept in the default display name. */
	static constexpr int32 NameSuffixLength = 4;

	/** Returns the identity interface of the Null online subsystem, or null. */
	IOnlineIdentityPtr GetIdentity()
	{
		const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(SubsystemName);
		return Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	}
}

// ============================================================
// Public Interface
// ============================================================

bool UMKHAuthProvider_Null::IsBackendAvailable() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return MKHAuthNull::GetIdentity().IsValid();
#endif
}

bool UMKHAuthProvider_Null::SupportsMethod(EMKHAuthMethod Method) const
{
	return Method == EMKHAuthMethod::PlatformAccount || Method == EMKHAuthMethod::Guest;
}

FText UMKHAuthProvider_Null::GetMethodDisplayName(EMKHAuthMethod Method) const
{
	return Method == EMKHAuthMethod::PlatformAccount ? INVTEXT("LAN") : Super::GetMethodDisplayName(Method);
}

void UMKHAuthProvider_Null::Login(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent, const FMKHAuthLoginCompleteDelegate& OnComplete)
{
	FMKHAuthLoginResult Result;
	const IOnlineIdentityPtr Identity = MKHAuthNull::GetIdentity();
	if (!IsBackendAvailable() || !Identity.IsValid())
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

	// The Null identity answers inside Login, so the result is read right after the call.
	Identity->Login(LocalUserNum, FOnlineAccountCredentials());
	const FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(LocalUserNum);
	if (!UserId.IsValid() || Identity->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
	{
		Result.Error = EMKHAuthError::Unknown;
		Result.RawError = TEXT("Null identity login failed.");
		OnComplete.ExecuteIfBound(Result);
		return;
	}

	Result = MakeSuccessResult(Method, Params, UserId->ToString());
	UE_LOG(LogMKHAuth, Log, TEXT("Null provider: signed in as %s (%s)."), *Result.User.DisplayName, *Result.User.UserId);
	OnComplete.ExecuteIfBound(Result);
}

void UMKHAuthProvider_Null::Logout(const FSimpleDelegate& OnComplete)
{
	if (const IOnlineIdentityPtr Identity = MKHAuthNull::GetIdentity(); Identity.IsValid())
	{
		Identity->Logout(LocalUserNum);
	}
	OnComplete.ExecuteIfBound();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FMKHAuthLoginResult UMKHAuthProvider_Null::MakeSuccessResult(EMKHAuthMethod Method, const FMKHAuthParams& Params, const FString& UserId)
{
	FMKHAuthLoginResult Result;
	Result.bSuccess = true;
	Result.User.UserId = UserId;
	Result.User.Method = Method;
	Result.User.bHasSocialAccount = false;
	Result.User.DisplayName = (Method == EMKHAuthMethod::Guest && !Params.GuestDisplayName.IsEmpty())
		? Params.GuestDisplayName
		: FString::Printf(TEXT("LAN-%s"), *UserId.Right(MKHAuthNull::NameSuffixLength));
	return Result;
}
