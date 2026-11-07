# Design Patterns Used

## Table of Contents

1. [State Pattern — Movement State Machine](#1-state-pattern--movement-state-machine)
2. [Observer Pattern — State Change Delegates](#2-observer-pattern--state-change-delegates)
3. [Component Pattern — Movement System Composition](#3-component-pattern--movement-system-composition)
4. [Strategy Pattern — Execution Calculations](#4-strategy-pattern--execution-calculations)
5. [Template Method — Character Initialisation](#5-template-method--character-initialisation)
6. [Abstract Factory — State Object Creation](#6-abstract-factory--state-object-creation)
7. [Mediator Pattern — Player Character](#7-mediator-pattern--player-character)
8. [Interface Segregation — UE Interfaces](#8-interface-segregation--ue-interfaces)
9. [Model-View-Controller (MVC) — UI System](#9-model-view-controller-mvc--ui-system)
10. [Facade Pattern — Blueprint Libraries](#10-facade-pattern--blueprint-libraries)
11. [Flyweight / Data-Driven — Data Assets](#11-flyweight--data-driven--data-assets)
12. [Builder Pattern — Equipment Entry Construction](#12-builder-pattern--equipment-entry-construction)

---

## 1. State Pattern — Movement State Machine

### Where

- `UMovementStateMachine` — `Source/Makhia/Public/Character/MovementStateMachine/MovementStateMachine.h`
- `UMovementState` (base) — `Source/Makhia/Public/Character/MovementStateMachine/MovementState.h`
- 10 concrete states — `Source/Makhia/Public/Character/MovementStateMachine/States/`

### How

The state machine holds a pointer to the current `UMovementState` object. Each concrete state (e.g., `UIdleMovementState`, `USprintingMovementState`, `UDodgingMovementState`) overrides:

| Virtual Method | Purpose |
|---|---|
| `GetStateType()` | Returns the enum value identifying this state |
| `CanTransitionTo(NewState)` | Validates whether a transition is allowed |
| `GetDesiredTransition()` | Returns automatic transitions (e.g., Falling → LandingInPlace) |
| `EnterState(PreviousState)` | Called on entry; sets speed, adds ASC tag |
| `UpdateState(DeltaTime)` | Per-frame logic |
| `ExitState(NextState)` | Called on exit; removes ASC tag |

The state machine never contains `if (currentState == Walking)` branches. All behaviour differences are encapsulated in the state objects themselves.

### Why

- Eliminates monolithic switch statements for movement behaviour.
- Each state is independently testable and can be overridden in Blueprint.
- New states can be added without modifying existing state classes (Open/Closed Principle).

---

## 2. Observer Pattern — State Change Delegates

### Where

- `UMovementStateMachine::OnStateChanged` delegate
- `ACharacterBase::OnHealthChanged`, `OnStaminaChanged` delegates
- `UMKHAbilitySystemComponent::OnAttributesGiven` delegate

### How

The state machine broadcasts an `FOnMovementStateChanged` dynamic multicast delegate whenever a transition occurs. Subscribers bind to this delegate without the state machine knowing who they are.

**Publisher** (state machine):
```cpp
OnStateChanged.Broadcast(OldState, NewState);
```

**Subscriber** (animation instance):
```cpp
StateMachine->OnStateChanged.AddDynamic(this, &UMKHMKHPlayerAnimInstance::OnMovementStateChanged);
```

The same pattern is used for attribute changes: `ACharacterBase` broadcasts `OnHealthChanged` and `OnStaminaChanged` when GAS attribute callbacks fire. UI widget controllers subscribe to these to update HUD elements.

### Why

- Decouples the state machine from all consumers (animation, audio, UI, effects).
- New subscribers can be added without modifying the publisher.
- Works with both C++ (`AddDynamic`) and Blueprint (`Bind Event`).

---

## 3. Component Pattern — Movement System Composition

### Where

`AMKHPlayerCharacter` composes its movement capabilities from five independent `UActorComponent` subclasses:

| Component | Responsibility |
|---|---|
| `UBasicMovementComponent` | WASD movement and look input |
| `UDodgeSystemComponent` | Dodge mechanics and cooldown |
| `UCrouchSystemComponent` | Crouch toggle and collision checks |
| `UJumpSystemComponent` | Jump input and landing detection |
| `USprintSystemComponent` | Sprint toggle and interruption |

### How

Each component:
- Is created in the `AMKHPlayerCharacter` constructor as a default subobject.
- Exposes a `SetupInput(UEnhancedInputComponent*)` method called from `SetupPlayerInputComponent`.
- Reads and modifies shared state through the owning `AMKHPlayerCharacter` reference.
- Coordinates with other components through that shared reference (e.g., DodgeSystem asks CrouchSystem for `CanUncrouchSafely()`).

### Why

- Each mechanic can be developed, tested, and iterated independently.
- Components can be added or removed per-character (e.g., an NPC might have basic movement but no dodge).
- Properties of each system are grouped logically in the editor Details panel.

---

## 4. Strategy Pattern — Execution Calculations

### Where

- `UExecCalc_Damage` — `Source/Makhia/Public/AbilitySystem/ExecCalc/ExecCalc_Damage.h`
- `UExecCalc_DodgeCost` — `Source/Makhia/Public/AbilitySystem/ExecCalc/ExecCalc_DodgeCost.h`

### How

GAS `UGameplayEffectExecutionCalculation` subclasses define interchangeable algorithms for effect calculations. A `UGameplayEffect` asset references one of these execution calculations in its configuration. At application time, the engine calls `Execute_Implementation()` on the referenced calculation.

- **ExecCalc_Damage**: Reads base damage from `SetByCaller`, captures CritChance and CritDamageMod from the source, rolls for critical hits, and outputs to `IncomingDamage`.
- **ExecCalc_DodgeCost**: Reads stamina attributes, calculates the dodge cost clamped to available stamina, and outputs the deduction.

### Why

- Damage calculation logic is separated from the effect definition — the same effect class can use different calculations.
- New calculation strategies (e.g., elemental damage, armour penetration) can be added without modifying existing ones.
- Calculations are pure functions (no side effects on the ASC), making them predictable and reusable.

---

## 5. Template Method — Character Initialisation

### Where

- `ACharacterBase` declares the virtual hooks: `InitAbilityActorInfo()`, `BindCallbacksToDependencies()`, `InitClassDefaults()`, `BroadcastInitialValues()`.
- `AMKHPlayerCharacter` and `AEnemyBase` override these hooks to provide class-specific logic.

### How

The base class defines the skeleton of the initialisation algorithm. Subclasses override individual steps:

```
ACharacterBase (template)
  InitAbilityActorInfo()           → blank in base
  BindCallbacksToDependencies()    → binds the Health delegate
  InitClassDefaults()              → loads abilities/attributes from CharacterClassInfo
  BroadcastInitialValues()         → broadcasts the initial Health value

AMKHPlayerCharacter (override)
  InitAbilityActorInfo()           → gets ASC from PlayerState, calls Super hooks
  BindCallbacksToDependencies()    → calls Super, adds Stamina delegate
  BroadcastInitialValues()         → calls Super, adds Stamina broadcast

AEnemyBase (override)
  InitAbilityActorInfo()           → creates ASC on self
  BindCallbacksToDependencies()    → calls Super, adds OnAttributesGiven lambda
  BroadcastInitialValues()         → calls Super
```

### Why

- Common initialisation logic is written once in the base class.
- Subclasses only override the parts that differ (where the ASC lives, which extra callbacks to bind).
- The order of operations is enforced by the base class.

---

## 6. Abstract Factory — State Object Creation

### Where

`UMovementStateMachine::InitializeDefaultStates()` and `CreateStateObject()`.

### How

The state machine maintains a `TMap<EMovementStateValue, TSubclassOf<UMovementState>> DefaultStateClasses` that maps each state enum value to its concrete class. During initialisation, it iterates this map and calls `CreateStateObject()` for each entry.

```cpp
void UMovementStateMachine::CreateStateObject(EMovementStateValue StateType, TSubclassOf<UMovementState> StateClass)
{
    UMovementState* NewState = NewObject<UMovementState>(this, StateClass);
    NewState->Initialize(this, OwnerPlayerCharacter);
    StateObjects.Add(StateType, NewState);
}
```

The `DefaultStateClasses` map is marked `EditDefaultsOnly`, so designers can replace any default state class with a Blueprint subclass without touching C++.

### Why

- Creation logic is centralised; adding a new state requires only one map entry.
- Blueprint subclasses can replace C++ states entirely through data.
- The state machine does not depend on concrete state types — it works through the `UMovementState` interface.

---

## 7. Mediator Pattern — Player Character

### Where

`AMKHPlayerCharacter` acts as the mediator between movement components, the state machine, and the camera system.

### How

Components do not reference each other directly. Instead, they go through `AMKHPlayerCharacter`:

```
DodgeSystem needs CrouchSystem → DodgeSystem->GetOwner<AMKHPlayerCharacter>()->CrouchSystem->CanUncrouchSafely()
SprintSystem needs StateMachine → SprintSystem->GetOwner<AMKHPlayerCharacter>()->TransitionToMovementState(Sprinting)
```

The player character provides helper methods that abstract the internal wiring:

```cpp
void SetMaxWalkSpeed(float Speed);
EMovementStateValue GetCurrentMovementState();
bool TransitionToMovementState(EMovementStateValue NewState, bool bForce);
```

### Why

- Components are loosely coupled — replacing the crouch system does not require modifying the dodge system.
- The player character class serves as the single coordination point, making the system easier to understand.
- Cross-component dependencies are explicit and localised.

---

## 8. Interface Segregation — UE Interfaces

### Where

Four Unreal Interfaces are used to segregate capabilities:

| Interface | Provides | Implemented By |
|---|---|---|
| `IAbilitySystemInterface` | `GetAbilitySystemComponent()` | `ACharacterBase`, `AMKHPlayerState` |
| `IMKHAbilitySystemInterface` | `GetDynamicSpawnPoint()`, `SetDynamicProjectile()` | `AMKHPlayerCharacter` |
| `IEquipmentInterface` | `GetEquipmentManagerComponent()` | Player controller or character |
| `IInventoryInterface` | `GetInventoryComponent()` | Player controller or character |

### How

Interfaces allow systems to query capabilities without depending on concrete classes. For example, the projectile ability obtains its spawn point through the interface:

```cpp
USceneComponent* SpawnPoint = IMKHAbilitySystemInterface::Execute_GetDynamicSpawnPoint(AvatarActor);
```

This works regardless of whether `AvatarActor` is a player character, an NPC, or any other class that implements the interface.

### Why

- GAS abilities do not need to know about `AMKHPlayerCharacter` specifically.
- Equipment and inventory systems can be used by any actor type that implements the interface.
- Each interface is small and focused (one or two methods), following the Interface Segregation Principle.

---

## 9. Model-View-Controller (MVC) — UI System

### Where

- **Model**: `ACharacterBase` (attribute delegates), `UEquipmentManagerComponent` (equipment data)
- **View**: `UMKHSystemWidget` and subclasses (HUD overlay, inventory dashboard)
- **Controller**: `UWidgetController` and subclasses (`UHUDOverlayController`, `UInventoryDashboardController`)

### How

The controller layer (`UWidgetController`) sits between the game data and the UI widgets:

1. `SetOwningActor(AActor*)` — connects the controller to a game actor.
2. `BindCallbacksToDependencies()` — subscribes to model events (attribute changes, equipment changes).
3. `BindDelegatesToWidget()` — connects controller delegates to widget update functions.
4. `BroadcastInitialValues()` — pushes current state to the widget on setup.

When an attribute changes:
```
UMKHAttributeSet → ACharacterBase::OnHealthChanged → UHUDOverlayController → UHUDOverlayWidget
```

### Why

- Widgets do not access game logic directly — they react to controller events.
- Controllers can be tested independently from UI rendering.
- The same controller can drive different widget implementations (e.g., compact vs. full HUD).

---

## 10. Facade Pattern — Blueprint Libraries

### Where

- `UMKHAbilitySystemLibrary` — `Source/Makhia/Public/Libraries/MKHAbilitySystemLibrary.h`
- `UEquipmentRollLibrary` — `Source/Makhia/Public/Libraries/EquipmentRollLibrary.h`

### How

These `UBlueprintFunctionLibrary` subclasses provide simplified static interfaces to complex subsystems:

**MKHAbilitySystemLibrary**:
- `GetCharacterClassDefaultInfo()` — hides GameMode casting.
- `GetProjectileInfo()` — hides GameMode casting.
- `ApplyDamageEffect()` — encapsulates effect context creation, `SetByCaller` assignment, and application.
- `GetDataTableRowByTag<T>()` — templated helper for tag-based DataTable lookups.

**EquipmentRollLibrary**:
- `RollRarity()` — weighted probability selection from a DataTable.
- `RollPassiveStats()` — builds candidate pools and performs weighted random selection.
- `RollActiveAbilities()` — same pattern for ability rolling.

### Why

- Blueprints (and C++ callers) get a clean, one-call API instead of multi-step procedures.
- Implementation details (casting, iteration, weighted selection) are hidden behind meaningful method names.
- Common operations are centralised, reducing duplication.

---

## 11. Flyweight / Data-Driven — Data Assets

### Where

- `UCharacterClassInfo` — maps `FGameplayTag` → `FCharacterClassDefaultInfo` (default attributes, abilities, passives).
- `UProjectileInfo` — maps `FGameplayTag` → `FProjectileParams`.
- `UEquipmentDefinition` — defines equipment properties, possible stat rolls, and possible ability rolls.
- `UMKHInputConfig` — maps `UInputAction` → `FGameplayTag`.
- `UEquipmentStatEffects` — master stat map of `FGameplayTag` → `UDataTable`.
- `URarityDefinition` — rarity tiers with probability weights and stat/ability counts.

### How

All character classes, projectile types, equipment definitions, and input bindings are configured through `UDataAsset` instances edited in the Unreal Editor. The code references these assets by tag or type rather than hard-coding values.

For example, adding a new character class requires:
1. Create a `UGameplayEffect` asset for default attributes.
2. Add an entry to the `UCharacterClassInfo` map with the class tag and effect.
3. Set the `CharacterTag` on the character Blueprint.

No C++ changes needed.

### Why

- Designers can add content without programmer involvement.
- Shared data (e.g., projectile parameters) is defined once and referenced by tag.
- Data can be hot-reloaded in the editor for rapid iteration.

---

## 12. Builder Pattern — Equipment Entry Construction

### Where

`UEquipmentManagerComponent::BuildEquipmentEntry()` — `Source/Makhia/Private/Equipment/EquipmentManagerComponent.cpp`

### How

Constructing an `FMKHEquipmentEntry` involves multiple steps:

1. Look up the `UEquipmentDefinition` CDO from the inventory item.
2. Roll a rarity via `UEquipmentRollLibrary::RollRarity()`.
3. Roll passive stats via `UEquipmentRollLibrary::RollPassiveStats()`.
4. Roll active abilities via `UEquipmentRollLibrary::RollActiveAbilities()`.
5. Assemble the `FEquipmentEffectPackage`.
6. Set entry metadata (tags, rarity, item ID).

The `BuildEquipmentEntry` method orchestrates this multi-step construction and returns a fully configured entry ready for use by `AddEntry`.

### Why

- Separates the complex construction process from the representation.
- Each construction step (rarity, stats, abilities) can evolve independently.
- The resulting entry is immutable after construction — all data is set before it enters the equipment list.

---

## Pattern Summary

| Pattern | Where Applied | Benefit |
|---|---|---|
| **State** | Movement State Machine | Eliminates conditional branches; each state encapsulates its own behaviour |
| **Observer** | Delegates on StateMachine, CharacterBase, ASC | Decouples publishers from subscribers |
| **Component** | Movement components on MKHPlayerCharacter | Each mechanic is independent and swappable |
| **Strategy** | Execution Calculations (Damage, DodgeCost) | Interchangeable calculation algorithms |
| **Template Method** | Character init chain (Base → Player/Enemy) | Shared skeleton with customisable steps |
| **Abstract Factory** | State machine DefaultStateClasses map | Blueprint-replaceable state creation |
| **Mediator** | MKHPlayerCharacter coordinating components | Reduces direct component coupling |
| **Interface Segregation** | UE Interfaces (Ability, Equipment, Inventory) | Small, focused capability contracts |
| **MVC** | Widget + WidgetController + CharacterBase | Separates UI from game logic |
| **Facade** | Blueprint libraries (AbilitySystem, EquipmentRoll) | Simplifies complex subsystem access |
| **Flyweight/Data-Driven** | Data Assets (ClassInfo, ProjectileInfo, etc.) | Designer-editable configuration |
| **Builder** | Equipment entry construction | Multi-step object assembly |


