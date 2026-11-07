// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateColor.h"
#include "MKHDuelTitleWidget.generated.h"

class UTextBlock;

/**
 * Career title of one duel participant on the scoreboard (parent of W_DuelTitle), shown under its name; hidden for a
 * participant without a title (IMKHDuelParticipant::GetParticipantCareerLevels). It follows the participant with a
 * timer, because a player and its career levels replicate at different times and a collapsed widget does not tick. The Blueprint owns the layout. Full model: see Documentation/OfflineModes.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHDuelTitleWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Shows the title of the participant of SlotIndex when it changed, or hides the line without a human participant. */
	void RefreshTitle();

	// ============================================================
	// Properties
	// ============================================================

	/** Scoreboard slot of the participant (0 or 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|Title", meta = (ClampMin = "0", ClampMax = "1"))
	int32 SlotIndex = 0;

	/** Colour of the title. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|Title")
	FSlateColor TitleColor = FSlateColor(FLinearColor(0.930f, 0.807f, 0.578f));

	/** Colour of the highest title of the career. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duel|Title")
	FSlateColor HighestTitleColor = FSlateColor(FLinearColor(0.807f, 0.527f, 0.102f));

	/** Shows the title. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

private:
	/** Seconds between two checks of the player state. */
	static constexpr float RefreshInterval = 0.5f;

	/** Repeats RefreshTitle while the widget is on screen. */
	FTimerHandle RefreshTimer;

	/** Career levels shown now; INDEX_NONE while the line is hidden. */
	int32 ShownCareerLevels = INDEX_NONE;
};
