// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/MKHFriendTypes.h"
#include "MKHLobbyInviteEntryWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * One pending lobby invite in the friend list: who sent it, with Join and Decline actions.
 *
 * Spawned by UMKHFriendListWidget once per invite. The Blueprint (W_LobbyInviteEntry) must provide
 * MessageText, JoinButton and DeclineButton. Join is disabled outside the lobby: accepting during a
 * match would drop the player out of it.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHLobbyInviteEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows an invite.
	 *
	 * @param InInvite  Invite to display and answer.
	 */
	void SetInvite(const FMKHLobbyInvite& InInvite);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Join button callback. */
	UFUNCTION()
	void HandleJoinClicked();

	/** Decline button callback. */
	UFUNCTION()
	void HandleDeclineClicked();

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** "{Name} invited you to their lobby". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** Joins the inviting player's lobby. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> JoinButton;

	/** Drops the invite. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> DeclineButton;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Invite currently displayed. */
	FMKHLobbyInvite Invite;
};
