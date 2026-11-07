// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Voice/MKHVoiceTypes.h"
#include "MKHVoiceTalkerEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * One player of the voice chat: avatar, name, animated "is talking" waves, mute button on the other players' rows
 * and a "voice unavailable" icon whose tooltip says why the server closed the voice (FMKHVoicePermissionPolicy).
 * Used by the Voice chat panel of the game menu (W_VoiceTalkerEntry) and, with the optional widgets left out, by the
 * talking list of the HUD (W_VoiceHUDTalker). Full model: see Documentation/VoiceChat.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVoiceTalkerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a player.
	 *
	 * @param InInfo  Row of the player (UMKHVoiceChatSubsystem::GetTalkers).
	 */
	void SetTalker(const FMKHVoiceTalkerInfo& InInfo);

	/** Returns the NetId of the player shown. */
	const FString& GetPlayerId() const { return Info.PlayerId; }

	/**
	 * Returns the tooltip explaining why the voice with a player is closed.
	 *
	 * @param Reason  Server decision.
	 * @return The text, empty for None.
	 */
	static FText GetBlockReasonText(EMKHVoiceBlockReason Reason);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	/** Binds the mute button and keeps the placeholder avatar brush. */
	virtual void NativeOnInitialized() override;

	/** Follows the avatar loads. */
	virtual void NativeConstruct() override;

	/** Stops following the avatar loads. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Mute button: mutes or unmutes the player (saved). */
	UFUNCTION()
	void HandleMuteClicked();

	/**
	 * A profile picture loaded.
	 *
	 * @param NetId  User whose picture changed.
	 */
	UFUNCTION()
	void HandleAvatarChanged(const FString& NetId);

	/** Shows the avatar of the player, or the placeholder. */
	void RefreshAvatar();

	/** Applies the mute button state, icon and tooltip. */
	void RefreshMuteButton();

	/** Applies the "voice unavailable" icon and its tooltip. */
	void RefreshUnavailableIcon();

	// ============================================================
	// Properties - Look
	// ============================================================

	/** Icon of the mute button while the player is heard (speaker with waves). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	TObjectPtr<UTexture2D> UnmutedIcon;

	/** Icon of the mute button while the player is muted (speaker crossed out). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	TObjectPtr<UTexture2D> MutedIcon;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Player name. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	/** Speaker with waves, shown and pulsing while the player talks. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TalkingIcon;

	/** Optional profile picture (platforms with avatars). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AvatarImage;

	/** Optional "voice unavailable" icon with the reason as tooltip. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> UnavailableIcon;

	/** Optional mute button (other players only). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MuteButton;

	/** Optional icon inside MuteButton. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> MuteIcon;

	/** Optional "(You)" label on the local player's row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LocalPlayerText;

private:
	/** Player shown. */
	FMKHVoiceTalkerInfo Info;

	/** Designer brush of AvatarImage, shown while the picture loads or when the user has none. */
	FSlateBrush PlaceholderAvatarBrush;
};
