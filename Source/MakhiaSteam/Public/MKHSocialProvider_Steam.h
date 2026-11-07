// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/Auth/MKHAuthTypes.h"
#include "Online/Backend/MKHSocialProvider.h"
#include "MKHSocialProvider_Steam.generated.h"

class FMKHSteamSocialCallbacks;
class UTexture2D;

/**
 * Social provider on the Steamworks friends API (ISteamFriends) of the running Steam client: friend list with
 * presence ("In Makhia" needs the game's own rich presence key, since AppID 480 is shared by every Steamworks
 * developer; "In your party" when the friend's lobby is the local player's; lobby or match with round and score
 * through the game's rich presence keys), friend requests received and sent (answered in the Steam overlay: Steam
 * has no API for them), avatars, recent players (Steam coplay list fed by ReportPlayedWith) and the lobby invites received in game (LobbyInvite_t,
 * not handled by the Steam online subsystem). An invite is joined through the lobby the sender is in
 * (IOnlineSession::FindFriendSession), since the Steam subsystem cannot find a lobby by id.
 *
 * The service follows the game sign-in (UMKHAuthSubsystem): signing out takes the friend list offline too.
 * Steam notifications arrive on the online thread and are handled on the game thread.
 * Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIASTEAM_API UMKHSocialProvider_Steam : public UMKHSocialProvider
{
	GENERATED_BODY()

	friend class FMKHSteamSocialCallbacks;

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Registers the Steam notifications and follows the game sign-in. */
	virtual void InitializeProvider() override;

	/** Stops the service, unregisters the notifications and releases the avatars. */
	virtual void ShutdownProvider() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Unavailable without Steam, SignedOut while the game is not signed in, else Available. */
	virtual EMKHSocialAvailability GetAvailability() const override;

	/** Publishes the "in Makhia" rich presence and starts relaying the notifications. */
	virtual void StartService() override;

	/** Removes the rich presence and stops relaying the notifications. */
	virtual void StopService() override;

	/** Steam keeps the friend list up to date in the client: completes at once. */
	virtual void ReadFriends(const FMKHFriendsReadComplete& OnComplete) override;

	/** Friends, requests received and requests sent, read from the Steam client. */
	virtual void GetCachedFriends(TArray<FMKHFriendInfo>& OutFriends) const override;

	/** Finds the lobby the sender is in (FindFriendSession), retried while another search runs. */
	virtual void ResolveLobbyInvite(const FMKHLobbyInvite& Invite, const FMKHLobbyInviteResolved& OnResolved) override;

	/** Medium Steam avatar (64x64), converted once into a texture. */
	virtual UTexture2D* GetAvatar(const FString& UserId) override;

	/** Steam coplay list of this game, without friends and pending requests, most recent first. */
	virtual void GetRecentPlayers(TArray<FMKHFriendInfo>& OutPlayers) const override;

	/** ISteamFriends::SetPlayedWith for each user. */
	virtual void ReportPlayedWith(const TArray<FString>& UserIds) override;

	/** Steam overlay: friend list, profile ("steamid") or add-friend prompt ("friendadd"). */
	virtual bool OpenOverlay(EMKHSocialOverlayPage Page, const FString& UserId) override;

	/** Steam block list (ISteamFriends::HasFriend with k_EFriendFlagBlocked). */
	virtual bool IsBlocked(const FString& UserId) const override;

	/** Rich presence keys mkh_act / mkh_lobby / mkh_round / mkh_score, read back into the friends' entries. */
	virtual void SetLocalActivity(const FMKHPlayerActivity& Activity) override;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Auth notification: the game signed in or out. */
	void HandleAuthStateChanged(EMKHAuthState NewState, EMKHAuthState OldState);

	/**
	 * Steam notification (game thread): something changed about a user.
	 *
	 * @param SteamId      User who changed.
	 * @param ChangeFlags  EPersonaChange bits.
	 */
	void HandlePersonaStateChanged(uint64 SteamId, int32 ChangeFlags);

	/** Steam notification (game thread): the rich presence of a friend changed. */
	void HandleRichPresenceChanged(uint64 SteamId);

	/** Steam notification (game thread): a member entered or left a lobby of the local player. */
	void HandleLobbyMembersChanged();

	/** Steam notification (game thread): an avatar requested by GetAvatar finished downloading. */
	void HandleAvatarLoaded(uint64 SteamId);

	/**
	 * Steam notification (game thread): a friend invited the local player to a lobby of this game.
	 *
	 * @param SenderId  SteamID64 of the friend.
	 * @param LobbyId   SteamID64 of the lobby.
	 */
	void HandleLobbyInviteReceived(uint64 SenderId, uint64 LobbyId);

	/** Starts (or retries) the FindFriendSession of the invite being resolved. */
	void StartInviteLookup();

	/** FindFriendSession completion of the invite being resolved. */
	void HandleFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& Results);

	/**
	 * Ends the invite being resolved and reports it.
	 *
	 * @param bSuccess  True when the lobby of the invite was found.
	 * @param Lobby     Lobby found (invalid on failure).
	 */
	void FinishInviteLookup(bool bSuccess, const FMKHLobbySearchResult& Lobby);

	/** Forgets the cached avatar of a user and tells the UI to ask again. */
	void InvalidateAvatar(uint64 SteamId);

	/**
	 * Converts a loaded Steam image into a transient texture.
	 *
	 * @param ImageHandle  Steam image handle (> 0).
	 * @return The texture, or null when Steam cannot read the image.
	 */
	UTexture2D* CreateAvatarTexture(int32 ImageHandle);

	/** Publishes (true) or removes (false) the rich presence key that marks the player as in Makhia. */
	static void SetInMakhiaPresence(bool bInMakhia);

	// ============================================================
	// Properties
	// ============================================================

	/** Avatars converted so far, by NetId. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> Avatars;

private:
	/** Steam notifications, registered while the provider lives (null without Steam). */
	TSharedPtr<FMKHSteamSocialCallbacks> Callbacks;

	/** Users whose avatar was asked to Steam and is not converted yet. */
	TSet<uint64> PendingAvatarRequests;

	/** Invite being resolved; empty InviteId when none. */
	FMKHLobbyInvite PendingInvite;

	/** Completion of the invite being resolved. */
	FMKHLobbyInviteResolved PendingInviteResolved;

	/** FindFriendSession attempts made for the invite being resolved. */
	int32 InviteLookupAttempts = 0;

	/** FindFriendSession notification handle, bound only while an invite is being resolved. */
	FDelegateHandle FindFriendSessionHandle;

	/** Auth state notification handle. */
	FDelegateHandle AuthStateHandle;

	/** True between StartService and StopService. */
	bool bIsServiceRunning = false;
};
