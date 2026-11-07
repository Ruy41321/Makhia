// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHVoiceProvider_OSS.h"

#include "Online/Backend/MKHVoiceOutputEndpoint.h"
#include "Net/VoiceConfig.h"
#include "AudioDeviceNotificationSubsystem.h"
#include "Engine/Engine.h"
#include "DSP/MultithreadedPatching.h"
#include "UObject/UObjectIterator.h"
#include "VoipListenerSynthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "MKHLogChannels.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/VoiceCapture.h"
#include "OnlineSubsystemUtils.h"
#include "VoiceModule.h"

namespace MKHVoiceOSS
{
	/** Makhia has a single local player. */
	static constexpr uint8 LocalUserNum = 0;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceProvider_OSS::InitializeProvider()
{
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UMKHVoiceProvider_OSS::ReleaseWorldPlaybacks);
}

void UMKHVoiceProvider_OSS::ShutdownProvider()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
	SetTransmitting(false);
	StopMicrophoneTest();
	if (const IOnlineVoicePtr Voice = NotifyingVoice.Pin())
	{
		Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingHandle);
	}
	NotifyingVoice.Reset();
	TalkingIds.Reset();
	if (const IOnlineSessionPtr Sessions = WatchedSessions.Pin())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}
	WatchedSessions.Reset();
	if (UAudioDeviceNotificationSubsystem* Notifications = GEngine ? UAudioDeviceNotificationSubsystem::Get() : nullptr)
	{
		Notifications->DefaultRenderDeviceChangedNative.Remove(DefaultDeviceHandle);
	}
	DefaultDeviceHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(StreamTickHandle);
	StreamTickHandle.Reset();
	OutputEndpoint.Reset();
	DroppedPlaybacks.Reset();
	DroppedPlaybackSink.Reset();
	SetGameMixVoiceMuted(false);
	AppliedOutputDevice.Reset();
	EndpointVoice.Reset();
}

// ============================================================
// Public Interface
// ============================================================

bool UMKHVoiceProvider_OSS::IsVoiceAvailable() const
{
	return GetVoiceInterface().IsValid();
}

void UMKHVoiceProvider_OSS::SetTransmitting(bool bTransmit)
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (!Voice.IsValid())
	{
		bTransmitting = false;
		return;
	}
	if (bTransmit)
	{
		EnsureLocalTalker(Voice);
		Voice->StartNetworkedVoice(MKHVoiceOSS::LocalUserNum);
	}
	else if (bTransmitting)
	{
		Voice->StopNetworkedVoice(MKHVoiceOSS::LocalUserNum);
	}
	if (bTransmitting != bTransmit)
	{
		UE_LOG(LogMKHVoice, Log, TEXT("Voice: microphone %s."), bTransmit ? TEXT("transmitting") : TEXT("closed"));
	}
	bTransmitting = bTransmit;
}

void UMKHVoiceProvider_OSS::RegisterRemoteTalker(const FUniqueNetIdRepl& PlayerId)
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (Voice.IsValid() && PlayerId.IsValid())
	{
		Voice->RegisterRemoteTalker(*PlayerId);
		RemoteTalkers.Add(PlayerId.ToString(), PlayerId);
	}
}

void UMKHVoiceProvider_OSS::UnregisterRemoteTalker(const FUniqueNetIdRepl& PlayerId)
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (Voice.IsValid() && PlayerId.IsValid())
	{
		Voice->UnregisterRemoteTalker(*PlayerId);
	}
	RemoteTalkers.Remove(PlayerId.ToString());
}

void UMKHVoiceProvider_OSS::SetRemoteTalkerMuted(const FUniqueNetIdRepl& PlayerId, bool bMuted)
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (!Voice.IsValid() || !PlayerId.IsValid())
	{
		return;
	}
	if (bMuted)
	{
		Voice->MuteRemoteTalker(MKHVoiceOSS::LocalUserNum, *PlayerId, false);
	}
	else
	{
		Voice->UnmuteRemoteTalker(MKHVoiceOSS::LocalUserNum, *PlayerId, false);
	}
}

bool UMKHVoiceProvider_OSS::IsLocalPlayerTalking() const
{
	BindTalkingNotifications(GetVoiceInterface());
	const FString LocalId = GetLocalPlayerId();
	return bTransmitting && !LocalId.IsEmpty() && TalkingIds.Contains(LocalId);
}

bool UMKHVoiceProvider_OSS::IsRemotePlayerTalking(const FUniqueNetIdRepl& PlayerId) const
{
	BindTalkingNotifications(GetVoiceInterface());
	return PlayerId.IsValid() && TalkingIds.Contains(PlayerId.ToString());
}

void UMKHVoiceProvider_OSS::SetVoiceOutputDevice(const FString& DeviceName)
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (!Voice.IsValid() || (EndpointVoice.Pin() == Voice && AppliedOutputDevice == DeviceName && OutputEndpoint.IsValid()))
	{
		return;
	}
	AppliedOutputDevice = DeviceName;
	EndpointVoice = Voice;
	BindDefaultDeviceNotifications();
	OpenOutputEndpoint(*Voice);
}

void UMKHVoiceProvider_OSS::SetOutputVolume(float Volume)
{
	OutputGain = FMath::Clamp(Volume, 0.f, 1.f) * MaxOutputGain;
	if (OutputEndpoint.IsValid())
	{
		OutputEndpoint->SetGain(OutputGain);
	}
}

float UMKHVoiceProvider_OSS::ConsumeOutputPeakLevel()
{
	return OutputEndpoint.IsValid() && OutputEndpoint->IsRunning() ? OutputEndpoint->ConsumePeakLevel() : -1.f;
}

bool UMKHVoiceProvider_OSS::StartMicrophoneTest()
{
	if (TestCapture.IsValid())
	{
		return true;
	}
	if (!FVoiceModule::IsAvailable() || !FVoiceModule::Get().IsVoiceEnabled())
	{
		return false;
	}
	TestCapture = FVoiceModule::Get().CreateVoiceCapture(FString());
	if (!TestCapture.IsValid() || !TestCapture->Start())
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice: the microphone test could not open the microphone."));
		TestCapture.Reset();
		return false;
	}
	TestBuffer.SetNumUninitialized(TestCapture->GetBufferSize());
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: microphone test started."));
	return true;
}

void UMKHVoiceProvider_OSS::StopMicrophoneTest()
{
	if (!TestCapture.IsValid())
	{
		return;
	}
	TestCapture->Stop();
	TestCapture->Shutdown();
	TestCapture.Reset();
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: microphone test stopped."));
}

float UMKHVoiceProvider_OSS::GetMicrophoneTestLevel()
{
	if (!TestCapture.IsValid())
	{
		return 0.f;
	}
	uint32 Available = 0;
	if (TestCapture->GetCaptureState(Available) == EVoiceCaptureState::Ok && Available > 0)
	{
		TestBuffer.SetNumUninitialized(FMath::Max<int32>(TestBuffer.Num(), Available));
		TestCapture->GetVoiceData(TestBuffer.GetData(), TestBuffer.Num(), Available);
	}
	return FMath::Clamp(TestCapture->GetCurrentAmplitude(), 0.f, 1.f);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

IOnlineVoicePtr UMKHVoiceProvider_OSS::GetVoiceInterface() const
{
	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	return IsValid(World) ? Online::GetVoiceInterface(World) : nullptr;
}

void UMKHVoiceProvider_OSS::EnsureLocalTalker(const IOnlineVoicePtr& Voice)
{
	if (RegisteredVoice.Pin() == Voice)
	{
		return;
	}
	// Fails harmlessly when the session code of the subsystem registered it already.
	Voice->RegisterLocalTalker(MKHVoiceOSS::LocalUserNum);
	RegisteredVoice = Voice;
}

void UMKHVoiceProvider_OSS::BindTalkingNotifications(const IOnlineVoicePtr& Voice) const
{
	BindSessionNotifications();
	if (NotifyingVoice.Pin() == Voice)
	{
		return;
	}
	if (const IOnlineVoicePtr Previous = NotifyingVoice.Pin())
	{
		Previous->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingHandle);
	}
	TalkingIds.Reset();
	NotifyingVoice = Voice;
	TalkingHandle = Voice.IsValid() ? Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(
		FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &UMKHVoiceProvider_OSS::HandleTalkingStateChanged)) : FDelegateHandle();
}

void UMKHVoiceProvider_OSS::HandleTalkingStateChanged(TSharedRef<const FUniqueNetId> TalkerId, bool bIsTalking) const
{
	if (bIsTalking)
	{
		TalkingIds.Add(TalkerId->ToString());
	}
	else
	{
		TalkingIds.Remove(TalkerId->ToString());
	}
}

void UMKHVoiceProvider_OSS::BindSessionNotifications() const
{
	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const IOnlineSessionPtr Sessions = IsValid(World) ? Online::GetSessionInterface(World) : nullptr;
	if (WatchedSessions.Pin() == Sessions || !Sessions.IsValid())
	{
		return;
	}
	if (const IOnlineSessionPtr Previous = WatchedSessions.Pin())
	{
		Previous->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Previous->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}
	WatchedSessions = Sessions;
	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMKHVoiceProvider_OSS::HandleSessionRegisteredLocalTalker));
	JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMKHVoiceProvider_OSS::HandleSessionJoined));
}

void UMKHVoiceProvider_OSS::HandleSessionRegisteredLocalTalker(FName SessionName, bool bWasSuccessful) const
{
	const IOnlineVoicePtr Voice = GetVoiceInterface();
	if (!bTransmitting && Voice.IsValid())
	{
		Voice->StopNetworkedVoice(MKHVoiceOSS::LocalUserNum);
	}
}

void UMKHVoiceProvider_OSS::HandleSessionJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result) const
{
	HandleSessionRegisteredLocalTalker(SessionName, Result == EOnJoinSessionCompleteResult::Success);
}

FString UMKHVoiceProvider_OSS::GetLocalPlayerId() const
{
	const UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const IOnlineIdentityPtr Identity = IsValid(World) ? Online::GetIdentityInterface(World) : nullptr;
	const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(MKHVoiceOSS::LocalUserNum) : nullptr;
	return LocalId.IsValid() ? LocalId->ToString() : FString();
}

void UMKHVoiceProvider_OSS::SetGameMixVoiceMuted(bool bMuted)
{
	if (IConsoleVariable* MuteGameMix = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MuteAudioEngineOutput")))
	{
		MuteGameMix->Set(bMuted ? 1 : 0, ECVF_SetByGameSetting);
	}
}

void UMKHVoiceProvider_OSS::OpenOutputEndpoint(IOnlineVoice& Voice)
{
	OutputEndpoint.Reset();
	OutputEndpoint = MakeShared<FMKHVoiceOutputEndpoint>(AppliedOutputDevice, UVOIPStatics::GetVoiceNumChannels(), static_cast<float>(UVOIPStatics::GetVoiceSampleRate()));
	OutputEndpoint->SetGain(OutputGain);
	OutputEndpoint->SetInput(Voice.GetRemoteTalkerOutput());
	if (!StreamTickHandle.IsValid())
	{
		StreamTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMKHVoiceProvider_OSS::HandleStreamTick), 0.f);
	}
	// Without its stream the voice falls back to the game mix (silenced while the window is not focused).
	const bool bRunning = OutputEndpoint->IsRunning();
	SetGameMixVoiceMuted(bRunning);
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: other players play on %s%s."),
		AppliedOutputDevice.IsEmpty() ? TEXT("the system default output") : *FString::Printf(TEXT("'%s'"), *AppliedOutputDevice),
		bRunning ? TEXT("") : TEXT(" (stream failed, game mix used)"));
}

void UMKHVoiceProvider_OSS::BindDefaultDeviceNotifications()
{
	UAudioDeviceNotificationSubsystem* Notifications = GEngine ? UAudioDeviceNotificationSubsystem::Get() : nullptr;
	if (DefaultDeviceHandle.IsValid() || !IsValid(Notifications))
	{
		return;
	}
	DefaultDeviceHandle = Notifications->DefaultRenderDeviceChangedNative.AddUObject(this, &UMKHVoiceProvider_OSS::HandleDefaultRenderDeviceChanged);
}

void UMKHVoiceProvider_OSS::HandleDefaultRenderDeviceChanged(EAudioDeviceChangedRole Role, FString DeviceId)
{
	const IOnlineVoicePtr Voice = EndpointVoice.Pin();
	if (Role != EAudioDeviceChangedRole::Console || !AppliedOutputDevice.IsEmpty() || !Voice.IsValid())
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: Windows default output changed, voice stream reopened."));
	OpenOutputEndpoint(*Voice);
}

bool UMKHVoiceProvider_OSS::HandleStreamTick(float DeltaTime)
{
	DetachDroppedPlaybacks();
	return true;
}

void UMKHVoiceProvider_OSS::DetachDroppedPlaybacks()
{
	if (!DroppedPlaybackSink.IsValid())
	{
		DroppedPlaybackSink = MakeShared<Audio::FPatchMixerSplitter>();
	}
	for (TObjectIterator<UVoipListenerSynthComponent> It; It; ++It)
	{
		UVoipListenerSynthComponent* Playback = *It;
		if (!IsValid(Playback) || Playback->IsTemplate() || Playback->IsRegistered() || DroppedPlaybacks.Contains(Playback))
		{
			continue;
		}
		Playback->ConnectToSplitter(*DroppedPlaybackSink);
		DroppedPlaybacks.Add(Playback);
	}
	for (auto It = DroppedPlaybacks.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	// Releases the inputs of the playbacks garbage collected since (the sink has no outputs: nothing is mixed).
	DroppedPlaybackSink->ProcessAudio();
}

void UMKHVoiceProvider_OSS::ReleaseWorldPlaybacks(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!IsValid(World))
	{
		return;
	}
	int32 NumReleased = 0;
	for (TObjectIterator<UVoipListenerSynthComponent> It; It; ++It)
	{
		UVoipListenerSynthComponent* Playback = *It;
		if (!IsValid(Playback) || Playback->IsTemplate() || !Playback->IsRegistered() || Playback->GetWorld() != World)
		{
			continue;
		}
		Playback->Stop();
		if (UAudioComponent* AudioComponent = Playback->GetAudioComponent(); IsValid(AudioComponent) && AudioComponent->IsRegistered())
		{
			AudioComponent->UnregisterComponent();
		}
		Playback->UnregisterComponent();
		++NumReleased;
	}
	UE_CLOG(NumReleased > 0, LogMKHVoice, Log, TEXT("Voice: %d voice playbacks released with world %s."), NumReleased, *World->GetName());
}
