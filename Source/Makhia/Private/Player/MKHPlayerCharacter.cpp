// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Player/MKHPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"

#include "Character/Components/Crouch/CrouchSystemComponent.h"
#include "Character/Components/BasicMovement/BasicMovementComponent.h"
#include "Character/Components/Jump/JumpSystemComponent.h"
#include "Character/Components/Sprint/SprintSystemComponent.h"
#include "Character/MovementStateMachine/MovementStateMachine.h"

#include "Player/PlayerState/MKHPlayerState.h"
#include "Cosmetics/MKHSkinComponent.h"
#include "GameInstance/MKHGameInstance.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHAbilityTypes.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Net/UnrealNetwork.h"

AMKHPlayerCharacter::AMKHPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	UCharacterMovementComponent *const mov = GetCharacterMovement();
	if (mov)
	{
		mov->bOrientRotationToMovement = true;
	}
	bUseControllerRotationYaw = false;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Player Camera"));
	Camera->SetupAttachment(CameraBoom);

	SkinComponent = CreateDefaultSubobject<UMKHSkinComponent>(TEXT("Skin Component"));

}

void AMKHPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// Ensure mesh replicates smoothly on clients
	if (GetMesh())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void AMKHPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMKHPlayerCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent *EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Delegate input setup to each component
		if (BasicMovementSystem)
			BasicMovementSystem->SetupInput(EnhancedInputComponent);

		if (SprintSystem)
			SprintSystem->SetupInput(EnhancedInputComponent);

		if (JumpSystem)
			JumpSystem->SetupInput(EnhancedInputComponent);

		if (CrouchSystem)
			CrouchSystem->SetupInput(EnhancedInputComponent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerInputComponent is not of type UEnhancedInputComponent"));
	}
}

void AMKHPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AMKHPlayerCharacter, Username);
}

void AMKHPlayerCharacter::SetUsername(const FString& NewUsername)
{
	// Only the authority may commit the replicated value; a client routes it through the server.
	if (!HasAuthority())
	{
		Server_SetUsername(NewUsername);
		return;
	}

	if (Username == NewUsername)
	{
		return;
	}

	Username = NewUsername;

	// OnRep never runs on the authority, so the server notifies itself here. Clients are notified
	// by OnRep_Username once the new value replicates in.
	OnUsernameChanged.Broadcast(Username);
}

void AMKHPlayerCharacter::Server_SetUsername_Implementation(const FString& NewUsername)
{
	// Re-enter through the setter so the commit and the notification stay in one place.
	SetUsername(NewUsername);
}

void AMKHPlayerCharacter::OnRep_Username()
{
	OnUsernameChanged.Broadcast(Username);
}

void AMKHPlayerCharacter::OnHitInflicted(const FGameplayEventData* GameplayEventData)
{
	if (!GameplayEventData)
	{
		return;
	}
	
	bool bIsCritical = false;
	FMKHGameplayEffectContext* MKHContext = FMKHGameplayEffectContext::GetEffectContext(GameplayEventData->ContextHandle);
	if (MKHContext)
	{
		bIsCritical = MKHContext->IsCriticalHit();
	}
	float Damage = GameplayEventData->EventMagnitude;
	FTransform Transform = GameplayEventData->Target->GetActorTransform();
	ShowFloatingDamageText(Transform, Damage, bIsCritical);
}

void AMKHPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority())
	{
		InitAbilityActorInfo();
		BroadcastInitialValues();
		MKHAbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::HitInflicted)
																.AddUObject(this, &AMKHPlayerCharacter::OnHitInflicted);
	}

	// Authority path (listen-server host): the controller is now set, so the local game instance
	// username can be committed directly.
	TrySeedUsernameFromLocalGameInstance();
	ApplySkinFromPlayerState();
}

void AMKHPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
	BroadcastInitialValues();
	ApplySkinFromPlayerState();
}

void AMKHPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Re-init only when a previous init cached a null PlayerController (PlayerState
	// replicated first). The guard keeps the common path from re-running the init and
	// double-binding attribute delegates. When the ASC is still null the controller arrived
	// first: OnRep_PlayerState will run the init with the controller already in place.
	if (IsValid(MKHAbilitySystemComponent)
		&& MKHAbilitySystemComponent->AbilityActorInfo.IsValid()
		&& !MKHAbilitySystemComponent->AbilityActorInfo->PlayerController.IsValid())
	{
		InitAbilityActorInfo();
	}

	// Owning client: the controller has now replicated, so the pawn is locally controlled and owned
	// by the local player controller — the ownership required to route the username server RPC.
	TrySeedUsernameFromLocalGameInstance();
}

void AMKHPlayerCharacter::TrySeedUsernameFromLocalGameInstance()
{
	// Run once, and only for the pawn the local player actually controls: a simulated proxy would
	// otherwise seed the wrong machine's username, and its server RPC would be dropped for lack of ownership.
	if (bUsernameSeeded || !IsLocallyControlled())
	{
		return;
	}

	if (const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this))
	{
		bUsernameSeeded = true;
		SetUsername(GameInstance->GetUsername());
	}
}

void AMKHPlayerCharacter::Multicast_ResetToSpawnTransform_Implementation(const FTransform& SpawnTransform)
{
	// Clear leftover velocity and pending input first: with bOrientRotationToMovement the movement
	// component keeps steering the actor towards its acceleration, which would undo the rotation
	// we are about to apply within the very next frame.
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->StopMovementImmediately();
	}

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ApplySpawnViewRotation(SpawnTransform.Rotator());
}

void AMKHPlayerCharacter::ResetForNewRound(const FTransform& SpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	// Multicast because a server-only transform change never restores the rotation on the owning client (see
	// Multicast_ResetToSpawnTransform); the forced transition is the only way out of the Dead state.
	Multicast_ResetToSpawnTransform(SpawnTransform);
	Multicast_TransitionToMovementState(EMovementStateValue::Idle, true);
}

int32 AMKHPlayerCharacter::GetParticipantCareerLevels() const
{
	const AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>();
	return IsValid(MKHPlayerState) ? MKHPlayerState->GetCareerLevels() : INDEX_NONE;
}

FText AMKHPlayerCharacter::GetParticipantDisplayName() const
{
	// Player names are data, never translated.
	if (!Username.IsEmpty())
	{
		return FText::AsCultureInvariant(Username);
	}

	// Not signed in (offline match): no account name, the local player is simply "You".
	return IsLocallyControlled() ? NSLOCTEXT("MKHPlayerCharacter", "LocalPlayerName", "You") : FText::GetEmpty();
}

void AMKHPlayerCharacter::ApplySpawnViewRotation(const FRotator& SpawnRotation)
{
	AController* OwningController = GetController();
	if (!IsValid(OwningController) || (!IsLocallyControlled() && !HasAuthority()))
	{
		return;
	}

	// Yaw only: pitch and roll belong to the camera, not to the spawn orientation.
	OwningController->SetControlRotation(FRotator(0.f, SpawnRotation.Yaw, 0.f));
}

void AMKHPlayerCharacter::ApplySkinFromPlayerState()
{
	const AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>();
	if (!IsValid(MKHPlayerState) || !IsValid(SkinComponent) || MKHPlayerState->GetSkinId().IsNone())
	{
		return;
	}

	SkinComponent->SetSkin(MKHPlayerState->GetSkinId());
}

void AMKHPlayerCharacter::InitAbilityActorInfo()
{
	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>())
	{
		MKHAbilitySystemComponent = MKHPlayerState->GetRPGAbilitySystemComponent();
		MKHAttributeSet = MKHPlayerState->GetRPGAttributeSet();

		if (IsValid(MKHAbilitySystemComponent))
		{
			MKHAbilitySystemComponent->InitAbilityActorInfo(MKHPlayerState, this);
			BindCallbacksToDependencies();

			// The movement state machine sets its initial state during BeginPlay,
			// before the ASC is available, so the gameplay tag was never added.
			// Sync it now that the ASC is ready.
			if (MovementStateMachine)
			{
				MovementStateMachine->SyncCurrentStateTagToASC();
			}

			if (HasAuthority())
			{
				InitClassDefaults();
			}
			
			// Notify the local machine that the player character is fully initialized and ready to be used.
			OnPlayerCharacterReady.Broadcast();
		}
	}
}

void AMKHPlayerCharacter::InitClassDefaults()
{
	// The shared logic (AddCharacterAbilities, InitializeDefaultAttributes, ...) lives in ACharacterBase::InitClassDefaults.
	Super::InitClassDefaults();
}
