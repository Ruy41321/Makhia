# Character System Architecture

## Table of Contents

1. [Overview](#overview)
2. [Class Hierarchy](#class-hierarchy)
3. [ACharacterBase](#acharacterbase)
4. [AMKHPlayerCharacter](#amkhplayercharacter)
5. [AEnemyBase and the Duel Bot](#aenemybase-and-the-duel-bot)
6. [Movement Components](#movement-components)
7. [Movement State Machine](#movement-state-machine)
8. [Animation System](#animation-system)
9. [Camera Setup](#camera-setup)
10. [Input System](#input-system)
11. [Adding a New Movement State](#adding-a-new-movement-state)
12. [Adding a New Observer](#adding-a-new-observer)

---

## Overview

The Makhia character system uses a **component-based architecture** combined with a **finite state machine** for movement management and an **observer pattern** for state-change notifications. This design separates each movement ability into its own component, delegates state management to a dedicated state machine, and notifies dependent systems (such as animation) through delegates.

The whole movement framework (components, state machine, states, anim instance) lives on **`ACharacterBase`**, so the player and the offline duel bot (`AMKHDuelBot`) use the same moves with the same rules. Only what is truly player-specific stays on `AMKHPlayerCharacter` (camera, Enhanced Input bindings, username, skin from the player state, ASC from the player state, floating damage text, spawn/view reset).

Key principles:
- **Single Responsibility**: Each component handles exactly one movement mechanic.
- **Open/Closed**: New movement states can be added without modifying existing state classes.
- **Decoupled Communication**: Systems react to state changes through delegates, not direct coupling.
- **Same moves for every character**: components, states and abilities only depend on `ACharacterBase`; nothing in the movement framework includes `MKHPlayerCharacter.h`.
- **Blueprint Accessibility**: All major properties and functions are exposed for designer iteration.

---

## Class Hierarchy

```
ACharacter (Engine)
  └─ ACharacterBase                  [IAbilitySystemInterface]   movement framework, stamina, crowd control
       ├─ AMKHPlayerCharacter        [IMKHDuelParticipant]       camera, input, ASC on AMKHPlayerState
       └─ AEnemyBase                                             ASC on the actor (Minimal)
            └─ AMKHDuelBot           [IMKHDuelParticipant]       offline duel opponent (AI)
```

---

## ACharacterBase

**Location**: `Source/Makhia/Public/Character/CharacterBase.h`

The base class shared by players and AI. It provides the GAS integration point, the stamina and crowd-control rules and the whole movement framework.

### Responsibilities

- Implements `IAbilitySystemInterface::GetAbilitySystemComponent()`.
- Declares virtual hooks for GAS initialisation: `InitAbilityActorInfo()`, `BindCallbacksToDependencies()`, `InitClassDefaults()`, `BroadcastInitialValues()`.
- `BindCallbacksToDependencies()` (rebind-safe) binds Health, **Stamina** (`HandleStaminaChanged()` manages the `OutOfStamina` tag and the regeneration, then `OnStaminaChanged`) and **crowd control**: `Stunned` / `Staggered` call `HandleCrowdControlTagChanged()`, which forces an uncrouch and interrupts a jump. Bot and player pay and suffer the same rules.
- **Stamina regeneration** (server only, `UpdateStaminaRegen`, moved from `BP_MKHPlayerCharacter`'s graph in M8.1 so the bot regenerates too): any stamina spend removes the running regeneration (effects granting `State.General.Stamina.Recovery`) and restarts the `StaminaRegenDelay` timer (1 s); when it fires, `StaminaRegenEffectClass` (`GE_Status_StaminaRegen`, set on `BP_MKHPlayerCharacter` and `BP_DuelBot`) is applied unless the character blocks; full stamina removes it. The block holds it: `BP_BlockingSystemComp` removes it when a block starts and applies it again when the block ends.
- Creates the movement components and the state machine in its constructor and exposes the movement API (below).
- `Landed()` hands the landing to `JumpSystem->OnLanded()`.

### Movement Components Created in Constructor

The subobject names are the same for every subclass (and were the player's before the framework moved here), so Blueprint children keep their component overrides (input actions, speeds, component class overrides) by name.

```cpp
TObjectPtr<UDodgeSystemComponent> DodgeSystem;              // "Dodge System Component"
TObjectPtr<UCrouchSystemComponent> CrouchSystem;            // "Crouch System Component"
TObjectPtr<UBlockingSystemComponent> BlockingSystem;        // "Blocking System Component"
TObjectPtr<UBasicMovementComponent> BasicMovementSystem;    // "Basic Movement Component"
TObjectPtr<UJumpSystemComponent> JumpSystem;                // "Jump System Component"
TObjectPtr<USprintSystemComponent> SprintSystem;            // "Sprint System Component"
TObjectPtr<UMovementStateMachine> MovementStateMachine;     // "Movement State Machine"
bool bIsHoldingWeapon;                                      // Replicated, switches the locomotion blendspace
```

`BP_MKHPlayerCharacter` replaces the class of "Blocking System Component" with `BP_BlockingSystemComp` (component class override: stamina regeneration paused while blocking). `BP_DuelBot` copies the player's non-input tuning (walk speed 100, run speed 400, sprint as default, `BlockAbilityClass` = `GA_Block`) and must use the same component class (set in the Blueprint editor: component class overrides are not reachable from Python/MCP).

### Public Helpers

| Method | Description |
|---|---|
| `SetMaxWalkSpeed(float)` | Sets `CharacterMovement->MaxWalkSpeed` and `MaxWalkSpeedCrouched` |
| `GetCurrentMovementState()` / `GetPreviousMovementState()` | Read the state machine (None without one) |
| `TransitionToMovementState(NewState, bForce)` | Calls `MovementStateMachine->TransitionToState()` on this machine |
| `Multicast_TransitionToMovementState(NewState, bForce)` | Same on every machine (death, revive) |
| `GetMovementStateMachine()` / `UnsubscribeFromMovementStateChanges()` | State machine access |
| `Multicast_StopMontage(Montage, BlendOut)` | Stops one montage on every machine (cancelled abilities) |

### Key Properties

```cpp
UPROPERTY(EditAnywhere, Category = "Custom Values | Character Info")
FGameplayTag CharacterTag;                              // Identifies the character class for data lookup

TObjectPtr<UMKHAbilitySystemComponent> MKHAbilitySystemComponent;
TObjectPtr<UMKHAttributeSet> MKHAttributeSet;
```

### Delegates

| Delegate | Signature | Fired When |
|---|---|---|
| `OnHealthChanged` | `(float OldHealth, float CurrentHealth, float MaxHealth)` | Health attribute changes |
| `OnStaminaChanged` | `(float OldStamina, float CurrentStamina, float MaxStamina)` | Stamina attribute changes |

---

## AMKHPlayerCharacter

**Location**: `Source/Makhia/Public/Player/MKHPlayerCharacter.h`

The player-controlled character. It owns the camera rig and the skin component, binds the Enhanced Input actions of the inherited movement components, and delegates GAS ownership to `AMKHPlayerState`.

### Components Created in Constructor

```cpp
TObjectPtr<USpringArmComponent> CameraBoom;
TObjectPtr<UCameraComponent> Camera;
TObjectPtr<UMKHSkinComponent> SkinComponent;
```

### GAS Initialisation

The ASC and attribute set are obtained from `AMKHPlayerState` in `InitAbilityActorInfo()`:

```
PossessedBy() [Server]
  └─ InitAbilityActorInfo()
       ├─ ASC = PlayerState->GetMKHAbilitySystemComponent()
       ├─ AttributeSet = PlayerState->GetMKHAttributeSet()
       ├─ ASC->InitAbilityActorInfo(PlayerState, this)
       ├─ BindCallbacksToDependencies()          (ACharacterBase: health, stamina, crowd control)
       ├─ MovementStateMachine->SyncCurrentStateTagToASC()
       └─ InitClassDefaults()  [Authority only]

OnRep_PlayerState() [Client]
  └─ InitAbilityActorInfo()   (same flow, minus InitClassDefaults)
```

The state machine enters its first state in `BeginPlay`, before the player state's ASC is known, hence the tag sync.

### Input Setup

`SetupPlayerInputComponent` casts to `UEnhancedInputComponent` and delegates binding to each movement component's `SetupInput` method.

### Round Reset

`ResetForNewRound` (server) multicasts `Multicast_ResetToSpawnTransform` and `Multicast_TransitionToMovementState(Idle, true)`, the only way out of Dead. Death itself is `GA_Die` → `Multicast_TransitionToMovementState(Dead, true)`.

**Death presentation: no ragdoll.** Players and the duel bot never ragdoll: the Dead state of `ABP_MKHPlayerCharacter` plays the `A_Die` animation sequence on every machine (`A_Die_Montage` exists but nothing uses it). Death sounds and effects that must follow the fall go on `A_Die` as anim notifies (see `AudioSystemArchitecture.md`).

---

## AEnemyBase and the Duel Bot

**Location**: `Source/Makhia/Public/Character/EnemyBase.h`, `Source/Makhia/Public/AI/MKHDuelBot.h`

AI-controlled characters. Unlike the player, enemies own their ASC directly. They have the full movement framework of `ACharacterBase` (`BP_Enemy_Primary` too: it simply stays Idle).

### Key Differences from Player

| Aspect | Player | Enemy |
|---|---|---|
| ASC Owner | `AMKHPlayerState` | `AEnemyBase` itself |
| ASC Actor Info | `InitAbilityActorInfo(PlayerState, this)` | `InitAbilityActorInfo(this, this)` |
| Replication Mode | `Mixed` | `Minimal` |
| Init Trigger | `PossessedBy` / `OnRep_PlayerState` | `BeginPlay` (every machine, after the components' BeginPlay) |
| Movement input | Enhanced Input (`SetupInput`) | AI path following (`CharacterMovementComponent` acceleration) |

Enemies replicate a `bInitAttributes` flag so clients can broadcast initial values after attribute setup completes server-side. `AEnemyBase::InitAbilityActorInfo` also calls `SyncCurrentStateTagToASC()` like the player.

### Duel bot death and revive

`AMKHDuelBot` dies through the movement state machine, like the player: `HandleDeathEvent` (`Event.Combat.Death`, server) → `SetDead(true)` → `Multicast_TransitionToMovementState(Dead, true)`. `UDeadState` adds `State.Movement.Dead` (read by `AMKHDuelBotController::IsCharacterDead`, the damage pipeline and ability blocking) and switches the capsule to the dead collision on every machine. The bot only adds its own presentation: the replicated `bIsDead` flag and the optional ragdoll (`bRagdollOnDeath`, off on `BP_DuelBot`: it shares `ABP_MKHPlayerCharacter` with the player, so the Dead state plays the same death animation; a ragdoll would replace it). The revive (`ResetForNewRound` → `SetDead(false)`) forces `Idle`, which restores the Pawn collision. A single source for the Dead tag avoids two writers (state machine loose tag + replicated loose tag count) overwriting each other.

---

## Movement Components

All movement components inherit from `UActorComponent` and follow a consistent pattern: each component receives input (players) or is driven by abilities and the state machine (AI), manages its own state, and coordinates with other components through the shared **`ACharacterBase`** owner (`GetValidCharacter()`, BlueprintCallable `GetOwnerCharacter()`).

### UBasicMovementComponent

**Location**: `Source/Makhia/Public/Character/Components/BasicMovement/BasicMovementComponent.h`

- Processes WASD/gamepad movement input and mouse/gamepad look input (players only: never bound on AI).
- Tracks whether the character has active movement input (used by other systems).
- Owns the walk speed (`WalkSpeed`) used by the state machine for the default states.

### UDodgeSystemComponent

**Location**: `Source/Makhia/Public/Character/Components/Dodge/DodgeSystemComponent.h`

- Handles dodge initiation, direction blending, and cooldown.
- Checks the current movement state against a dodgeable-state whitelist (`Idle`, `CrouchingIdle`, `CrouchingMoving`, `Walking`, `Sprinting`, `Blocking`, `Attacking`, `Dodging`).
- Crouches the character for the dodge and stands it up afterwards **only if the dodge really crouched it** (`bCrouchedForDodge`: the crouch is refused from Attacking and Blocking, and a dodge from a crouch stays crouched). When the headroom is blocked at the end (e.g. the opponent's capsule), the stand up stays pending and is retried every tick (`StandUpAfterDodge`, log `has no headroom to stand up, retrying`).
- Failsafe: a dodge still running after `MaxDodgeDuration` (2 s) is reset with a warning (`GA_Dodge` resets it only in its `OnEndAbility`).
- Replicates `bIsDodging` for multiplayer.
- **Direction source**:
  - local player (`IsLocallyControlled() && IsPlayerControlled()`): Enhanced Input from `BasicMovementSystem`, blended with the initial direction during the dodge (`InputInfluenceFactor`);
  - AI: `SetRequestedDodgeDirection(FVector)` before `StartDodge` (e.g. from `GA_Dodge`'s caller); the dodge then keeps that direction for its whole duration, no blending. Without a request, the AI falls back to the character's acceleration, like remote proxies;
  - the movement input is applied where the character is locally controlled (owning client, or the server for an AI).

### UCrouchSystemComponent

**Location**: `Source/Makhia/Public/Character/Components/Crouch/CrouchSystemComponent.h`

- Toggles crouching with collision-safe uncrouch checks.
- Provides `CanUncrouchSafely()` used by Sprint, Dodge and Blocking: a capsule overlap at standing height on `ECC_Pawn`, ignoring the character, the actors attached to it (its weapons) and **other pawns** (duelists often touch; the opponent's capsule left a character crouched after a dodge next to it, M8.1; the character movement then stands up as soon as there is room); the blocking component is logged (`LogMKHMovement` Verbose).
- Manages crouch speed settings. Server RPCs (`ServerCrouch`, `ServerUnCrouch`) are only sent by non-authority callers.

### UBlockingSystemComponent

**Location**: `Source/Makhia/Public/Character/Components/Blocking/BlockingSystemComponent.h`

- `StartBlocking` / `StopBlocking` (called by `GA_Block`), replicated `bWantsToBlock`, enters/leaves the `Blocking` state.
- `ApplyBlockingStateModifiers` / `RemoveBlockingStateModifiers` (BlueprintNativeEvent, extended by `BP_BlockingSystemComp` for stamina recovery).

### UJumpSystemComponent

**Location**: `Source/Makhia/Public/Character/Components/Jump/JumpSystemComponent.h`

- Handles jump input and delegates to `ACharacter::Jump()`.
- Tracks landing state and timing for animation.
- Receives `Landed()` callbacks from `ACharacterBase`.

### USprintSystemComponent

**Location**: `Source/Makhia/Public/Character/Components/Sprint/SprintSystemComponent.h`

- Toggles sprinting on/off (`bSprintAsDefault` inverts the key: sprint is the default pace).
- Automatically uncrouches when sprint starts (with collision check).
- Prevents sprinting while dodging.

### Component Cooperation

Components coordinate through the shared `ACharacterBase` reference:

**Sprint ↔ Crouch**: Sprint auto-uncrouches if `CanUncrouchSafely()` returns true; otherwise sprint is cancelled.

**Dodge ↔ Crouch**: Dodge records whether it crouched the character itself (`bCrouchedForDodge`, never a blind toggle) and stands it up on dodge end, retrying while the headroom is blocked; a character crouched before the dodge stays crouched (`bWasCrouchingPreDodge`). A player walks away from a blocked spot; the bot has nobody to do it, and `Root > Uncrouch` would retry forever from the same place, so `MKHSetCrouchTask` calls `UCrouchSystemComponent::TryStandUpWithNudge` (server only): it steps the bot to the nearest spot within 40 cm where the standing capsule fits and logs the blocker.

**Dodge ↔ State Machine**: Before dodging, the component checks `MovementStateMachine->GetCurrentState()` against the dodgeable-state whitelist.

The automatic transitions between Idle, Walking, Sprinting and the crouch states use the `CharacterMovementComponent` velocity, not the input: an AI's path following drives them exactly like a player's input.

### UMKHCharacterMovementComponent (pawn stacking)

**Location**: `Source/Makhia/Public/Character/Components/CharacterMovement/MKHCharacterMovementComponent.h`

`ACharacterBase` replaces the engine `CharacterMovement` subobject with this class (`SetDefaultSubobjectClass` in its constructor; the constructor takes a defaulted `FObjectInitializer`, so subclasses keep their default constructors and Blueprint children keep their `CharacterMovement` settings).

**Problem it solves:** a character that jumps (or drops from a ledge) onto the other one's head stood on it, or rested on the top of the capsule and slid off slowly in the falling animation. The engine rejects a pawn as a *base* (`CanCharacterStepUpOn = No` → `JumpOff` in a random direction) but still accepts it as a *landing spot*, and when the landing is refused the falling slide on the almost flat top of a capsule is very slow. Lowering the jump would not help: ledges in the map give the same height.

**Rule:**
- `IsValidLandingSpot` returns false for any pawn hit: a character never lands on another one.
- `HandleImpact` records a falling contact on the upper part of another pawn (contact normal up component ≥ `PawnTopMinNormalZ`, default 0.3; side contacts are left to the engine).
- The next `PhysFalling` adds horizontal speed away from that pawn (centre to centre, own facing when aligned) up to `PawnPushOffSpeed` (default 350 cm/s), keeping the vertical speed. It is applied before the step because the engine rewrites `Velocity` after an impact.

The push depends only on positions, so server and owning client predict the same move. It applies to players and the duel bot alike. Tune the two properties on the `CharacterMovement` component of the character Blueprints.

---

## Movement State Machine

### UMovementStateMachine

**Location**: `Source/Makhia/Public/Character/MovementStateMachine/MovementStateMachine.h`

An `UActorComponent` on `ACharacterBase` that manages a map of `EMovementStateValue` → `UMovementState*` objects. It runs on every machine (server, owner, simulated proxies).

**Tick behaviour**:
1. Calls `UpdateState(DeltaTime)` on the current state.
2. Calls `EvaluateStateTransitions()` which asks the current state for a `GetDesiredTransition()`. If the returned value is not `None`, it attempts a transition.

**Transition logic**:
- `TransitionToState(NewState, bForceTransition)` returns `false` if already in the target state or if the current state's `CanTransitionTo()` returns `false` (unless forced).
- `PerformStateTransition(NewState)` calls `ExitState` on the old state, updates `PreviousState`/`CurrentState`, calls `EnterState` on the new state, and broadcasts `OnStateChanged`.
- `SyncCurrentStateTagToASC()` adds the current state's tag once the ASC is ready (called by the characters' `InitAbilityActorInfo`).

### EMovementStateValue

**Location**: `Source/Makhia/Public/Character/MovementStateMachine/MovementStateTypes.h`

```cpp
UENUM(BlueprintType)
enum class EMovementStateValue : uint8
{
    None, Idle, Walking, Sprinting, CrouchingIdle, CrouchingMoving, Jumping, Falling,
    LandingInPlace, LandingMoving, Dodging, Blocking, Attacking, Dead
};
```

### UMovementStateTypes Utilities

Static helper class with:
- `MovementStateToString(EMovementStateValue)` — enum to display string.
- `IsGroundedState()` — returns `true` for Idle, Walking, Sprinting, CrouchingIdle, CrouchingMoving, LandingInPlace, LandingMoving, Dodging, Blocking, Attacking.
- `IsAirborneState()` — returns `true` for Jumping, Falling.
- `CanReceiveMovementInput()` — returns `true` for Idle, Walking, Sprinting, CrouchingIdle, CrouchingMoving, Jumping, Falling, Blocking, Attacking.

### UMovementState (Base)

**Location**: `Source/Makhia/Public/Character/MovementStateMachine/MovementState.h`

Abstract base for all movement states. `Initialize(UMovementStateMachine*, ACharacterBase*)` stores the owner; states read it through `GetOwnerCharacter()` (BlueprintPure).

**Lifecycle**:
- `EnterState(PreviousState)` — Sets movement speed via `SetStateSpeed()`, adds the state's gameplay tag to the ASC, calls the Blueprint event `OnEnterState`.
- `UpdateState(DeltaTime)` — Calls the Blueprint event `OnUpdateState`.
- `ExitState(NextState)` — Removes the state's gameplay tag from the ASC, calls the Blueprint event `OnExitState`.

**Transition hooks** (BlueprintNativeEvent):
- `CanTransitionTo(NewState)` — Default returns `true`; subclasses override to restrict transitions.
- `GetDesiredTransition()` — Default returns `None`; subclasses override for automatic transitions.

**Speed management** (`GetSpeedForState`):
| State | Speed |
|---|---|
| Dodging | `DodgeSystem->GetDodgeSpeed()` |
| CrouchingIdle / CrouchingMoving | `CrouchSystem->GetCrouchSpeed()` |
| Sprinting | `SprintSystem->GetRunSpeed()` |
| Blocking | `BlockingSystem->GetBlockingSpeed()` |
| Attacking / Dead | 0 |
| All others | `BasicMovementSystem->GetWalkSpeed()` |

### Concrete State Classes

Located in `Source/Makhia/Public/Character/MovementStateMachine/States/`:

| State Class | Auto-Transitions To |
|---|---|
| `UIdleMovementState` | Walking, Sprinting, Jumping, Falling, Dodging, CrouchingIdle/Moving, Attacking |
| `UWalkingMovementState` | Idle, Sprinting, Jumping, Falling, Dodging, CrouchingIdle/Moving, Attacking |
| `USprintingMovementState` | Walking, Idle, Jumping, Falling, Dodging, CrouchingIdle/Moving, Attacking |
| `UCrouchingIdleMovementState` | CrouchingMoving, Idle, Walking, Dodging, Blocking, Attacking |
| `UCrouchingMovingMovementState` | CrouchingIdle, Walking, Sprinting, Idle, Dodging, Blocking, Attacking |
| `UJumpingMovementState` | Falling, landing and ground states, Attacking |
| `UFallingMovementState` | LandingInPlace, LandingMoving, Blocking, Attacking |
| `ULandingInPlaceMovementState` | Idle, CrouchingIdle, Blocking, Attacking |
| `ULandingMovingMovementState` | Walking, Sprinting, Idle, CrouchingMoving, Blocking, Attacking |
| `UDodgingMovementState` | Ground states, Blocking, Attacking (when the dodge ends) |
| `UBlockingState` | Idle, Jumping, Falling, Dodging, Attacking (priority) |
| `UAttackingMovementState` | Idle, Blocking (when `Ability.Attacking` ends) |
| `UDeadState` | none: only a forced transition leaves it (revive) |

`UDeadState` makes the capsule ignore everything but the world, and on the machine where a **player** locally controls the character swaps to the dead input mapping (`AMKHPlayerController::SetGameplayInputEnabled`); AI-controlled characters have no input to block.

### Observer Pattern — OnStateChanged

The state machine exposes a dynamic multicast delegate:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChanged,
    EMovementStateValue, OldState,
    EMovementStateValue, NewState);

UPROPERTY(BlueprintAssignable, Category = "Movement State Machine")
FOnMovementStateChanged OnStateChanged;
```

Any system can subscribe. The animation instance (`UMKHPlayerAnimInstance`) is the primary subscriber:

```cpp
void UMKHPlayerAnimInstance::SubscribeToMovementStateChanges()
{
    OwnerCharacter->GetMovementStateMachine()->OnStateChanged.AddDynamic(
        this, &UMKHPlayerAnimInstance::OnMovementStateChanged);
}
```

`UnsubscribeFromStateChanges(UObject*)` removes all bindings for a given subscriber.

---

## Animation System

**Class**: `UMKHPlayerAnimInstance` (inherits `UAnimInstance`, parent of `ABP_MKHPlayerCharacter`)
**Location**: `Source/Makhia/Public/Character/Animation/MKHPlayerAnimInstance.h`

Despite its name it animates every `ACharacterBase`: `BP_MKHPlayerCharacter` and `BP_DuelBot` both use `ABP_MKHPlayerCharacter`.

### Key Properties

| Property | Type | Purpose |
|---|---|---|
| `OwnerCharacter` | `ACharacterBase*` | Pawn owner, cached at `NativeBeginPlay` |
| `CurrentSpeed` / `Direction` | `float` | Velocity size and direction for blend spaces |
| `bIsHoldingWeapon` | `bool` | Copied from the owner, switches the locomotion blendspace |
| `CurrentMovementState` | `EMovementStateValue` | Current state for animation state machine |
| `PreviousMovementState` | `EMovementStateValue` | Previous state for transition logic |
| `CrouchingTransitionTime` | `float` | 0–100 blend value for crouching pose |

### Lifecycle

1. **`NativeBeginPlay`**: Gets the `ACharacterBase` owner, initialises the state to None, subscribes to state changes.
2. **`NativeUpdateAnimation`**: Speed, direction, `bIsHoldingWeapon`.
3. **`OnMovementStateChanged`**: Updates state properties, snaps the crouch transition value.
4. **`NativeUninitializeAnimation`**: Unsubscribes from state changes.

---

## Camera Setup

`AMKHPlayerCharacter` creates a standard third-person camera rig in its constructor:

```cpp
CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
CameraBoom->SetupAttachment(RootComponent);
CameraBoom->bUsePawnControlRotation = true;

Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Player Camera"));
Camera->SetupAttachment(CameraBoom);
```

The character uses `bUseControllerRotationYaw = false` with `CharacterMovement->bOrientRotationToMovement = true` so the character rotates to face the direction of movement while the camera follows the controller rotation. The duel bot uses the same orientation settings. Abilities that face the attack (`UMKHDamageAbility::FaceCharacterTowardsAttack`, `SetCharacterOrientation`) use the control rotation: for an AI it follows the controller's focus (`SetFocus` on the opponent).

**Characters never block the camera boom.** `ACharacterBase::IgnoreCameraCollision` sets the capsule and the mesh to ignore `ECC_Camera` and the custom `CameraObstacle` channel (`ECC_MKHCameraObstacle` in `MKHCollisionChannels.h`): the `Camera Boom` of `BP_MKHPlayerCharacter` probes `CameraObstacle`, not `ECC_Camera`, so ignoring only `ECC_Camera` has no effect on it (called in `PostInitializeComponents`, so it wins over the Blueprint component settings). Otherwise the boom probe hits the other character and shortens/lengthens every frame it moves: the clearest case was a dead player's camera snapping in and out while the opponent (or the bot) crouch-spammed over the corpse. `SetCollisionProfileName` resets the per-channel response, so every profile change on those components calls it again (`UDeadState::ExitState` on revive, `AMKHDuelBot::SetMeshRagdoll`).

---

## Input System

Makhia uses the Enhanced Input system. Each movement component exposes a `SetupInput(UEnhancedInputComponent*)` method called from `AMKHPlayerCharacter::SetupPlayerInputComponent`. AI characters never call it: they move through path following and act through abilities.

Components bind their specific `UInputAction` properties (set via Blueprint/editor) to trigger events:
- `ETriggerEvent::Started` / `ETriggerEvent::Triggered` for input start.
- `ETriggerEvent::Completed` for input release.

For ability inputs, `UMKHSystemInputComponent::BindAbilityActions` iterates an `UMKHInputConfig` data asset, filters by a parent gameplay tag, and binds each action to `AbilityInputPressed`/`AbilityInputReleased` on the ASC.

---

## Adding a New Movement State

### Step 1 — Add the Enum Value

In `MovementStateTypes.h`, add the new value to `EMovementStateValue`:

```cpp
enum class EMovementStateValue : uint8
{
    // ... existing values ...
    Dead              UMETA(DisplayName = "Dead"),
    YourNewState      UMETA(DisplayName = "Your New State")
};
```

### Step 2 — Create the State Class

Create `YourNewStateMovementState.h` and `.cpp` in `Source/Makhia/Public/Character/MovementStateMachine/States/`. Read the owner through `GetOwnerCharacter()` (an `ACharacterBase`): the state must work for players and AI.

```cpp
UCLASS(BlueprintType, Blueprintable)
class MAKHIA_API UYourNewStateMovementState : public UMovementState
{
    GENERATED_BODY()

public:
    virtual EMovementStateValue GetStateType() const override
    {
        return EMovementStateValue::YourNewState;
    }

    virtual EMovementStateValue GetDesiredTransition_Implementation() const override;
    virtual bool CanTransitionTo_Implementation(EMovementStateValue NewState) const override;

    virtual void EnterState(EMovementStateValue PreviousState) override;
    virtual void UpdateState(float DeltaTime) override;
    virtual void ExitState(EMovementStateValue NextState) override;
};
```

### Step 3 — Register the State

In `MovementStateMachine.cpp`, add to `InitializeDefaultStates()`:

```cpp
DefaultStateClasses.Add(EMovementStateValue::YourNewState, UYourNewStateMovementState::StaticClass());
```

### Step 4 — Update Utilities

In `MovementStateTypes.cpp`, add the new state to `MovementStateToString`, `IsGroundedState`, `IsAirborneState`, and `CanReceiveMovementInput` as appropriate.

### Step 5 — Add a Gameplay Tag

In `MKHGameplayTags.h/.cpp`, declare and define a tag for the new state:

```cpp
UE_DECLARE_GAMEPLAY_TAG_EXTERN(YourNewState);
UE_DEFINE_GAMEPLAY_TAG_COMMENT(YourNewState, "State.Movement.YourNewState", "Description");
```

Then add the mapping in `UMovementState::GetTagForState`.

---

## Adding a New Observer

To subscribe a new system to movement state changes:

```cpp
UCLASS()
class UMySystem : public UObject
{
    GENERATED_BODY()

public:
    void Subscribe(ACharacterBase* Character)
    {
        Character->GetMovementStateMachine()->OnStateChanged.AddDynamic(
            this, &UMySystem::OnMovementStateChanged);
    }

    UFUNCTION()
    void OnMovementStateChanged(EMovementStateValue OldState, EMovementStateValue NewState)
    {
        // React to state change
    }
};
```

For cleanup, call `RemoveDynamic` or use `MovementStateMachine->UnsubscribeFromStateChanges(this)`.

Blueprint subscribers can bind to `OnStateChanged` directly in the Event Graph.
