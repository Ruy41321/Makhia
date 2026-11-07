// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHVoicePanelWidget.generated.h"

class UButton;
class UImage;
class UMKHButtonStyle;
class UMKHKeyHintWidget;
class UMKHVoiceTalkerEntryWidget;
class UPanelWidget;
class UTextBlock;
class UTexture2D;

/**
 * Voice chat panel at the top of the friends page of the game menu (W_VoicePanel, above W_FriendList), in the
 * Makhia look: title and current channel (Lobby / Match / Not connected), the microphone state ("Push-to-talk" with
 * the key as W_KeyCap, "Open mic", "Listen only", "Voice chat off"), two icon buttons (voice chat on/off, own
 * microphone mute for the session), a collapse arrow and the players in voice, the local player first. The two toggle
 * buttons switch to a red button style while their function is off (voice chat off, microphone muted).
 *
 * Reads and drives everything through UMKHVoiceChatSubsystem; works with the gamepad (MKH buttons, tooltips on the
 * focused button). Full model: see Documentation/VoiceChat.md and Documentation/GameMenuArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVoicePanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	/** Binds the buttons. */
	virtual void NativeOnInitialized() override;

	/** Follows the voice state and shows it. */
	virtual void NativeConstruct() override;

	/** Stops following the voice state. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Voice state changed: header, status and buttons. */
	UFUNCTION()
	void HandleVoiceStateChanged();

	/** Player list changed: rows. */
	UFUNCTION()
	void HandleTalkersChanged();

	/** Voice chat on/off button. */
	UFUNCTION()
	void HandleVoiceToggleClicked();

	/** Microphone mute button. */
	UFUNCTION()
	void HandleMicToggleClicked();

	/** Collapse arrow. */
	UFUNCTION()
	void HandleCollapseClicked();

	/** Writes the channel and the microphone state. */
	void RefreshHeader() const;

	/** Applies icons, tooltips, enabled state and on/off style of the two buttons. */
	void RefreshButtons() const;

	/**
	 * Gives a toggle button the look of its state.
	 *
	 * @param Button  Toggle button (an MKH Button; any other button is left as it is).
	 * @param bOn     True while the function of the button is on.
	 */
	void ApplyToggleStyle(UButton* Button, bool bOn) const;

	/** Creates, updates and removes the player rows to match the voice chat. */
	void RefreshTalkers();

	/** Shows or hides the panel content. */
	void ApplyCollapsed() const;

	/** Returns the text of the current channel. */
	FText GetChannelText() const;

	/** Returns the text of the microphone state; bOutShowKey is true when the push-to-talk key goes after it. */
	FText GetStatusText(bool& bOutShowKey) const;

	// ============================================================
	// Properties - Look
	// ============================================================

	/** Row Blueprint of a player (W_VoiceTalkerEntry). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	TSubclassOf<UMKHVoiceTalkerEntryWidget> TalkerEntryClass;

	/** Voice chat on (headset). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> VoiceOnIcon;

	/** Voice chat off (headset crossed out). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> VoiceOffIcon;

	/** Microphone on. */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> MicOnIcon;

	/** Microphone muted (crossed out). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> MicOffIcon;

	/** Arrow shown while the panel is open (click collapses). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> CollapseIcon;

	/** Arrow shown while the panel is collapsed (click expands). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Icons")
	TObjectPtr<UTexture2D> ExpandIcon;

	/** Style of a toggle button whose function is on (BS_IconButton). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Look")
	TObjectPtr<UMKHButtonStyle> ToggleOnStyle;

	/** Style of a toggle button whose function is off: voice chat off, microphone muted (BS_IconButtonOff, red). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Look")
	TObjectPtr<UMKHButtonStyle> ToggleOffStyle;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Current channel. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ChannelText;

	/** Microphone state. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Optional push-to-talk key after StatusText (W_KeyCap; its input action is set by the panel). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHKeyHintWidget> PushToTalkKeyCap;

	/** Voice chat on/off. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> VoiceToggleButton;

	/** Icon inside VoiceToggleButton. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> VoiceToggleIcon;

	/** Microphone mute. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> MicToggleButton;

	/** Icon inside MicToggleButton. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MicToggleIcon;

	/** Optional collapse arrow. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CollapseButton;

	/** Optional icon inside CollapseButton. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CollapseButtonIcon;

	/** Optional part hidden by the collapse arrow (status and players). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ContentBox;

	/** Container of the player rows (a vertical box). Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> TalkerList;

	/** Optional text shown when the backend has no voice chat. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnavailableText;

private:
	/** Player rows, in the order of UMKHVoiceChatSubsystem::GetTalkers. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHVoiceTalkerEntryWidget>> TalkerEntries;

	/** True while the content is collapsed (kept while the menu stays open). */
	bool bCollapsed = false;
};
