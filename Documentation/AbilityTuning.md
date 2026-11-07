# Ability Tuning

Balancing and AI data of every damage ability lives in **one DataTable**, `DT_AbilityTuning`
(`/Game/Makhia/Data/Equipment/Abilities/DataTables/`), one row per ability. A designer balances every attack of every
weapon from this table alone, and the duel bot AI reads its evaluations (reach, damage, cooldown) from the same rows.

## 1. What goes where

Ability data is split by **who changes it and why**:

| Data | Lives in | Examples |
|---|---|---|
| Balancing and AI | `FMKHAbilityTuningRow` in `DT_AbilityTuning` | strikes (damage %, stamina damage, status effects), skill cooldown time, DMT radius/angle/stop distance, AOE radius, projectile effective range |
| Identity, UI and roll | `FEquipmentAbilityDefinition` in the per-weapon tables (`DT_SwordAbilities`, `DT_HatchetAbilities`, `DT_HammerAbilities`) | `AbilityTag`, `AbilityClass`, `bIsSkillAbility`, icon, name, description, `CooldownTag`, `ContextTag`, `ProbabilityToSelect` |
| Technical and anti-cheat | the ability class (`UMKHMeleeAbility`, `UMKHAOEAbility`, `UMKHProjectileAbility` and their Blueprints) | hit scan frequency, server hit validation range/cone, re-hit throttle, lifetime failsafe, AOE validation slack, debug draw, aim trace range |
| Presentation | the ability Blueprint | montages, VFX, cues |

The per-weapon definition tables stay split by weapon because they are the roll pools of each weapon. The tuning table
is a single one because balancing means comparing attacks across weapons. If it ever grows too large, split it and
recombine the parts with a `UCompositeDataTable` set as `AbilityTuningTable`: no code changes, every reader goes
through `UMKHDataSubsystem`.

## 2. The row

`FMKHAbilityTuningRow` (`Public/AbilitySystem/MKHAbilityTuning.h`):

- **Identity**: `AbilityTag` (equal to the row name, e.g. `GameplayAbility.Sword.Basic.Light`), `Archetype`
  (`Melee`, `AOE`, `Projectile`: shows only the reach fields that apply).
- **Damage**: `Attacks`, one `FAttackData` per combo strike in order (one entry for non combo abilities).
- **Cooldown**: `CooldownTime`, used only by skills (`bIsSkillAbility` in the definition); basic attacks ignore it.
- **Reach / DMT** (Melee, AOE): `bEnableDMT`, `DMTRadius`, `DMTMaxAngle`, `AttackRange` (where DMT stops the
  attacker). See `DirectionalMagneticTargetingSystem.md`.
- **Reach / AOE**: `AOERadius`.
- **Reach / Projectile**: `EffectiveRange`, the distance up to which the projectile is worth throwing (used for aim
  and AI evaluation; the projectile itself flies on).
- `GetEffectiveReach()`: the distance up to which the ability can take its target (DMT radius, AOE radius or
  projectile range), for the AI.

AI-only fields are added to this row when the bot needs them (duel bot plan, `Handoff/DuelBotAIHandoff.md`). Strike
times are not in the row: they come from the montage (section 3b).

## 3. Runtime flow

1. `UMKHAbilitySystemComponent::GrantEquipmentAbility` adds the definition's `AbilityTag` to the spec's dynamic source
   tags, next to the input tag. Dynamic source tags replicate with the spec; the grant payload
   (`UMKHAbilityGrantPayload`, `Spec.SourceObject`) does not.
2. `UMKHDamageAbility::OnGiveAbility` (server and owning client) calls `ResolveAbilityTuning`: the spec tag that has a
   row becomes `TuningTag`, and the data subsystem is cached.
3. `GetAbilityTuning()` looks the row up **on every use** (a DataTable lookup by row name): an edit of the table in the
   editor applies from the next strike, even during PIE. Pointers to rows are never stored.
4. Readers: `GetAttacks()` (damage capture, self activation effects, combo clamp), `ApplyCooldown` (skill cooldown
   duration), `UMKHMeleeAbility::TryActivateDMT`, `UMKHAOEAbility` (sphere trace and server validation range).
5. Without a row the ability logs a warning on grant and uses a default row (one neutral strike, default reach).

Item entries (`FMKHItemEntry`) carry only the definition: tuning is never copied into items, so a balancing change
applies to every item, already generated ones included, and does not travel in item replication.

The payload keeps only per-grant identity data read on the server: `InputTag`, `ProjectileToSpawnTag`,
`bIsSkillAbility`, `CooldownTags`.

## 3b. Timing

When the strikes of an ability land is not typed in the table: it is read from the ability's montage, so it never
drifts from the animation.

- The strike notifies are native: `AN_HitScanStart` (`Event.Animation.HitScan.Start`), `AN_SpawnProjectile`
  (`Event.Animation.SpawnProjectile`) and the combo windows `AN_ContinueComboStart/End` derive from
  `UMKHAnimNotify_GameplayEvent` (`Public/Character/Animation/`), which sends its `EventTag` to the mesh owner's ability
  system (no Blueprint graph). A new event notify only needs a Blueprint child of it with `EventTag` set.
- `FMKHAbilityTiming` (`MKHAbilityTuning.h`): `StrikeHitTimes` (one per strike notify, in time order) and
  `MontageDuration`, both divided by the montage's `RateScale`. Abilities play their montage at rate 1, from the start,
  with no section jumps (`UMKHGameplayAbility::PlayAnimation`), so the notify time is the time from the activation.
- `UMKHDataSubsystem::FindAbilityTiming(AbilityTag)`: finds the ability's definition, takes the class default object's
  `GetMontageToPlay()` and caches the timing per `AbilityTag`. It never loads the class: while it is not in memory (the
  abilities of a weapon never equipped) there is no timing and nothing is cached, the next call retries. A montage
  without strike notifies logs a `LogMKHData` warning. The cache lives as long as the GameInstance: after editing a
  montage's notifies restart PIE.
- Reader: the duel bot's attack choice (`TimeToHit`, crowd control fit), see `DuelBotAI.md`.

## 4. Validation

- **Row** (`FMKHAbilityTuningRow::IsDataValid`, editor *Validate*): AbilityTag set, `Attacks` not empty, warnings when
  `AttackRange` is larger than `DMTRadius` or, for AOE, than `AOERadius`.
- **Row name** (`OnDataTableChanged`): an empty `AbilityTag` is filled from the row name; a different one logs a
  warning, because lookups go by row name.
- **Coverage** (`UMKHDataSubsystem::ValidateAbilityTuning`, editor only, at GameInstance start): every definition whose
  `AbilityClass` is a `UMKHDamageAbility` must have a row, otherwise `LogMKHData` warns.

## 5. Adding or balancing an attack

1. Add the definition row in the weapon's ability table (identity, class, UI, roll weight).
2. Add a row with the **same tag as row name** in `DT_AbilityTuning`, pick the archetype, fill one `Attacks` entry per
   strike, the cooldown for a skill and the reach.
3. Balance only in `DT_AbilityTuning`. Run *Validate* on the table after large edits.
