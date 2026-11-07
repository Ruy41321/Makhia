// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHLobbyNoticeWidget.generated.h"

class UTextBlock;

/**
 * Non-blocking lobby toast ("Luigi left the lobby"): fades in, stays a few seconds, fades out and removes itself.
 * It never takes focus nor catches clicks. AMKHLobbyHUD creates it for the notices of UMKHLobbySubsystem; the
 * Blueprint (W_LobbyNotice) owns the layout. Full model: see Documentation/LoadingScreen.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHLobbyNoticeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a notice and restarts the fade in / hold / fade out sequence.
	 *
	 * @param Message  Notice to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Notice")
	void ShowNotice(const FText& Message);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Returns the opacity at a point of the sequence.
	 *
	 * @param Elapsed  Seconds since the notice was shown.
	 */
	float ComputeOpacity(float Elapsed) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Seconds the notice takes to appear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Notice", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.25f;

	/** Seconds the notice stays fully visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Notice", meta = (ClampMin = "0.0", Units = "s"))
	float HoldDuration = 4.f;

	/** Seconds the notice takes to disappear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|Notice", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.6f;

	/** Notice text. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

private:
	/** Seconds since the notice was shown. */
	float Elapsed = 0.f;
};
