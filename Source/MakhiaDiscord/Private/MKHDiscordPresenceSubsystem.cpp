// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHDiscordPresenceSubsystem.h"

#include "DiscordLocalPlayerSubsystem.h"
#include "DiscordPartnerSDKSettings.h"
#include "DiscordUnreal.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Guid.h"
#include "MKHDiscordLog.h"
#include "MKHDiscordSettings.h"
#include "Online/MKHFriendsSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHDiscordPresence"

// ============================================================
// Lifecycle
// ============================================================

bool UMKHDiscordPresenceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if DISCORD_PARTNER_SDK_STUB
	return false;
#else
	if (!Super::ShouldCreateSubsystem(Outer) || IsRunningDedicatedServer())
	{
		return false;
	}

	const UMKHDiscordSettings* Settings = UMKHDiscordSettings::Get();
	const UDiscordPartnerSDKSettings* SDKSettings = UDiscordPartnerSDKSettings::Get();
	return IsValid(Settings) && Settings->bEnableRichPresence && IsValid(SDKSettings) && SDKSettings->GetApplicationIdUint64() != 0;
#endif
}

void UMKHDiscordPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UMKHDiscordPresenceSubsystem::HandleTick), UMKHDiscordSettings::Get()->RefreshInterval);
	UE_LOG(LogMKHDiscord, Log, TEXT("UMKHDiscordPresenceSubsystem: rich presence enabled (application %s)."),
		*UDiscordPartnerSDKSettings::Get()->GetApplicationIdString());
}

void UMKHDiscordPresenceSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	// The plugin client may already be gone with its local player; Discord also clears the presence when the game exits.
	if (UDiscordClient* Client = BoundClient.Get(); IsValid(Client) && bPresencePublished)
	{
		Client->ClearRichPresence();
	}
	BoundClient.Reset();

	Super::Deinitialize();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHDiscordPresenceSubsystem::HandleTick(float DeltaTime)
{
	UDiscordClient* Client = ResolveClient();
	if (!IsValid(Client))
	{
		return true;
	}

	RefreshActivity();
	if (bPublishPending && !bPublishInFlight && FPlatformTime::Seconds() >= NextRetryTime)
	{
		PublishPresence(*Client);
	}
	return true;
}

UDiscordClient* UMKHDiscordPresenceSubsystem::ResolveClient()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = IsValid(GameInstance) ? GameInstance->GetFirstGamePlayer() : nullptr;
	const UDiscordLocalPlayerSubsystem* DiscordPlayer = IsValid(LocalPlayer) ? LocalPlayer->GetSubsystem<UDiscordLocalPlayerSubsystem>() : nullptr;
	UDiscordClient* Client = IsValid(DiscordPlayer) ? DiscordPlayer->Client : nullptr;
	if (!IsValid(Client) || BoundClient.Get() == Client)
	{
		return Client;
	}

	// Without Client::Connect the application id is what tells the Discord app which game is playing.
	Client->SetApplicationId(UDiscordPartnerSDKSettings::Get()->GetApplicationId());
	BoundClient = Client;
	bPublishPending = true;
	bPublishInFlight = false;
	NextRetryTime = 0.0;
	UE_LOG(LogMKHDiscord, Log, TEXT("UMKHDiscordPresenceSubsystem: Discord client bound."));
	return Client;
}

void UMKHDiscordPresenceSubsystem::RefreshActivity()
{
	const FMKHPlayerActivity Activity = UMKHFriendsSubsystem::BuildLocalActivity(GetGameInstance(), CurrentActivity);
	const FString Culture = FInternationalization::Get().GetCurrentLanguage()->GetName();
	if (Activity == CurrentActivity && Culture == PublishedCulture && ActivityStartMs != 0)
	{
		return;
	}

	// A new activity (or a new match of the same kind, the round going back) restarts the timer on Discord.
	if (Activity.Activity != CurrentActivity.Activity || Activity.Round < CurrentActivity.Round || ActivityStartMs == 0)
	{
		ActivityStartMs = GetUnixTimeMs();
		if (Activity.Activity == EMKHPlayerActivity::InLobby)
		{
			PartyId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		}
	}

	CurrentActivity = Activity;
	PublishedCulture = Culture;
	bPublishPending = true;
}

void UMKHDiscordPresenceSubsystem::PublishPresence(UDiscordClient& Client)
{
	UDiscordActivity* Activity = BuildDiscordActivity();
	if (!IsValid(Activity))
	{
		return;
	}

	bPublishPending = false;
	bPublishInFlight = true;
	Client.UpdateRichPresence(Activity, FDiscordClientUpdateRichPresenceCallback::CreateWeakLambda(
		this, [this](UDiscordClientResult* Result) { HandlePublishResult(Result); }));
}

UDiscordActivity* UMKHDiscordPresenceSubsystem::BuildDiscordActivity()
{
	UDiscordActivity* Activity = NewObject<UDiscordActivity>(this);
	UDiscordActivityAssets* Assets = NewObject<UDiscordActivityAssets>(this);
	UDiscordActivityTimestamps* Timestamps = NewObject<UDiscordActivityTimestamps>(this);
	if (!IsValid(Activity) || !IsValid(Assets) || !IsValid(Timestamps))
	{
		UE_LOG(LogMKHDiscord, Warning, TEXT("UMKHDiscordPresenceSubsystem: could not create the Discord activity."));
		return nullptr;
	}

	const UMKHDiscordSettings* Settings = UMKHDiscordSettings::Get();
	const FText Details = GetDetailsText(CurrentActivity);
	Activity->Init();
	Activity->SetType(EDiscordActivityTypes::Playing);
	// Discord refuses texts shorter than 2 characters: an empty line is left unset.
	const FText State = GetStateText(CurrentActivity);
	if (!Details.IsEmpty())
	{
		Activity->SetDetails(Details.ToString());
	}
	if (!State.IsEmpty())
	{
		Activity->SetState(State.ToString());
	}

	Assets->Init();
	Assets->SetLargeImage(Settings->LargeImageKey);
	Assets->SetLargeText(Settings->LargeImageText);
	if (const FString SmallImage = GetSmallImageKey(CurrentActivity); !SmallImage.IsEmpty() && !Details.IsEmpty())
	{
		Assets->SetSmallImage(SmallImage);
		Assets->SetSmallText(Details.ToString());
	}
	Activity->SetAssets(Assets);

	Timestamps->Init();
	Timestamps->SetStart(FDiscordUniqueID(static_cast<uint64>(ActivityStartMs)));
	Activity->SetTimestamps(Timestamps);

	// The player count of the lobby: Discord shows it after the state line, e.g. "Waiting for an opponent (1 of 2)".
	if (CurrentActivity.Activity == EMKHPlayerActivity::InLobby && CurrentActivity.LobbyMaxPlayers > 0)
	{
		UDiscordActivityParty* Party = NewObject<UDiscordActivityParty>(this);
		if (IsValid(Party))
		{
			Party->Init();
			Party->SetId(PartyId);
			Party->SetCurrentSize(CurrentActivity.LobbyPlayers);
			Party->SetMaxSize(CurrentActivity.LobbyMaxPlayers);
			Activity->SetParty(Party);
		}
	}
	return Activity;
}

void UMKHDiscordPresenceSubsystem::HandlePublishResult(UDiscordClientResult* Result)
{
	bPublishInFlight = false;
	bPresencePublished = IsValid(Result) && Result->Successful();
	if (bPresencePublished)
	{
		UE_CLOG(bFailureLogged, LogMKHDiscord, Log, TEXT("UMKHDiscordPresenceSubsystem: rich presence published again."));
		bFailureLogged = false;
		return;
	}

	// Discord closed or not installed: keep the update and try again later.
	bPublishPending = true;
	NextRetryTime = FPlatformTime::Seconds() + UMKHDiscordSettings::Get()->RetryInterval;
	const FString Error = IsValid(Result) ? Result->ToString() : FString(TEXT("no result"));
	if (!bFailureLogged)
	{
		UE_LOG(LogMKHDiscord, Log, TEXT("UMKHDiscordPresenceSubsystem: Discord refused the rich presence (%s). Retrying every %.0f s."),
			*Error, UMKHDiscordSettings::Get()->RetryInterval);
		bFailureLogged = true;
	}
	else
	{
		UE_LOG(LogMKHDiscord, Verbose, TEXT("UMKHDiscordPresenceSubsystem: rich presence refused again (%s)."), *Error);
	}
}

FText UMKHDiscordPresenceSubsystem::GetDetailsText(const FMKHPlayerActivity& Activity)
{
	switch (Activity.Activity)
	{
	case EMKHPlayerActivity::InLobby:
		return LOCTEXT("DetailsLobby", "In the lobby");
	case EMKHPlayerActivity::OfflineMatch:
		return LOCTEXT("DetailsOfflineMatch", "Offline duel");
	case EMKHPlayerActivity::OnlineMatch:
		return LOCTEXT("DetailsOnlineMatch", "Online duel");
	default:
		return FText::GetEmpty();
	}
}

FText UMKHDiscordPresenceSubsystem::GetStateText(const FMKHPlayerActivity& Activity)
{
	if (Activity.Activity == EMKHPlayerActivity::InLobby)
	{
		return Activity.LobbyPlayers < Activity.LobbyMaxPlayers
			? LOCTEXT("StateLobbyWaiting", "Waiting for an opponent")
			: LOCTEXT("StateLobbyFull", "Ready to fight");
	}
	if (Activity.Activity == EMKHPlayerActivity::None)
	{
		return FText::GetEmpty();
	}
	if (Activity.Round <= 0)
	{
		return LOCTEXT("StateMatchStarting", "Entering the arena");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Round"), FText::AsNumber(Activity.Round));
	Args.Add(TEXT("OwnScore"), FText::AsNumber(Activity.OwnScore));
	Args.Add(TEXT("OpponentScore"), FText::AsNumber(Activity.OpponentScore));
	return FText::Format(LOCTEXT("StateMatchRound", "Round {Round} · {OwnScore}–{OpponentScore}"), Args);
}

FString UMKHDiscordPresenceSubsystem::GetSmallImageKey(const FMKHPlayerActivity& Activity)
{
	const UMKHDiscordSettings* Settings = UMKHDiscordSettings::Get();
	switch (Activity.Activity)
	{
	case EMKHPlayerActivity::InLobby:
		return Settings->LobbyImageKey;
	case EMKHPlayerActivity::OfflineMatch:
		return Settings->BotMatchImageKey;
	case EMKHPlayerActivity::OnlineMatch:
		return Settings->OnlineMatchImageKey;
	default:
		return FString();
	}
}

int64 UMKHDiscordPresenceSubsystem::GetUnixTimeMs()
{
	const FDateTime Now = FDateTime::UtcNow();
	return Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
}

#undef LOCTEXT_NAMESPACE
