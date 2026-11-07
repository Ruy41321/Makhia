// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/MKHInputDeviceTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MKHActiveInputDeviceSubsystem.generated.h"

class FMKHInputDeviceProcessor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHActiveInputDeviceChangedSignature, EMKHInputDevice, NewDevice);

/**
 * Remembers which device family the local player used last (keyboard and mouse, or gamepad), so
 * on-screen key prompts show the keys of the device actually in hand.
 *
 * Fed by a Slate input pre-processor, which sees every input before the game or the UI consumes it:
 * the device switches in menus as well as in game. Only the switch is broadcast, never every input.
 * See Documentation/SettingsArchitecture.md ("Key prompts").
 */
UCLASS()
class MAKHIA_API UMKHActiveInputDeviceSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Registers the input pre-processor (skipped when Slate is not running, e.g. dedicated server). */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unregisters the input pre-processor. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface
	// ============================================================

	/** Device family the player used last; keyboard and mouse until the first gamepad input. */
	UFUNCTION(BlueprintPure, Category = "Input|Device")
	EMKHInputDevice GetActiveInputDevice() const { return ActiveDevice; }

	/**
	 * Records an input coming from a device family and broadcasts when it differs from the current one.
	 *
	 * @param Device  Device family of the input just received.
	 */
	void ReportInput(EMKHInputDevice Device);

	/** Broadcast when the player switches between keyboard/mouse and gamepad. */
	UPROPERTY(BlueprintAssignable, Category = "Input|Device")
	FMKHActiveInputDeviceChangedSignature OnActiveInputDeviceChanged;

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Device family the player used last. */
	EMKHInputDevice ActiveDevice = EMKHInputDevice::KeyboardMouse;

	/** Slate pre-processor feeding ReportInput. */
	TSharedPtr<FMKHInputDeviceProcessor> InputProcessor;
};
