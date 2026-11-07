# System Analysis

## Table of Contents

1. [Project Overview](#project-overview)
2. [Module Configuration](#module-configuration)
3. [Core Systems](#core-systems)
4. [Networking Architecture](#networking-architecture)
5. [Equipment System](#equipment-system)
6. [Inventory System](#inventory-system)
7. [UI Architecture](#ui-architecture)
8. [File Layout Reference](#file-layout-reference)

---

## Project Overview

Makhia is an Unreal Engine 5.7 C++ project structured as a single runtime module named `Makhia`. It implements an action MKH framework with:

- **Gameplay Ability System (GAS)** for attributes, abilities, and effects.
- A **component-based player character** with a finite state machine for movement.
- A **rarity-based equipment system** with stat and ability rolling.
- An **inventory system** with quick-slot support.
- A **UI layer** using Model-View-Controller separation via widget controllers.
- **Multiplayer support** with authority checks, server RPCs, and replicated state.

---

## Module Configuration

**File**: `Makhia.uproject`

```
Engine: 5.7
Module: Makhia (Runtime, Default loading phase)
Dependencies: Engine, GameplayAbilities, UMG, CoreUObject, EnhancedInput
Plugins: GameplayAbilities, GameplayStateTree, ModelingToolsEditorMode (editor only)
```

**Build dependencies** (`Makhia.Build.cs`) include `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `EnhancedInput`, `UMG`, and `Niagara`.

---

## Core Systems

### Character Hierarchy

```
ACharacter (Engine)
  └─ ACharacterBase            [IAbilitySystemInterface]
       ├─ AMKHPlayerCharacter     [IMKHAbilitySystemInterface]
       └─ AEnemyBase
```

`ACharacterBase` provides:
- GAS integration (`UMKHAbilitySystemComponent`, `UMKHAttributeSet`).
- Virtual initialisation hooks (`InitAbilityActorInfo`, `BindCallbacksToDependencies`, `InitClassDefaults`, `BroadcastInitialValues`).
- Attribute change delegates (`OnHealthChanged`, `OnStaminaChanged`).
- Stamina management via the `OutOfStamina` gameplay tag.

`AMKHPlayerCharacter` adds:
- Five movement components (Basic, Dodge, Crouch, Jump, Sprint).
- `UMovementStateMachine` for state management.
- Camera rig (spring arm + camera).
- `DynamicProjectileSpawnPoint` for ability projectile spawning.
- GAS ownership delegated to `AMKHPlayerState`.

`AEnemyBase` owns its ASC directly and uses `Minimal` replication mode.

### Gameplay Ability System

See [GASArchitecture.md](GASArchitecture.md) for full details. Key points:

- **Attribute set**: Health, MaxHealth, Armor, Stamina, MaxStamina, DodgeStaminaCost, CritChance, CritDamageMod, IncomingDamage (meta).
- **Damage pipeline**: Ability → `FDamageEffectInfo` → `ApplyDamageEffect` → `ExecCalc_Damage` (with crit roll) → `IncomingDamage` → `PostGameplayEffectExecute` → Health reduction (armor applied in the exec calc).
- **Armor mitigation**: permanent armor rating with diminishing returns — see [ArmorMitigation.md](ArmorMitigation.md).
- **Custom effect context**: `FMKHGameplayEffectContext` with serialised `bCriticalHit` flag.
- **Global override**: `UMKHAbilitySystemGlobals` ensures all effects use the custom context.

### Movement State Machine

See [CharacterSystemArchitecture.md](CharacterSystemArchitecture.md) for full details. Key points:

- 10 states defined in `EMovementStateValue`: None, Idle, Walking, Sprinting, CrouchingIdle, CrouchingMoving, Jumping, Falling, LandingInPlace, LandingMoving, Dodging.
- Each state is a `UMovementState` subclass with `EnterState`, `UpdateState`, `ExitState` lifecycle.
- Automatic transitions evaluated every tick via `GetDesiredTransition`.
- Movement gameplay tags (`State.Movement.*`) are added/removed on the ASC during state entry/exit.
- `OnStateChanged` delegate notifies animation, UI, and other systems.

### Input System

Makhia uses Enhanced Input with a tag-based configuration layer:

- `UMKHInputConfig`: Data asset mapping `UInputAction` → `FGameplayTag`.
- `UMKHSystemInputComponent`: Template method `BindAbilityActions` that filters actions by tag and binds them to `AbilityInputPressed`/`AbilityInputReleased` on the ASC.
- Movement component inputs are bound directly in `SetupPlayerInputComponent` by each component's `SetupInput` method.

### Animation

`UMKHMKHPlayerAnimInstance` subscribes to `UMovementStateMachine::OnStateChanged` and updates:
- `Speed` — used for movement blend spaces.
- `CurrentMovementState` / `PreviousMovementState` — used by animation state machines.
- `CrouchingTransitionTime` — 0–100 blend value for crouching pose.

---

## Networking Architecture

### Player GAS Replication

- ASC lives on `AMKHPlayerState` with **Mixed** replication mode.
- Network update frequency: 100 Hz primary, 66 Hz minimum.
- `PossessedBy` (server) initialises abilities and attributes; `OnRep_PlayerState` (client) initialises actor info.
- Abilities are predicted client-side; effects are replicated.

### Enemy GAS Replication

- ASC lives on `AEnemyBase` with **Minimal** replication mode.
- `bInitAttributes` is replicated; its `OnRep` triggers `BroadcastInitialValues` on clients.

### Equipment Replication

- `FMKHEquipmentList` uses `FFastArraySerializer` for efficient delta replication.
- Each `FMKHEquipmentEntry` is an `FFastArraySerializerItem` with pre/post replication callbacks.
- `GrantedHandles` (ability and effect handles) are **not replicated** — they are recreated on the owning client.
- `EquipItem` and `UnEquipItem` have authority checks; non-authority calls delegate to server RPCs.

### Dodge Replication

- `bIsDodging` is replicated on `UDodgeSystemComponent`.
- `StartDodge` has a Server RPC for non-authority clients.

---

## Equipment System

### Overview

The equipment system manages equipping/unequipping items, applying stat effects and abilities through GAS, and spawning visual actors.

### Key Classes

| Class | Location | Purpose |
|---|---|---|
| `UEquipmentManagerComponent` | `Public/Equipment/` | Manages the equipment list, handles equip/unequip flow |
| `UEquipmentDefinition` | `Public/Equipment/` | Data asset defining an equipment item's properties |
| `UEquipmentInstance` | `Public/Equipment/` | Runtime instance of equipped equipment |
| `AEquipmentActor` | `Public/Equipment/` | Visual representation spawned in the world |
| `FMKHEquipmentEntry` | `Public/Equipment/` | Replicated entry in the equipment list |
| `FEquipmentEffectPackage` | `Public/Equipment/` | Bundle of stat effects and abilities |
| `UEquipmentRollLibrary` | `Public/Libraries/` | Static library for rarity and stat rolling |
| `URarityDefinition` | `Public/Equipment/Rarity/` | Rarity tier configuration |

### Equip Flow

```
EquipItem(UInventoryItem)
  ├─ [Non-authority] → ServerEquipItem RPC
  └─ [Authority]
       ├─ BuildEquipmentEntry()
       │    ├─ Get UEquipmentDefinition CDO
       │    ├─ RollRarity()            → FRarityDefinition
       │    ├─ RollPassiveStats()      → TArray<FEquipmentStatEffectDefinition>
       │    ├─ RollActiveAbilities()   → TArray<FEquipmentAbilityDefinition>
       │    └─ Assemble FMKHEquipmentEntry
       └─ FMKHEquipmentList::AddEntry()
            ├─ Handle slot conflict (remove existing)
            ├─ Create UEquipmentInstance
            ├─ ASC->AddEquipmentEffects()
            ├─ ASC->AddEquipmentAbility()
            └─ Instance->SpawnEquipmentActors()
```

### Stat Rolling

`UEquipmentRollLibrary` uses weighted random selection:

1. Each `UEquipmentDefinition` defines `PossibleStatRolls` (gameplay tags).
2. Tags are looked up in the master stat DataTable via `UEquipmentStatEffects`.
3. Each candidate has a `ProbabilityToSelect` weight.
4. `WeightedRandomSelect` accumulates weights and selects based on a random roll.
5. Selected stats get their `CurrentValue` randomised between `MinStatLevel` and `MaxStatLevel`.
6. The number of stats to roll is determined by the rarity tier.

### Async Loading

Both stat effects and abilities use `TSoftClassPtr` with `FStreamableManager::RequestAsyncLoad`. This avoids synchronous loads of GameplayEffect and Ability classes that may not be in memory.

---

## Inventory System

### Key Classes

| Class | Location | Purpose |
|---|---|---|
| `UInventoryComponent` | `Public/Inventory/` | Manages the item array and CRUD operations |
| `UInventoryItem` | `Public/Inventory/InventoryItem/` | Represents a single item instance |
| `UItemTypesToTables` | `Public/Inventory/` | Maps item types to DataTables |
| `IInventoryInterface` | `Public/Interfaces/` | Interface to access the inventory component |

### Quick Slots

`UQuickSlotManagerComponent` manages quick-slot assignments using gameplay tags (`Equipment.Slot.QuickSlot.*`). It provides an interface (`IQuickSlotInterface`) for accessing quick-slot functionality from any actor.

---

## UI Architecture

### MVC Structure

```
Model (Data Source)           Controller               View (Widget)
───────────────────          ──────────────           ──────────────
ACharacterBase          ←→   UWidgetController    →   UMKHSystemWidget
  OnHealthChanged            UHUDOverlayController     UHUDOverlayWidget
  OnStaminaChanged           UInventoryDashController   UInventoryDashboardWidget
UEquipmentManagerComponent
UInventoryComponent
```

### UWidgetController (Base)

Located at `Source/Makhia/Public/UI/WidgetControllers/WidgetController.h`:

- `SetOwningActor(AActor*)` — connects to game data.
- `BindCallbacksToDependencies()` — subscribes to data changes.
- `BindDelegatesToWidget()` — Blueprint Native Event that connects controller outputs to widget inputs.
- `BroadcastInitialValues()` — pushes current values on setup.
- `UnbindAllEventsFromDelegates()` — cleanup.

### MainHUD

`AMainHUD` (inherits `AHUD`) is the entry point for the HUD. It creates and manages the widget hierarchy and associated controllers.

---

## File Layout Reference

```
Source/Makhia/
├── Makhia.Build.cs / Makhia.h / Makhia.cpp         Module definition
├── Public/
│   ├── AbilitySystem/                           GAS classes (see GASArchitecture.md)
│   ├── Actors/                                  Effect actors
│   ├── Character/                               CharacterBase, EnemyBase
│   ├── Data/                                    Data assets (ClassInfo, ProjectileInfo, StatEffects)
│   ├── Equipment/                               Equipment system (Definition, Instance, Manager, Types)
│   │   └── Rarity/                              Rarity definitions
│   ├── GameMode/                                MKHGameMode
│   ├── Input/                                   MKHInputConfig, MKHSystemInputComponent
│   ├── Interfaces/                              UE Interfaces (Ability, Equipment, Inventory, QuickSlot)
│   ├── Inventory/                               Inventory system
│   │   └── InventoryItem/                       Inventory item class
│   ├── Libraries/                               Blueprint function libraries
│   ├── Player/
│   │   ├── Components/                          Movement components (5 subdirectories)
│   │   ├── MovementStateMachine/                State machine + base state + types
│   │   │   └── States/                          10 concrete movement states
│   │   ├── PlayerAnimation/                     MKHPlayerAnimInstance
│   │   ├── PlayerController/                    MKHPlayerController
│   │   └── PlayerState/                         MKHPlayerState (ASC owner)
│   ├── Projectiles/                             MKHProjectileBase
│   ├── QuickSlot/                               QuickSlotManagerComponent
│   ├── UI/
│   │   ├── HUD/                                 MainHUD, HUDOverlay, Inventory dashboard
│   │   ├── MKHSystemWidget.h                    Base widget class
│   │   └── WidgetControllers/                   MVC controllers
│   └── Utils/                                   Utility helpers
└── Private/
    └── (mirrors Public/ with .cpp implementations)
```


