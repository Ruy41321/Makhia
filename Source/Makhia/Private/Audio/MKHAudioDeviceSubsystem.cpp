// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Audio/MKHAudioDeviceSubsystem.h"

#include "AudioCaptureCore.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "MKHLogChannels.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#if !UE_BUILD_SHIPPING
namespace MKHAudioDevicesDebug
{
	/** Returns the audio device subsystem of the world given to a console command. */
	static UMKHAudioDeviceSubsystem* FromWorld(const UWorld* World)
	{
		const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAudioDeviceSubsystem>() : nullptr;
	}

	/** Returns the id of the listed device at an index given on the command line, empty for -1 or a bad index. */
	static FString DeviceIdAt(const UMKHAudioDeviceSubsystem& Devices, const TArray<FString>& Args)
	{
		const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
		const TArray<FMKHAudioOutputDevice> List = Devices.GetOutputDevices();
		return List.IsValidIndex(Index) ? List[Index].DeviceId : FString();
	}

	static FAutoConsoleCommandWithWorldAndArgs ListCommand(TEXT("MKH.Audio.Devices"), TEXT("Logs the output devices and the microphone."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (const UMKHAudioDeviceSubsystem* Devices = FromWorld(World))
			{
				const TArray<FMKHAudioOutputDevice> List = Devices->GetOutputDevices();
				for (int32 Index = 0; Index < List.Num(); ++Index)
				{
					UE_LOG(LogMKHAudio, Display, TEXT("Audio devices: [%d] '%s'%s"), Index, *List[Index].DisplayName, List[Index].bIsSystemDefault ? TEXT(" (default)") : TEXT(""));
				}
				UE_LOG(LogMKHAudio, Display, TEXT("Audio devices: game '%s', voice '%s' -> '%s', microphone '%s'."), *Devices->GetGameOutputDeviceId(),
					*Devices->GetVoiceOutputDeviceId(), *Devices->ResolveVoiceOutputDeviceName(), *Devices->GetInputDeviceName());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GameCommand(TEXT("MKH.Audio.SetGameOutput"), TEXT("MKH.Audio.SetGameOutput <index|-1>: game output device."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMKHAudioDeviceSubsystem* Devices = FromWorld(World))
			{
				Devices->SetGameOutputDeviceId(DeviceIdAt(*Devices, Args));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs VoiceCommand(TEXT("MKH.Audio.SetVoiceOutput"), TEXT("MKH.Audio.SetVoiceOutput <index|-1>: voice output device."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMKHAudioDeviceSubsystem* Devices = FromWorld(World))
			{
				Devices->SetVoiceOutputDeviceId(DeviceIdAt(*Devices, Args));
			}
		}));
}
#endif

// ============================================================
// Lifecycle
// ============================================================

void UMKHAudioDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMKHAudioDeviceSubsystem::HandlePostLoadMap);
	ReadInputDeviceName();
}

void UMKHAudioDeviceSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHAudioDeviceSubsystem* UMKHAudioDeviceSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAudioDeviceSubsystem>() : nullptr;
}

void UMKHAudioDeviceSubsystem::RefreshDevices()
{
	const UWorld* World = GetGameInstance()->GetWorld();
	if (!IsValid(World) || !World->GetAudioDeviceRaw())
	{
		return;
	}
	ReadInputDeviceName();
	FOnAudioOutputDevicesObtained OnObtained;
	OnObtained.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMKHAudioDeviceSubsystem, HandleOutputDevicesObtained));
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(World, OnObtained);
}

FString UMKHAudioDeviceSubsystem::GetGameOutputDeviceId() const
{
	const UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	return IsValid(Settings) ? Settings->GetGameOutputDeviceId() : FString();
}

void UMKHAudioDeviceSubsystem::SetGameOutputDeviceId(const FString& DeviceId)
{
	UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(Settings) || Settings->GetGameOutputDeviceId() == DeviceId)
	{
		return;
	}
	UE_LOG(LogMKHAudio, Log, TEXT("Audio devices: game output set to '%s'."), DeviceId.IsEmpty() ? TEXT("system default") : *DeviceId);
	Settings->SetGameOutputDeviceId(DeviceId);
	Settings->SaveSettings();
	ApplyGameOutputDevice();
	OnAudioDevicesChanged.Broadcast();
}

FString UMKHAudioDeviceSubsystem::GetVoiceOutputDeviceId() const
{
	const UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	return IsValid(Settings) ? Settings->GetVoiceOutputDeviceId() : FString();
}

void UMKHAudioDeviceSubsystem::SetVoiceOutputDeviceId(const FString& DeviceId)
{
	UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings();
	if (!IsValid(Settings) || Settings->GetVoiceOutputDeviceId() == DeviceId)
	{
		return;
	}
	UE_LOG(LogMKHAudio, Log, TEXT("Audio devices: voice output set to '%s'."), DeviceId.IsEmpty() ? TEXT("same as the game") : *DeviceId);
	Settings->SetVoiceOutputDeviceId(DeviceId);
	Settings->SaveSettings();
	OnAudioDevicesChanged.Broadcast();
}

FString UMKHAudioDeviceSubsystem::ResolveVoiceOutputDeviceName() const
{
	const FString VoiceDeviceId = GetVoiceOutputDeviceId();
	const FMKHAudioOutputDevice* Device = VoiceDeviceId.IsEmpty() ? nullptr : FindDevice(VoiceDeviceId);
	if (!Device || Device->DeviceId == AppliedGameDeviceId.Get(FString()))
	{
		return FString();
	}
	return Device->DisplayName;
}

FString UMKHAudioDeviceSubsystem::ResolveVoicePlaybackDeviceName(bool bAllowSeparateDevice) const
{
	const FString VoiceDeviceName = bAllowSeparateDevice ? ResolveVoiceOutputDeviceName() : FString();
	if (!VoiceDeviceName.IsEmpty())
	{
		return VoiceDeviceName;
	}
	const FString GameDeviceId = AppliedGameDeviceId.Get(FString());
	const FMKHAudioOutputDevice* GameDevice = GameDeviceId.IsEmpty() ? nullptr : FindDevice(GameDeviceId);
	return GameDevice ? GameDevice->DisplayName : FString();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHAudioDeviceSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!bHasReadDevices && IsValid(World) && World->GetGameInstance() == GetGameInstance())
	{
		RefreshDevices();
	}
}

void UMKHAudioDeviceSubsystem::HandleOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices)
{
	OutputDevices.Reset(AvailableDevices.Num());
	for (const FAudioOutputDeviceInfo& Info : AvailableDevices)
	{
		FMKHAudioOutputDevice& Device = OutputDevices.AddDefaulted_GetRef();
		Device.DeviceId = Info.DeviceId;
		Device.DisplayName = Info.Name;
		Device.bIsSystemDefault = Info.bIsSystemDefault;
	}
	UE_LOG(LogMKHAudio, Log, TEXT("Audio devices: %d output devices, microphone '%s'."), OutputDevices.Num(), *InputDeviceName);
	bHasReadDevices = true;
	ApplyGameOutputDevice();
	OnAudioDevicesChanged.Broadcast();
}

void UMKHAudioDeviceSubsystem::HandleDeviceSwapCompleted(const FSwapAudioOutputResult& SwapResult)
{
	UE_CLOG(SwapResult.Result != ESwapAudioOutputDeviceResultState::Success, LogMKHAudio, Warning,
		TEXT("Audio devices: swap to '%s' refused (current '%s')."), *SwapResult.RequestedDeviceId, *SwapResult.CurrentDeviceId);
}

void UMKHAudioDeviceSubsystem::ApplyGameOutputDevice()
{
	const UWorld* World = GetGameInstance()->GetWorld();
	if (!bHasReadDevices || !IsValid(World) || !World->GetAudioDeviceRaw())
	{
		return;
	}
	const FString SavedId = GetGameOutputDeviceId();
	const FString TargetId = !SavedId.IsEmpty() && FindDevice(SavedId) ? SavedId : FString();
	UE_CLOG(TargetId != SavedId, LogMKHAudio, Log, TEXT("Audio devices: saved game output '%s' not connected, using the system default."), *SavedId);
	if (AppliedGameDeviceId.IsSet() && AppliedGameDeviceId.GetValue() == TargetId)
	{
		return;
	}
	// The mixer starts on the system default: nothing to swap the first time unless a device was chosen.
	const bool bFirstApplyOnDefault = !AppliedGameDeviceId.IsSet() && TargetId.IsEmpty();
	AppliedGameDeviceId = TargetId;
	if (bFirstApplyOnDefault)
	{
		return;
	}
	FOnCompletedDeviceSwap OnSwapped;
	OnSwapped.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMKHAudioDeviceSubsystem, HandleDeviceSwapCompleted));
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(World, TargetId, OnSwapped);
}

const FMKHAudioOutputDevice* UMKHAudioDeviceSubsystem::FindDevice(const FString& DeviceId) const
{
	return OutputDevices.FindByPredicate([&DeviceId](const FMKHAudioOutputDevice& Device)
	{
		return Device.DeviceId == DeviceId;
	});
}

void UMKHAudioDeviceSubsystem::ReadInputDeviceName()
{
	Audio::FAudioCapture Capture;
	Audio::FCaptureDeviceInfo Info;
	InputDeviceName = Capture.GetCaptureDeviceInfo(Info, Audio::DefaultDeviceIndex) ? Info.DeviceName : FString();
}
