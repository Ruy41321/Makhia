// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Voice/MKHVoicePlayerComponent.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MKHLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "Online/MKHOnlineSettings.h"
#include "Voice/MKHVoiceAuthoritySubsystem.h"
#include "Voice/MKHVoiceChatSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHVoicePlayerComponent::UMKHVoicePlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMKHVoicePlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice component on %s: not a player controller, ignored."), *GetNameSafe(GetOwner()));
		return;
	}
	if (PlayerController->HasAuthority())
	{
		if (UMKHVoiceAuthoritySubsystem* Authority = UMKHVoiceAuthoritySubsystem::Get(this); IsValid(Authority))
		{
			Authority->RegisterVoiceComponent(this);
		}
	}
	if (PlayerController->IsLocalController())
	{
		if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
		{
			VoiceChat->RegisterLocalComponent(this);
		}
	}
}

void UMKHVoicePlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMKHVoiceAuthoritySubsystem* Authority = UMKHVoiceAuthoritySubsystem::Get(this); IsValid(Authority))
	{
		Authority->UnregisterVoiceComponent(this);
	}
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->UnregisterLocalComponent(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UMKHVoicePlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMKHVoicePlayerComponent, PeerStatuses, COND_OwnerOnly);
}

// ============================================================
// Public Interface
// ============================================================

void UMKHVoicePlayerComponent::SetupInput(UEnhancedInputComponent* EnhancedInput)
{
	const UMKHOnlineSettings* Settings = UMKHOnlineSettings::Get();
	UInputAction* PushToTalk = IsValid(Settings) ? Settings->PushToTalkAction.LoadSynchronous() : nullptr;
	if (!IsValid(EnhancedInput) || !IsValid(PushToTalk))
	{
		UE_LOG(LogMKHVoice, Warning, TEXT("Voice component: no input component or no push-to-talk action (Project Settings -> Makhia Online)."));
		return;
	}
	EnhancedInput->BindAction(PushToTalk, ETriggerEvent::Started, this, &UMKHVoicePlayerComponent::HandlePushToTalkStarted);
	EnhancedInput->BindAction(PushToTalk, ETriggerEvent::Completed, this, &UMKHVoicePlayerComponent::HandlePushToTalkEnded);
	EnhancedInput->BindAction(PushToTalk, ETriggerEvent::Canceled, this, &UMKHVoicePlayerComponent::HandlePushToTalkEnded);
}

APlayerController* UMKHVoicePlayerComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

FString UMKHVoicePlayerComponent::GetPlayerId() const
{
	const APlayerController* PlayerController = GetPlayerController();
	const APlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->PlayerState.Get() : nullptr;
	return IsValid(PlayerState) && PlayerState->GetUniqueId().IsValid() ? PlayerState->GetUniqueId().ToString() : FString();
}

void UMKHVoicePlayerComponent::SendPreferences(const FMKHVoicePreferences& Preferences)
{
	if (bHasSentPreferences && SentPreferences == Preferences)
	{
		return;
	}
	if (!CanSendToServer() || !Preferences.IsWithinLimits())
	{
		return;
	}

	UE_LOG(LogMKHVoice, Log, TEXT("Voice: sending preferences (voice %s, non-friends %s, party %s, %d muted)."),
		Preferences.bVoiceEnabled ? TEXT("on") : TEXT("off"), Preferences.bAllowNonFriends ? TEXT("allowed") : TEXT("refused"),
		Preferences.bJoinedAsParty ? TEXT("yes") : TEXT("no"), Preferences.MutedPlayerIds.Num());
	SentPreferences = Preferences;
	bHasSentPreferences = true;
	ServerSetVoicePreferences(Preferences);
}

EMKHVoiceBlockReason UMKHVoicePlayerComponent::GetPeerReason(const FString& PlayerId) const
{
	const FMKHVoicePeerStatus* Status = PeerStatuses.FindByPredicate([&PlayerId](const FMKHVoicePeerStatus& Entry)
	{
		return Entry.PlayerId == PlayerId;
	});
	return Status ? Status->Reason : EMKHVoiceBlockReason::Unavailable;
}

bool UMKHVoicePlayerComponent::HasOpenPeer() const
{
	return PeerStatuses.ContainsByPredicate([](const FMKHVoicePeerStatus& Entry)
	{
		return Entry.Reason == EMKHVoiceBlockReason::None;
	});
}

void UMKHVoicePlayerComponent::SetPeerStatuses(const TArray<FMKHVoicePeerStatus>& NewStatuses)
{
	if (PeerStatuses == NewStatuses)
	{
		return;
	}
	PeerStatuses = NewStatuses;
	OnRep_PeerStatuses();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHVoicePlayerComponent::ServerSetVoicePreferences_Validate(const FMKHVoicePreferences& NewPreferences)
{
	return NewPreferences.IsWithinLimits();
}

void UMKHVoicePlayerComponent::ServerSetVoicePreferences_Implementation(const FMKHVoicePreferences& NewPreferences)
{
	ServerPreferences = NewPreferences;
	bHasServerPreferences = true;
	if (UMKHVoiceAuthoritySubsystem* Authority = UMKHVoiceAuthoritySubsystem::Get(this); IsValid(Authority))
	{
		Authority->HandlePreferencesChanged(this);
	}
}

void UMKHVoicePlayerComponent::OnRep_PeerStatuses()
{
	LogPeerStatuses();
	OnPeerStatusesChanged.Broadcast(this);
}

bool UMKHVoicePlayerComponent::CanSendToServer() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return IsValid(PlayerController) && PlayerController->IsLocalController()
		&& (PlayerController->HasAuthority() || PlayerController->GetNetConnection() != nullptr);
}

void UMKHVoicePlayerComponent::HandlePushToTalkStarted(const FInputActionValue& Value)
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->SetPushToTalkHeld(true);
	}
}

void UMKHVoicePlayerComponent::HandlePushToTalkEnded(const FInputActionValue& Value)
{
	if (UMKHVoiceChatSubsystem* VoiceChat = UMKHVoiceChatSubsystem::Get(this); IsValid(VoiceChat))
	{
		VoiceChat->SetPushToTalkHeld(false);
	}
}

void UMKHVoicePlayerComponent::LogPeerStatuses() const
{
	const APlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}
	for (const FMKHVoicePeerStatus& Status : PeerStatuses)
	{
		UE_LOG(LogMKHVoice, Log, TEXT("Voice: with %s -> %s."), *Status.PlayerId, *UEnum::GetValueAsString(Status.Reason));
	}
}
