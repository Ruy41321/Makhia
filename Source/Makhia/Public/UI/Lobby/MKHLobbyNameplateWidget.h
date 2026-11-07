// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "MKHLobbyNameplateWidget.generated.h"

class APlayerState;
class UBorder;
class UImage;
class UTextBlock;

/**
 * Nameplate shown above a lobby player platform: player name, career title on a tab over the top edge, ready state
 * chip (dot + label) and a
 * highlighted frame for the local player. The ready chip pulses when the state changes.
 *
 * BP_LobbyPlayerPlatform feeds it from its replicated player info (SetPlayer); the Blueprint (W_PlayerStatus)
 * owns the layout, the C++ owns texts, colours and the frame. Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHLobbyNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a player on the nameplate.
	 *
	 * @param PlayerName   Name to display (shown as is, never translated).
	 * @param bIsReady     Ready state of the player.
	 * @param PlayerState  Player state of the platform owner; used only to recognise the local player (may be null).
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Nameplate")
	void SetPlayer(const FString& PlayerName, bool bIsReady, const APlayerState* PlayerState);

	/**
	 * Shows the career title of the player.
	 *
	 * @param Title         Title in this machine's language; empty hides the tab.
	 * @param bHighestTitle  True for the highest title of the career: the tab takes its own colours.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Nameplate")
	void SetTitle(const FText& Title, bool bHighestTitle);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Applies the ready label and colour to the status chip. */
	void ApplyReadyLook() const;

	/** Applies the local or remote frame brush. */
	void ApplyFrameLook() const;

	/** Advances the status chip pulse and writes its scale. */
	void UpdatePulse(float DeltaSeconds);

	/**
	 * Returns the ready state to show: the player's own, or Ready for the local player while a match against the
	 * bot is pending (UMKHLobbySubsystem::IsOfflineMatchPending, a local wait that is not a real ready state).
	 */
	bool ComputeShownReady() const;

	/** Shows ComputeShownReady when it changed, with the pulse. */
	void RefreshShownReady();

	/**
	 * UMKHLobbySubsystem callback: the wait before a match against the bot started or stopped.
	 *
	 * @param bPending  True while the match is pending.
	 */
	void HandleOfflineMatchPendingChanged(bool bPending);

	/** Returns true when PlayerState belongs to the player owning this widget. */
	bool IsLocalPlayerState(const APlayerState* PlayerState) const;

	/** Shows TalkingIcon while the voice chat reports the player as talking (UMKHVoiceChatSubsystem). */
	void RefreshTalking() const;

	// ============================================================
	// Properties - Look
	// ============================================================

	/** Colour of the status dot and label when the player is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateColor ReadyColor = FSlateColor(FLinearColor(0.584f, 0.309f, 0.048f));

	/** Colour of the status dot and label when the player is not ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateColor NotReadyColor = FSlateColor(FLinearColor(0.309f, 0.249f, 0.195f));

	/** Frame of the local player's nameplate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateBrush LocalFrameBrush;

	/** Frame of the other players' nameplates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateBrush RemoteFrameBrush;

	/** Colour of the title tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FLinearColor TitleTabColor = FLinearColor(0.156f, 0.102f, 0.035f);

	/** Colour of the title text on the tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateColor TitleTextColor = FSlateColor(FLinearColor(0.922f, 0.838f, 0.686f));

	/** Colour of the title tab for the highest title of the career. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FLinearColor HighestTitleTabColor = FLinearColor(0.584f, 0.366f, 0.107f);

	/** Colour of the title text on the tab for the highest title of the career. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate")
	FSlateColor HighestTitleTextColor = FSlateColor(FLinearColor(0.023f, 0.014f, 0.008f));

	/** Length of the status chip pulse played when the ready state changes, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate", meta = (ClampMin = "0.0"))
	float PulseDuration = 0.35f;

	/** Peak scale of the status chip during the pulse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Nameplate", meta = (ClampMin = "1.0"))
	float PulseScale = 1.18f;

	/** Designer preview only: shows the ready look. */
	UPROPERTY(EditAnywhere, Category = "Lobby|Nameplate|Preview")
	bool bPreviewReady = false;

	/** Designer preview only: shows the local player frame. */
	UPROPERTY(EditAnywhere, Category = "Lobby|Nameplate|Preview")
	bool bPreviewLocal = false;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Shows the player name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	/** Optional tab over the top edge of the frame, holding TitleText. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TitleTab;

	/** Optional career title of the player. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Shows "Ready" / "Not ready". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Dot next to StatusText, tinted with the ready colour. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> StatusDot;

	/** Container of the dot and the label; scaled by the pulse. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatusChip;

	/** Nameplate frame; takes LocalFrameBrush or RemoteFrameBrush. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Frame;

	/** Optional speaker with waves, shown while the player talks in the voice chat. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> TalkingIcon;

private:
	/** Ready state of the player, as last given to SetPlayer. */
	bool bPlayerReady = false;

	/** Ready state currently shown. */
	bool bReady = false;

	/** True when the nameplate shows the local player. */
	bool bLocal = false;

	/** NetId of the player shown, for the voice chat talking state; empty when unknown. */
	FString VoicePlayerId;

	/** False until the first SetPlayer, so the first state shown does not pulse. */
	bool bHasPlayer = false;

	/** Seconds left in the current pulse (0 when idle). */
	float PulseTimeLeft = 0.f;
};
