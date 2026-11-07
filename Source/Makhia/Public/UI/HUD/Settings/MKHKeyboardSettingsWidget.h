// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "UI/HUD/Settings/MKHSettingsPageWidget.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "MKHKeyboardSettingsWidget.generated.h"

class UPanelWidget;
class UMKHKeybindRowWidget;
class UMKHKeybindCategoryWidget;
class UMKHKeyCaptureWidget;
class UInputAction;

/**
 * One slot of one mapping as it was when the page opened, used to implement Revert.
 *
 * The engine tracks "changed from the IMC default" but not "changed since the last save", so the
 * page snapshots the saved state itself instead of inferring it.
 */
USTRUCT()
struct FMKHKeybindSnapshotEntry
{
	GENERATED_BODY()

	/** Mapping the entry belongs to. */
	UPROPERTY()
	FName MappingName = NAME_None;

	/** Slot the key was bound to. */
	UPROPERTY()
	EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First;

	/** Key bound at snapshot time; invalid when the slot was empty. */
	UPROPERTY()
	FKey Key;
};

/**
 * A display category and the mappings shown under it.
 *
 * A plain array rather than a map keyed by FText: FText has no hash, and the handful of
 * categories a game ships with makes a linear lookup cheaper than the alternative anyway.
 */
struct FMKHKeybindCategoryGroup
{
	/** Section label, taken from the actions' Display Category. */
	FText DisplayName;

	/** Mappings belonging to this category. */
	TArray<FName> MappingNames;
};

/**
 * Settings page listing every remappable action of one device family (InputDevice), grouped by the
 * Display Category authored on the action's Player Mappable Key Settings. W_KeyboardSettings shows the
 * keyboard and mouse bindings, W_ControllerSettings the gamepad ones; an action appears on the
 * controller page only when a mapping context gives it a default gamepad key.
 *
 * The page is a view over UEnhancedInputUserSettings: rows are rebuilt from the active key profile
 * and never cache a binding of their own. A rebind writes through MapPlayerKey() immediately, so
 * the new key is live in game right away; Apply only flushes the profile to disk. Both pages share
 * that profile, so leaving a page rolls its unsaved rebinds back.
 *
 * Actions listed in ExcludedMappingNames are hidden. IA_Look lives there by default: it is a 2D
 * axis driven by the mouse and cannot be expressed as a single key.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHKeyboardSettingsWidget : public UMKHSettingsPageWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (UMKHSettingsPageWidget overrides)
	// ============================================================

	/** Flushes the key profile to the local player's save. */
	virtual void ApplyChanges() override;

	/** Restores the snapshot taken when the page opened, dropping every rebind made since. */
	virtual void RevertChanges() override;

	/** Restores every action of this page's device to its shipped key; the other device is left alone. */
	virtual void ResetToDefaults() override;

	/** True while a rebind has not been written to disk yet. */
	virtual bool HasPendingChanges() const override;

	/** True while the capture modal is up: Esc belongs to the modal, not to the screen. */
	virtual bool IsBlockingClose() const override;

	/** Rebuilds the whole list from the active key profile. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Keybinds")
	void RebuildKeybindList();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UMKHSettingsPageWidget Interface
	virtual void NativeOnPageActivated() override;
	virtual void NativeOnPageDeactivated() override;
	//~ End UMKHSettingsPageWidget Interface

	//~ Begin UUserWidget Interface
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Collects the mappings to display, grouped by display category and sorted for a stable order.
	 *
	 * @param OutCategories  Filled with one entry per display category, sorted by label.
	 */
	void CollectMappingsByCategory(TArray<FMKHKeybindCategoryGroup>& OutCategories) const;

	/**
	 * Spawns the header and the rows of one category into the list container.
	 *
	 * @param CategoryLabel  Section label.
	 * @param MappingNames   Mappings shown under that label.
	 */
	void BuildCategorySection(const FText& CategoryLabel, const TArray<FName>& MappingNames);

	/** Row callback: opens the capture modal for the clicked slot. */
	void HandleSlotClicked(FName InMappingName, EPlayerMappableKeySlot InSlot);

	/** Row callback: restores one action's keys on this page's device to their defaults. */
	void HandleRowResetClicked(FName InMappingName);

	/** Capture callback: a key was produced; checks for conflicts before writing it. */
	void HandleKeyCaptured(const FKey& CapturedKey);

	/** Capture callback: the player confirmed a key that was already bound elsewhere. */
	void HandleConflictConfirmed(const FKey& CapturedKey);

	/** Capture callback: the player asked to leave the slot empty. */
	void HandleClearRequested();

	/** Capture callback: the rebind was aborted. */
	void HandleCaptureCancelled();

	/**
	 * Writes a key into the pending slot. Whichever binding already owned the key is cleared on
	 * keyboard and mouse, and receives the pending slot's previous key on the controller: a gamepad
	 * slot is never left empty, because the engine falls back to the default key for an empty one.
	 *
	 * @param NewKey  Key to bind; an invalid key clears the slot instead.
	 */
	void ApplyPendingRebind(const FKey& NewKey);

	/**
	 * Gives the action that owns the pending key its replacement: nothing on keyboard and mouse,
	 * the pending slot's previous key on the controller.
	 *
	 * @param NewKey       Key being moved to the pending slot.
	 * @param PreviousKey  Key the pending slot held before the rebind.
	 */
	void ReleaseConflictingKey(const FKey& NewKey, const FKey& PreviousKey);

	/**
	 * Player-facing explanation shown when the captured key already belongs to another action.
	 *
	 * @param Key           Captured key.
	 * @param ConflictName  Mapping that owns the key.
	 * @return The message for this page's device.
	 */
	FText BuildConflictMessage(const FKey& Key, FName ConflictName) const;

	/**
	 * Writes every mapping of this page's device in a row back to its default key.
	 *
	 * @param InMappingName  Row to reset.
	 */
	void ResetRowToDefaults(FName InMappingName);

	/**
	 * Finds the mapping that already uses a key, ignoring the slot being edited.
	 *
	 * @param Key                Key to look up.
	 * @param OutConflictName    Mapping name that owns the key, when one does.
	 * @return True when another mapping already uses the key.
	 */
	bool FindConflictingMapping(const FKey& Key, FName& OutConflictName) const;

	/**
	 * Chord required by a mapping on this page's device (e.g. Block for the controller skills).
	 *
	 * @param InMappingName  Mapping to inspect.
	 * @return The chord action, or nullptr when the mapping is not chorded.
	 */
	const UInputAction* GetMappingChordAction(FName InMappingName) const;

	/** Tears the capture modal down and unbinds its delegates. */
	void CloseCaptureModal();

	/** Player-facing name of a mapping, falling back to the mapping name when none is authored. */
	FText GetMappingDisplayName(FName InMappingName) const;

	/**
	 * Position of a category in CategoryOrder.
	 *
	 * @param CategoryLabel  Category to look up.
	 * @return Its index, or MAX_int32 when the designer did not pin it.
	 */
	int32 GetCategoryRank(const FText& CategoryLabel) const;

	/**
	 * Position of a mapping in MappingOrder.
	 *
	 * @param InMappingName  Mapping to look up.
	 * @return Its index, or MAX_int32 when the designer did not pin it.
	 */
	int32 GetMappingRank(FName InMappingName) const;

	/** Captures the saved state of every mapping of this page's device, so Revert has something to restore. */
	void TakeSnapshot();

	/**
	 * Writes one key into one slot of this page's device in the active profile.
	 *
	 * @param InMappingName  Mapping to write.
	 * @param InSlot         Slot to write.
	 * @param NewKey         Key to bind; an invalid key clears the slot instead.
	 * @return True when the profile accepted the change.
	 */
	bool WriteMapping(FName InMappingName, EPlayerMappableKeySlot InSlot, const FKey& NewKey);

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Device family whose bindings this page lists and edits. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	EMKHInputDevice InputDevice = EMKHInputDevice::KeyboardMouse;

	/** Row Blueprint spawned once per remappable action. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TSubclassOf<UMKHKeybindRowWidget> KeybindRowClass;

	/** Section header Blueprint spawned once per display category. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TSubclassOf<UMKHKeybindCategoryWidget> CategoryHeaderClass;

	/** Modal shown while waiting for the player to press a key. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TSubclassOf<UMKHKeyCaptureWidget> KeyCaptureWidgetClass;

	/** Mappings never shown on this page. IA_Look is excluded by default: a 2D axis has no keycap. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TArray<FName> ExcludedMappingNames;

	/**
	 * Categories in the order they should be listed, by display name (e.g. "Movement", "Combat").
	 * Matched against the English source text, so it holds in every language; categories not named
	 * here follow, in alphabetical order.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TArray<FText> CategoryOrder;

	/**
	 * Actions in the order they should be listed inside their own category, by mapping name
	 * (e.g. IA_MoveForward). Actions not named here follow, alphabetically by display name.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	TArray<FName> MappingOrder;

	/** Label used for actions whose Player Mappable Key Settings carry no display category. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	FText UncategorizedLabel = NSLOCTEXT("MKHSettings", "UncategorizedKeybinds", "Other");

	/** Z-order the capture modal is added to the viewport with. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	int32 CaptureModalZOrder = 1000;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Container the category headers and rows are added to. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> KeybindListContainer;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Rows currently in the list, in display order. */
	UPROPERTY()
	TArray<TObjectPtr<UMKHKeybindRowWidget>> RowWidgets;

	/** Capture modal while a rebind is in progress. */
	UPROPERTY()
	TObjectPtr<UMKHKeyCaptureWidget> ActiveCaptureWidget;

	/** Mapping being rebound while the capture modal is open. */
	FName PendingMappingName = NAME_None;

	/** Slot being rebound while the capture modal is open. */
	EPlayerMappableKeySlot PendingSlot = EPlayerMappableKeySlot::First;

	/** Mapping that owns the key the player is being asked to steal. */
	FName PendingConflictName = NAME_None;

	/** Saved state captured when the page opened, restored by Revert. */
	UPROPERTY()
	TArray<FMKHKeybindSnapshotEntry> SavedSnapshot;

	/** True once a rebind happened that has not been written to disk yet. */
	bool bHasUnsavedRebinds = false;
};
