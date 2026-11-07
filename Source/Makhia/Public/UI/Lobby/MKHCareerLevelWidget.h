// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHCareerLevelWidget.generated.h"

class UMKHButton;
class UMKHButtonStyle;
class UMKHCareerLevelWidget;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHCareerLevelSignature, UMKHCareerLevelWidget* /*Level*/);

/**
 * One level of the career in the lobby mode selector (parent of W_CareerLevel): state, opponent and the title it
 * awards. Reports its clicks to UMKHModeSelectorWidget. The Blueprint owns the layout.
 * Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHCareerLevelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a level.
	 *
	 * @param State     Line on top ("Next", "Locked", "Completed").
	 * @param Opponent  Who the level is played against.
	 * @param Reward    Title the level awards.
	 * @param bLocked   True when the level cannot be chosen yet.
	 */
	void SetLevel(const FText& State, const FText& Opponent, const FText& Reward, bool bLocked);

	/**
	 * Marks the level as the chosen one (button style).
	 *
	 * @param bSelected  True for the chosen level.
	 */
	void SetSelected(bool bSelected);

	/** Returns the button of the level, the widget that takes the focus. */
	UMKHButton* GetButton() const { return LevelButton; }

	/** Broadcast when the level is clicked. */
	FMKHCareerLevelSignature OnLevelClicked;

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

	/** Button callback: the level was clicked. */
	UFUNCTION()
	void HandleClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** Whole level: one MKH Button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Career Level")
	TObjectPtr<UMKHButton> LevelButton;

	/** Line on top ("Next", "Locked", "Completed"). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Career Level")
	TObjectPtr<UTextBlock> StateText;

	/** Who the level is played against. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Career Level")
	TObjectPtr<UTextBlock> OpponentText;

	/** Title the level awards. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Career Level")
	TObjectPtr<UTextBlock> RewardText;

	/** Button style of a level not chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career Level")
	TObjectPtr<UMKHButtonStyle> NormalStyle;

	/** Button style of the chosen level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Career Level")
	TObjectPtr<UMKHButtonStyle> SelectedStyle;
};
