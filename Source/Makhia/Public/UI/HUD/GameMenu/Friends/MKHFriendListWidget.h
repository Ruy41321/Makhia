// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/MKHFriendTypes.h"
#include "MKHFriendListWidget.generated.h"

class UButton;
class UEditableTextBox;
class UListView;
class UMKHFriendsSubsystem;
class UMKHLobbyInviteEntryWidget;
class UPanelWidget;
class UTextBlock;
class UWidget;

/**
 * Friend list panel of the game menu, driven by UMKHFriendsSubsystem.
 *
 * Shows pending lobby invites, the friend list (UListView of UMKHFriendEntryWidget), an add-friend
 * row and a status message when the service is not ready. Depending on the backend capabilities it
 * also lists the recent players and offers a button that opens the platform friend list. It reads the
 * list again every time it becomes visible. The Blueprint (W_FriendList) must provide FriendListView
 * and StateText; the other bound widgets are optional so the layout can evolve.
 * See Documentation/GameMenuArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHFriendListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Asks the friends service to read the list again; the view updates when it answers. */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void RefreshFriendList();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Binds the subsystem notifications. */
	void BindSubsystem();

	/** Unbinds the subsystem notifications. */
	void UnbindSubsystem();

	/** Rebuilds the list, the counter and the status message from the subsystem. */
	void RefreshFriendsView();

	/** Rebuilds the lobby invites section from the subsystem. */
	void RefreshLobbyInvitesView();

	/** Rebuilds the recent players section and the platform friends button from the subsystem. */
	void RefreshPlatformView();

	/**
	 * Fills a list view with friend entries.
	 *
	 * @param ListView  Target list (optional widgets are skipped).
	 * @param Entries   Entries to show; an empty list collapses the view.
	 */
	void FillListView(UListView* ListView, const TArray<FMKHFriendInfo>& Entries);

	/** Returns the status message for a service state, or empty text when the list itself is enough. */
	FText GetStateMessage(EMKHFriendsServiceState State, bool bListIsEmpty) const;

	/** Returns the message shown after a friend request. */
	static FText GetFriendRequestMessage(EMKHFriendRequestResult Result);

	/** Sends the friend request typed in AddFriendNameBox. */
	void SubmitFriendRequest();

	/**
	 * Shows a feedback line under the add-friend row or the invites section.
	 *
	 * @param Target    Text widget to fill (optional widgets are skipped).
	 * @param Message   Text to show; empty collapses the widget.
	 * @param bIsError  Picks the error or the success colour.
	 */
	void ShowFeedback(UTextBlock* Target, const FText& Message, bool bIsError) const;

	/** Subsystem callback: list, presence or service state changed. */
	UFUNCTION()
	void HandleFriendsChanged();

	/** Subsystem callback: pending lobby invites changed. */
	UFUNCTION()
	void HandleLobbyInvitesChanged();

	/** Subsystem callback: a friend request completed. */
	UFUNCTION()
	void HandleFriendRequestResult(EMKHFriendRequestResult Result);

	/** Subsystem callback: accepting a lobby invite resolved the lobby or failed. */
	UFUNCTION()
	void HandleLobbyInviteAcceptResult(bool bWasSuccessful);

	/** Reads the list again whenever the panel is shown. */
	UFUNCTION()
	void HandleVisibilityChanged(ESlateVisibility InVisibility);

	/** Refresh button callback. */
	UFUNCTION()
	void HandleRefreshClicked();

	/** Add friend button callback. */
	UFUNCTION()
	void HandleAddFriendClicked();

	/** Platform friends button callback: opens the platform friend list in its overlay. */
	UFUNCTION()
	void HandlePlatformFriendsClicked();

	/** Name box callback: Enter sends the request. */
	UFUNCTION()
	void HandleNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Entry spawned once per pending lobby invite. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends")
	TSubclassOf<UMKHLobbyInviteEntryWidget> LobbyInviteEntryClass;

	/** Colour of positive feedback lines. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor SuccessColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Colour of error feedback lines. Linear value of #D0634A. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor ErrorColor = FLinearColor(0.63076f, 0.12477f, 0.06848f, 1.0f);

	/** Space left above every lobby invite entry after the first one. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style", meta = (ClampMin = "0"))
	float LobbyInviteSpacing = 6.0f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Friends and requests. Its entry class must derive from UMKHFriendEntryWidget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UListView> FriendListView;

	/** Message shown instead of (or above) the list when the service is not ready or the list is empty. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StateText;

	/** "{Online} / {Total} online" counter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Reads the list again. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	/** Row holding the add-friend controls; hidden while the service is not ready. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AddFriendSection;

	/** Display name of the player to add (backends with in-game friend requests only). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> AddFriendNameBox;

	/** Sends the friend request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> AddFriendButton;

	/** Outcome of the last friend request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AddFriendResultText;

	/** Section holding the lobby invites; hidden while there are none. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LobbyInvitesSection;

	/** Container the lobby invite entries are added to. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> LobbyInvitesContainer;

	/** Outcome of the last lobby invite accepted. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LobbyInviteResultText;

	/** Section holding the recent players; hidden without the capability or without recent players. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RecentPlayersSection;

	/** Players met in recent matches. Its entry class must derive from UMKHFriendEntryWidget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UListView> RecentPlayersListView;

	/** Opens the platform friend list in its overlay; shown only when the backend has an overlay. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlatformFriendsButton;

	/** Label of PlatformFriendsButton, filled by code ("Steam friends"). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlatformFriendsButtonText;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Subsystem the panel is bound to. */
	TWeakObjectPtr<UMKHFriendsSubsystem> FriendsSubsystem;
};
