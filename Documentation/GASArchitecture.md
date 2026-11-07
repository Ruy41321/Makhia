# Gameplay Ability System (GAS) Architecture

## Table of Contents

1. [Overview](#overview)
2. [Directory Structure](#directory-structure)
3. [Initialization Flow](#initialization-flow)
4. [Ability System Component](#ability-system-component)
5. [Input Binding & Ability Queue](#input-binding--ability-queue)
6. [Ability Grant Payload](#ability-grant-payload)
7. [Attribute Set](#attribute-set)
8. [Gameplay Abilities](#gameplay-abilities)
9. [Execution Calculations](#execution-calculations)
10. [Custom Effect Context](#custom-effect-context)
11. [Damage Pipeline](#damage-pipeline)
12. [Equipment Integration](#equipment-integration)
13. [Gameplay Tags](#gameplay-tags)
14. [Data Assets](#data-assets)

---

## Overview

Makhia uses Unreal Engine's **Gameplay Ability System (GAS)** to manage character attributes (Health, Armor, Stamina), combat logic, equipment effects, and ability activation. The GAS integration is built on several custom classes that extend the default engine types:

| Custom Class | Engine Base | Purpose |
|---|---|---|
| `UMKHAbilitySystemComponent` | `UAbilitySystemComponent` | Ability granting, tag-based input routing, input queue, equipment integration |
| `UMKHAttributeSet` | `UAttributeSet` | Character attributes, damage/stamina-damage resolution, death event |
| `UMKHGameplayAbility` | `UGameplayAbility` | Base ability: input tags, montage orchestration, priority tags, queue hooks |
| `UMKHDamageAbility` | `UMKHGameplayAbility` | Damage-dealing abilities: attack data, combos, skill cooldowns |
| `UMKHMeleeAbility` | `UMKHDamageAbility` | Melee attacks: weapon hit scans + DMT soft-homing |
| `UMKHProjectileAbility` | `UMKHDamageAbility` | Projectile-spawning abilities |
| `UMKHAbilityGrantPayload` | `UObject` | Per-grant configuration attached to `FGameplayAbilitySpec::SourceObject` |
| `UMKHAbilityTask_ApplyDMT` | `UAbilityTask` | Ticking task rotating/moving the character towards the best melee target |
| `UExecCalc_Damage` | `UGameplayEffectExecutionCalculation` | Damage computation: block check, dodge check, crit roll |
| `UExecCalc_DodgeCost` | `UGameplayEffectExecutionCalculation` | Stamina cost for dodging |
| `FMKHGameplayEffectContext` | `FGameplayEffectContext` | Extended context: critical hit, dodge flag, on-hit status effects |
| `UMKHAbilitySystemGlobals` | `UAbilitySystemGlobals` | Allocates the custom effect context |

Logging for this layer uses the dedicated `LogMKHAbility` category (`MKHLogChannels.h`).

---

## Directory Structure

```
Source/Makhia/
├── Public/AbilitySystem/
│   ├── MKHAbilitySystemComponent.h
│   ├── MKHAbilitySystemGlobals.h
│   ├── MKHAbilityGrantPayload.h     # Per-grant data via Spec.SourceObject
│   ├── MKHAbilityTuning.h           # FAttackData + FMKHAbilityTuningRow (balancing/AI table rows)
│   ├── MKHAbilityTypes.h            # FMKHGameplayEffectContext, FProjectileParams, FDamageEffectInfo
│   ├── MKHGameplayTags.h            # Native gameplay tag declarations
│   ├── Abilities/
│   │   ├── MKHGameplayAbility.h     # Base ability
│   │   ├── MKHDamageAbility.h       # Damage-dealing ability base
│   │   ├── MKHMeleeAbility.h        # Melee + hit scan + DMT
│   │   └── MKHProjectileAbility.h   # Projectile-spawning ability
│   ├── Attributes/
│   │   └── MKHAttributeSet.h        # Character attributes + damage resolution
│   ├── ExecCalc/
│   │   ├── ExecCalc_Damage.h        # Damage computation (block/dodge/crit)
│   │   └── ExecCalc_DodgeCost.h     # Stamina cost for dodging
│   ├── Tasks/
│   │   └── MKHAbilityTask_ApplyDMT.h
│   └── Cue/
│       └── MkhCameraShakeGcn_Burst.h
├── Public/Libraries/
│   └── MKHAbilitySystemLibrary.h    # ApplyDamageEffect, class-info accessors, skill tag assignment
├── Public/MKHLogChannels.h          # LogMKHAbility category
└── Public/Data/
    ├── CharacterClassInfo.h         # Per-class ability/attribute defaults
    ├── ProjectileInfo.h             # Projectile configuration data
    └── GenericClassReference.h      # Named BP class references (GE_Damage, skill cooldown)
```

---

## Initialization Flow

GAS components are initialised at different points depending on whether the character is a player or an AI enemy.

### Player Characters

For player characters the ASC and attribute set live on the **Player State** (`AMKHPlayerState`), not on the character itself. This follows the recommended GAS pattern for multiplayer games where the Player State persists across respawns. `AMKHPlayerState` raises its net update frequency (100 Hz, min 66 Hz) to keep the ASC responsive in the 1v1 setting.

```
AMKHPlayerState (constructor)
  ├─ Creates UMKHAbilitySystemComponent (Replicated, Mixed mode)
  └─ Creates UMKHAttributeSet

AMKHPlayerCharacter::PossessedBy()          [Server]
  └─ InitAbilityActorInfo()
       ├─ Gets ASC and AttributeSet from PlayerState
       ├─ Calls ASC->InitAbilityActorInfo(PlayerState, this)
       ├─ BindCallbacksToDependencies()   (Health, Stamina delegates)
       ├─ MovementStateMachine->SyncCurrentStateTagToASC()
       └─ InitClassDefaults()             [Authority only]
            ├─ Loads CharacterClassInfo by CharacterTag
            ├─ ASC->AddCharacterAbilities(StartingAbilities)
            ├─ ASC->AddCharacterPassiveAbilities(StartingPassives)
            └─ ASC->InitializeDefaultAttributes(DefaultAttributes)

AMKHPlayerCharacter::OnRep_PlayerState()    [Client]
  └─ InitAbilityActorInfo()              (same flow, minus InitClassDefaults)
```

### Enemy Characters

Enemies own their ASC and attribute set directly:

```
AEnemyBase (constructor)
  ├─ Creates UMKHAbilitySystemComponent (Replicated, Minimal mode)
  └─ Creates UMKHAttributeSet

AEnemyBase::BeginPlay()
  ├─ BindCallbacksToDependencies()
  └─ InitAbilityActorInfo()
       ├─ ASC->InitAbilityActorInfo(this, this)
       └─ InitClassDefaults()            [Authority only]
```

**Replication modes**:
- **Player**: `Mixed` — the owner client predicts abilities locally while the server replicates Gameplay Effects to all clients.
- **Enemy**: `Minimal` — the server handles all ability logic; only effect results are replicated.

---

## Ability System Component

**Class**: `UMKHAbilitySystemComponent` (inherits `UAbilitySystemComponent`)
**Location**: `Source/Makhia/Public/AbilitySystem/MKHAbilitySystemComponent.h`

### Public API

| Method | Description |
|---|---|
| `AddCharacterAbilities(Abilities)` | Grants active abilities. Each ability's `InputTag` (from the CDO) is added to the spec's dynamic source tags so it can be activated by input. |
| `AddCharacterPassiveAbilities(Passives)` | Grants and immediately activates passive abilities via `GiveAbilityAndActivateOnce`. |
| `InitializeDefaultAttributes(AttributeEffect)` | Applies a `UGameplayEffect` that sets the initial attribute values, then broadcasts `OnAttributesGiven`. |
| `AbilityInputPressed(InputTag, bForceQueue)` | Routes a pressed input tag: queues it (`ShouldQueueAbility` / `bForceQueue`), sends the quick-slot event, or activates/forwards to the matching spec. |
| `AbilityInputReleased(InputTag)` | Sends `InputReleased` replicated event to matching active abilities. |
| `NotifyWeaponSwapRequested/Completed`, `IsWeaponSwapInFlight` | Client-side weapon-swap synchronization (see [Input Binding & Ability Queue](#input-binding--ability-queue)). |
| `HasGrantedAbilityForInputTag(InputTag)` | True when an activatable spec is already granted for the input tag (used to decide queue vs activate during equip round trips). |
| `GetCooldownRemainingForTag(CooldownTag, ...)` | Returns remaining/total cooldown duration for the effect granting the tag. |
| `AddEquipmentEffects(Entry)` / `RemoveEquipmentEffects(Entry)` | Applies/removes stat effects from an equipment entry (supports async loading via `FStreamableManager`). |
| `AddEquipmentAbility(Entry)` / `RemoveEquipmentAbility(Entry)` | Grants/clears abilities from an equipment entry (supports async loading). |

### OnGiveAbility (client-side retry)

`OnGiveAbility` also fires on clients when specs replicate down (e.g. after a weapon-equip RPC round trip). When a queued input matches the granted spec (or a weapon swap is in flight), the component schedules a **next-tick** flush of the input queue (`FlushQueuedAbilityAfterGrant`). The deferral is mandatory: the callback runs mid FastArray delta serialize, where server-removed specs are still physically inside `ActivatableAbilities` — activating synchronously would pick a stale spec.

### Delegate

```cpp
DECLARE_MULTICAST_DELEGATE(FOnAttributesGiven)
```

Broadcast after `InitializeDefaultAttributes` completes. Used by `AEnemyBase` to set a replicated flag so clients know attributes are ready.

---

## Input Binding & Ability Queue

Abilities are activated through a tag-based input system using Enhanced Input.

### Configuration & Binding

`UMKHInputConfig` (data asset) maps `UInputAction` assets to `FGameplayTag` values. `UMKHSystemInputComponent::BindAbilityActions` filters the config by a parent tag (`Input.Ability`) and binds `Started` → `AMKHPlayerController::AbilityInputPressed(Tag)` and `Completed` → `AbilityInputReleased(Tag)`.

### Activation Path

```
AMKHPlayerController::AbilityInputPressed(InputTag)
  ├─ For non-dodge attacks: EnsureWeaponEquipped()
  │    ├─ If a weapon swap is in flight → force-queue the input
  │    ├─ Armed check: equipment entry OR ASC already grants the input tag
  │    └─ If unarmed: quick-equip flow (queue or TryEquipWeapon + queue)
  └─ UMKHAbilitySystemComponent::AbilityInputPressed(InputTag, bForceQueue)
       ├─ ShouldQueueAbility?  → QueueAbility (input buffer)
       ├─ Quick-slot input     → Event::UseQuickSlot gameplay event
       └─ Otherwise            → TryActivateAbility / replicated InputPressed
```

### Input Queue

The buffering/priority logic (input buffer window, swap-into-attack chains, consumable priority) is documented in **`Documentation/QueueAbilitySystem.md`**, including the client-side **weapon swap in flight** state that keeps buffered attacks queued until the authoritative swap result replicates down.

### Priority Tags

Each ability may set a `PriorityTag` (`GameplayAbility.Active.Priority.First/Second`). While active, the tag blocks activation of same-or-lower priority abilities (`DoesAbilitySatisfyTagRequirements`). The `Event::MakeAbilityCancellable` gameplay event (raised by animation notifies) removes the tag early and flushes the input queue, enabling recovery-frame cancels.

---

## Ability Grant Payload

**Class**: `UMKHAbilityGrantPayload`
**Location**: `Source/Makhia/Public/AbilitySystem/MKHAbilityGrantPayload.h`

Equipment can grant the same ability class with different data (dynamic skill input tags, skill flag, cooldown tags). Writing that data on the ability CDO would turn the CDO into shared mutable state and cross-contaminate every owner using the same class. Instead:

1. `UMKHAbilitySystemComponent::GrantEquipmentAbility` fills a payload (`InputTag`, `ProjectileToSpawnTag`, `bIsSkillAbility`, `CooldownTags`) and assigns it to `Spec.SourceObject`.
2. The input tag is also added to `Spec.GetDynamicSpecSourceTags()`, which is what input matching actually reads (the ability's own `InputTag` member is a CDO-level design value, not consumed per-instance). The definition's `AbilityTag` is added there too: it identifies the ability's row in the ability tuning table.
3. `UMKHDamageAbility` / `UMKHProjectileAbility` read the payload in `OnGiveAbility` via the shared `GetGrantPayload(Spec)` helper and override their instance fields. This runs synchronously inside `GiveAbility` on the authority, so the payload does not need to outlive the grant call.
4. No explicit CDO fallback is needed: these abilities are `InstancedPerActor`, so each instance already starts from the CDO defaults. A null payload (remote clients, where `SourceObject` is not network-addressable; or non-equipment grants) simply leaves those defaults in place. All payload-driven data is consumed server-side, so empty client-side defaults are harmless.

Balancing data (strikes, cooldown time, DMT/AOE reach) is **not** in the payload: every machine reads it from `DT_AbilityTuning` by the `AbilityTag` found in the spec's dynamic source tags, which replicate with the spec. See `AbilityTuning.md`.

Character base abilities (granted via `AddCharacterAbilities`) are plain `UMKHGameplayAbility` BPs with no per-grant data — they carry no payload and rely purely on their CDO `InputTag`.

**Rule: never mutate `Spec.Ability` — it is the class CDO.**

---

## Attribute Set

**Class**: `UMKHAttributeSet` (inherits `UAttributeSet`)
**Location**: `Source/Makhia/Public/AbilitySystem/Attributes/MKHAttributeSet.h`

### Attributes

| Attribute | Replicated | Purpose |
|---|---|---|
| `Health` / `MaxHealth` | Yes | Current/maximum health, clamped to `[0, MaxHealth]` |
| `Armor` | Yes | Permanent damage reduction from equipped armor, `>= 0`, never consumed by hits ([ArmorMitigation.md](ArmorMitigation.md)) |
| `Stamina` / `MaxStamina` | Yes | Resource for dodge/sprint/block, clamped to `[0, MaxStamina]` |
| `DodgeStaminaCost` | Yes | How much Stamina a dodge consumes |
| `CritChance` | Yes | Probability of a critical hit (0–100) |
| `CritDamageMod` | Yes | Bonus damage multiplier on critical hits |
| `IncomingDamage` | No (meta) | Written by `ExecCalc_Damage`, consumed in `PostGameplayEffectExecute` |
| `IncomingStaminaDamage` | No (meta) | Written by `ExecCalc_Damage` when the attack is blocked |

Each replicated attribute has an `OnRep_*` function that calls `GAMEPLAYATTRIBUTE_REPNOTIFY`.

### Key Overrides

- **`PreAttributeChange`**: clamp-only (Health, Stamina to their max values, Armor to `>= 0`). No side effects here — the callback also runs on clients during replication.
- **`PostAttributeChange`**: when a max attribute changes, scales the current value proportionally via `AdjustAttributeForMaxChange` so the character keeps the same percentage.
- **`PostGameplayEffectExecute`**: resolves the meta attributes — `HandleIncomingDamage` (health damage, hit events, status effects, death) and `HandleIncomingStaminaDamage` (block/guard-break pipeline).

### Damage Resolution (`HandleIncomingDamage`)

1. If the context is flagged **dodged** (`FMKHGameplayEffectContext::IsDodged`): send `Event.Combat.AttackDodged` to both actors, apply nothing.
2. Otherwise subtract the (already armor-mitigated, rounded) damage from Health. Armor is applied earlier, in `UExecCalc_Damage::ApplyArmorMitigation` — see **[ArmorMitigation.md](ArmorMitigation.md)**.
3. Send `Event.Combat.HitReceived` (defender) and `Event.Combat.HitInflicted` (attacker).
4. Apply the on-hit **status effects** carried by the context (`ApplyStatusEffectsFromContext`), with their duration passed via the `Combat.Data.StatusEffectDuration` set-by-caller.
5. **Death**: if Health reached 0, `TrySendDeathEvent` sends `Event.Combat.Death` to the avatar — authority-only, avatar-type agnostic (works for players and enemies), guarded by the `State.Movement.Dead` tag against double-firing.

### Stamina Damage Resolution (`HandleIncomingStaminaDamage`)

Runs when the attack was **blocked**:

1. Send `Event.Combat.BlockSuccessful` to both actors.
2. If stamina damage ≥ current Stamina → **Guard Break**: stamina zeroed, `Event.Combat.GuardBreak` sent to both actors.
3. Otherwise subtract the stamina damage.

### Armor Mitigation

Armor is a **permanent rating, not a pool**: equipped pieces grant it (helmet 20, legs 30, chest 50),
hits never wear it down, and it scales every hit by `1 - Armor / (Armor + 100)` before it reaches
Health. Sources, formula and balance tables are documented in **[ArmorMitigation.md](ArmorMitigation.md)**.

---

## Gameplay Abilities

### Ability Hierarchy

```
UGameplayAbility
  └─ UMKHGameplayAbility            (input tags, montage, priority, queue hooks)
       └─ UMKHDamageAbility         (attack data, combos, skill cooldowns, owning weapon)
            ├─ UMKHMeleeAbility     (hit scans + DMT)
            └─ UMKHProjectileAbility (projectile spawning)
```

### UMKHGameplayAbility

Base class for every ability. Key behaviors:

- **`InputTag`** (`EditDefaultsOnly`): added to the spec's dynamic source tags on grant; matched by `AbilityInputPressed`. Skills receive theirs dynamically via the grant payload.
- **`ActivateAbility`**: binds input press/release tasks, **commits the ability and aborts on a failed commit** (cost/cooldown), adds the loose `PriorityTag` (locally controlled), plays `MontageToPlay` when set.
- **`EndAbility`**: removes the priority tag and fires `Event::ActivateQueuedAbility` (before `Super`, guarded against double-firing) so buffered inputs resolve immediately; force-stops the montage on remote peers only when cancelled.
- **`OnMakeAbilityCancellable`**: animation-notify driven early-cancel point — removes the priority tag and flushes the input queue.
- **Blueprint hooks**: `OnAbilityActivatedAgain`, `OnAbilityReleased`, `OnAbilityRemoved`, `OnMontageStarted`, `OnMontageFinished`.

### UMKHDamageAbility

- **Tuning** (`GetAbilityTuning()`, `GetAttacks()`): the ability's `FMKHAbilityTuningRow`, looked up on every use by `TuningTag` (resolved in `OnGiveAbility`). Its `Attacks` hold per-strike damage percent, stamina damage, and status effects, indexed by the combo hit counter. See `AbilityTuning.md`.
- **Combo system**: `Event.Animation.ContinueCombo.Start/End` notifies open/close the combo window; re-pressing the input inside the window triggers `OnComboTriggered(++ComboHitCounter)`, otherwise the ability ends.
- **Skill cooldowns**: skills (`bIsSkillAbility`) apply a generic cooldown effect whose duration (tuning `CooldownTime`) travels as the `Combat.Data.AbilityCooldownTime` set-by-caller and whose granted tags come from the rolled `CooldownTags`.
- **`CaptureDamageEffectInfo`**: fills `FDamageEffectInfo` (base damage from weapon damage × attack percent, stamina damage, status effects, source/target ASC) consumed by `UMKHAbilitySystemLibrary::ApplyDamageEffect`.
- **`OwningWeapon`**: resolved on activation (authority) through the controller's `UEquipmentManagerComponent`.

### UMKHMeleeAbility

- **Hit scans**: `Event.Animation.HitScan.Start/End` notifies (bound authority-only) drive `AMKHWeaponBase::HitScanStart/End` with the captured damage info.
- **DMT (Directional Movement Technique)**: radius, angle and stop distance come from the ability's tuning row. `UMKHAbilityTask_ApplyDMT` is a ticking taskthat picks the best target in a radius/angle (score = alignment + proximity) and interpolates the character's rotation/position towards it during the attack. Re-triggered on every combo strike and by the `Event.Animation.ApplyDMT` notify. Note: it moves the character with `SetActorLocation` outside the CharacterMovementComponent — acceptable for short bursts, but a known source of server corrections under heavy latency.

### UMKHProjectileAbility

- `ProjectileToSpawnTag` (per grant, via payload) resolves `FProjectileParams` from `UProjectileInfo`.
- `Event.Animation.SpawnProjectile` triggers `SpawnProjectile`, which deferred-spawns the `AMKHProjectileBase`, injects params + `FDamageEffectInfo`, and finishes spawning.

---

## Execution Calculations

Execution calculations are **pure computation**: they read captures/set-by-callers, compute output modifiers, and annotate the effect context. All side effects (gameplay events, status effect application) live in `UMKHAttributeSet::PostGameplayEffectExecute`.

### ExecCalc_Damage

1. Early-out if the target is dead.
2. **Block check** (`IsAttackBlocked`): target has `State.Movement.Blocking` and faces the attacker (2D dot product > 0, via `ComputeDefenderFacingDot`) → the attack is blocked:
   - Reads `Combat.Data.StaminaDamage` set-by-caller, reduces it by the defender weapon's block stability (`RefineStaminaDamage`), outputs to `IncomingStaminaDamage`.
3. Otherwise (**health damage path**):
   - Reads `Combat.Data.Damage` set-by-caller.
   - **Dodge check**: target has `State.Movement.Dodging` → sets `bDodged` on the context and outputs the would-be damage (the attribute set skips application and only notifies).
   - **Back attack check** (`IsBackAttack`): attacker inside the defender's rear cone (`BackAttackRearConeAngleDegrees`, 120°) → the crit roll is skipped and the hit always crits.
   - **Crit roll**: back attack, or `RandRange(1,100) <= CritChance` → `Damage × (1 + CritDamageMod)`; result flagged on the context (`bCriticalHit`).
   - Outputs to `IncomingDamage`.

Both the block check and the back attack check read the front/back geometry from the same helper (`ComputeDefenderFacingDot`: defender forward · direction to attacker, flattened on Z), so they can never disagree. The rear cone (dot ≤ -0.5) is strictly narrower than the half space the block check rejects (dot > 0), which makes a back attack impossible to block and leaves a neutral side band that is neither.

### ExecCalc_DodgeCost

Captures `Stamina`, `MaxStamina`, `DodgeStaminaCost`; consumes `min(DodgeStaminaCost, Stamina)` so a low-stamina dodge drains whatever is left.

---

## Custom Effect Context

**Struct**: `FMKHGameplayEffectContext` (inherits `FGameplayEffectContext`)
**Location**: `Source/Makhia/Public/AbilitySystem/MKHAbilityTypes.h`

Extends the default effect context with:

- `bCriticalHit` — set by `ExecCalc_Damage`, replicated (bit 7 of `RepBits`), readable by UI.
- `StatusEffects` (`TArray<FStatusEffectData>`) — on-hit effects set by `ApplyDamageEffect`, replicated (bit 8), applied by the attribute set after damage.
- `bDodged` — server-only flag (intentionally **not** serialized): written by the ExecCalc and consumed by the attribute set in the same frame.

`UMKHAbilitySystemGlobals::AllocGameplayEffectContext` returns this type for every effect in the game (configured via `AbilitySystemGlobalsClassName`).

---

## Damage Pipeline

```
1. Ability activates (UMKHMeleeAbility hit scan / UMKHProjectileAbility hit)
       │
2. CaptureDamageEffectInfo() fills FDamageEffectInfo
       │
3. UMKHAbilitySystemLibrary::ApplyDamageEffect()
       ├─ Creates effect context with source actor
       ├─ Stores AdditionalStatusEffects in the FMKHGameplayEffectContext
       ├─ Creates outgoing spec from DamageEffect class
       ├─ SetByCaller: Combat.Data.Damage + Combat.Data.StaminaDamage
       └─ Applies spec to target ASC
              │
4. ExecCalc_Damage (pure computation)
       ├─ Target dead?      → no output
       ├─ Blocked (facing)? → IncomingStaminaDamage (reduced by block stability)
       ├─ Dodged?           → context.bDodged, IncomingDamage (skipped later)
       ├─ Back attack?      → crit guaranteed (rear cone, no roll)
       └─ Else crit roll    → context.bCriticalHit, IncomingDamage
              │
5. UMKHAttributeSet::PostGameplayEffectExecute (server-side effects)
       ├─ HandleIncomingStaminaDamage: BlockSuccessful → stamina loss / GuardBreak
       └─ HandleIncomingDamage:
            ├─ bDodged → AttackDodged events, no damage
            ├─ Health reduction (armor already applied in the exec calc)
            ├─ HitReceived / HitInflicted events
            ├─ Status effects from context (Data.StatusEffectDuration set-by-caller)
            └─ Health ≤ 0 → Event.Combat.Death (authority, once, any avatar type)
```

---

## Equipment Integration

Equipment items can grant both passive stat effects and active abilities through the GAS.

### Equipping Flow

```
UInventoryComponent::UseItem (server)
  └─ EquipmentItemUsedDelegate → UEquipmentManagerComponent::EquipItem
       └─ FRPGEquipmentList::AddEntry
            ├─ Creates UEquipmentInstance
            ├─ ASC->AddEquipmentEffects()   (GE stat modifiers, async-load aware)
            ├─ ASC->AddEquipmentAbility()   (abilities via GrantEquipmentAbility + payload)
            └─ Instance->SpawnEquipmentActors()
```

Rarity, passive stats, and active abilities are rolled at **inventory add time** (`FRPGInventoryList::RollEquipmentEntry` via `UEquipmentRollLibrary`), where skills also receive their dynamic input tags (`UMKHAbilitySystemLibrary::AssignDynamicSkillInputTag` → `Input.Ability.Attacks.Skill.First/Second/Third`).

### Effect Handles

`FEquipmentGrantedHandles` tracks granted ability spec handles and active effect handles per entry, enabling clean removal on unequip.

### Async Loading

Both `AddEquipmentEffects` and `AddEquipmentAbility` support soft class pointers with asynchronous loading via `FStreamableManager::RequestAsyncLoad`; quick-slotted weapons are preloaded (`UInventoryComponent::PreloadItem`) so swaps don't hitch.

---

## Gameplay Tags

**Location**: `Source/Makhia/Public/AbilitySystem/MKHGameplayTags.h` (declared) / `.cpp` (defined). Organized in namespaces:

### Combat

| Tag | String |
|---|---|
| `Data_Damage` | `Combat.Data.Damage` |
| `Data_StaminaDamage` | `Combat.Data.StaminaDamage` |
| `Data_StatusEffectDuration` | `Combat.Data.StatusEffectDuration` |
| `Data_AbilityCooldownTime` | `Combat.Data.AbilityCooldownTime` |
| `InputBufferWindow` | `Combat.InputBufferWindow` |

### State::Movement

`Idle`, `Walking`, `Sprinting`, `CrouchingIdle`, `CrouchingMoving`, `Jumping`, `Falling`, `LandingInPlace`, `LandingMoving`, `Dodging`, `Blocking`, `Attacking`, `Dead` — all under `State.Movement.*`, synced by the movement state machine and consumed by ability activation and the damage pipeline.

### State (General)

| Tag | String |
|---|---|
| `OutOfStamina` | `State.General.Stamina.Out` |
| `Stunned` | `State.Combat.CC.Stunned` |
| `Staggered` | `State.Combat.CC.Staggered` |
| `QuickSlotUse` | `State.General.QuickSlot.Use` |

### Equip

| Tag | String |
|---|---|
| `Category_Equipment` / `Category_Weapon` / `Category_Consumable` | `Item.Equipment`, `Item.Equipment.Weapon`, `Item.Consumable` |
| `ArmorSlot` / `WeaponSlot` | `Equipment.Slot.Armor`, `Equipment.Slot.Weapon` |
| `ConsumableQuickSlot1–3` | `Input.Ability.QuickSlot.Consumable.{First,Second,Third}` |
| `WeaponQuickSlot1–2` | `Input.Ability.QuickSlot.Weapon.{Primary,Secondary}` |

Note: quick-slot tags live under the `Input.Ability.QuickSlot` hierarchy so they double as input tags.

### Input

| Tag | String |
|---|---|
| `Ability` | `Input.Ability` |
| `QuickSlot` | `Input.Ability.QuickSlot` |
| `WeaponQuickSlotCategory` / `ConsumableQuickSlotCategory` | `Input.Ability.QuickSlot.Weapon` / `.Consumable` |
| `SheatheWeapon` | `Input.Ability.SheatheWeapon` |
| `Attacks` | `Input.Ability.Attacks` |
| `Dodge` | `Input.Ability.Attacks.Dodge` |
| `Block` | `Input.Ability.Attacks.Basics.Block` |
| `Skill` / `SkillSlot1–3` | `Input.Ability.Attacks.Skill{,.First,.Second,.Third}` |
| `Inventory` | `Input.Inventory` |

### Event

| Tag | String |
|---|---|
| `Death` | `Event.Combat.Death` |
| `HitScanStart` / `HitScanEnd` | `Event.Animation.HitScan.Start/End` |
| `ContinueComboStart` / `ContinueComboEnd` | `Event.Animation.ContinueCombo.Start/End` |
| `ApplyDMT` | `Event.Animation.ApplyDMT` |
| `SpawnProjectile` | `Event.Animation.SpawnProjectile` |
| `MakeAbilityCancellable` | `Event.Animation.MakeAbilityCancellable` |
| `UseQuickSlot` | `Event.QuickSlot.Use` |
| `BlockSuccessful` / `GuardBreak` | `Event.Combat.BlockSuccessful` / `.GuardBreak` |
| `AttackDodged` | `Event.Combat.AttackDodged` |
| `HitInflicted` / `HitReceived` | `Event.Combat.HitInflicted` / `.HitReceived` |
| `ActivateQueuedAbility` | `Event.Combat.ActivateQueuedAbility` |

### Ability

| Tag | String |
|---|---|
| `All` | `GameplayAbility` (asset tag on every ability; also in `CancelAbilitiesWithTag`) |
| `AbilityActive` / `Attacking` | `GameplayAbility.Active` / `.Active.Attack` |
| `Priority1` / `Priority2` | `GameplayAbility.Active.Priority.First/Second` |
| `Type_Attacks` (+ `Basic`, `Basic.Heavy`, `Basic.Light`, `Skill`) | `GameplayAbility.Type.Attacks.*` |
| `Type_Block` | `GameplayAbility.Type.Block` |

---

## Data Assets

### UCharacterClassInfo

Maps `FGameplayTag` character class identifiers to default attributes and starting (passive) abilities. Stored on `AMKHGameMode`, accessed via `UMKHAbilitySystemLibrary::GetCharacterClassDefaultInfo`.

### UProjectileInfo

Maps `FGameplayTag` → `FProjectileParams` (projectile class, mesh, speed, gravity, bounce). Stored on `AMKHGameMode`, accessed via `UMKHAbilitySystemLibrary::GetProjectileInfo`.

### UGenericClassReference

Maps well-known `FName` keys to Blueprint classes that native code must reference without hard dependencies. The canonical keys are named constants on the class — never spell them inline:

| Constant | Key | Used by |
|---|---|---|
| `UGenericClassReference::DamageEffectKey` | `GE_Damage` | `UMKHDamageAbility::AssignBPClasses` |
| `UGenericClassReference::SkillCooldownEffectKey` | `GE_GenericSkillCooldown` | `UMKHDamageAbility::AssignBPClasses` |

Use the typed accessor `GetGameplayEffectByName` for gameplay effect classes.
