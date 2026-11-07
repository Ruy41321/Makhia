// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Arena/MKHArenaGate.h"

#include "Audio/MKHAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameMode/MKHGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

// ============================================================
// Lifecycle
// ============================================================

AMKHArenaGate::AMKHArenaGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	SetRootComponent(GateRoot);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(GateRoot);
	GateMesh->SetMobility(EComponentMobility::Movable);
	// The mesh has no collision of its own: GrilleCollision stands in for it.
	GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateMesh->SetCanEverAffectNavigation(false);
	// Deep in the shaded tunnel: no sun reaches it, and the room torches cast no shadows.
	GateMesh->SetCastShadow(false);

	// Rides with the grille: 5 x 4.6 m passage, 20 cm thick (Makhion gate arch). Closed, the box starts 10 cm under
	// the floor: edges lying on it z-fight with it in the editor viewport.
	GrilleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("GrilleCollision"));
	GrilleCollision->SetupAttachment(GateMesh);
	GrilleCollision->SetMobility(EComponentMobility::Movable);
	GrilleCollision->SetBoxExtent(FVector(10.f, 250.f, 235.f));
	GrilleCollision->SetRelativeLocation(FVector(0.f, 0.f, 225.f));
	GrilleCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// The bars are see-through: the camera boom must not snap in front of them.
	GrilleCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GrilleCollision->SetCanEverAffectNavigation(false);
}

void AMKHArenaGate::BeginPlay()
{
	Super::BeginPlay();

	LoadGateSounds();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (AGameStateBase* GameState = World->GetGameState())
	{
		BindGameState(GameState);
		return;
	}

	// On clients the game state may replicate after the level's actors begin play.
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &AMKHArenaGate::BindGameState);
}

void AMKHArenaGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameState();

	Super::EndPlay(EndPlayReason);
}

void AMKHArenaGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Duration = bTargetOpen ? OpenDuration : CloseDuration;
	const float Step = DeltaSeconds / FMath::Max(Duration, KINDA_SMALL_NUMBER);
	OpenAlpha = FMath::Clamp(OpenAlpha + (bTargetOpen ? Step : -Step), 0.f, 1.f);
	UpdateGrillePosition();

	if (OpenAlpha == (bTargetOpen ? 1.f : 0.f))
	{
		SetActorTickEnabled(false);
		// The travel sound plays out by itself; only the end-of-travel sound starts here.
		TravelAudio = nullptr;
		PlayGateSound(bTargetOpen ? RaisedSound : SlamSound, false);
	}
}

// ============================================================
// Public Interface
// ============================================================

void AMKHArenaGate::SetGateOpen(bool bOpen, bool bInstant)
{
	if (bOpen != bTargetOpen)
	{
		// Each direction has its own easing: keep the grille where it is when it turns back.
		const float Lift = ComputeLift(OpenAlpha);
		bTargetOpen = bOpen;
		OpenAlpha = FindAlphaForLift(Lift);

		StopTravelSound();
		if (!bInstant)
		{
			PlayGateSound(bTargetOpen ? RaiseSound : LowerSound, true);
		}
	}

	if (bInstant)
	{
		StopTravelSound();
		OpenAlpha = bTargetOpen ? 1.f : 0.f;
	}

	UpdateGrillePosition();
	SetActorTickEnabled(OpenAlpha != (bTargetOpen ? 1.f : 0.f));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHArenaGate::BindGameState(AGameStateBase* GameState)
{
	AMKHGameState* MatchState = Cast<AMKHGameState>(GameState);
	if (!IsValid(MatchState))
	{
		// Not a duel map (e.g. a test level): the gate stays as placed.
		return;
	}

	UnbindGameState();
	BoundGameState = MatchState;
	MatchState->OnMatchPhaseChanged.AddDynamic(this, &AMKHArenaGate::HandleMatchPhaseChanged);

	// Joining a phase already under way: no animation.
	ApplyMatchState(true);
}

void AMKHArenaGate::UnbindGameState()
{
	if (UWorld* World = GetWorld(); IsValid(World) && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();

	if (AMKHGameState* MatchState = BoundGameState.Get())
	{
		MatchState->OnMatchPhaseChanged.RemoveDynamic(this, &AMKHArenaGate::HandleMatchPhaseChanged);
	}
	BoundGameState.Reset();
}

void AMKHArenaGate::HandleMatchPhaseChanged(EMKHMatchPhase NewPhase)
{
	ApplyMatchState(false);
}

void AMKHArenaGate::ApplyMatchState(bool bInstant)
{
	const AMKHGameState* MatchState = BoundGameState.Get();
	if (!IsValid(MatchState))
	{
		return;
	}

	switch (MatchState->GetMatchPhase())
	{
	case EMKHMatchPhase::Duel:
	case EMKHMatchPhase::SuddenDeath:
	case EMKHMatchPhase::MatchOver:
		SetGateOpen(true, bInstant);
		break;
	default:
		SetGateOpen(false, bInstant);
		break;
	}
}

float AMKHArenaGate::ComputeLift(float Alpha) const
{
	const float Eased = bTargetOpen
		? FMath::SmoothStep(0.f, 1.f, Alpha)          // heavy grille hauled up: slow start and stop
		: 1.f - FMath::Square(1.f - Alpha);           // dropped: falls faster and faster
	return Eased * LiftHeight;
}

float AMKHArenaGate::FindAlphaForLift(float Lift) const
{
	// Both easings rise monotonically with Alpha: bisection.
	float Low = 0.f;
	float High = 1.f;
	for (int32 Iteration = 0; Iteration < 16; ++Iteration)
	{
		const float Mid = 0.5f * (Low + High);
		(ComputeLift(Mid) < Lift ? Low : High) = Mid;
	}
	return 0.5f * (Low + High);
}

void AMKHArenaGate::UpdateGrillePosition() const
{
	if (!IsValid(GateMesh))
	{
		return;
	}

	GateMesh->SetRelativeLocation(FVector(0.f, 0.f, ComputeLift(OpenAlpha)));
}

void AMKHArenaGate::LoadGateSounds()
{
	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	if (!IsValid(Settings) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Small one-shots loaded once per gate, so the first rise does not hitch.
	RaiseSound = Settings->GateRaiseSound.LoadSynchronous();
	RaisedSound = Settings->GateRaisedSound.LoadSynchronous();
	LowerSound = Settings->GateLowerSound.LoadSynchronous();
	SlamSound = Settings->GateSlamSound.LoadSynchronous();
	SoundAttenuation = Settings->GateSoundAttenuation.LoadSynchronous();
}

void AMKHArenaGate::PlayGateSound(USoundBase* Sound, bool bTravelling)
{
	if (!IsValid(Sound) || !IsValid(GateMesh))
	{
		return;
	}

	const UMKHAudioSettings* Settings = UMKHAudioSettings::Get();
	const float Volume = IsValid(Settings) ? Settings->GateSoundVolume : 1.f;

	// Attached at the grille's foot, so the sound follows it up and down.
	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(Sound, GateMesh, NAME_None, FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset, true, Volume, 1.f, 0.f, SoundAttenuation);
	if (bTravelling)
	{
		TravelAudio = Audio;
	}
}

void AMKHArenaGate::StopTravelSound()
{
	if (IsValid(TravelAudio))
	{
		TravelAudio->FadeOut(0.2f, 0.f);
	}
	TravelAudio = nullptr;
}
