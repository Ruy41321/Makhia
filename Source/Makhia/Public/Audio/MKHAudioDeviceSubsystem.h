// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHAudioDeviceSubsystem.generated.h"

/** One audio output device of the machine, as listed in the settings. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHAudioOutputDevice
{
	GENERATED_BODY()

	/** Platform id of the device (saved in the settings). */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Devices")
	FString DeviceId;

	/** Name of the device as the system shows it (a device name, never translated). */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Devices")
	FString DisplayName;

	/** True for the system default output. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Devices")
	bool bIsSystemDefault = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHAudioDevicesChangedSignature);

/**
 * Audio devices of the machine for the settings: the list of output devices, the output device of the game audio
 * (UAudioMixerBlueprintLibrary::SwapAudioOutputDevice, saved by id in UMKHGameUserSettings, back to the system
 * default when the saved device is gone) and the name of the microphone the voice chat records from (read only:
 * the native voice always captures from the Windows default recording device).
 *
 * The output device of the voice is chosen here too, but applied by the voice chat (UMKHVoiceChatSubsystem, voice
 * endpoint of the backend). Full model: see Documentation/VoiceChat.md and Documentation/AudioSystemArchitecture.md.
 */
UCLASS()
class MAKHIA_API UMKHAudioDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	/** Follows the level loads: the device list is read once a world with an audio device exists. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops following the level loads. */
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the device list was read again or the game output device changed. */
	UPROPERTY(BlueprintAssignable, Category = "Audio|Devices")
	FMKHAudioDevicesChangedSignature OnAudioDevicesChanged;

	/**
	 * Returns the audio device subsystem of the game instance of an object.
	 *
	 * @param WorldContextObject  Any object living in a game world.
	 * @return The subsystem, or null outside a game instance.
	 */
	static UMKHAudioDeviceSubsystem* Get(const UObject* WorldContextObject);

	/** Reads the output devices and the microphone name again (asynchronous; OnAudioDevicesChanged reports it). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	void RefreshDevices();

	/** Returns the output devices read last (empty until the first read completes). */
	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	TArray<FMKHAudioOutputDevice> GetOutputDevices() const { return OutputDevices; }

	/** Returns the saved output device of the game audio; empty for the system default. */
	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FString GetGameOutputDeviceId() const;

	/**
	 * Plays the game audio on another device and saves it.
	 *
	 * @param DeviceId  Id from GetOutputDevices; empty for the system default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	void SetGameOutputDeviceId(const FString& DeviceId);

	/** Returns the saved output device of the voice; empty for "same as the game". */
	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FString GetVoiceOutputDeviceId() const;

	/**
	 * Saves the output device of the voice (applied by the voice chat on its next tick).
	 *
	 * @param DeviceId  Id from GetOutputDevices; empty for "same as the game".
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Devices")
	void SetVoiceOutputDeviceId(const FString& DeviceId);

	/**
	 * Returns the name of the device the voice of the others must play on, for the voice endpoint.
	 *
	 * @return The name; empty for "same as the game", when the saved device is the game's or is not connected.
	 */
	FString ResolveVoiceOutputDeviceName() const;

	/**
	 * Returns the name of the device the voice stream plays on: the separate voice device, else the game's.
	 *
	 * @param bAllowSeparateDevice  False when the backend has no separate voice device: always the game's.
	 * @return The name; empty for the system default output.
	 */
	FString ResolveVoicePlaybackDeviceName(bool bAllowSeparateDevice) const;

	/** Returns the name of the Windows default recording device the voice chat captures from; empty when unknown. */
	UFUNCTION(BlueprintPure, Category = "Audio|Devices")
	FString GetInputDeviceName() const { return InputDeviceName; }

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Level loaded: reads the devices (and applies the saved game output device) the first time. */
	void HandlePostLoadMap(UWorld* World);

	/**
	 * The audio mixer listed its output devices.
	 *
	 * @param AvailableDevices  Every output device of the machine.
	 */
	UFUNCTION()
	void HandleOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices);

	/**
	 * The audio mixer answered a device swap.
	 *
	 * @param SwapResult  Requested and current device.
	 */
	UFUNCTION()
	void HandleDeviceSwapCompleted(const FSwapAudioOutputResult& SwapResult);

	/** Swaps the game audio to the saved device, or to the system default when it is not connected. */
	void ApplyGameOutputDevice();

	/**
	 * Returns a listed device.
	 *
	 * @param DeviceId  Platform id.
	 * @return The device, or null when it is not connected.
	 */
	const FMKHAudioOutputDevice* FindDevice(const FString& DeviceId) const;

	/** Reads the name of the default recording device. */
	void ReadInputDeviceName();

	// ============================================================
	// Properties
	// ============================================================

	/** Output devices read last. */
	UPROPERTY(Transient)
	TArray<FMKHAudioOutputDevice> OutputDevices;

private:
	/** Name of the default recording device. */
	FString InputDeviceName;

	/** Device id the game audio was last swapped to (empty: system default); unset before the first swap. */
	TOptional<FString> AppliedGameDeviceId;

	/** Handle of the level load notification. */
	FDelegateHandle PostLoadMapHandle;

	/** True once the devices were read at least once. */
	bool bHasReadDevices = false;
};
