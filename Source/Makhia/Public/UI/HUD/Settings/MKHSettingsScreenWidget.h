// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHSettingsScreenWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UWidgetSwitcher;
class UMKHSettingsPageWidget;
class UMKHSettingsNavButtonWidget;

/**
 * Full screen settings shell: a navigation rail on the left, one page at a time in a
 * UWidgetSwitcher, and a shared action bar at the bottom.
 *
 * Pages are data, not code: fill SettingsPages with the page Blueprints to show and the screen
 * builds the rail, instantiates each page and routes Apply / Revert / Restore Defaults to
 * whichever page is visible. Adding Video or Gameplay later is one array entry, no C++ change.
 *
 * The Blueprint subclass must provide a UWidgetSwitcher named PageSwitcher and a panel named
 * NavButtonContainer; every other bound widget is optional so the layout can grow gradually.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSettingsScreenWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Makes one page visible, deactivating the previous one.
	 *
	 * @param PageIndex  Index into SettingsPages; out of range values are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	void ShowPage(int32 PageIndex);

	/** Writes the visible page's values to its backing store. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	void ApplyActivePage();

	/** Drops the visible page's unsaved edits. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	void RevertActivePage();

	/** Restores the visible page to its shipped defaults. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	void ResetActivePageToDefaults();

	/**
	 * Requests the screen to close, dropping whatever the visible page has not written to disk.
	 * Apply is the only thing that persists a change: leaving the screen any other way
	 * (Back, or the menu input action) rolls the page back to its last saved state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	void CloseSettings();

	/**
	 * Close request coming from outside: the game menu input action, or the Back button.
	 * The screen listens for neither raw keys nor focus changes, so an OS-level event such as
	 * closing the window is never mistaken for the player leaving the panel.
	 * Ignored while the visible page is in a state that must not be interrupted.
	 *
	 * @return True when the screen actually closed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings|Screen")
	bool RequestClose();

	/** Returns the page currently visible, or nullptr while the screen is still empty. */
	UFUNCTION(BlueprintPure, Category = "Settings|Screen")
	UMKHSettingsPageWidget* GetActivePage() const;

	/** Broadcast when the player asks to leave the settings screen. The HUD owns the teardown. */
	DECLARE_MULTICAST_DELEGATE(FSettingsScreenClosedSignature);
	FSettingsScreenClosedSignature OnSettingsClosed;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Takes the focus back on a click that lands on the screen's own empty space.
	 * Without this such a click moves the focus to the viewport and the focus-lost rule
	 * would close the screen the player is still using.
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Gamepad: B closes the screen like Esc, LB / RB show the previous / next page. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Instantiates every page in SettingsPages and adds it to the switcher. */
	void BuildPages();

	/** Spawns one navigation entry per page and wires its click delegate. */
	void BuildNavigationRail();

	/** Enables or disables the action bar buttons from the visible page's pending-change state. */
	void RefreshActionBar();

	/** Rolls the visible page back to its saved state when it holds unsaved edits. */
	void DiscardPendingChanges();

	/** Updates the header label with the visible page's display name. */
	void RefreshHeader() const;

	/**
	 * Shows the page next to the visible one, wrapping around, and moves the gamepad focus into it.
	 *
	 * @param Step  +1 for the next page, -1 for the previous one.
	 */
	void ShowAdjacentPage(int32 Step);

	/** Navigation callback: switches to the clicked page. */
	void HandleNavButtonClicked(int32 PageIndex);

	/** Action bar callback wrappers, needed because UButton delegates are dynamic. */
	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleRevertClicked();

	UFUNCTION()
	void HandleDefaultsClicked();

	UFUNCTION()
	void HandleBackClicked();

	/** Page callback: the visible page reports its pending-change state may have changed. */
	void HandlePageDirtyStateChanged();

	/** Binds the action bar buttons. */
	void BindActionBar();

	/** Unbinds everything this widget registered, on both the buttons and the pages. */
	void UnbindAll();

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Page Blueprints to host, in the order they appear in the navigation rail. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	TArray<TSubclassOf<UMKHSettingsPageWidget>> SettingsPages;

	/** Navigation entry Blueprint spawned once per page. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	TSubclassOf<UMKHSettingsNavButtonWidget> NavButtonClass;

	/** Footer text shown while the visible page has nothing to save. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	FText NoChangesText = NSLOCTEXT("MKHSettings", "NoChanges", "No changes");

	/** Footer text shown while the visible page holds edits that are not on disk yet. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	FText UnsavedChangesText = NSLOCTEXT("MKHSettings", "UnsavedChanges", "Unsaved changes");

	/** Colour of the footer label while there is nothing to save. Linear value of #97897A. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	FLinearColor IdleFooterColor = FLinearColor(0.30947f, 0.25016f, 0.19462f, 1.0f);

	/** Colour of the footer label while edits are pending. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	FLinearColor DirtyFooterColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Gap left around each navigation entry. Entries are spawned at runtime, so it lives here. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen")
	FMargin NavButtonSpacing = FMargin(0.0f, 0.0f, 0.0f, 6.0f);

	/** Page selected when the screen opens. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Screen", meta = (ClampMin = "0"))
	int32 DefaultPageIndex = 0;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Hosts one page at a time. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	/** Container the navigation entries are added to. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> NavButtonContainer;

	/** Optional footer label telling the player whether the page has unsaved edits. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> DirtyStateText;

	/** Optional header label showing the visible page's name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> PageTitleText;

	/** Optional action bar buttons. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> RevertButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> DefaultsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> BackButton;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Page instances, index-aligned with SettingsPages. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHSettingsPageWidget>> PageInstances;

	/** Navigation entries, index-aligned with PageInstances. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHSettingsNavButtonWidget>> NavButtons;

	/** Index of the visible page, or INDEX_NONE while the screen is empty. */
	int32 ActivePageIndex = INDEX_NONE;
};
