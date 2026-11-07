// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHSettingsPageWidget.generated.h"

/**
 * Base class for a single page hosted by UMKHSettingsScreenWidget (Keyboard, Audio, Video, ...).
 *
 * The screen owns the navigation and the action bar; a page only owns its own settings model.
 * Pages are told when they become visible or hidden and are asked to apply, revert or reset
 * their own values, so the action bar never needs to know which page is active.
 *
 * Backing stores differ per page on purpose: audio lives in UMKHGameUserSettings
 * (GameUserSettings.ini), key bindings live in UEnhancedInputUserSettings (a per local player
 * SaveGame). Each page therefore persists itself instead of sharing a global save path.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSettingsPageWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (called by UMKHSettingsScreenWidget)
	// ============================================================

	/** Called every time this page becomes the visible one. Use it to rebuild from the current model. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Page")
	void ActivatePage();

	/** Called every time this page stops being the visible one. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Page")
	void DeactivatePage();

	/** Writes the page values to their backing store. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Page")
	virtual void ApplyChanges();

	/** Drops the unsaved edits and reloads the page from its backing store. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Page")
	virtual void RevertChanges();

	/** Restores every value on this page to its shipped default. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Page")
	virtual void ResetToDefaults();

	/**
	 * Reports whether the page holds edits that are not yet written to its backing store.
	 *
	 * @return True when Apply / Revert should be offered to the player.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Page")
	virtual bool HasPendingChanges() const;

	/**
	 * Reports whether the page is edited through the shared action bar (Restore Defaults / Revert /
	 * Apply). Pages that apply and persist every change on the spot, such as the audio sliders,
	 * return false so those buttons stay hidden while they are visible.
	 *
	 * @return True when the action bar buttons apply to this page.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Page")
	virtual bool UsesActionBar() const;

	/**
	 * Reports whether the page is in a state that must not be interrupted, such as the keyboard
	 * page waiting for the player to press a key. The screen refuses to close while this is true.
	 *
	 * @return True when a close request should be ignored.
	 */
	UFUNCTION(BlueprintPure, Category = "Settings|Page")
	virtual bool IsBlockingClose() const;

	/** Display name shown in the navigation rail and in the screen header. */
	UFUNCTION(BlueprintPure, Category = "Settings|Page")
	FText GetPageDisplayName() const { return PageDisplayName; }

	/** Broadcast whenever the page's pending-change state may have changed, so the screen can refresh its action bar. */
	DECLARE_MULTICAST_DELEGATE(FSettingsPageDirtyStateSignature);
	FSettingsPageDirtyStateSignature OnPageDirtyStateChanged;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Blueprint hook fired after the page becomes visible. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Page", meta = (DisplayName = "On Page Activated"))
	void BP_OnPageActivated();

	/** Blueprint hook fired after the page is hidden. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Page", meta = (DisplayName = "On Page Deactivated"))
	void BP_OnPageDeactivated();

	/** Native activation hook; override to rebuild the page contents. */
	virtual void NativeOnPageActivated();

	/** Native deactivation hook; override to release anything built on activation. */
	virtual void NativeOnPageDeactivated();

	/** Notifies the owning screen that this page's dirty state should be re-evaluated. */
	void NotifyDirtyStateChanged() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Name of this page as shown to the player. Set per Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings|Page")
	FText PageDisplayName;

	/** True while this page is the visible one. */
	UPROPERTY(BlueprintReadOnly, Category = "Settings|Page")
	bool bIsPageActive = false;
};
