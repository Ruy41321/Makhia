// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "UI/MKHSystemWidget.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "MKHKeybindRowWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * One remappable action in a keybind settings list: its display name and its key slots for one
 * device family (two for keyboard and mouse, one for the controller).
 *
 * The row holds no state of its own; RefreshFromProfile() re-reads the mapping straight from the
 * active key profile, so the page can rebuild every row after any change without tracking deltas.
 *
 * The Blueprint subclass provides ActionNameText, PrimaryKeyButton and PrimaryKeyText; the
 * secondary slot and the reset button are optional so a compact layout stays possible.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHKeybindRowWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Binds this row to one mappable action.
	 *
	 * @param InMappingName   Unique mapping name from the action's Player Mappable Key Settings.
	 * @param InDisplayName   Player-facing action name.
	 * @param InDevice        Device family whose bindings the row shows; the controller has no Secondary slot.
	 */
	void SetupRow(FName InMappingName, const FText& InDisplayName, EMKHInputDevice InDevice);

	/** Re-reads the slots from the active key profile and restyles the row. */
	void RefreshFromProfile();

	/** Mapping name this row edits. */
	UFUNCTION(BlueprintPure, Category = "Settings|Keybinds")
	FName GetMappingName() const { return MappingName; }

	/** Broadcast when the player clicks one of the two key slots. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FKeybindSlotClickedSignature, FName /*MappingName*/, EPlayerMappableKeySlot /*Slot*/);
	FKeybindSlotClickedSignature OnSlotClicked;

	/** Broadcast when the player asks to restore this action's default keys. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FKeybindRowResetSignature, FName /*MappingName*/);
	FKeybindRowResetSignature OnResetClicked;

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

	/**
	 * Blueprint hook letting the row restyle a slot that differs from its shipped default.
	 *
	 * @param KeySlot       Slot being described.
	 * @param bIsCustomized True when the player changed this slot.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings|Keybinds", meta = (DisplayName = "On Slot State Changed"))
	void BP_OnSlotStateChanged(EPlayerMappableKeySlot KeySlot, bool bIsCustomized);

	/** Click handlers; UButton delegates must be UFUNCTIONs. */
	UFUNCTION()
	void HandlePrimaryClicked();

	UFUNCTION()
	void HandleSecondaryClicked();

	UFUNCTION()
	void HandleResetClicked();

	/**
	 * Writes one slot's label and customization state.
	 *
	 * @param KeySlot    Slot to update.
	 * @param SlotText   Label widget of that slot; ignored when null.
	 */
	void RefreshSlot(EPlayerMappableKeySlot KeySlot, UTextBlock* SlotText);

	// ============================================================
	// Properties
	// ============================================================

	/** Action name label. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> ActionNameText;

	/** Primary slot button and label. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UButton> PrimaryKeyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> PrimaryKeyText;

	/** Secondary slot button and label. Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> SecondaryKeyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> SecondaryKeyText;

	/** Per-row reset button. Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UButton> ResetButton;

	/** Text shown in a slot with no key bound. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	FText UnboundSlotText = NSLOCTEXT("MKHSettings", "UnboundSlot", "—");

	/** Keycap colour for a key still on its shipped default. Linear value of #EDE3D2. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	FLinearColor DefaultKeyColor = FLinearColor(0.84687f, 0.76815f, 0.64448f, 1.0f);

	/** Keycap colour for a key the player changed. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	FLinearColor CustomizedKeyColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Keycap colour for an empty slot. Linear value of #6B6055. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Keybinds")
	FLinearColor UnboundKeyColor = FLinearColor(0.14703f, 0.11697f, 0.09084f, 1.0f);

private:

	/** Mapping name this row edits. */
	FName MappingName = NAME_None;

	/** Device family whose bindings this row shows. */
	EMKHInputDevice Device = EMKHInputDevice::KeyboardMouse;
};
