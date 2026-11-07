// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHVoiceOutputEndpoint.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "MKHLogChannels.h"

// ============================================================
// Lifecycle
// ============================================================

FMKHVoiceOutputEndpoint::FMKHVoiceOutputEndpoint(const FString& DeviceName, int32 InNumChannels, float InSampleRate)
	: NumInputChannels(FMath::Max(InNumChannels, 1))
{
	FAudioDeviceManager* DeviceManager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
	IAudioDeviceModule* AudioModule = DeviceManager ? DeviceManager->GetAudioDeviceModule() : nullptr;
	if (!AudioModule)
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice: no audio device module, separate output not opened."));
		return;
	}
	Platform.Reset(AudioModule->CreateAudioMixerPlatformInterface());
	if (!Platform.IsValid() || !Platform->InitializeHardware())
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice: audio platform not initialized, separate output not opened."));
		return;
	}
	if (!OpenStream(DeviceName, InSampleRate))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice: could not open '%s' for the voice output."), *DeviceName);
		return;
	}
	Platform->StartAudioStream();
	Platform->FadeIn();
	bIsRunning = true;
}

FMKHVoiceOutputEndpoint::~FMKHVoiceOutputEndpoint()
{
	if (!Platform.IsValid())
	{
		return;
	}
	if (bIsRunning)
	{
		Platform->StopAudioStream();
	}
	Platform->CloseAudioStream();
	Platform->TeardownHardware();
	// InitializeHardware registered it with the Windows device notifications; TeardownHardware does not undo that.
	Platform->UnregisterDeviceChangedListener();
	Platform.Reset();
}

// ============================================================
// Public Interface
// ============================================================

void FMKHVoiceOutputEndpoint::SetInput(const Audio::FPatchOutputStrongPtr& InOutput)
{
	FScopeLock Lock(&InputLock);
	Input = InOutput;
}

bool FMKHVoiceOutputEndpoint::OnProcessAudioStream(Audio::FAlignedFloatBuffer& OutputBuffer)
{
	FScopeLock Lock(&InputLock);
	const int32 NumFrames = DeviceInfo.NumChannels > 0 ? OutputBuffer.Num() / DeviceInfo.NumChannels : 0;
	const int32 NumInputSamples = NumFrames * NumInputChannels;
	if (!Input.IsValid() || NumFrames == 0 || Input->GetNumSamplesAvailable() < NumInputSamples)
	{
		FMemory::Memzero(OutputBuffer.GetData(), OutputBuffer.Num() * sizeof(float));
		return true;
	}
	if (NumInputChannels == DeviceInfo.NumChannels)
	{
		Input->PopAudio(OutputBuffer.GetData(), OutputBuffer.Num(), false);
	}
	else
	{
		// Different channel counts: every device channel plays input channel (index modulo input channels).
		InputBuffer.SetNumUninitialized(NumInputSamples);
		Input->PopAudio(InputBuffer.GetData(), NumInputSamples, false);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			for (int32 Channel = 0; Channel < DeviceInfo.NumChannels; ++Channel)
			{
				OutputBuffer[Frame * DeviceInfo.NumChannels + Channel] = InputBuffer[Frame * NumInputChannels + Channel % NumInputChannels];
			}
		}
	}
	ApplyGain(OutputBuffer);
	TrackPeak(OutputBuffer);
	return true;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHVoiceOutputEndpoint::ApplyGain(Audio::FAlignedFloatBuffer& Buffer) const
{
	const float CurrentGain = Gain.load();
	if (FMath::IsNearlyEqual(CurrentGain, 1.f))
	{
		return;
	}
	for (float& Sample : Buffer)
	{
		Sample = FMath::Clamp(Sample * CurrentGain, -1.f, 1.f);
	}
}

void FMKHVoiceOutputEndpoint::TrackPeak(const Audio::FAlignedFloatBuffer& Buffer)
{
	float Peak = 0.f;
	for (const float Sample : Buffer)
	{
		Peak = FMath::Max(Peak, FMath::Abs(Sample));
	}
	if (Peak > PeakLevel.load())
	{
		PeakLevel.store(Peak);
	}
}

bool FMKHVoiceOutputEndpoint::OpenStream(const FString& DeviceName, float SampleRate)
{
	int32 DeviceIndex = DeviceName.IsEmpty() ? AUDIO_MIXER_DEFAULT_DEVICE_INDEX : Platform->GetIndexForDevice(DeviceName);
	if (DeviceIndex == INDEX_NONE)
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice: output device '%s' not found, using the default output."), *DeviceName);
		DeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
	}
	Audio::FAudioMixerOpenStreamParams Params;
	Params.NumBuffers = 3;
	Params.NumFrames = Platform->GetNumFrames(1024);
	Params.OutputDeviceIndex = DeviceIndex;
	Params.SampleRate = static_cast<uint32>(SampleRate);
	Params.AudioMixer = this;
	Params.MaxSources = 0;
	if (!Platform->OpenAudioStream(Params))
	{
		return false;
	}
	// Never follows the Windows default by itself: the provider opens a new endpoint (on the game thread).
	Platform->SetIsListeningForDeviceEvents(false);
	Platform->PostInitializeHardware();
	DeviceInfo = Platform->GetPlatformDeviceInfo();
	return DeviceInfo.NumChannels > 0;
}
