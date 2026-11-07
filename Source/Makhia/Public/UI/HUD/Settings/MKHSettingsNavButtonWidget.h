// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHSettingsNavButtonWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

/**
 * One entry of the settings navigation rail.
 *
 * The Blueprint subclass places a UButton named NavButton and a UTextBlock named NavLabelText.
 * The owning UMKHSettingsScreenWidget spawns one of these per registered page and listens to
 * OnNavButtonClicked to switch the visible page.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSettingsNavButtonWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Fills the entry with the data of the page it points at.
	 *
	 * @param InPageIndex    Index of the page inside the screen's page array.
	 * @param InDisplayName  Label shown to the player.
	 */
	void SetupNavButton(int32 InPageIndex, const FText& InDisplayName);

	/**
	 * Marks this entry as the selected one and lets the Blueprint restyle itself.
	 *
	 * @param bInSelected  True when this entry's page is the visible one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Navigation")
	void SetSelected(bool bInSelected);

	/** Returns true when this entry points at the currently visible page. */
	UFUNCTION(BlueprintPure, Category = "Settings|Navigation")
	bool IsSelected() const { return bIsSelected; }

	/** Index of the page this entry points at. */
	UFUNCTION(BlueprintPure, Category = "Settings|Navigation")
	int32 GetPageIndex() const { return PageIndex; }

	/** Broadcast with the page index when the player clicks this entry. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FSettingsNavButtonClickedSignature, int32 /*PageIndex*/);
	FSettingsNavButtonClickedSignature OnNavButtonClicked;

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

	/** Blueprint hook fired when the selected state changes, so the entry can restyle itself. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Navigation", meta = (DisplayName = "On Selection Changed"))
	void BP_OnSelectionChanged(bool bInSelected);

	/** Forwards the button click to the owning screen. */
	UFUNCTION()
	void HandleButtonClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** Clickable area of the entry. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UButton> NavButton;

	/** Label of the entry. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> NavLabelText;

	/** Optional accent bar revealed while this entry is the selected one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UBorder> SelectionIndicator;

	/**
	 * Optional wash shown behind the label of the selected entry. Kept as a plain widget so the
	 * Blueprint is free to build it as a gradient, an image or a flat panel.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UWidget> SelectionBackground;

	/** Label colour while the entry is not selected. Linear value of the #97897A muted tone. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Navigation")
	FLinearColor NormalLabelColor = FLinearColor(0.30947f, 0.25016f, 0.19462f, 1.0f);

	/** Label colour while the entry is selected. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Navigation")
	FLinearColor SelectedLabelColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);


private:

	/** Index of the page this entry selects. */
	int32 PageIndex = INDEX_NONE;

	/** True while this entry is the selected one. */
	bool bIsSelected = false;
};
