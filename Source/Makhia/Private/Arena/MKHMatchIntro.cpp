// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Arena/MKHMatchIntro.h"

#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/MKHGameState.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MKHLogChannels.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "UI/HUD/HUDOverlay/MKHIntroSkipHintWidget.h"
#include "UI/HUD/MainHUD.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

AMKHMatchIntro::AMKHMatchIntro()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.1f;
	SetReplicates(false);
}

void AMKHMatchIntro::BeginPlay()
{
	Super::BeginPlay();

	// A dedicated server has no view to fly; every other peer plays it for its own local player.
	if (GetNetMode() == NM_DedicatedServer || Variants.IsEmpty())
	{
		bDone = true;
		return;
	}
	SetActorTickEnabled(true);
}

void AMKHMatchIntro::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &AMKHMatchIntro::HandleIntroFinished);
		SequencePlayer->Stop();
		SetHUDOverlayHidden(false);
	}
	EndSkipInput();
	Super::EndPlay(EndPlayReason);
}

void AMKHMatchIntro::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHoldingFirstFrame)
	{
		// Started under the loading screen: hold the first frame until it is gone, then fly.
		const UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(this);
		if (!IsValid(Loading) || !Loading->IsLoadingScreenActive())
		{
			bHoldingFirstFrame = false;
			SetActorTickEnabled(false);
			if (IsValid(SequencePlayer))
			{
				SequencePlayer->Play();
				BeginSkipInput();
			}
		}
		return;
	}

	WaitedSeconds += DeltaSeconds;
	FVector PawnLocation;
	if (IsReadyToPlay(PawnLocation))
	{
		bDone = true;
		SetActorTickEnabled(false);
		if (const FMKHMatchIntroVariant* Variant = FindVariant(PawnLocation))
		{
			PlayIntro(*Variant);
		}
		return;
	}

	if (WaitedSeconds > WaitTimeout)
	{
		UE_LOG(LogMKHArena, Log, TEXT("Match intro: gave up after %.0f s (no fresh initial preparation)."), WaitedSeconds);
		bDone = true;
		SetActorTickEnabled(false);
	}
}

// ============================================================
// Public Interface
// ============================================================

bool AMKHMatchIntro::IsIntroPlaying() const
{
	return IsValid(SequencePlayer);
}

void AMKHMatchIntro::SkipIntro()
{
	if (!IsValid(SequencePlayer))
	{
		return;
	}

	// GoToEndAndStop fires OnStop, not OnFinished: finish by hand on the last frame (the player camera).
	UE_LOG(LogMKHArena, Log, TEXT("Match intro: skipped."));
	bHoldingFirstFrame = false;
	SetActorTickEnabled(false);
	SequencePlayer->OnFinished.RemoveDynamic(this, &AMKHMatchIntro::HandleIntroFinished);
	SequencePlayer->GoToEndAndStop();
	HandleIntroFinished();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool AMKHMatchIntro::IsReadyToPlay(FVector& OutPawnLocation) const
{
	if (bDone)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(Pawn) || !PlayerController->IsLocalController() || !IsValid(GameState))
	{
		return false;
	}

	// The sequence restores the view target it found when it started: wait until it is the pawn, or a client
	// starting right after joining gets its controller back as view target (camera stuck in the pawn's body).
	if (PlayerController->GetViewTarget() != Pawn)
	{
		return false;
	}

	if (GameState->GetMatchPhase() != EMKHMatchPhase::InitialPreparation ||
		GameState->GetPhaseTimeRemaining() < MinPhaseTimeLeft)
	{
		return false;
	}

	OutPawnLocation = Pawn->GetActorLocation();
	return true;
}

const FMKHMatchIntroVariant* AMKHMatchIntro::FindVariant(const FVector& PawnLocation) const
{
	const FMKHMatchIntroVariant* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FMKHMatchIntroVariant& Variant : Variants)
	{
		const double Distance = FVector::DistSquared(Variant.EndLocation, PawnLocation);
		if (!Variant.Sequence.IsNull() && Distance < BestDistance)
		{
			Best = &Variant;
			BestDistance = Distance;
		}
	}
	return Best;
}

void AMKHMatchIntro::PlayIntro(const FMKHMatchIntroVariant& Variant)
{
	ULevelSequence* Sequence = Variant.Sequence.LoadSynchronous();
	if (!IsValid(Sequence))
	{
		UE_LOG(LogMKHArena, Warning, TEXT("Match intro: sequence %s not found."), *Variant.Sequence.ToString());
		return;
	}

	SetExteriorLoaded(true);

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = true;
	Settings.bDisableLookAtInput = true;
	Settings.bHideHud = true;
	ALevelSequenceActor* OutActor = nullptr;
	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, OutActor);
	SequenceActor = OutActor;
	if (!IsValid(SequencePlayer))
	{
		SetExteriorLoaded(false);
		return;
	}

	SetHUDOverlayHidden(true);
	SequencePlayer->OnFinished.AddDynamic(this, &AMKHMatchIntro::HandleIntroFinished);
	// Play then pause evaluates frame 0 (the camera cut takes the view); Tick resumes once the loading screen is gone.
	SequencePlayer->Play();
	SequencePlayer->Pause();
	bHoldingFirstFrame = true;
	SetActorTickEnabled(true);
	UE_LOG(LogMKHArena, Log, TEXT("Match intro: playing %s."), *Sequence->GetName());
}

void AMKHMatchIntro::HandleIntroFinished()
{
	EndSkipInput();
	SetHUDOverlayHidden(false);
	SetExteriorLoaded(false);
	if (IsValid(SequenceActor))
	{
		SequenceActor->Destroy();
	}
	SequencePlayer = nullptr;
	SequenceActor = nullptr;
	RestorePlayerView();
}

void AMKHMatchIntro::RestorePlayerView() const
{
	const UWorld* World = GetWorld();
	APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	if (!IsValid(Pawn) || PlayerController->GetViewTarget() == Pawn)
	{
		return;
	}

	UE_LOG(LogMKHArena, Log, TEXT("Match intro: view target was %s at the end, handed back to the pawn."),
		*GetNameSafe(PlayerController->GetViewTarget()));
	PlayerController->SetViewTarget(Pawn);
}

void AMKHMatchIntro::BeginSkipInput()
{
	const UWorld* World = GetWorld();
	AMKHPlayerController* PlayerController = IsValid(World) ? Cast<AMKHPlayerController>(World->GetFirstPlayerController()) : nullptr;
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMKHArena, Warning, TEXT("Match intro: no local MKH player controller, the intro cannot be skipped."));
		return;
	}

	SkipController = PlayerController;
	PlayerController->SetMatchIntroInputActive(true);
	SkipRequestedHandle = PlayerController->OnSkipIntroRequested.AddUObject(this, &AMKHMatchIntro::SkipIntro);
	SkipHoldHandle = PlayerController->OnSkipIntroHoldChanged.AddUObject(this, &AMKHMatchIntro::HandleSkipHoldChanged);

	if (IsValid(SkipHintClass))
	{
		SkipHint = CreateWidget<UMKHIntroSkipHintWidget>(PlayerController, SkipHintClass);
		if (IsValid(SkipHint))
		{
			SkipHint->SetSkipAction(PlayerController->GetSkipIntroAction());
			SkipHint->AddToViewport(SkipHintZOrder);
		}
	}
}

void AMKHMatchIntro::EndSkipInput()
{
	if (AMKHPlayerController* PlayerController = SkipController.Get())
	{
		// Removing a single handle is safe even from inside the broadcast that skipped the intro.
		PlayerController->OnSkipIntroRequested.Remove(SkipRequestedHandle);
		PlayerController->OnSkipIntroHoldChanged.Remove(SkipHoldHandle);
		PlayerController->SetMatchIntroInputActive(false);
	}
	SkipController.Reset();
	SkipRequestedHandle.Reset();
	SkipHoldHandle.Reset();

	if (IsValid(SkipHint))
	{
		SkipHint->RemoveFromParent();
	}
	SkipHint = nullptr;
}

void AMKHMatchIntro::HandleSkipHoldChanged(const float HoldFraction) const
{
	if (IsValid(SkipHint))
	{
		SkipHint->SetHoldProgress(HoldFraction);
	}
}

void AMKHMatchIntro::SetHUDOverlayHidden(bool bHideOverlay) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (AMainHUD* HUD = IsValid(PlayerController) ? PlayerController->GetHUD<AMainHUD>() : nullptr)
	{
		HUD->SetHUDOverlayHidden(bHideOverlay);
	}
}

void AMKHMatchIntro::SetExteriorLoaded(bool bLoaded)
{
	if (ExteriorLevel.IsNone() || bLoaded == bExteriorLoaded)
	{
		return;
	}

	// Unique latent ids: a second request on the same id would be dropped while the first is pending.
	FLatentActionInfo LatentInfo;
	LatentInfo.UUID = static_cast<int32>(GetUniqueID()) * 2 + (bLoaded ? 0 : 1);
	if (bLoaded)
	{
		// Blocking: the flight starts over the exterior, never over an empty world.
		UGameplayStatics::LoadStreamLevel(this, ExteriorLevel, true, true, LatentInfo);
	}
	else
	{
		UGameplayStatics::UnloadStreamLevel(this, ExteriorLevel, LatentInfo, false);
	}
	bExteriorLoaded = bLoaded;
}
