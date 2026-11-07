// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Voice/MKHVoiceChatSubsystem.h"

#include "Audio/MKHAudioDeviceSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameInstance/MKHGameInstance.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Lobby/MKHLobbyPlayerState.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Online/Backend/MKHVoiceProvider.h"
#include "Online/MKHFriendsSubsystem.h"
#include "Voice/MKHVoicePlayerComponent.h"

namespace MKHVoiceChat
{
	/** voice.MicInputGain at a full input volume slider (the middle keeps the captured level). */
	static constexpr float MaxMicInputGain = 2.f;

	/**
	 * voice.SilenceDetectionThreshold at a full activation slider. The engine default (0.08) cut normal speech in the
	 * 2-PC test: the default slider 0.2 now gives 0.02. See Documentation/VoiceChat.md -> Devices and volumes.
	 */
	static constexpr float MaxSilenceThreshold = 0.1f;

	/** Seconds a talker keeps its "talking" state after the voice engine reports a pause (no flickering icons). */
	static constexpr double TalkingHoldSeconds = 0.5;

	/** Silence threshold while pushing to talk: everything said while the key is held is sent. */
	static constexpr float PushToTalkSilenceThreshold = 0.01f;

	/** Sets a float console variable of the voice engine, when it exists. */
	static void SetVoiceCVar(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}

	/** Returns the voice chat subsystem of the world given to a console command. */
	static UMKHVoiceChatSubsystem* FromWorld(const UWorld* World)
	{
		const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHVoiceChatSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(TEXT("MKH.Voice.Dump"), TEXT("Logs the local voice chat state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (const UMKHVoiceChatSubsystem* VoiceChat = FromWorld(World))
			{
				VoiceChat->DumpState();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs PushToTalkCommand(TEXT("MKH.Voice.PTT"), TEXT("MKH.Voice.PTT <0|1>: holds or releases push-to-talk."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMKHVoiceChatSubsystem* VoiceChat = FromWorld(World); VoiceChat && Args.Num() > 0)
			{
				VoiceChat->SetPushToTalkHeld(FCString::Atoi(*Args[0]) != 0);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs SetCommand(TEXT("MKH.Voice.Set"),
		TEXT("MKH.Voice.Set <Enabled|NonFriends|MicMuted 0|1>, <Mode Open|PTT|Disabled>, <OutputVolume 0..1> or <Mute PlayerId 0|1>: changes a voice setting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UMKHVoiceChatSubsystem* VoiceChat = FromWorld(World);
			if (!VoiceChat || Args.Num() < 2)
			{
				return;
			}
			const bool bValue = FCString::Atoi(*Args[1]) != 0;
			if (Args[0] == TEXT("Enabled")) { VoiceChat->SetVoiceChatEnabled(bValue); }
			else if (Args[0] == TEXT("NonFriends")) { VoiceChat->SetAllowVoiceWithNonFriends(bValue); }
			else if (Args[0] == TEXT("MicMuted")) { VoiceChat->SetMicMuted(bValue); }
			else if (Args[0] == TEXT("Mode"))
			{
				VoiceChat->SetMicMode(Args[1] == TEXT("Open") ? EMKHVoiceMicMode::OpenMic
					: Args[1] == TEXT("Disabled") ? EMKHVoiceMicMode::Disabled : EMKHVoiceMicMode::PushToTalk);
			}
			else if (Args[0] == TEXT("OutputVolume"))
			{
				if (UMKHGameUserSettings* Settings = UMKHGameUserSettings::GetMKHGameUserSettings())
				{
					Settings->SetVoiceOutputVolume(FCString::Atof(*Args[1]));
				}
			}
			else if (Args[0] == TEXT("Mute") && Args.Num() >= 3) { VoiceChat->SetPlayerMuted(Args[1], FCString::Atoi(*Args[2]) != 0); }
		}));

#if !UE_BUILD_SHIPPING
	/** Development only: the LAN backend has no invites, so a party is simulated on the host to test the known pair. */
	static FAutoConsoleCommandWithWorldAndArgs AddPartyCommand(TEXT("MKH.Voice.Debug.AddParty"),
		TEXT("MKH.Voice.Debug.AddParty <PlayerId>: host only, treats a player as joined by invite."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
			if (UMKHLobbySubsystem* Lobby = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHLobbySubsystem>() : nullptr; Lobby && Args.Num() > 0)
			{
				UE_LOG(LogMKHVoice, Warning, TEXT("Voice debug: %s added to the voice party."), *Args[0]);
				Lobby->AddVoicePartyMember(Args[0]);
			}
		}));
#endif
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHVoiceChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMKHOnlineServicesSubsystem* Services = Collection.InitializeDependency<UMKHOnlineServicesSubsystem>();
	Collection.InitializeDependency<UMKHFriendsSubsystem>();
	Collection.InitializeDependency<UMKHLobbySubsystem>();
	Collection.InitializeDependency<UMKHAudioDeviceSubsystem>();
	const UMKHOnlineBackend* Backend = IsValid(Services) ? Services->GetBackend() : nullptr;
	const TSubclassOf<UMKHVoiceProvider> ProviderClass = IsValid(Backend) ? Backend->GetVoiceProviderClass() : nullptr;
	Provider = NewObject<UMKHVoiceProvider>(this, IsValid(ProviderClass) ? ProviderClass.Get() : UMKHVoiceProvider::StaticClass());
	Provider->InitializeProvider();

	ApplyVoiceSettings();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMKHVoiceChatSubsystem::HandleVoiceTick), TickInterval);
	UE_LOG(LogMKHVoice, Log, TEXT("Voice chat: provider %s."), *Provider->GetClass()->GetName());
}

void UMKHVoiceChatSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (IsValid(Provider))
	{
		Provider->SetTransmitting(false);
		Provider->ShutdownProvider();
	}
	Provider = nullptr;
	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHVoiceChatSubsystem* UMKHVoiceChatSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHVoiceChatSubsystem>() : nullptr;
}

bool UMKHVoiceChatSubsystem::IsVoiceChatSupported() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	return IsValid(Services) && Services->GetCapabilities().bSupportsVoiceChat && IsValid(Provider) && Provider->IsVoiceAvailable();
}

bool UMKHVoiceChatSubsystem::IsVoiceChatEnabled() const
{
	const UMKHGameUserSettings* Settings = GetSettings();
	return IsValid(Settings) && Settings->IsVoiceChatEnabled();
}

void UMKHVoiceChatSubsystem::SetVoiceChatEnabled(bool bEnable)
{
	UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Settings) || Settings->IsVoiceChatEnabled() == bEnable)
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice chat %s."), bEnable ? TEXT("enabled") : TEXT("disabled"));
	Settings->SetVoiceChatEnabled(bEnable);
	SaveSettings();
	UpdateTransmission();
	OnVoiceStateChanged.Broadcast();
}

EMKHVoiceMicMode UMKHVoiceChatSubsystem::GetMicMode() const
{
	const UMKHGameUserSettings* Settings = GetSettings();
	return IsValid(Settings) ? Settings->GetVoiceMicMode() : EMKHVoiceMicMode::PushToTalk;
}

void UMKHVoiceChatSubsystem::SetMicMode(EMKHVoiceMicMode NewMode)
{
	UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Settings) || Settings->GetVoiceMicMode() == NewMode)
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: microphone mode %s."), *UEnum::GetValueAsString(NewMode));
	Settings->SetVoiceMicMode(NewMode);
	SaveSettings();
	ApplyVoiceSettings();
	UpdateTransmission();
	OnVoiceStateChanged.Broadcast();
}

bool UMKHVoiceChatSubsystem::AllowsVoiceWithNonFriends() const
{
	const UMKHGameUserSettings* Settings = GetSettings();
	return IsValid(Settings) && Settings->IsVoiceWithNonFriendsAllowed();
}

void UMKHVoiceChatSubsystem::SetAllowVoiceWithNonFriends(bool bAllow)
{
	UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Settings) || Settings->IsVoiceWithNonFriendsAllowed() == bAllow)
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice with non-friends %s."), bAllow ? TEXT("allowed") : TEXT("refused"));
	Settings->SetVoiceWithNonFriendsAllowed(bAllow);
	SaveSettings();
	OnVoiceStateChanged.Broadcast();
}

bool UMKHVoiceChatSubsystem::CanMuteMic() const
{
	return IsVoiceChatEnabled() && GetMicMode() != EMKHVoiceMicMode::Disabled;
}

void UMKHVoiceChatSubsystem::SetMicMuted(bool bMuted)
{
	if (bMicMuted == bMuted)
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: microphone %s for this session."), bMuted ? TEXT("muted") : TEXT("unmuted"));
	bMicMuted = bMuted;
	UpdateTransmission();
	OnVoiceStateChanged.Broadcast();
}

void UMKHVoiceChatSubsystem::SetPushToTalkHeld(bool bHeld)
{
	if (bPushToTalkHeld == bHeld)
	{
		return;
	}
	bPushToTalkHeld = bHeld;
	UpdateTransmission();
}

bool UMKHVoiceChatSubsystem::IsLocalPlayerTalking() const
{
	return bTransmitting && IsValid(Provider) && Provider->IsLocalPlayerTalking();
}

bool UMKHVoiceChatSubsystem::FindTalker(const FString& PlayerId, FMKHVoiceTalkerInfo& OutInfo) const
{
	const FMKHVoiceTalkerInfo* Found = Talkers.FindByPredicate([&PlayerId](const FMKHVoiceTalkerInfo& Info)
	{
		return Info.PlayerId == PlayerId;
	});
	if (Found)
	{
		OutInfo = *Found;
	}
	return Found != nullptr;
}

void UMKHVoiceChatSubsystem::SetPlayerMuted(const FString& PlayerId, bool bMuted)
{
	UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Settings) || !Settings->SetVoicePlayerMuted(PlayerId, bMuted))
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: %s %s."), *PlayerId, bMuted ? TEXT("muted") : TEXT("unmuted"));
	SaveSettings();
	RefreshTalkers();
}

bool UMKHVoiceChatSubsystem::IsPlayerMuted(const FString& PlayerId) const
{
	const UMKHGameUserSettings* Settings = GetSettings();
	const UMKHFriendsSubsystem* Friends = GetGameInstance()->GetSubsystem<UMKHFriendsSubsystem>();
	return (IsValid(Settings) && Settings->GetMutedVoicePlayerIds().Contains(PlayerId))
		|| (IsValid(Friends) && Friends->IsBlockedUser(PlayerId));
}

void UMKHVoiceChatSubsystem::StartMicrophoneTest()
{
	if (++MicrophoneTestUsers == 1 && IsValid(Provider))
	{
		Provider->StartMicrophoneTest();
	}
}

void UMKHVoiceChatSubsystem::StopMicrophoneTest()
{
	MicrophoneTestUsers = FMath::Max(0, MicrophoneTestUsers - 1);
	if (MicrophoneTestUsers == 0 && IsValid(Provider))
	{
		Provider->StopMicrophoneTest();
	}
}

float UMKHVoiceChatSubsystem::GetMicrophoneTestLevel() const
{
	return IsValid(Provider) ? FMath::Clamp(Provider->GetMicrophoneTestLevel() / MKHVoiceChat::MaxSilenceThreshold, 0.f, 1.f) : 0.f;
}

void UMKHVoiceChatSubsystem::ApplyVoiceSettings()
{
	const UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Settings))
	{
		return;
	}
	const float SilenceThreshold = Settings->GetVoiceMicMode() == EMKHVoiceMicMode::PushToTalk
		? MKHVoiceChat::PushToTalkSilenceThreshold : Settings->GetVoiceActivationThreshold() * MKHVoiceChat::MaxSilenceThreshold;
	MKHVoiceChat::SetVoiceCVar(TEXT("voice.MicInputGain"), Settings->GetVoiceInputVolume() * MKHVoiceChat::MaxMicInputGain);
	MKHVoiceChat::SetVoiceCVar(TEXT("voice.SilenceDetectionThreshold"), SilenceThreshold);
	MKHVoiceChat::SetVoiceCVar(TEXT("voice.MicNoiseGateThreshold"), SilenceThreshold);
}

void UMKHVoiceChatSubsystem::RegisterLocalComponent(UMKHVoicePlayerComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}
	LocalComponent = Component;
	Component->OnPeerStatusesChanged.AddWeakLambda(this, [this](UMKHVoicePlayerComponent*)
	{
		RefreshTalkers();
		UpdateTransmission();
	});
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: local voice component of %s ready."), *GetNameSafe(Component->GetOwner()));
}

void UMKHVoiceChatSubsystem::UnregisterLocalComponent(UMKHVoicePlayerComponent* Component)
{
	if (LocalComponent.Get() != Component)
	{
		return;
	}
	if (IsValid(Component))
	{
		Component->OnPeerStatusesChanged.RemoveAll(this);
	}
	LocalComponent.Reset();
	bPushToTalkHeld = false;
	UpdateTransmission();
	SetChannel(EMKHVoiceChannel::None);
	RefreshTalkers();
}

void UMKHVoiceChatSubsystem::DumpState() const
{
	UE_LOG(LogMKHVoice, Display, TEXT("Voice dump: supported %d, channel %s, enabled %d, mode %s, non-friends %d, mic muted %d, PTT %d, transmitting %d, talking %d, output peak %.3f."),
		IsVoiceChatSupported(), *UEnum::GetValueAsString(Channel), IsVoiceChatEnabled(), *UEnum::GetValueAsString(GetMicMode()),
		AllowsVoiceWithNonFriends(), bMicMuted, bPushToTalkHeld, bTransmitting, IsLocalPlayerTalking(),
		IsValid(Provider) ? Provider->ConsumeOutputPeakLevel() : -1.f);
	for (const FMKHVoiceTalkerInfo& Info : Talkers)
	{
		UE_LOG(LogMKHVoice, Display, TEXT("Voice dump:   %s%s talking %d, muted %d, blocked %d, reason %s."), *Info.PlayerId,
			Info.bIsLocalPlayer ? TEXT(" (local)") : TEXT(""), Info.bIsTalking, Info.bIsMutedByLocalPlayer, Info.bIsBlocked,
			*UEnum::GetValueAsString(Info.BlockReason));
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHVoiceChatSubsystem::HandleVoiceTick(float DeltaTime)
{
	RefreshTalkers();
	SetChannel(ComputeChannel());
	if (UMKHVoicePlayerComponent* Component = LocalComponent.Get())
	{
		Component->SendPreferences(BuildPreferences());
	}
	UpdateTransmission();
	ApplyOutputRouting();
	return true;
}

void UMKHVoiceChatSubsystem::ApplyOutputRouting() const
{
	const UMKHOnlineServicesSubsystem* Services = UMKHOnlineServicesSubsystem::Get(this);
	const UMKHAudioDeviceSubsystem* Devices = GetGameInstance()->GetSubsystem<UMKHAudioDeviceSubsystem>();
	const UMKHGameUserSettings* Settings = GetSettings();
	if (!IsValid(Provider) || !IsValid(Services) || !IsValid(Devices) || !IsValid(Settings))
	{
		return;
	}
	Provider->SetOutputVolume(Settings->GetVoiceOutputVolume());
	Provider->SetVoiceOutputDevice(Devices->ResolveVoicePlaybackDeviceName(Services->GetCapabilities().bSupportsVoiceOutputDevice));
}

void UMKHVoiceChatSubsystem::RefreshTalkers()
{
	TArray<FMKHVoiceTalkerInfo> NewTalkers;
	TSet<FString> PresentIds;
	const UMKHVoicePlayerComponent* Component = LocalComponent.Get();
	const APlayerController* LocalController = IsValid(Component) ? Component->GetPlayerController() : nullptr;
	const UWorld* World = GetGameInstance()->GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (IsValid(LocalController) && IsValid(LocalController->PlayerState) && IsValid(GameState))
	{
		NewTalkers.Add(MakeTalkerInfo(*LocalController->PlayerState, true));
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!IsValid(PlayerState) || PlayerState == LocalController->PlayerState || PlayerState->IsABot()
				|| PlayerState->IsInactive() || !PlayerState->GetUniqueId().IsValid())
			{
				continue;
			}
			const FMKHVoiceTalkerInfo& Info = NewTalkers.Add_GetRef(MakeTalkerInfo(*PlayerState, false));
			PresentIds.Add(Info.PlayerId);
			SyncRemoteTalker(*PlayerState, Info);
		}
	}
	ForgetAbsentTalkers(PresentIds);
	HoldTalkingStates(NewTalkers);
	LogTalkingChanges(NewTalkers);

	if (NewTalkers != Talkers)
	{
		Talkers = MoveTemp(NewTalkers);
		OnTalkersChanged.Broadcast();
	}
}

void UMKHVoiceChatSubsystem::HoldTalkingStates(TArray<FMKHVoiceTalkerInfo>& InOutTalkers)
{
	const double Now = FPlatformTime::Seconds();
	for (FMKHVoiceTalkerInfo& Info : InOutTalkers)
	{
		if (Info.bIsTalking)
		{
			LastTalkingTimes.Add(Info.PlayerId, Now);
		}
		else if (const double* LastTalking = LastTalkingTimes.Find(Info.PlayerId))
		{
			Info.bIsTalking = Now - *LastTalking < MKHVoiceChat::TalkingHoldSeconds && !Info.bIsMutedByLocalPlayer
				&& Info.BlockReason == EMKHVoiceBlockReason::None;
		}
	}
	for (auto It = LastTalkingTimes.CreateIterator(); It; ++It)
	{
		if (Now - It.Value() >= MKHVoiceChat::TalkingHoldSeconds)
		{
			It.RemoveCurrent();
		}
	}
}

void UMKHVoiceChatSubsystem::LogTalkingChanges(const TArray<FMKHVoiceTalkerInfo>& NewTalkers) const
{
	for (const FMKHVoiceTalkerInfo& Info : NewTalkers)
	{
		const bool bWasTalking = Talkers.ContainsByPredicate([&Info](const FMKHVoiceTalkerInfo& Old) { return Old.PlayerId == Info.PlayerId && Old.bIsTalking; });
		UE_CLOG(!Info.bIsLocalPlayer && Info.bIsTalking != bWasTalking, LogMKHVoice, Log, TEXT("Voice: %s %s talking."),
			*Info.PlayerId, Info.bIsTalking ? TEXT("started") : TEXT("stopped"));
	}
}

FMKHVoiceTalkerInfo UMKHVoiceChatSubsystem::MakeTalkerInfo(const APlayerState& PlayerState, bool bIsLocalPlayer) const
{
	FMKHVoiceTalkerInfo Info;
	Info.PlayerId = PlayerState.GetUniqueId().IsValid() ? PlayerState.GetUniqueId().ToString() : FString();
	Info.bIsLocalPlayer = bIsLocalPlayer;
	const AMKHLobbyPlayerState* LobbyState = Cast<AMKHLobbyPlayerState>(&PlayerState);
	Info.DisplayName = IsValid(LobbyState) && !LobbyState->GetLobbyDisplayName().IsEmpty() ? LobbyState->GetLobbyDisplayName() : PlayerState.GetPlayerName();
	if (bIsLocalPlayer)
	{
		Info.bIsTalking = IsLocalPlayerTalking();
		return Info;
	}

	const UMKHVoicePlayerComponent* Component = LocalComponent.Get();
	const UMKHFriendsSubsystem* Friends = GetGameInstance()->GetSubsystem<UMKHFriendsSubsystem>();
	Info.bIsBlocked = IsValid(Friends) && Friends->IsBlockedUser(Info.PlayerId);
	Info.bIsMutedByLocalPlayer = IsPlayerMuted(Info.PlayerId);
	Info.BlockReason = IsValid(Component) ? Component->GetPeerReason(Info.PlayerId) : EMKHVoiceBlockReason::Unavailable;
	Info.bIsTalking = Info.BlockReason == EMKHVoiceBlockReason::None && IsValid(Provider) && Provider->IsRemotePlayerTalking(PlayerState.GetUniqueId());
	return Info;
}

void UMKHVoiceChatSubsystem::SyncRemoteTalker(const APlayerState& PlayerState, const FMKHVoiceTalkerInfo& Info)
{
	if (!IsValid(Provider))
	{
		return;
	}
	if (!RegisteredTalkers.Contains(Info.PlayerId))
	{
		Provider->RegisterRemoteTalker(PlayerState.GetUniqueId());
		RegisteredTalkers.Add(Info.PlayerId, PlayerState.GetUniqueId());
		AppliedPlaybackMutes.Remove(Info.PlayerId);
	}

	const bool bMutePlayback = Info.BlockReason != EMKHVoiceBlockReason::None || Info.bIsMutedByLocalPlayer;
	const bool* Applied = AppliedPlaybackMutes.Find(Info.PlayerId);
	if (!Applied || *Applied != bMutePlayback)
	{
		Provider->SetRemoteTalkerMuted(PlayerState.GetUniqueId(), bMutePlayback);
		AppliedPlaybackMutes.Add(Info.PlayerId, bMutePlayback);
	}
}

void UMKHVoiceChatSubsystem::ForgetAbsentTalkers(const TSet<FString>& PresentIds)
{
	for (auto It = RegisteredTalkers.CreateIterator(); It; ++It)
	{
		if (PresentIds.Contains(It.Key()))
		{
			continue;
		}
		if (IsValid(Provider))
		{
			Provider->UnregisterRemoteTalker(It.Value());
		}
		AppliedPlaybackMutes.Remove(It.Key());
		It.RemoveCurrent();
	}
}

EMKHVoiceChannel UMKHVoiceChatSubsystem::ComputeChannel() const
{
	const UMKHGameInstance* GameInstance = Cast<UMKHGameInstance>(GetGameInstance());
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	if (!IsValid(World) || World->GetNetMode() == NM_Standalone || !LocalComponent.IsValid() || Talkers.Num() < 2)
	{
		return EMKHVoiceChannel::None;
	}
	return GameInstance->IsInLobbyLevel() ? EMKHVoiceChannel::Lobby : EMKHVoiceChannel::Match;
}

FMKHVoicePreferences UMKHVoiceChatSubsystem::BuildPreferences() const
{
	FMKHVoicePreferences Preferences;
	Preferences.bVoiceEnabled = IsVoiceChatEnabled() && IsVoiceChatSupported();
	Preferences.bAllowNonFriends = AllowsVoiceWithNonFriends();
	const UMKHLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UMKHLobbySubsystem>();
	Preferences.bJoinedAsParty = IsValid(Lobby) && Lobby->HasJoinedThroughInvite();
	for (const FMKHVoiceTalkerInfo& Info : Talkers)
	{
		if (!Info.bIsLocalPlayer && Info.bIsMutedByLocalPlayer)
		{
			Preferences.MutedPlayerIds.Add(Info.PlayerId);
		}
	}
	Preferences.MutedPlayerIds.Sort();
	return Preferences;
}

bool UMKHVoiceChatSubsystem::ShouldTransmit() const
{
	const UMKHVoicePlayerComponent* Component = LocalComponent.Get();
	const EMKHVoiceMicMode Mode = GetMicMode();
	return Channel != EMKHVoiceChannel::None && IsValid(Component) && Component->HasOpenPeer() && IsVoiceChatEnabled()
		&& Mode != EMKHVoiceMicMode::Disabled && !bMicMuted && (Mode == EMKHVoiceMicMode::OpenMic || bPushToTalkHeld);
}

void UMKHVoiceChatSubsystem::UpdateTransmission()
{
	const bool bShouldTransmit = ShouldTransmit();
	if (bShouldTransmit == bTransmitting || !IsValid(Provider))
	{
		return;
	}
	bTransmitting = bShouldTransmit;
	Provider->SetTransmitting(bTransmitting);
	OnVoiceStateChanged.Broadcast();
}

void UMKHVoiceChatSubsystem::SetChannel(EMKHVoiceChannel NewChannel)
{
	if (Channel == NewChannel)
	{
		return;
	}
	UE_LOG(LogMKHVoice, Log, TEXT("Voice: channel %s."), *UEnum::GetValueAsString(NewChannel));
	Channel = NewChannel;
	UpdateTransmission();
	OnVoiceStateChanged.Broadcast();
}

UMKHGameUserSettings* UMKHVoiceChatSubsystem::GetSettings()
{
	return UMKHGameUserSettings::GetMKHGameUserSettings();
}

void UMKHVoiceChatSubsystem::SaveSettings()
{
	if (UMKHGameUserSettings* Settings = GetSettings(); IsValid(Settings))
	{
		Settings->SaveSettings();
	}
}
