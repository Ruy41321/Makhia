# Hit Detection Architecture

How the four damage ability classes detect hits and get them applied on the authority:
`UMKHDamageAbility` (shared foundation), `UMKHMeleeAbility`, `UMKHAOEAbility`, `UMKHProjectileAbility`.

## Table of Contents

1. [Overview](#overview)
2. [Shared Principles](#shared-principles)
3. [UMKHDamageAbility — The Foundation](#umkhdamageability--the-foundation)
4. [UMKHMeleeAbility — Client-Predicted Weapon Scan](#umkhmeleeability--client-predicted-weapon-scan)
5. [UMKHAOEAbility — Instant Sphere Around the Player](#umkhaoeability--instant-sphere-around-the-player)
6. [UMKHProjectileAbility — Server-Authoritative Spawn](#umkhprojectileability--server-authoritative-spawn)
7. [Comparison Table](#comparison-table)
8. [Known Limits & Future Work](#known-limits--future-work)

---

## Overview

All four classes answer the same three questions differently:

| Question | Melee | AOE | Projectile |
|---|---|---|---|
| **Where is the hit detected?** | Locally-controlled machine (weapon scan) | Locally-controlled machine (sphere trace) | Server (projectile overlap) |
| **What travels to the server?** | Each hit + combo index | Each hit + combo index | The aim point |
| **Who applies damage?** | Authority, after validation | Authority, after validation | Authority (only machine with damage bindings) |

The invariant across the whole system: **damage is only ever applied by the network authority**, via
`UMKHAbilitySystemLibrary::ApplyDamageEffect`. Clients detect, predict, and report — they never damage.

The reason detection starts on the locally-controlled machine (owning client for players, server for AI)
is animation notifies: `HitScanStart`/`HitScanEnd`/`SpawnProjectile` are montage notifies, and the only
machine whose montage timeline is reliable is the one driving it. The server proxy of a remote player
plays a lag-shifted copy of the montage, so it never reacts to its own notifies for detection purposes.

---

## Shared Principles

- **Locally-detected, server-validated.** The locally-controlled machine listens for the notify-driven
  gameplay events and performs detection. If that machine is also the authority (listen-server host, AI),
  it applies results directly; otherwise it ships them to the server as **replicated target data**
  (`UAbilitySystemComponent::CallServerSetReplicatedTargetData`) inside a `FScopedPredictionWindow`,
  which lets the server accept data sent outside the original activation prediction window.
- **The server proxy binds `AbilityTargetDataSetDelegate`** (keyed by spec handle + activation prediction
  key) on activation, consumes every received batch, and unbinds + consumes leftovers in `EndAbility`
  (idempotent, guarded by `IsEndAbilityValid`).
- **Executions stay pure.** Detection classes only decide *who* gets hit. The damage math lives in
  `UExecCalc_Damage`; side effects live in `UMKHAttributeSet::PostGameplayEffectExecute`
  (see `DamageAndStatusEffects.md`).

---

## UMKHDamageAbility — The Foundation

`UMKHDamageAbility` performs **no hit detection itself**. It owns everything the detecting subclasses
need at the moment a hit is confirmed:

| Service | Member | Role in hit detection |
|---|---|---|
| Damage capture | `CaptureDamageEffectInfo(TargetActor, OutInfo)` | Fills `FDamageEffectInfo` (source/target ASC, damage effect class, current attack multiplier, status effect lists) for `ApplyDamageEffect` |
| Combo pricing | `GetComboHitCounter()` | The combo strike index a detected hit belongs to; determines which `FAttackData` prices the hit |
| Combo sync | `SyncAuthoritativeComboCounter(Index)` | The authority aligns its (lagging) combo bookkeeping with the index reported by the detecting machine — forward-only and clamped to the configured attack data, so a client cannot price a strike beyond the strongest configured combo step |
| Weapon resolution | `OwningWeapon` / `InitOwningWeapon()` | Resolves the weapon actor from the avatar's attached actors; works on every machine because the weapon replicates already attached |
| Self buffs | `TryApplySelfActivationStatusEffects()` | Authority-only, once per combo strike, guaranteed to run before the strike's damage spec is created (so buffs like Empower are captured by the damage calculation) |

Per-grant data (skill flag, cooldown tags, projectile tag) is injected at grant time through
`UMKHAbilityGrantPayload` on `Spec.SourceObject` — never by mutating the class CDO. Balancing data
(strikes, cooldown time, DMT/AOE reach) comes from the ability tuning table (`AbilityTuning.md`).

---

## UMKHMeleeAbility — Client-Predicted Weapon Scan

### Flow

```
LOCALLY-CONTROLLED MACHINE                      SERVER (proxy of a remote player)
--------------------------                      ---------------------------------
ActivateAbility                                 ActivateAbility
  BindHitScanEvents()                             BindServerTargetDataDelegate()
                                                  ScheduleServerLifetimeFailsafe()
[notify] Event.HitScanStart
  HitScanStart()
    weapon->StartMeleeScan(HitScanFrequency)
    OnMeleeHitDetected -> HandleMeleeHitDetected
      authority?  -> ProcessAuthoritativeMeleeHit ----+
      otherwise   -> SendMeleeHitToServer  ---------> OnServerMeleeTargetDataReceived
                     (FMKHGameplayAbilityTargetData_    ConsumeClientReplicatedTargetData
                      MeleeHit: FHitResult+ComboIndex)  ProcessAuthoritativeMeleeHit
[notify] Event.HitScanEnd                                 IsMeleeHitPlausible? -> ApplyDamageEffect
  HitScanEnd() -> weapon->StopMeleeScan()
```

### Detection

- The scan window is opened/closed by the `Event.HitScanStart` / `Event.HitScanEnd` gameplay events
  (montage notifies), bound **only when `IsLocallyControlled()`**.
- Detection itself is delegated to the weapon: `AMKHWeaponBase::StartMeleeScan(HitScanFrequency)` steps
  the weapon's native melee scan and broadcasts `OnMeleeHitDetected` once per unique actor.
- `bHitScanActive` gates late/stale weapon callbacks; `EndAbility` force-closes a lingering window.

### Server Validation — `IsMeleeHitPlausible`

Every reported hit (replicated or listen-host local) passes through the same rules before damage:

| Rule | Config | Purpose |
|---|---|---|
| Target has an ASC | — | Only ability-system actors can be damaged |
| Range | `GetHitValidationMaxRange()` → `MaxMeleeHitRange` (500) | Generous on purpose: absorbs client/server position divergence under latency |
| Swing cone | `GetHitValidationMaxConeAngleDegrees()` → `MaxMeleeHitConeAngleDegrees` (120°) | Rejects hits behind the attacker; wide by design for swing arcs |
| Per-swing dedup | `AcceptedMeleeHits` map | One accepted hit per target per combo strike |
| Re-hit throttle | `MeleeRehitCooldown` (0.2s) | Rejects duplicated/forged sends claiming a new strike too quickly |

The two validation thresholds are **virtual getters** so subclasses with different scan shapes (AOE)
keep validation coherent with their actual reach.

### Lifetime

The server proxy of a remote player **never self-terminates from its own montage timeline** (it lags the
client's and would truncate swings the client already started). It ends through the client's replicated
`EndAbility`, bounded by `ServerLifetimeFailsafeDuration` (10s) against clients that never send it.

---

## UMKHAOEAbility — Instant Sphere Around the Player

Subclass of `UMKHMeleeAbility` that **replaces the detection shape and reuses the entire pipeline**.

### Detection

- `HitScanStart_Implementation` override: instead of starting the weapon scan, it performs a single
  `UKismetSystemLibrary::SphereTraceMultiForObjects` centered on the avatar with radius `AOERadius`,
  filtered by `AOEObjectTypes` (defaults to Pawn), ignoring the avatar itself.
- Hits are de-duplicated **per actor** (a sphere trace can report several components of the same target)
  and each unique hit is forwarded into the inherited `HandleMeleeHitDetected` — from there the flow is
  byte-for-byte the melee one: direct authoritative processing or replicated target data to the server.
- `HitScanEnd_Implementation` is a no-op: the trace is instantaneous, there is no window to close.
- The `Event.HitScanStart` notify therefore marks the exact montage frame the area triggers.

### Validation Overrides

| Getter | Melee | AOE |
|---|---|---|
| `GetHitValidationMaxRange()` | `MaxMeleeHitRange` | `AOERadius + AOEHitValidationSlack` |
| `GetHitValidationMaxConeAngleDegrees()` | `MaxMeleeHitConeAngleDegrees` | `180°` (cone check disabled — the AOE covers 360°) |

Dedup, re-hit throttling, combo sync, the server lifetime failsafe and the DMT hooks are all inherited
unchanged. `bDebugDrawAOE` draws the trace for tuning.

---

## UMKHProjectileAbility — Server-Authoritative Spawn

Projectiles invert the melee approach: **detection happens on the server**, inside the spawned actor.
What travels client → server is not a hit but the **aim point**.

### Flow

```
LOCALLY-CONTROLLED MACHINE                      SERVER (proxy of a remote player)
--------------------------                      ---------------------------------
ActivateAbility                                 ActivateAbility
  BindSpawnProjectileEvent()                      BindServerSpawnTargetDataDelegate()

[notify] Event.Animation.SpawnProjectile
  HandleProjectileFiredReaction()
  ComputeAimPoint()
    authority?  -> SpawnProjectile(AimPoint) --+
    otherwise   -> SendSpawnProjectileToServer -> OnServerSpawnTargetDataReceived
                   (FGameplayAbilityTargetData_    ConsumeClientReplicatedTargetData
                    LocationInfo)                  HandleProjectileFiredReaction()
                                                   SpawnProjectile(AimPoint)
                                                     -> AMKHProjectileBase (replicates
                                                        to every client)
```

### Aiming — `ComputeAimPoint`

Mirrors the Blueprint `WaitTargetData` single-line-trace setup the ability previously used
(`NoCollision` trace profile, aim-pitch affecting): with no geometry to hit, the target point is simply
the endpoint `AimTraceMaxRange` (default 999999) units along the **controller's view direction**,
re-projected so the firing direction originates from the avatar instead of the camera. AI (no player
controller) falls back to the avatar's forward vector. Because only the locally-controlled machine has
the real camera/view state, the aim point must be computed there and shipped — the server cannot derive
it reliably from replicated control rotation.

### Why the spawn is authority-only

`AMKHProjectileBase` binds its damage overlaps **only under `HasAuthority()`** — but an actor spawned on
a client has *local* authority, so a client-side spawn would produce a fake projectile that applies
ghost damage and visually duplicates the server's replicated one. Hence: the notify is listened to only
on the locally-controlled machine (the only place it is reliable — on a non-locally-controlled peer the
notify may fire late or not at all), and the actual spawn happens only on the authority, either directly
(listen host, AI) or upon receiving the replicated aim point.

### Detection & damage (inside the projectile)

- `AMKHProjectileBase` replicates (`bReplicates`, `SetReplicateMovement`); movement is simulated
  server-side and replicated down.
- On the authority, `OnSphereBeginOverlap` checks the overlapped actor for an ASC, injects it as
  `TargetASC` into the `FDamageEffectInfo` captured **at spawn time** (so the combo/buff state of the
  firing moment is preserved even if the ability has since ended), applies the damage and destroys the
  projectile. Blocking hits (`OnSphereHit`) destroy it without damage.
- The projectile owns its damage info by value: the ability instance ending or the caster dying mid-flight
  is safe (`ApplyDamageEffect` re-validates both ASCs).

### Cosmetic reaction

`HandleProjectileFiredReaction` (stop camera-follow orientation, hide the owning weapon) runs on the
locally-controlled machine at the notify (immediate feedback) **and** on the authority when it processes
the spawn — the authoritative call is what makes the weapon's `bHidden` replicate to every other client
and keeps it symmetric with the authoritative un-hide in `EndAbility`.

---

## Comparison Table

| | `UMKHMeleeAbility` | `UMKHAOEAbility` | `UMKHProjectileAbility` |
|---|---|---|---|
| Detection primitive | Weapon native scan (stepped, windowed) | One multi sphere trace, radius `AOERadius` | Projectile overlap sphere |
| Detection machine | Locally controlled | Locally controlled | **Server** |
| Trigger | `Event.HitScanStart` / `End` notifies | `Event.HitScanStart` notify (instant) | `Event.Animation.SpawnProjectile` notify |
| Replicated payload | `FMKHGameplayAbilityTargetData_MeleeHit` (hit + combo index) | Same as melee | `FGameplayAbilityTargetData_LocationInfo` (aim point) |
| Server validation | Range + cone + dedup + re-hit throttle | Same, range = radius + slack, cone off | None needed (server simulates the hit itself) |
| Damage application | `ProcessAuthoritativeMeleeHit` → `ApplyDamageEffect` | Inherited | `AMKHProjectileBase::OnSphereBeginOverlap` → `ApplyDamageEffect` |
| Anti-cheat surface | Client reports *who was hit* → fully validated | Same | Client reports *where it aimed* → harmless (damage fully server-side) |

---

## Known Limits & Future Work

- **Notify near montage end + jitter.** The client's replicated `EndAbility` can, under network jitter,
  reach the server slightly before the last target data batch. Melee is protected (the server proxy
  ignores its own montage end and relies on the lifetime failsafe); the projectile ability is not — a
  spawn notify placed very close to the end of the montage could occasionally lose the shot. If it shows
  up in practice, apply the melee-style `OnMontageFinished` guard + failsafe to the projectile.
- **Aim point is trusted.** The server spawns towards whatever point the client reports. It is only a
  firing direction (equivalent to free aiming), so no validation is performed; add a cone/range clamp if
  design ever restricts firing arcs.
- **Friendly fire.** `AMKHProjectileBase` damages any ASC-bearing pawn except its owner; melee/AOE damage
  any ASC-bearing actor that passes validation. Team filtering, if needed, belongs in a shared place
  (target-side check in the damage pipeline) rather than per detection class.
