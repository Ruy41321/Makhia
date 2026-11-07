// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Online/MKHFriendTypes.h"
#include "MKHFriendEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

/** List view item wrapping one friend entry; recreated on every friend list refresh. */
UCLASS(BlueprintType)
class MAKHIA_API UMKHFriendListItem : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Friend shown by the entry widget. */
	UPROPERTY(BlueprintReadOnly, Category = "Friends")
	FMKHFriendInfo Info;
};

/**
 * One row of the friend list: avatar, presence dot, name, status line and the actions that fit the entry
 * (invite to lobby for friends playing Makhia, accept / reject for received requests, or a platform button that
 * answers a request or adds a recent player in the platform overlay when the backend keeps those out of the game).
 *
 * Used as the entry class of the friend list's UListViews (friends and recent players). The Blueprint
 * (W_FriendEntry) must provide NameText, StatusText and PresenceIndicator; the avatar and the buttons are optional.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHFriendEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin IUserObjectListEntry Interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~ End IUserObjectListEntry Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Updates every visual element from Info. */
	void RefreshEntry();

	/** Shows the buttons that apply to the entry and hides the others. */
	void RefreshActions();

	/** Shows the avatar of the entry, the placeholder while it loads, nothing without the capability. */
	void RefreshAvatar();

	/** Subsystem callback: an avatar finished loading; refreshes it when it is this entry's. */
	UFUNCTION()
	void HandleAvatarChanged(const FString& NetId);

	/** Returns the status line for the entry (presence, or the pending request state). */
	FText GetStatusText() const;

	/**
	 * Returns the status line of a friend in Makhia from the activity they publish: lobby with its player count, or
	 * match with round and score (the friend's score first); "Playing Makhia" when nothing is published.
	 */
	FText GetActivityText() const;

	/**
	 * Puts the friend's career title in front of a status of a friend in the game.
	 *
	 * @param Status  Status line to complete.
	 * @return The title and the status; the status alone without the career.
	 */
	FText AddCareerTitle(const FText& Status) const;

	/** Returns the presence dot colour for the entry. */
	FLinearColor GetPresenceColor() const;

	/** Invite button callback. */
	UFUNCTION()
	void HandleInviteClicked();

	/** Accept button callback. */
	UFUNCTION()
	void HandleAcceptClicked();

	/** Reject button callback. */
	UFUNCTION()
	void HandleRejectClicked();

	/** Platform button callback: answers the request or adds the recent player in the platform overlay. */
	UFUNCTION()
	void HandlePlatformClicked();

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Dot colour of a friend playing Makhia. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor InMakhiaColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Dot colour of a friend online elsewhere. Linear value of #6FAF6A. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor OnlineColor = FLinearColor(0.15896f, 0.42869f, 0.14126f, 1.0f);

	/** Dot colour of a friend marked away. Linear value of #C7A04A. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor AwayColor = FLinearColor(0.57112f, 0.35153f, 0.06848f, 1.0f);

	/** Dot colour of an offline friend. Linear value of #5B544C. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor OfflineColor = FLinearColor(0.10461f, 0.08865f, 0.07036f, 1.0f);

	/** Dot colour of a pending request. Linear value of #B8743A. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor RequestColor = FLinearColor(0.47932f, 0.17789f, 0.04231f, 1.0f);

	/** Dot colour of a recent player (not a friend). Linear value of #8C8173. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style")
	FLinearColor RecentPlayerColor = FLinearColor(0.26225f, 0.21952f, 0.17144f, 1.0f);

	/** Render opacity of offline entries, so online friends stand out. */
	UPROPERTY(EditDefaultsOnly, Category = "Friends|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float OfflineOpacity = 0.55f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Friend's display name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	/** Presence or request state line under the name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Small dot tinted with the presence colour. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PresenceIndicator;

	/** Invites the friend to the current lobby. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> InviteButton;

	/** Accepts a received friend request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> AcceptButton;

	/** Rejects a received friend request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RejectButton;

	/** Profile picture; keeps its designer brush as placeholder while the picture loads. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AvatarImage;

	/** Opens the platform overlay to answer a received request or to add a recent player. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlatformButton;

	/** Label of PlatformButton, filled by code ("Respond on Steam", "Add friend"). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlatformButtonText;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Entry currently displayed. */
	FMKHFriendInfo Info;

	/** Designer brush of AvatarImage, shown while the picture loads or when the user has none. */
	FSlateBrush PlaceholderAvatarBrush;
};
