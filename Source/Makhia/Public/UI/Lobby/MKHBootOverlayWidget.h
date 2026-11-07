// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHBootOverlayWidget.generated.h"

class UTextBlock;
enum class EMKHBootPhase : uint8;

/**
 * Full-screen cover of the lobby while the start-up login runs (UMKHGameInstance::GetBootPhase).
 *
 * AMKHLobbyHUD creates it only while the boot is in progress. It stays up during the travel to the hosted
 * lobby (the new lobby creates it again at once, so nothing flashes), then fades out and removes itself.
 * The Blueprint (W_BootOverlay) owns the layout. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHBootOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Game instance callback: updates the status line, and starts the fade-out once the boot is over.
	 *
	 * @param NewPhase  Phase entered.
	 */
	void HandleBootPhaseChanged(EMKHBootPhase NewPhase);

	/**
	 * Returns the status line of a boot phase.
	 *
	 * @param Phase  Phase to describe.
	 */
	static FText GetPhaseText(EMKHBootPhase Phase);

	// ============================================================
	// Properties
	// ============================================================

	/** Status line under the logo ("Connecting..."). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Seconds the overlay takes to fade out once the boot is over. */
	UPROPERTY(EditDefaultsOnly, Category = "Boot", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.35f;

private:
	/** True while the overlay fades out. */
	bool bFadingOut = false;

	/** Seconds spent fading out. */
	float FadeElapsed = 0.f;
};
