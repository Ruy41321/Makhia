// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Auth/MKHAuthProvider.h"
#include "MKHAuthProvider_Null.generated.h"

/**
 * Development auth provider on the Null online subsystem: every login succeeds at once with a local id (a different
 * one per process, see FOnlineIdentityNull). The display name is the guest name, else "LAN-" plus the end of the id.
 * Refuses every login in Shipping builds (the Null backend is then offline only).
 * Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIA_API UMKHAuthProvider_Null : public UMKHAuthProvider
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Returns true outside Shipping when the Null online subsystem exposes an identity interface. */
	virtual bool IsBackendAvailable() const override;

	/** Supports PlatformAccount and Guest. */
	virtual bool SupportsMethod(EMKHAuthMethod Method) const override;

	/** Names the platform account "LAN". */
	virtual FText GetMethodDisplayName(EMKHAuthMethod Method) const override;

	/** Never busy: the Null login answers at once. */
	virtual bool IsBusy() const override { return false; }

	/** Logs the local user in on the Null identity interface. */
	virtual void Login(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent, const FMKHAuthLoginCompleteDelegate& OnComplete) override;

	/** Logs the local user out of the Null identity interface. */
	virtual void Logout(const FSimpleDelegate& OnComplete) override;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Builds the result of a successful login.
	 *
	 * @param Method  Method used.
	 * @param Params  Login input (guest name).
	 * @param UserId  Id given by the Null identity interface.
	 */
	static FMKHAuthLoginResult MakeSuccessResult(EMKHAuthMethod Method, const FMKHAuthParams& Params, const FString& UserId);

	/** Local player index used for every online call. */
	static constexpr int32 LocalUserNum = 0;
};
