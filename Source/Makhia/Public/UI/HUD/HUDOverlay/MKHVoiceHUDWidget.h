// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHVoiceHUDWidget.generated.h"

class UMKHKeyHintWidget;
class UMKHVoiceTalkerEntryWidget;
class UPanelWidget;

/**
 * Voice chat on the HUD of a match and of the lobby (W_VoiceHUD): the "transmitting" indicator of the local player,
 * with the push-to-talk key (W_KeyCap) in push-to-talk mode, and the other players who are talking now. Hidden while
 * the voice chat is not connected. Full model: see Documentation/VoiceChat.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHVoiceHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	/** Sets the push-to-talk action on the keycap. */
	virtual void NativeOnInitialized() override;

	/** Follows the voice state. */
	virtual void NativeConstruct() override;

	/** Stops following the voice state. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Voice state or players changed: shows the indicator and the talking list again. */
	UFUNCTION()
	void HandleVoiceChanged();

	/** Shows the transmitting indicator and the push-to-talk key. */
	void RefreshTransmitIndicator() const;

	/** Rebuilds the list of the other players talking now. */
	void RefreshTalkingList();

	// ============================================================
	// Properties
	// ============================================================

	/** Compact row Blueprint of a talking player (W_VoiceHUDTalker). */
	UPROPERTY(EditDefaultsOnly, Category = "Voice")
	TSubclassOf<UMKHVoiceTalkerEntryWidget> TalkerEntryClass;

	/** Shown while the local microphone transmits. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> TransmitIndicator;

	/** Optional push-to-talk key inside the indicator (push-to-talk mode only). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHKeyHintWidget> PushToTalkKeyCap;

	/** Container of the talking players (a vertical box). Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> TalkingList;

private:
	/** Rows of the talking list. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHVoiceTalkerEntryWidget>> TalkingEntries;
};
