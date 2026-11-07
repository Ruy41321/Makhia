// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixer.h"
#include "DSP/MultithreadedPatching.h"

#include <atomic>

/**
 * Audio output stream that plays the remote talkers' voice, in place of the game mix and of the engine's
 * FVoiceEndpoint (IOnlineVoice::PatchRemoteTalkerOutputToEndpoint). A stream of its own is not silenced with the
 * game when the window loses the focus, and its gain is not limited to the game mix.
 *
 * The engine endpoint never unregisters its platform interface from the Windows device notifications, so after
 * DisconnectAllEndpoints the next default device change calls a freed object and crashes. This one tears its
 * platform interface down completely. See Documentation/VoiceChat.md -> Voice output stream.
 */
class FMKHVoiceOutputEndpoint final : public Audio::IAudioMixer
{
public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/**
	 * Opens and starts the output stream.
	 *
	 * @param DeviceName       Device to play on; empty or not found for the default output device.
	 * @param InNumChannels    Channels of the voice audio patched in.
	 * @param InSampleRate     Sample rate of the voice audio.
	 */
	FMKHVoiceOutputEndpoint(const FString& DeviceName, int32 InNumChannels, float InSampleRate);

	/** Stops the stream and unregisters the platform interface from the device notifications. */
	~FMKHVoiceOutputEndpoint();

	// ============================================================
	// Public Interface
	// ============================================================

	/** True when the output stream is running. */
	bool IsRunning() const { return bIsRunning; }

	/**
	 * Sets the audio the stream plays.
	 *
	 * @param InOutput  Patch output of the remote talkers' mix.
	 */
	void SetInput(const Audio::FPatchOutputStrongPtr& InOutput);

	/**
	 * Sets the gain applied to the voice (above 1 amplifies; the samples are clamped to [-1, 1]).
	 *
	 * @param InGain  Linear gain, at least 0.
	 */
	void SetGain(float InGain) { Gain.store(FMath::Max(InGain, 0.f)); }

	/** Returns the highest sample played since the previous call (after the gain), and resets it. */
	float ConsumePeakLevel() { return PeakLevel.exchange(0.f); }

	/** Audio render thread: fills the device buffer from the input patch (silence when not enough audio). */
	virtual bool OnProcessAudioStream(Audio::FAlignedFloatBuffer& OutputBuffer) override;

	/** Audio render thread: nothing to release. */
	virtual void OnAudioStreamShutdown() override {}

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Opens the stream on the device.
	 *
	 * @param DeviceName   Device to play on.
	 * @param SampleRate   Sample rate of the voice audio.
	 * @return True when the stream is open.
	 */
	bool OpenStream(const FString& DeviceName, float SampleRate);

	/**
	 * Audio render thread: applies the gain to the device buffer.
	 *
	 * @param Buffer  Interleaved samples, changed in place.
	 */
	void ApplyGain(Audio::FAlignedFloatBuffer& Buffer) const;

	/**
	 * Audio render thread: raises PeakLevel to the loudest sample of the buffer.
	 *
	 * @param Buffer  Samples just played.
	 */
	void TrackPeak(const Audio::FAlignedFloatBuffer& Buffer);

private:
	// ============================================================
	// Properties
	// ============================================================

	/** Platform output stream (WASAPI on Windows). */
	TUniquePtr<Audio::IAudioMixerPlatformInterface> Platform;

	/** Format of the opened device. */
	Audio::FAudioPlatformDeviceInfo DeviceInfo;

	/** Channels of the voice audio patched in. */
	int32 NumInputChannels = 1;

	/** True after the stream started. */
	bool bIsRunning = false;

	/** Voice audio played, guarded by InputLock. */
	Audio::FPatchOutputStrongPtr Input;

	/** Guards Input between the game thread and the audio render thread. */
	FCriticalSection InputLock;

	/** Voice audio when its channel count differs from the device's. */
	Audio::FAlignedFloatBuffer InputBuffer;

	/** Gain applied on the audio render thread. */
	std::atomic<float> Gain = 1.f;

	/** Highest absolute sample played since ConsumePeakLevel (diagnostics, MKH.Voice.Dump). */
	std::atomic<float> PeakLevel = 0.f;
};
