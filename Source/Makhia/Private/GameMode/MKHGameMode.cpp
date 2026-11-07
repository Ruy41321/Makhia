// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/MKHGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Character/CharacterBase.h"
#include "AI/MKHDuelBot.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/MKHDifficultyDirector.h"
#include "AI/MKHTrainingDummyComponent.h"
#include "AIController.h"
#include "Career/MKHCareerLadder.h"
#include "Career/MKHCareerSubsystem.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Engine/PlayerStartPIE.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameInstance/MKHGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/MKHGameState.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/MKHDuelParticipantInterface.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/MKHItemSubsystem.h"
#include "Inventory/MKHStartingLoadout.h"
#include "Inventory/Spawn/MKHItemSpawnSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"

namespace MKHBotWeapons
{
	/** @return True when a loadout item is a weapon. */
	static bool IsWeapon(const FMKHStartingItem& Item)
	{
		return Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon);
	}

	/** @return Two different weapons among the given ones, in random order (fewer when fewer are given). */
	static TArray<FGameplayTag> PickRandomPair(const TArray<FGameplayTag>& Weapons)
	{
		TArray<FGameplayTag> Shuffled;
		for (const FGameplayTag& WeaponTag : Weapons)
		{
			Shuffled.Insert(WeaponTag, FMath::RandRange(0, Shuffled.Num()));
		}
		Shuffled.SetNum(FMath::Min(Shuffled.Num(), 2));
		return Shuffled;
	}
}

// ============================================================
// Lifecycle
// ============================================================

AMKHGameMode::AMKHGameMode()
{
	// Two fixed slots for the 1v1 participants.
	Players.SetNum(2);

	// Drive the replicated duel state / UI events through our custom game state.
	GameStateClass = AMKHGameState::StaticClass();
}

void AMKHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// No option means online, except the arena opened straight from a solo PIE: see Documentation/OfflineModes.md.
	const FString ModeOption = UGameplayStatics::ParseOption(Options, MKHMatchMode::OptionKey);
	const UWorld* World = GetWorld();
	const bool bSoloPlayInEditor = ModeOption.IsEmpty() && IsValid(World) && World->WorldType == EWorldType::PIE
		&& World->GetNetMode() == NM_Standalone;
	MatchMode = bSoloPlayInEditor || ModeOption.Equals(MKHMatchMode::OfflineBotValue, ESearchCase::IgnoreCase)
		? EMKHMatchMode::OfflineBot
		: EMKHMatchMode::OnlineDuel;

	const FString BotProfileOption = UGameplayStatics::ParseOption(Options, MKHMatchMode::BotProfileOptionKey);
	RequestedBotProfileId = BotProfileOption.IsEmpty() ? NAME_None : FName(*BotProfileOption);

	ReadOfflineRequest(Options);
	if (bSoloPlayInEditor)
	{
		OfflineRequest.Mode = EMKHOfflineMode::Training;
	}

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: match mode %s, bot profile '%s'."), *UEnum::GetValueAsString(MatchMode),
		*RequestedBotProfileId.ToString());
}

void AMKHGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	// After Super the pawn is spawned and possessed, so the controller's pawn is guaranteed valid. StartSpot is set
	// by the engine on login/travel and used by RestartPlayer, so it is the start the pawn really spawned at.
	if (IsValid(NewPlayer))
	{
		RegisterParticipant(Cast<ACharacterBase>(NewPlayer->GetPawn()), NewPlayer->StartSpot.Get());
		GrantStartingLoadout(NewPlayer);
	}

	// After the player, so the bot takes the start spot left free.
	if (MatchMode == EMKHMatchMode::OfflineBot)
	{
		SpawnBotOpponent();
	}

	TryStartMatch();
}

AActor* AMKHGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> FreeStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsValid(Start))
		{
			continue;
		}
		if (Start->IsA<APlayerStartPIE>())
		{
			// PIE "Play From Here": keep the engine behaviour.
			return Super::ChoosePlayerStart_Implementation(Player);
		}
		if (!IsStartSpotTaken(Start, Player))
		{
			FreeStarts.Add(Start);
		}
	}

	if (FreeStarts.IsEmpty())
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("AMKHGameMode::ChoosePlayerStart: no free PlayerStart for %s, using the engine's choice."), *GetNameSafe(Player));
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	APlayerStart* Chosen = FreeStarts[FMath::RandRange(0, FreeStarts.Num() - 1)];
	UE_LOG(LogMKHAbility, Log, TEXT("AMKHGameMode: %s starts at %s."), *GetNameSafe(Player), *GetNameSafe(Chosen));
	return Chosen;
}

void AMKHGameMode::GrantStartingLoadout(AController* NewPlayer) const
{
	const UDataTable* LoadoutTable = IsTrainingMatch() && IsValid(TrainingLoadoutTable) ? TrainingLoadoutTable.Get() : StartingLoadoutTable.Get();
	if (!IsValid(LoadoutTable))
	{
		UE_LOG(LogMKHInventory, Error, TEXT("MKHGameMode: %s has no StartingLoadoutTable, players get no items."), *GetNameSafe(GetClass()));
		return;
	}

	UInventoryComponent* Inventory = FindPlayerInventory(NewPlayer);
	if (IsValid(Inventory))
	{
		Inventory->GrantStartingLoadout(FMKHStartingLoadoutRow::ReadLoadout(*LoadoutTable));
	}
}

UInventoryComponent* AMKHGameMode::FindPlayerInventory(AController* Controller) const
{
	if (!IsValid(Controller) || !Controller->Implements<UInventoryInterface>())
	{
		return nullptr;
	}

	UInventoryComponent* Inventory = IInventoryInterface::Execute_GetInventoryComponent(Controller);
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("AMKHGameMode: %s has no inventory component."), *GetNameSafe(Controller));
	}
	return Inventory;
}

void AMKHGameMode::Logout(AController* Exiting)
{
	// Without this the ballot would keep counting a controller that is on its way out, and the
	// remaining player could never reach the (now lower) vote requirement.
	if (APlayerController* ExitingPlayer = Cast<APlayerController>(Exiting))
	{
		if (RematchVoters.Remove(ExitingPlayer) > 0)
		{
			PublishRematchVotes();
		}
		ReportRemotePlayerLeft(ExitingPlayer);
	}

	Super::Logout(Exiting);
	ReturnToLobbyAfterLogout();
}

void AMKHGameMode::ReturnToLobbyAfterLogout() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World) || World->bIsTearingDown)
	{
		return;
	}

	if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this))
	{
		Lobby->DecrementPlayersInLobby();
	}
	else
	{
		UE_LOG(LogMKHSession, Warning, TEXT("AMKHGameMode::ReturnToLobbyAfterLogout: no lobby subsystem, the lobby seat count is not updated."));
	}

	if (UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this))
	{
		GameInstance->BackToLobby(true);
	}
}

void AMKHGameMode::ReportRemotePlayerLeft(const APlayerController* ExitingPlayer) const
{
	// Must run before ReturnToLobbyAfterLogout, which starts the host's return to the lobby.
	const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
	if (!IsValid(ExitingPlayer) || ExitingPlayer->IsLocalController() || !IsValid(GameInstance) || !IsValid(Loading))
	{
		return;
	}

	// This machine is the one leaving: its own teardown logs the opponent out.
	if (GameInstance->IsReturningToLobby())
	{
		return;
	}

	if (Loading->IsLoadingScreenActive())
	{
		if (Loading->GetLoadingDestination() == EMKHLoadingDestination::Game)
		{
			Loading->ReportTravelErrorWithDetail(EMKHTravelError::OpponentLeft, GetNameSafe(ExitingPlayer));
		}
		return;
	}

	// Leaving once the match is over is the normal end of a match.
	const AMKHGameState* MKHGameState = GetGameState<AMKHGameState>();
	if (IsValid(MKHGameState) && MKHGameState->GetMatchPhase() == EMKHMatchPhase::MatchOver)
	{
		return;
	}

	Loading->ReportTravelErrorWithDetail(EMKHTravelError::ConnectionLost,
		FString::Printf(TEXT("%s left during the match"), *GetNameSafe(ExitingPlayer)));
}

// ============================================================
// Public Interface (Class default data accessors)
// ============================================================

UCharacterClassInfo* AMKHGameMode::GetCharacterClassDefaultInfo() const
{
	return ClassDefaults;
}

UProjectileInfo* AMKHGameMode::GetProjectileInfo() const
{
	return ProjectileInfo;
}

// ============================================================
// Public Interface (Match state queries)
// ============================================================

APlayerState* AMKHGameMode::FindParticipantAtStartSpot(const AActor* InStartSpot) const
{
	const FMKHPlayerData* Data = FindParticipantDataAtStartSpot(InStartSpot);
	return (Data && IsValid(Data->Participant)) ? Data->Participant->GetPlayerState() : nullptr;
}

bool AMKHGameMode::IsStartSpotHeldByBot(const AActor* InStartSpot) const
{
	const FMKHPlayerData* Data = FindParticipantDataAtStartSpot(InStartSpot);
	return Data && !UMKHDuelParticipantLibrary::IsHumanParticipant(Data->Participant);
}

// ============================================================
// Public Interface (Rematch ballot)
// ============================================================

void AMKHGameMode::RequestRematch(APlayerController* Voter)
{
	if (!IsValid(Voter))
	{
		return;
	}

	// The ballot only exists between the end of the match and the restart.
	if (MatchPhase != EMKHMatchPhase::MatchOver)
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: %s asked for a rematch outside the MatchOver phase, ignoring."), *GetNameSafe(Voter));
		return;
	}

	bool bAlreadyVoted = false;
	RematchVoters.Add(Voter, &bAlreadyVoted);
	if (bAlreadyVoted)
	{
		return;
	}

	const int32 VotesCast = RematchVoters.Num();
	const int32 VotesNeeded = CountRematchVotersNeeded();

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: %s voted for a rematch (%d/%d)."), *GetNameSafe(Voter), VotesCast, VotesNeeded);

	PublishRematchVotes();

	if (VotesCast < VotesNeeded)
	{
		return;
	}

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: every participant voted for a rematch, restarting the match."));
	RestartMatchInPlace();
}

// ============================================================
// Public Interface (Preparation skip ballot)
// ============================================================

void AMKHGameMode::ToggleSkipPreparationVote(APlayerController* Voter)
{
	// Not a warning: the press may have left the client just before the final countdown closed the ballot.
	if (!bSkipPreparationBallotOpen || !IsValid(Voter))
	{
		UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: skip vote from %s ignored, the ballot is closed."), *GetNameSafe(Voter));
		return;
	}

	APlayerState* VoterState = Voter->PlayerState;
	if (!IsValid(VoterState) || FindParticipantIndex(Cast<ACharacterBase>(Voter->GetPawn())) == INDEX_NONE)
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: %s is not a duel participant, skip vote ignored."), *GetNameSafe(Voter));
		return;
	}

	// A second press withdraws the vote.
	const bool bWithdrawn = SkipPreparationVoters.Remove(VoterState) > 0;
	if (!bWithdrawn)
	{
		SkipPreparationVoters.Add(VoterState);
	}

	// Humans only: against the bot a single "ready" skips the preparation.
	const int32 VotesCast = SkipPreparationVoters.Num();
	const int32 VotesNeeded = CountHumanParticipants();
	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: %s is %s (%d/%d)."),
		*GetNameSafe(Voter), bWithdrawn ? TEXT("no longer ready") : TEXT("ready to skip the preparation"), VotesCast, VotesNeeded);

	PublishSkipPreparationVotes();

	if (VotesCast >= VotesNeeded)
	{
		UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: every participant is ready, skipping to the final countdown."));
		StartFinalCountdown();
	}
}

// ============================================================
// Protected / Internal Logic (Match flow)
// ============================================================

void AMKHGameMode::RegisterParticipant(ACharacterBase* Participant, AActor* InStartSpot)
{
	if (!IsValid(Participant))
	{
		return;
	}

	if (!Cast<IMKHDuelParticipant>(Participant))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: %s does not implement IMKHDuelParticipant, it cannot take a duel slot."), *GetNameSafe(Participant));
		return;
	}

	// Ignore characters already registered (OnPostLogin + BeginPlay can both see the host).
	if (FindParticipantIndex(Participant) != INDEX_NONE)
	{
		return;
	}

	// Find the first free slot; a 1v1 match only accepts two participants.
	const int32 FreeIndex = Players.IndexOfByPredicate([](const FMKHPlayerData& Data) { return !Data.HasParticipant(); });
	if (FreeIndex == INDEX_NONE)
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: a third participant tried to join a 1v1 match, ignoring %s."), *GetNameSafe(Participant));
		return;
	}

	Players[FreeIndex].Participant = Participant;
	Players[FreeIndex].Score = 0;
	Players[FreeIndex].SpawnTransform = Participant->GetActorTransform();
	Players[FreeIndex].StartSpot = InStartSpot;

	if (AMKHGameState* GS = GetDuelGameState())
	{
		GS->RegisterParticipant(FreeIndex, Participant);
	}

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: registered %s in slot %d."), *GetNameSafe(Participant), FreeIndex);

	BindDeathEvent(Participant);
}

void AMKHGameMode::SpawnBotOpponent()
{
	const bool bBotRegistered = Players.ContainsByPredicate([](const FMKHPlayerData& Data)
	{
		return Data.HasParticipant() && !UMKHDuelParticipantLibrary::IsHumanParticipant(Data.Participant);
	});
	UWorld* World = GetWorld();
	if (bBotRegistered || !IsValid(World))
	{
		return;
	}

	AActor* BotStart = IsTrainingMatch() ? FindTrainingDummySpot() : FindFreeStartSpotForBot();
	if (!BotClass || !IsValid(BotStart))
	{
		UE_LOG(LogMKHAbility, Error, TEXT("MKHGameMode: cannot spawn the bot opponent (BotClass %s, start spot %s): the offline match will not start."),
			*GetNameSafe(BotClass), *GetNameSafe(BotStart));
		return;
	}

	// Same placement as a player pawn: the start's location and yaw.
	const FTransform SpawnTransform(FRotator(0.f, BotStart->GetActorRotation().Yaw, 0.f), BotStart->GetActorLocation());
	AMKHDuelBot* Bot = SpawnBotActor(SpawnTransform);
	if (!IsValid(Bot))
	{
		UE_LOG(LogMKHAbility, Error, TEXT("MKHGameMode: spawning %s failed, the offline match will not start."), *GetNameSafe(BotClass));
		return;
	}

	ConfigureBot(*Bot);
	RegisterParticipant(Bot, BotStart);
}

AMKHDuelBot* AMKHGameMode::SpawnBotActor(const FTransform& SpawnTransform) const
{
	AMKHDuelBot* Bot = GetWorld()->SpawnActorDeferred<AMKHDuelBot>(BotClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!IsValid(Bot))
	{
		return nullptr;
	}

	// The dummy uses BP_DuelBot as it is, minus the brain: no asset of its own to keep aligned with the bot.
	if (IsTrainingMatch())
	{
		Bot->AIControllerClass = AAIController::StaticClass();
		UMKHTrainingDummyComponent* Dummy = NewObject<UMKHTrainingDummyComponent>(Bot, TEXT("TrainingDummy"));
		Dummy->RegisterComponent();
	}

	Bot->FinishSpawning(SpawnTransform);
	return Bot;
}

void AMKHGameMode::ConfigureBot(AMKHDuelBot& Bot)
{
	Bot.SetOpponentSkin(ResolveBotSkinId());
	Bot.SetTitleCareerLevels(ResolveBotTitleCareerLevels());

	// AutoPossessAI creates the controller during the spawn, so it exists here; a null profile means its default.
	if (AMKHDuelBotController* BotController = Bot.GetController<AMKHDuelBotController>(); IsValid(BotController))
	{
		BotController->SetProfile(FindBotProfile(ResolveBotProfileId()));
	}
	else if (!IsTrainingMatch())
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHGameMode: %s has no AMKHDuelBotController, the difficulty profile is not applied."), *Bot.GetName());
	}

	const TArray<FMKHStartingItem> Loadout = BuildBotLoadout(Bot);
	Bot.EquipLoadout(Loadout, ChooseBotStartingWeapons(Loadout));
}

void AMKHGameMode::TryStartMatch()
{
	// Only start once both participants are present and the match has not begun yet.
	if (MatchPhase != EMKHMatchPhase::None)
	{
		return;
	}

	const bool bBothReady = Players[0].HasParticipant() && Players[1].HasParticipant();
	if (!bBothReady)
	{
		return;
	}

	// Offline the bot completes the pair inside AGameMode::HandleMatchHasStarted, before the actors' BeginPlay:
	// the phase must wait for it, or listeners registering in BeginPlay (item spawn points) miss InitialPreparation.
	UWorld* World = GetWorld();
	if (IsValid(World) && !World->HasBegunPlay())
	{
		if (!World->OnWorldBeginPlay.IsBoundToObject(this))
		{
			World->OnWorldBeginPlay.AddUObject(this, &AMKHGameMode::TryStartMatch);
		}
		return;
	}

	if (IsTrainingMatch())
	{
		StartTrainingPhase();
		return;
	}

	StartDifficultyTracking();
	StartInitialPreparationPhase();
}

void AMKHGameMode::StartTrainingPhase()
{
	CurrentRound = 1;
	SetPhase(EMKHMatchPhase::Duel, 0.f);
	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: training started."));
}

void AMKHGameMode::StartInitialPreparationPhase()
{
	CurrentRound = 1;
	SetPhase(EMKHMatchPhase::InitialPreparation, InitialPreparationDuration);
	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: initial preparation phase started (%.0fs)."), InitialPreparationDuration);

	// Opened before scheduling: a phase shorter than the final countdown closes it again right away.
	ResetSkipPreparationBallot(true);
	SchedulePreparationEnd(InitialPreparationDuration);
}

void AMKHGameMode::StartDuelPhase()
{
	SetPhase(EMKHMatchPhase::Duel, DuelDuration);

	ClearPhaseTimer();
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AMKHGameMode::HandleDuelTimeOver, DuelDuration, false);

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: duel phase started (max %.0fs)."), DuelDuration);
}

void AMKHGameMode::StartIntermissionPhase()
{
	// The intermission prepares the next round.
	++CurrentRound;
	SetPhase(EMKHMatchPhase::IntermissionPreparation, IntermissionPreparationDuration);
	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: intermission preparation started (%.0fs)."), IntermissionPreparationDuration);

	SchedulePreparationEnd(IntermissionPreparationDuration);
}

void AMKHGameMode::SchedulePreparationEnd(float PhaseDurationSeconds)
{
	ClearPhaseTimer();

	// Still one timer at a time: the phase timer first leads to the final countdown, which then leads to the duel.
	const float TimeBeforeFinalCountdown = PhaseDurationSeconds - FinalCountdownDuration;
	if (TimeBeforeFinalCountdown <= 0.f)
	{
		StartFinalCountdown();
		return;
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AMKHGameMode::StartFinalCountdown, TimeBeforeFinalCountdown, false);
}

void AMKHGameMode::StartFinalCountdown()
{
	if (bSkipPreparationBallotOpen)
	{
		ResetSkipPreparationBallot(false);
	}

	// Never extends the phase: one already closer to its end (a short phase) keeps its own deadline.
	AMKHGameState* GS = GetDuelGameState();
	const float Countdown = IsValid(GS)
		? FMath::Min(FinalCountdownDuration, GS->GetPhaseTimeRemaining())
		: FinalCountdownDuration;

	ClearPhaseTimer();
	if (Countdown <= 0.f)
	{
		StartDuelPhase();
		return;
	}

	if (IsValid(GS))
	{
		GS->StartFinalCountdown(Countdown);
	}
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AMKHGameMode::StartDuelPhase, Countdown, false);

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: final countdown started (%.1fs)."), Countdown);
}

void AMKHGameMode::HandleDuelTimeOver()
{
	// Sudden death has no countdown: the drain effect resolves the duel.
	SetPhase(EMKHMatchPhase::SuddenDeath, 0.f);
	ClearPhaseTimer();

	// Apply the lethal drain to both participants; whoever dies first hands the point to the survivor.
	for (FMKHPlayerData& Data : Players)
	{
		ApplySuddenDeathDrain(Data);
	}

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: duel time over, sudden-death drain applied."));
}

void AMKHGameMode::HandlePlayerDeath(const FGameplayEventData* Payload, ACharacterBase* DeadCharacter)
{
	// Only deaths during an active duel matter.
	if (MatchPhase != EMKHMatchPhase::Duel && MatchPhase != EMKHMatchPhase::SuddenDeath)
	{
		return;
	}

	const int32 DeadIndex = FindParticipantIndex(DeadCharacter);
	if (DeadIndex == INDEX_NONE || Players[DeadIndex].bDiedThisRound)
	{
		return;
	}

	// Each death awards the opposite slot; on a double KO both events pass through here.
	const int32 ScoringIndex = (DeadIndex == 0) ? 1 : 0;
	FMKHPlayerData& ScoringData = Players[ScoringIndex];
	if (!ScoringData.HasParticipant())
	{
		return;
	}

	Players[DeadIndex].bDiedThisRound = true;

	// First death of the round: stop the phase timer and schedule the resolution. The phase stays
	// unchanged so a same-frame second death still counts.
	if (!bRoundResolving)
	{
		bRoundResolving = true;
		ClearPhaseTimer();

		// Deferred by one tick so both drains can run their pending periodic execution in this frame:
		// two players on identical health then die together instead of the first one winning the race.
		GetWorldTimerManager().SetTimerForNextTick(this, &AMKHGameMode::RemoveSuddenDeathDrainFromParticipants);
		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AMKHGameMode::ResolveRound, DeathResolveDelay, false);
	}

	ScoringData.Score++;

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: %s died, point awarded to %s (score %d)."),
		*GetNameSafe(DeadCharacter), *GetNameSafe(ScoringData.Participant), ScoringData.Score);

	if (AMKHGameState* GS = GetDuelGameState())
	{
		GS->SetScore(ScoringIndex, ScoringData.Score);
		GS->Multicast_PlayerDied(DeadCharacter, ScoringData.Participant, ScoringData.Score);
	}

	SendEnemyKilledEvent(ScoringData.Participant, DeadCharacter);
}

void AMKHGameMode::SendEnemyKilledEvent(ACharacterBase* Killer, ACharacterBase* DeadCharacter) const
{
	if (!IsValid(Killer))
	{
		return;
	}

	// Event.Combat.EnemyKilled feeds the event-triggered sound effects (kill confirmation of the scorer).
	FGameplayEventData Payload;
	Payload.EventTag = MKHGameplayTags::Event::EnemyKilled;
	Payload.Instigator = Killer;
	Payload.Target = DeadCharacter;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Killer, MKHGameplayTags::Event::EnemyKilled, Payload);
}

void AMKHGameMode::ResolveRound()
{
	bRoundResolving = false;

	const int32 Score0 = Players[0].Score;
	const int32 Score1 = Players[1].Score;

	// Match over once either player reaches ScoreToWin; equal scores at that point mean a draw.
	if (Score0 >= ScoreToWin || Score1 >= ScoreToWin)
	{
		const bool bIsDraw = (Score0 == Score1);
		ACharacterBase* Winner = bIsDraw ? nullptr
			: (Score0 > Score1 ? Players[0].Participant : Players[1].Participant).Get();

		// Before the panel opens: it reads the progress to offer the next challenge.
		RecordCareerResult(Winner);

		SetPhase(EMKHMatchPhase::MatchOver, 0.f);
		if (AMKHGameState* GS = GetDuelGameState())
		{
			GS->Multicast_MatchOver(Winner, bIsDraw);
		}
		UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: match over (%d - %d), %s."),
			Score0, Score1, bIsDraw ? TEXT("draw") : *GetNameSafe(Winner));
		return;
	}

	// Before the reset: the health restored is not part of the round.
	ApplyDifficultyForNextRound();

	ResetParticipantsForNewRound();

	// After the revive: the swap goes through the normal equip flow, and it keeps the health ratio.
	ApplyWeaponsForNextRound();

	StartIntermissionPhase();
}

void AMKHGameMode::RestartMatchInPlace()
{
	// Close the ballot first so a vote arriving mid-restart cannot carry over into the new match.
	RematchVoters.Reset();
	PublishRematchVotes();

	// Wipe the authoritative scoreboard and mirror it, otherwise the replicated scores would still
	// show the result of the match that just ended.
	AMKHGameState* GS = GetDuelGameState();
	for (int32 SlotIndex = 0; SlotIndex < Players.Num(); ++SlotIndex)
	{
		Players[SlotIndex].Score = 0;

		if (IsValid(GS))
		{
			GS->SetScore(SlotIndex, 0);
		}
	}

	// Drop whatever the previous match left running before rebuilding the world state.
	bRoundResolving = false;
	ClearPhaseTimer();
	RemoveSuddenDeathDrainFromParticipants();

	// Before the loadouts: the bot gets the loadout of its starting difficulty.
	ResetDifficultyForRematch();

	// Before the revive, so the round-reset effect tops up the stats of the fresh loadout.
	ResetPlayerLoadouts();

	// Teleports both participants back to their spawns and forces them out of the Dead state, which
	// is also what hands gameplay input back to a player that lost the last round.
	ResetParticipantsForNewRound();

	UE_LOG(LogMKHAbility, Log, TEXT("MKHGameMode: match restarted in place, scores reset."));

	StartInitialPreparationPhase();
}

// ============================================================
// Protected / Internal Logic (Helpers)
// ============================================================

void AMKHGameMode::SetPhase(EMKHMatchPhase NewPhase, float PhaseDurationSeconds)
{
	MatchPhase = NewPhase;

	if (AMKHGameState* GS = GetDuelGameState())
	{
		GS->SetMatchPhase(NewPhase, PhaseDurationSeconds, CurrentRound);
	}

	// Before the pickup cleanup: at MatchOver the spawn points must stop before their pickups are destroyed.
	// Training never reports its phase, so the subsystem sees no running match and every spawn point stays idle.
	UWorld* World = GetWorld();
	UMKHItemSpawnSubsystem* SpawnSubsystem = IsValid(World) ? World->GetSubsystem<UMKHItemSpawnSubsystem>() : nullptr;
	if (IsValid(SpawnSubsystem) && !IsTrainingMatch())
	{
		SpawnSubsystem->HandleMatchPhaseChanged(NewPhase, CurrentRound);
	}

	// World pickups never outlive the match; within it, each spawn point decides whether its pickup survives a round.
	if (NewPhase == EMKHMatchPhase::MatchOver)
	{
		UMKHItemSubsystem* ItemSubsystem = UMKHItemSubsystem::Get(this);
		if (IsValid(ItemSubsystem))
		{
			ItemSubsystem->DestroyAllPickups();
		}
	}
}

void AMKHGameMode::ResetPlayerLoadouts() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		UInventoryComponent* Inventory = FindPlayerInventory(PlayerController);
		if (!IsValid(Inventory))
		{
			continue;
		}

		Inventory->ResetInventory();
		GrantStartingLoadout(PlayerController);
	}

	for (TActorIterator<AMKHDuelBot> It(World); It; ++It)
	{
		const TArray<FMKHStartingItem> Loadout = BuildBotLoadout(**It);
		It->ResetLoadout(Loadout, ChooseBotStartingWeapons(Loadout));
	}
}

AMKHGameState* AMKHGameMode::GetDuelGameState() const
{
	return GetGameState<AMKHGameState>();
}

int32 AMKHGameMode::CountRematchVotersNeeded() const
{
	// Counted from the connected player states, not from the participant pawns: at match over one
	// participant is dead, and whether its pawn is still possessed must not change the vote target.
	const int32 ConnectedPlayers = IsValid(GameState) ? GameState->PlayerArray.Num() : 0;

	// Never zero: a ballot needing no votes would restart the match on the very first press.
	return FMath::Max(ConnectedPlayers, 1);
}

void AMKHGameMode::PublishRematchVotes() const
{
	if (AMKHGameState* GS = GetDuelGameState())
	{
		GS->SetRematchVotes(RematchVoters.Num(), CountRematchVotersNeeded());
	}
}

int32 AMKHGameMode::CountHumanParticipants() const
{
	return Players.FilterByPredicate([](const FMKHPlayerData& Data)
	{
		return Data.HasParticipant() && UMKHDuelParticipantLibrary::IsHumanParticipant(Data.Participant);
	}).Num();
}

void AMKHGameMode::ResetSkipPreparationBallot(bool bOpen)
{
	bSkipPreparationBallotOpen = bOpen;
	SkipPreparationVoters.Reset();
	PublishSkipPreparationVotes();
}

void AMKHGameMode::PublishSkipPreparationVotes() const
{
	AMKHGameState* GS = GetDuelGameState();
	if (!IsValid(GS))
	{
		return;
	}

	// Never zero while open: a ballot needing no votes would read as closed on every peer.
	const int32 VotesNeeded = bSkipPreparationBallotOpen ? FMath::Max(CountHumanParticipants(), 1) : 0;
	GS->SetPreparationSkipVotes(SkipPreparationVoters.Array(), VotesNeeded);
}

void AMKHGameMode::BindDeathEvent(ACharacterBase* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character);
	if (!IsValid(ASC))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: cannot bind death event, %s has no ability system component."), *GetNameSafe(Character));
		return;
	}

	// The death event carries the dying actor implicitly; we pass the participant as a payload so
	// the handler resolves the correct slot without re-casting the event instigator.
	ASC->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::Death)
		.AddUObject(this, &AMKHGameMode::HandlePlayerDeath, Character);
}

void AMKHGameMode::ClearPhaseTimer()
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
}

void AMKHGameMode::ResetParticipantsForNewRound()
{
	for (FMKHPlayerData& Data : Players)
	{
		Data.bDiedThisRound = false;

		IMKHDuelParticipant* DuelParticipant = Cast<IMKHDuelParticipant>(Data.Participant.Get());
		if (!IsValid(Data.Participant) || !DuelParticipant)
		{
			continue;
		}

		// Top the character back up, then let it snap back to its spawn and leave the Dead state.
		ApplyRoundResetEffect(Data.Participant);
		DuelParticipant->ResetForNewRound(Data.SpawnTransform);
	}
}

void AMKHGameMode::ApplyRoundResetEffect(ACharacterBase* Character) const
{
	if (!RoundResetEffect || !IsValid(Character))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character);
	if (!IsValid(ASC))
	{
		return;
	}

	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RoundResetEffect, 1.f, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AMKHGameMode::ApplySuddenDeathDrain(FMKHPlayerData& PlayerData) const
{
	if (!SuddenDeathDrainEffect || !IsValid(PlayerData.Participant))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerData.Participant);
	if (!IsValid(ASC))
	{
		return;
	}

	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(SuddenDeathDrainEffect, 1.f, Context);
	if (Spec.IsValid())
	{
		// Keep the handle so the drain can be surgically removed when the round is decided.
		PlayerData.SuddenDeathDrainHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AMKHGameMode::RemoveSuddenDeathDrainFromParticipants()
{
	for (FMKHPlayerData& Data : Players)
	{
		if (!Data.SuddenDeathDrainHandle.IsValid())
		{
			continue;
		}

		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Data.Participant);
		if (IsValid(ASC))
		{
			ASC->RemoveActiveGameplayEffect(Data.SuddenDeathDrainHandle);
		}

		Data.SuddenDeathDrainHandle.Invalidate();
	}
}

int32 AMKHGameMode::FindParticipantIndex(const ACharacterBase* Character) const
{
	if (!Character)
	{
		return INDEX_NONE;
	}

	return Players.IndexOfByPredicate([Character](const FMKHPlayerData& Data)
	{
		return Data.Participant == Character;
	});
}

const FMKHPlayerData* AMKHGameMode::FindParticipantDataAtStartSpot(const AActor* InStartSpot) const
{
	if (!IsValid(InStartSpot))
	{
		return nullptr;
	}

	return Players.FindByPredicate([InStartSpot](const FMKHPlayerData& Data)
	{
		return Data.HasParticipant() && Data.StartSpot == InStartSpot;
	});
}

bool AMKHGameMode::IsStartSpotTaken(const AActor* InStartSpot, const AController* Asking) const
{
	if (!IsValid(InStartSpot))
	{
		return false;
	}

	if (FindParticipantDataAtStartSpot(InStartSpot))
	{
		return true;
	}

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		const AController* Other = It->Get();
		if (IsValid(Other) && Other != Asking && Other->StartSpot.Get() == InStartSpot)
		{
			return true;
		}
	}
	return false;
}

UMKHDuelBotProfile* AMKHGameMode::FindBotProfile(const FName ProfileId) const
{
	if (ProfileId.IsNone())
	{
		return nullptr;
	}

	for (UMKHDuelBotProfile* Profile : BotProfiles)
	{
		if (IsValid(Profile) && Profile->GetProfileId() == ProfileId)
		{
			return Profile;
		}
	}

	UE_LOG(LogMKHAI, Warning, TEXT("MKHGameMode: no bot profile '%s' in BotProfiles, the bot uses its default profile."), *ProfileId.ToString());
	return nullptr;
}

void AMKHGameMode::ReadOfflineRequest(const FString& Options)
{
	const FString ModeOption = UGameplayStatics::ParseOption(Options, MKHMatchMode::OfflineModeOptionKey);
	if (ModeOption.Equals(MKHMatchMode::TrainingValue, ESearchCase::IgnoreCase))
	{
		OfflineRequest.Mode = EMKHOfflineMode::Training;
	}
	else if (ModeOption.Equals(MKHMatchMode::CareerValue, ESearchCase::IgnoreCase))
	{
		OfflineRequest.Mode = EMKHOfflineMode::Career;
	}

	if (OfflineRequest.Mode != EMKHOfflineMode::Career)
	{
		return;
	}

	OfflineRequest.CareerLevelIndex = UGameplayStatics::GetIntOption(Options, MKHMatchMode::CareerLevelOptionKey, INDEX_NONE);
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	if (IsValid(Career) && !Career->IsLevelUnlocked(OfflineRequest.CareerLevelIndex))
	{
		OfflineRequest.CareerLevelIndex = Career->GetNextLevelIndex();
	}
}

bool AMKHGameMode::IsTrainingMatch() const
{
	return MatchMode == EMKHMatchMode::OfflineBot && OfflineRequest.Mode == EMKHOfflineMode::Training;
}

FName AMKHGameMode::ResolveBotProfileId() const
{
	// ?BotProfile= is the test override, and it turns the difficulty adjustment off (StartDifficultyTracking).
	if (!RequestedBotProfileId.IsNone())
	{
		return RequestedBotProfileId;
	}

	if (OfflineRequest.Mode == EMKHOfflineMode::Career)
	{
		const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
		const FMKHCareerLevel* Level = IsValid(Career) ? Career->FindLevel(OfflineRequest.CareerLevelIndex) : nullptr;
		return Level != nullptr ? Level->BotProfileId : NAME_None;
	}

	const int32 StartingLevel = DifficultySettings.StartingLevel;
	return DifficultySettings.ProfileOrder.IsValidIndex(StartingLevel) ? DifficultySettings.ProfileOrder[StartingLevel] : NAME_None;
}

int32 AMKHGameMode::ResolveBotTitleCareerLevels() const
{
	switch (OfflineRequest.Mode)
	{
	case EMKHOfflineMode::Training:
		return INDEX_NONE;
	case EMKHOfflineMode::Career:
		// Beating a level awards its title, so the level's bot holds that title.
		return OfflineRequest.CareerLevelIndex + 1;
	default:
		return RandomDuelTitleCareerLevels;
	}
}

FName AMKHGameMode::ResolveBotSkinId() const
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const UMKHSkinCatalog* Catalog = IsValid(Skins) ? Skins->GetCatalog() : nullptr;

	switch (OfflineRequest.Mode)
	{
	case EMKHOfflineMode::Training:
	{
		const FMKHSkinDefinition* DefaultSkin = IsValid(Catalog) ? Catalog->GetDefaultSkin() : nullptr;
		return DefaultSkin != nullptr ? DefaultSkin->SkinId : NAME_None;
	}
	case EMKHOfflineMode::Career:
	{
		const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
		const FMKHCareerLevel* Level = IsValid(Career) ? Career->FindLevel(OfflineRequest.CareerLevelIndex) : nullptr;
		return Level != nullptr ? Level->BotSkinId : NAME_None;
	}
	default:
		return PickRandomBotSkin();
	}
}

FName AMKHGameMode::PickRandomBotSkin() const
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const UMKHSkinCatalog* Catalog = IsValid(Skins) ? Skins->GetCatalog() : nullptr;
	if (!IsValid(Catalog))
	{
		return NAME_None;
	}

	TArray<FName> Candidates;
	for (const FMKHSkinDefinition& Skin : Catalog->GetSkins())
	{
		if (Skin.SkinId != Skins->GetSelectedSkinId())
		{
			Candidates.Add(Skin.SkinId);
		}
	}

	return Candidates.IsEmpty() ? Skins->GetSelectedSkinId() : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

AActor* AMKHGameMode::FindTrainingDummySpot() const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TrainingDummySpotTag))
		{
			return *It;
		}
	}

	UE_LOG(LogMKHAbility, Warning, TEXT("MKHGameMode: the level has no actor tagged %s, the training dummy uses a free start."), *TrainingDummySpotTag.ToString());
	return FindFreeStartSpotForBot();
}

AMKHDuelBotController* AMKHGameMode::FindBotController() const
{
	for (const FMKHPlayerData& Data : Players)
	{
		if (Data.HasParticipant() && !UMKHDuelParticipantLibrary::IsHumanParticipant(Data.Participant))
		{
			return Data.Participant->GetController<AMKHDuelBotController>();
		}
	}
	return nullptr;
}

void AMKHGameMode::StartDifficultyTracking()
{
	if (MatchMode != EMKHMatchMode::OfflineBot || OfflineRequest.Mode != EMKHOfflineMode::RandomDuel || !RequestedBotProfileId.IsNone())
	{
		return;
	}

	const bool bFirstIsHuman = UMKHDuelParticipantLibrary::IsHumanParticipant(Players[0].Participant);
	DifficultyDirector = NewObject<UMKHDifficultyDirector>(this);
	DifficultyDirector->StartTracking(Players[bFirstIsHuman ? 0 : 1].Participant, Players[bFirstIsHuman ? 1 : 0].Participant, DifficultySettings);
}

void AMKHGameMode::ApplyDifficultyForNextRound()
{
	if (!IsValid(DifficultyDirector))
	{
		return;
	}

	const FName NextProfile = DifficultyDirector->EvaluateRound();
	if (AMKHDuelBotController* BotController = FindBotController(); IsValid(BotController))
	{
		BotController->SetProfile(FindBotProfile(NextProfile));
	}
}

void AMKHGameMode::ApplyWeaponsForNextRound() const
{
	if (!IsValid(DifficultyDirector))
	{
		return;
	}

	for (const FMKHPlayerData& Data : Players)
	{
		if (AMKHDuelBot* Bot = Cast<AMKHDuelBot>(Data.Participant.Get()); IsValid(Bot))
		{
			Bot->AssignWeaponQuickSlots(MKHBotWeapons::PickRandomPair(Bot->GetOwnedWeaponTags()));
		}
	}
}

void AMKHGameMode::ResetDifficultyForRematch()
{
	if (!IsValid(DifficultyDirector))
	{
		return;
	}

	DifficultyDirector->ResetMatch();
	if (AMKHDuelBotController* BotController = FindBotController(); IsValid(BotController))
	{
		BotController->SetProfile(FindBotProfile(DifficultyDirector->GetCurrentProfileId()));
	}
}

void AMKHGameMode::RecordCareerResult(const ACharacterBase* Winner) const
{
	if (MatchMode != EMKHMatchMode::OfflineBot || OfflineRequest.Mode != EMKHOfflineMode::Career
		|| !IsValid(Winner) || !UMKHDuelParticipantLibrary::IsHumanParticipant(Winner))
	{
		return;
	}

	if (UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this))
	{
		Career->RecordLevelWon(OfflineRequest.CareerLevelIndex);
	}
}

TArray<FGameplayTag> AMKHGameMode::ResolveBotWeapons(const TArray<FMKHStartingItem>& Loadout) const
{
	if (IsTrainingMatch())
	{
		return { TrainingDummyWeapon };
	}

	if (OfflineRequest.Mode == EMKHOfflineMode::Career)
	{
		const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
		const FMKHCareerLevel* Level = IsValid(Career) ? Career->FindLevel(OfflineRequest.CareerLevelIndex) : nullptr;
		return Level != nullptr ? Level->BotWeapons : TArray<FGameplayTag>();
	}

	TArray<FGameplayTag> Weapons;
	for (const FMKHStartingItem& Item : Loadout)
	{
		if (MKHBotWeapons::IsWeapon(Item))
		{
			Weapons.Add(Item.ItemTag);
		}
	}
	return MKHBotWeapons::PickRandomPair(Weapons);
}

TArray<FMKHStartingItem> AMKHGameMode::BuildBotLoadout(const AMKHDuelBot& Bot) const
{
	if (IsTrainingMatch())
	{
		if (!IsValid(TrainingDummyLoadoutTable))
		{
			UE_LOG(LogMKHAI, Error, TEXT("MKHGameMode: %s has no TrainingDummyLoadoutTable, the dummy gets no items."), *GetNameSafe(GetClass()));
			return {};
		}

		return FMKHStartingLoadoutRow::ReadLoadout(*TrainingDummyLoadoutTable);
	}

	const AMKHDuelBotController* BotController = Bot.GetController<AMKHDuelBotController>();
	const UMKHDuelBotProfile* Profile = IsValid(BotController) ? BotController->GetActiveProfile() : nullptr;
	if (!IsValid(Profile))
	{
		UE_LOG(LogMKHAI, Error, TEXT("MKHGameMode: %s has no active profile, it gets no items."), *Bot.GetName());
		return {};
	}

	return Profile->BuildLoadout();
}

TArray<FGameplayTag> AMKHGameMode::ChooseBotStartingWeapons(const TArray<FMKHStartingItem>& Loadout) const
{
	// A forced ?BotProfile= test keeps the table's own weapons.
	const bool bModeChoosesWeapons = IsTrainingMatch() || (MatchMode == EMKHMatchMode::OfflineBot && RequestedBotProfileId.IsNone());
	return bModeChoosesWeapons ? ResolveBotWeapons(Loadout) : TArray<FGameplayTag>();
}

AActor* AMKHGameMode::FindFreeStartSpotForBot() const
{
	AActor* AnyStart = nullptr;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsValid(Start))
		{
			continue;
		}

		if (!IsStartSpotTaken(Start, nullptr))
		{
			return Start;
		}
		AnyStart = AnyStart ? AnyStart : Start;
	}

	// A level with a single start: the spawn collision handling moves the bot next to the player.
	return AnyStart;
}
