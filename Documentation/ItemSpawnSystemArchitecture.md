# Item Spawn System Architecture

How items are placed in levels: item spawn points decide **when**, **whether**, **what** and **for whom** a world
pickup appears. Server-authoritative; builds on the pickup system (`PickupSystemArchitecture.md`) and the match flow
(`MatchFlowArchitecture.md`).

> Status: complete (2026-09-28). Verified in PIE (listen server + client): phase triggers, delay, spawn chance,
> conditions, round range, fixed / weighted rarity, category loot, manual trigger, respawn and spawn limit, round
> survival flag, spawn groups, exclusive pickups (server refusal, client selection, replication), `MatchOver` cleanup
> and rematch.

## Overview

```
AMKHGameMode::SetPhase(Phase)                                   (server)
   │ HandleMatchPhaseChanged(Phase, CurrentRound)
   ▼
UMKHItemSpawnSubsystem (UWorldSubsystem) ── registry of AMKHItemSpawnPoint, spawn groups
   │ reset / round end / match end, then attempts of the points triggered by Phase
   ▼
AMKHItemSpawnPoint ── attempt: delay → round → conditions → group slot → chance → collector → loot roll
   │ UMKHItemSubsystem::GenerateItem(ItemTag, Quantity, RarityRule)
   ▼ AMKHPickupItem::SpawnPickupFromItem(Item, Transform, ExclusiveCollector, RiseFromGround)
AMKHPickupItem (replicated) ── collected → the point may respawn it
```

Pickups are never placed directly in a level: a `BP_ItemBase` dropped in a map would hold no item. Place
`BP_ItemSpawnPoint` instead (a point with a 100 % chance, the `InitialPreparation` trigger and one exact item is the
equivalent of the old level-placed pickup, with the difference that it comes back at every rematch).

### Source map

| File (`Source/Makhia/...`) | Responsibility |
|---|---|
| `Public/Inventory/Spawn/MKHItemSpawnPoint.h` | `AMKHItemSpawnPoint`: settings, attempt pipeline, live pickup, editor preview and validation |
| `Public/Inventory/Spawn/MKHItemSpawnSubsystem.h` | `UMKHItemSpawnSubsystem`: registry, match phases, spawn groups |
| `Public/Inventory/Spawn/MKHItemSpawnGroup.h` | `AMKHItemSpawnGroup`: cap on live pickups for a set of points |
| `Public/Inventory/Spawn/MKHLootTable.h` | `FMKHLootEntry`, `FMKHLootRoll`, `UMKHLootTable` (shared DataAsset, roll, validation) |
| `Public/Inventory/Spawn/MKHItemSpawnConditions.h` | `UMKHItemSpawnCondition` + `NoPawnNearby`, `Score`, `MatchPhase` |
| `Public/Inventory/MKHItemRarityRule.h` | `FMKHItemRarityRule` (`Default` / `Fixed` / `Weighted`) |
| `Public/Inventory/Pickup/MKHPickupItem.h` | Collection rights (`CanBeCollectedBy`), `FindRestLocation` |
| `Public/GameMode/MKHGameMode.h` | `CurrentRound`, `FMKHPlayerData::StartSpot`, `FindParticipantAtStartSpot`, `SetPhase` hook |

Asset: `BP_ItemSpawnPoint` (`/Game/Makhia/Blueprints/PickUpItem/Blueprints/`), child of `AMKHItemSpawnPoint` with no
extra logic (a place for editor visuals). `AMKHItemSpawnGroup` is placed as the native class.

## Server only

Spawn points and groups have `bNetLoadOnClient = false` and do not replicate: clients never load them. They are not
spatially loaded (`bIsSpatiallyLoaded = false`). Everything below runs on the server; clients only see the replicated
pickups.

## Spawn point settings

| Category | Property | Meaning |
|---|---|---|
| Spawn\|Loot | `LootTable` | Shared `UMKHLootTable`; when set it replaces `LootEntries` |
| | `LootEntries` | Inline weighted entries (see "Loot") |
| | `SpawnChance` | 0–100 %, rolled at every attempt |
| Spawn\|Timing | `Trigger` | `PhaseStart` (the match entering one of `TriggerPhases`) or `Manual` (`TrySpawn` only) |
| | `TriggerPhases` | Default `InitialPreparation` (start of every match). `None` / `MatchOver` never trigger |
| | `SpawnDelayMin/Max` | Random delay between the trigger and the attempt |
| | `MinRound` / `MaxRound` | Round range (0 = no limit), checked when the attempt runs |
| | `bRespawnAfterCollected`, `RespawnDelayMin/Max` | New attempt after the pickup is collected |
| | `MaxSpawnsPerMatch` | 0 = unlimited; reset at every match start |
| | `bKeepPickupBetweenRounds` | Default true. False: at `IntermissionPreparation` the uncollected pickup is destroyed and the pending attempt cancelled |
| Spawn\|Conditions | `Conditions` | Instanced `UMKHItemSpawnCondition` list, all must hold |
| | `ConditionFailPolicy`, `RetryInterval` | `Skip` or `RetryUntilSatisfied` (checks again every `RetryInterval`, until the match ends or the attempt is cancelled) |
| Spawn\|Placement | `SpawnGroup` | Optional `AMKHItemSpawnGroup` (instance only) |
| | `ScatterRadius`, `bSnapToGround`, `bRandomYaw` | Where the pickup lands (see "Placement") |
| Spawn\|Collection | `CollectRestriction`, `OwnerPlayerStart` | `Anyone`, or `PlayerStartOwner` = only the participant who started at that PlayerStart |

Blueprint API: `TrySpawn()` (`BlueprintAuthorityOnly`), `GetActivePickup()`, `ResolveExclusiveCollector()`, event
`OnItemSpawned(Pickup)` (server).

## Attempt pipeline

A point holds at most **one live pickup**. An attempt is started by a phase trigger (through the subsystem), by
`TrySpawn`, or by a respawn after collection. `BeginSpawnAttempt` refuses (nothing scheduled) while the match is not
running, when the point is occupied (live pickup or attempt already pending) or when `MaxSpawnsPerMatch` is reached;
otherwise it schedules `ExecuteAttempt` after the delay (right away for 0):

1. the match is still running and the current round is within `[MinRound, MaxRound]` — otherwise skip;
2. every condition holds and the spawn group has a free slot — otherwise `Skip`, or retry after `RetryInterval`;
3. `SpawnChance` — failure ends the attempt;
4. `SpawnPickup`: resolve the exclusive collector (before generating anything, so a missing owner never consumes an
   ItemID) → roll the loot → `UMKHItemSubsystem::GenerateItem(ItemTag, Quantity, RarityRule)` → compute the transform →
   `AMKHPickupItem::SpawnPickupFromItem(…, ExclusiveCollector, EMKHPickupAppearance::RiseFromGround)` → track the
   pickup (`OnDestroyed`) → `OnItemSpawned`. The pickup emerges from the ground (no bounce, `RiseDuration` 0.5 s) and
   cannot be collected until it has risen (see "Animations" in `PickupSystemArchitecture.md`).

When the tracked pickup is destroyed (collected), the point schedules a respawn after `RespawnDelayMin/Max` if
`bRespawnAfterCollected` and `BeginSpawnAttempt`'s checks allow it. At match end the phase is already `MatchOver` when
`DestroyAllPickups` destroys the pickups, so nothing respawns. The point destroys its own pickup (round end without
`bKeepPickupBetweenRounds`, match start) after unbinding, which never triggers a respawn.
A collected pickup is destroyed only at the end of its collection animation (`GetCollectDestroyDelay`, ~0.75 s), so
the respawn delay starts that much after the collection.

## Match flow

`AMKHGameMode::SetPhase` calls `UMKHItemSpawnSubsystem::HandleMatchPhaseChanged(Phase, CurrentRound)` after mirroring
the phase on the game state and **before** `DestroyAllPickups`. The subsystem stores the phase and round (so
`IsMatchRunning` / `GetCurrentRound` are up to date while the points react), then:

| Phase entered | Every point | Then |
|---|---|---|
| `InitialPreparation` (match start, rematch) | `HandleMatchStarted`: cancel the pending attempt, destroy a leftover pickup, reset the spawn counter | triggered points attempt |
| `IntermissionPreparation` (next round) | `HandleRoundEnded`: if not `bKeepPickupBetweenRounds`, destroy the pickup and cancel the pending attempt | triggered points attempt |
| `Duel`, `SuddenDeath` | — | triggered points attempt |
| `MatchOver`, `None` | `HandleMatchEnded`: cancel the pending attempt | nothing (the pickups are destroyed by `DestroyAllPickups`) |

The match is running from `InitialPreparation` to `SuddenDeath`. Rounds: 1 from `InitialPreparation`, +1 at every
`IntermissionPreparation` (`AMKHGameMode::GetCurrentRound`). No pickup of a spawn point survives the match.

## Loot

`FMKHLootEntry { ItemTag, Weight, QuantityMin, QuantityMax, RarityRule }`. `UMKHLootTable::RollEntries`:

1. picks an entry by `Weight` (`UEquipmentRollLibrary::WeightedRandomSelect`);
2. resolves `ItemTag` (`ResolveCandidateItems`): an **exact item** when a definition exists with that tag, otherwise a
   **category** — one item picked uniformly among every definition under it (`UMKHDataSubsystem::GetItemTagsMatching`),
   e.g. `Item.Equipment.Weapon` → any weapon;
3. quantity: 1 for equipment, uniform in `[QuantityMin, QuantityMax]` otherwise;
4. forwards `RarityRule` to `GenerateItem`: `Default` (the item's own rule: table roll restricted to the definition's `RarityTags`), `Fixed`
   (one rarity) or `Weighted` (a roll restricted to the listed rarities with their weights). Only equipment rolls a
   rarity: consumables always take the rarity picked from their definition's `RarityTags` and ignore the rule. A rule naming no rarity of the
   table falls back to `Default` with a warning.

The roll only **selects** what to generate; items and their rolls are still created by `UMKHItemSubsystem` only.

## Spawn groups

`AMKHItemSpawnGroup.MaxActivePickups` = K caps how many member points hold (or are about to spawn) a pickup at once.
When a phase triggers several members, the subsystem shuffles them and lets start only as many as the free slots
(K − members holding a pickup or with an attempt already scheduled). Every attempt checks the capacity again when it
runs (step 2 of the pipeline, with the point's fail policy), counting only the **live pickups** of the other members:
attempts run one at a time and spawn synchronously, so the cap holds, and two waiting members never block each other.
A selected member whose chance or conditions fail leaves its slot empty for that trigger. Example: six points around the arena in one group with K = 2 → two random weapons per trigger.

## Conditions

`UMKHItemSpawnCondition` (Abstract, `EditInlineNew`, `DefaultToInstanced`, `Blueprintable`): override `IsSatisfied`
(C++ `IsSatisfied_Implementation` or Blueprint); `bNegate` inverts the result. `GetWorld` returns the spawn point's world,
so Blueprint conditions can use world nodes. Native conditions:

| Condition | Holds when |
|---|---|
| `No Pawn Nearby` (`Radius`, 300 cm) | No player pawn within `Radius` of the point (no item appears in a player's face) |
| `Score` (`Rule`, `ScoreDifference`) | `Tied`; `LeadAtLeast` (either side); `OwnerBehindAtLeast` / `OwnerAheadAtLeast` (the exclusive owner vs the opponent — catch-up or reward items; needs a `PlayerStartOwner` point) |
| `Match Phase` (`Phases`) | The current phase is listed (useful with manual triggers and respawns) |

## Exclusive pickups

For the preparation rooms: `CollectRestriction = PlayerStartOwner` and `OwnerPlayerStart` = the PlayerStart of that
room. The owner is the participant who **started the match** there: `AMKHGameMode::RegisterParticipant` stores
`AController::StartSpot` (the start the engine spawned the pawn at) in `FMKHPlayerData::StartSpot`, and
`FindParticipantAtStartSpot` resolves it to the participant's player state. Which player gets which start does not
matter (verified with both assignments); the two players always get different starts (see "Start spots" in
`MatchFlowArchitecture.md`). No participant at that start → nothing spawns (warning).

The pickup gets `ExclusiveCollector` = that player state (see "Collection rights" in `PickupSystemArchitecture.md`):
the other player sees it but never gets focus, widget or icon on it, and the server refuses a forged request
(`Server_PickupItem: … is reserved to another player`). Once collected it is a normal item.

Central spawn points keep `CollectRestriction = Anyone`.

## Placement

`ComputeSpawnTransform`: the point's location and yaw (random yaw with `bRandomYaw`). With `ScatterRadius > 0` a random
offset within the radius is traced horizontally from 50 cm above the point (walls stop it, 30 cm clearance); with
`bSnapToGround` the result drops to the ground below (500 cm search, the point's height without ground). Both use
`AMKHPickupItem::FindRestLocation`, shared with inventory drops; the traces ignore every pawn and its attached actors.
Without ground snapping, place the point on the floor: the pickup's root lies on the ground and `BP_ItemBase` lifts
the mesh.

## Placement in Makhion

Folder `Gameplay/ItemSpawns` of `Makhion` (placed 2026-10-18, verified in PIE listen server + 2 clients over two rounds):

| Points | Where | Settings |
|---|---|---|
| `ItemSpawn_East_*`, `ItemSpawn_West_*` (8 per room) | Preparation rooms: `Left1-3` / `Right1-3` along the side walls (x ±2000 / ±2200 / ±2400, 3.6 m off the axis, in front of the bench), `Back1-2` under the back wall (x ±2630, y ±230, beside the weapon rack). Left/right as seen by the room's player facing the arena; the west room is the point mirror of the east one | `DA_ArenaLootTable`, triggers `InitialPreparation` + `IntermissionPreparation`, `bKeepPickupBetweenRounds` false (every round the room gets 8 fresh items), `PlayerStartOwner` with the room's `PlayerStart_East/West` |
| `ItemSpawn_Center_Rare` | Arena centre (0, 0) | `DA_ArenaLootTable_Rare`, trigger `Duel`, delay 20–50 s, `No Pawn Nearby` (300 cm) with `RetryUntilSatisfied`, `bKeepPickupBetweenRounds` false (one rare item per round), `Anyone` |

## Editor

- **Sprite and arrow** (editor only) show the point and the yaw given to the pickup.
- **Preview** (`OnConstruction`, editor worlds only): the pickup mesh of the first loot entry naming an exact item, with
  the same composition as the pickup (item `PickupMeshTransform` on top of the `PickupItemClass` mesh layout). Uses
  `UMKHDataSubsystem::FindItemDefinitionForContext`, which reads the settings when no GameInstance exists.
- **Validation** (`IsDataValid`): empty loot, zero weights, tags matching no item, quantity ranges, equipment quantity,
  rarity rules (unknown rarities, rule on consumables), trigger without phases, dead phases, delay and round ranges,
  0 % chance, exclusive point without `OwnerPlayerStart`, empty condition slots, conditions invalid for the point
  (`UMKHItemSpawnCondition::ValidateForSpawnPoint`: an Owner score rule on a free point). The Blueprint compiler also
  validates the class defaults: on templates the empty loot list and the missing `OwnerPlayerStart` (instance only) are
  not reported. `UMKHLootTable` validates its own entries.
- **Logs** (`LogMKHInventory`): every spawn (`Log`: item, rarity, ID, collector), skipped attempts with the reason
  (`Verbose`), misconfiguration (`Warning`).

## Testing

PIE, listen server + 2 players. Place test points from a Python script run with `py <file>` in the editor console
(`EditorActorSubsystem.spawn_actor_from_class`, properties through `set_editor_property`; instanced conditions with
`unreal.new_object(<ConditionClass>, outer=<point>)`), without saving the level. Drive the match on the **server world**
only: `Server_PickupItem` / `Server_RequestRematch` through `call_method`, deaths with
`AbilitySystemLibrary.send_gameplay_event_to_actor(pawn, Event.Combat.Death)`, `try_spawn()` on the points.

## Known limitations

- Exclusive pickups stay visible to the other player (only the interaction is hidden and refused).
- A group member selected at a trigger whose chance or conditions fail leaves its slot empty until the next trigger,
  respawn or `TrySpawn`.
- `RetryUntilSatisfied` keeps checking across rounds until the match ends (or the round ends for points that do not
  keep their pickup).
