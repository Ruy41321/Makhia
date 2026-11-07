// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/MKHNetDiagnosticsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "MKHLogChannels.h"
#include "Online/Backend/MKHNetTransportProvider.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Player/PlayerState/MKHPlayerState.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHNetDiagnosticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMKHOnlineServicesSubsystem* Services = Collection.InitializeDependency<UMKHOnlineServicesSubsystem>();
	const UMKHOnlineBackend* Backend = IsValid(Services) ? Services->GetBackend() : nullptr;
	const TSubclassOf<UMKHNetTransportProvider> TransportClass = IsValid(Backend) ? Backend->GetTransportProviderClass() : nullptr;
	Transport = NewObject<UMKHNetTransportProvider>(this, IsValid(TransportClass) ? TransportClass.Get() : UMKHNetTransportProvider::StaticClass());
	Transport->OnLocalNatTypeChanged.AddUObject(this, &UMKHNetDiagnosticsSubsystem::HandleLocalNatTypeChanged);
	Transport->InitializeProvider();
	bNatTypeReported = IsValid(Services) && Services->GetCapabilities().bReportsNatType;

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UMKHNetDiagnosticsSubsystem::HandleTick), TickInterval);
}

void UMKHNetDiagnosticsSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	LogConnectionSummary();
	if (IsValid(Transport))
	{
		Transport->OnLocalNatTypeChanged.RemoveAll(this);
		Transport->ShutdownProvider();
	}
	Transport = nullptr;

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHNetDiagnosticsSubsystem* UMKHNetDiagnosticsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHNetDiagnosticsSubsystem>() : nullptr;
}

EMKHNatType UMKHNetDiagnosticsSubsystem::GetLocalNatType() const
{
	return IsValid(Transport) ? Transport->GetLocalNatType() : EMKHNatType::Unknown;
}

FMKHNetStatsSnapshot UMKHNetDiagnosticsSubsystem::GetNetStats() const
{
	FMKHNetStatsSnapshot Stats;
	Stats.bNatTypeReported = bNatTypeReported;
	Stats.LocalNatType = GetLocalNatType();

	UNetConnection* Connection = FindGameConnection(Stats.bIsHost);
	if (!IsValid(Connection))
	{
		return Stats;
	}

	Stats.bConnected = true;
	Stats.PingMs = FMath::RoundToInt(Connection->AvgLag * 1000.f);
	Stats.JitterMs = FMath::RoundToInt(Connection->GetAverageJitterInMS());
	Stats.PacketLossIn = Connection->GetInLossPercentage().GetAvgLossPercentage();
	Stats.PacketLossOut = Connection->GetOutLossPercentage().GetAvgLossPercentage();
	Stats.Path = GetConnectionPath(*Connection);
	Stats.RelayLocation = IsValid(Transport) && Stats.Path == EMKHConnectionPath::Relayed ? Transport->GetRelayLocation(*Connection) : FString();
	Stats.bQualityReported = IsValid(Transport) && Transport->GetConnectionQuality(*Connection, Stats.QualityLocal, Stats.QualityRemote);

	if (const AMKHPlayerState* RemotePlayerState = FindRemotePlayerState(); IsValid(RemotePlayerState))
	{
		Stats.RemoteNatType = RemotePlayerState->GetNatType();
	}
	return Stats;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHNetDiagnosticsSubsystem::HandleTick(float DeltaTime)
{
	if (IsValid(Transport))
	{
		Transport->Refresh();
	}
	SampleGameConnection();
	return true;
}

void UMKHNetDiagnosticsSubsystem::HandleLocalNatTypeChanged(EMKHNatType NewNatType)
{
	OnLocalNatTypeChanged.Broadcast(NewNatType);
}

void UMKHNetDiagnosticsSubsystem::SampleGameConnection()
{
	bool bIsHost = false;
	UNetConnection* Connection = FindGameConnection(bIsHost);
	if (Connection != SampledConnection.Get())
	{
		LogConnectionSummary();
		SampledConnection = Connection;
		SampledRemoteAddress = IsValid(Connection) ? Connection->LowLevelGetRemoteAddress(false) : FString();
	}

	// AvgLag stays at zero until the first stat period of the connection has elapsed.
	if (!IsValid(Connection) || Connection->AvgLag <= 0.f)
	{
		return;
	}

	const int32 PingMs = FMath::RoundToInt(Connection->AvgLag * 1000.f);
	PingSumMs += PingMs;
	PingMaxMs = FMath::Max(PingMaxMs, PingMs);
	++PingSampleCount;
	LogRouteChange(*Connection, GetConnectionPath(*Connection));
}

void UMKHNetDiagnosticsSubsystem::LogRouteChange(UNetConnection& Connection, EMKHConnectionPath NewPath)
{
	// ICE may move a relayed connection to a direct route after a few seconds: every change is worth a line.
	const FString RelayLocation = IsValid(Transport) && NewPath == EMKHConnectionPath::Relayed ? Transport->GetRelayLocation(Connection) : FString();
	if (NewPath != SampledPath || RelayLocation != SampledRelayLocation)
	{
		UE_LOG(LogMKHNet, Log, TEXT("Route to %s: %s%s."), *SampledRemoteAddress, *UEnum::GetDisplayValueAsText(NewPath).ToString(),
			RelayLocation.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" via %s"), *RelayLocation));
	}
	SampledPath = NewPath;
	SampledRelayLocation = RelayLocation;
}

void UMKHNetDiagnosticsSubsystem::LogConnectionSummary()
{
	if (PingSampleCount > 0)
	{
		UE_LOG(LogMKHNet, Log, TEXT("Connection to %s ended: ping avg %d ms, max %d ms over %d s, route %s."),
			*SampledRemoteAddress, FMath::RoundToInt(PingSumMs / PingSampleCount), PingMaxMs, PingSampleCount,
			*UEnum::GetDisplayValueAsText(SampledPath).ToString());
	}

	PingSumMs = 0.0;
	PingMaxMs = 0;
	PingSampleCount = 0;
	SampledPath = EMKHConnectionPath::Unknown;
	SampledRelayLocation.Reset();
}

UNetConnection* UMKHNetDiagnosticsSubsystem::FindGameConnection(bool& bOutIsHost) const
{
	bOutIsHost = false;
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const UNetDriver* NetDriver = IsValid(World) ? World->GetNetDriver() : nullptr;
	if (!IsValid(NetDriver))
	{
		return nullptr;
	}

	if (IsValid(NetDriver->ServerConnection))
	{
		return NetDriver->ServerConnection;
	}

	// Duels are 1v1: the first client connection of the listen server is the opponent.
	for (UNetConnection* ClientConnection : NetDriver->ClientConnections)
	{
		if (IsValid(ClientConnection))
		{
			bOutIsHost = true;
			return ClientConnection;
		}
	}
	return nullptr;
}

EMKHConnectionPath UMKHNetDiagnosticsSubsystem::GetConnectionPath(UNetConnection& Connection) const
{
	return IsValid(Transport) ? Transport->GetConnectionPath(Connection) : EMKHConnectionPath::Direct;
}

const AMKHPlayerState* UMKHNetDiagnosticsSubsystem::FindRemotePlayerState() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return nullptr;
	}

	const APlayerController* LocalController = GameInstance->GetFirstLocalPlayerController(World);
	const APlayerState* LocalPlayerState = IsValid(LocalController) ? LocalController->PlayerState : nullptr;
	for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
	{
		const AMKHPlayerState* Candidate = Cast<AMKHPlayerState>(PlayerState);
		if (IsValid(Candidate) && Candidate != LocalPlayerState && !Candidate->IsABot())
		{
			return Candidate;
		}
	}
	return nullptr;
}
