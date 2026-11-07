# Match Flow Architecture

How a 1v1 arena duel is orchestrated, replicated and presented: `AMKHGameMode` (authoritative rules),
`AMKHGameState` (replicated state + event hub), `UMKHPreparationSkipWidget` (ready-up prompt of the initial
preparation) and `UMatchOverWidget` (end-of-match panel and rematch ballot). The same rules run online (two players,
listen server) and offline against a bot (`AMKHDuelBot`), see [Match Modes & the Offline Bot](#match-modes--the-offline-bot).

## Table of Contents

1. [Overview](#overview)
2. [Match Phases](#match-phases)
3. [AMKHGameMode — The Authoritative Orchestrator](#amkhgamemode--the-authoritative-orchestrator)
4. [AMKHGameState — The Replicated Hub](#amkhgamestate--the-replicated-hub)
5. [Final Countdown & Preparation Skip](#final-countdown--preparation-skip)
6. [Arena Gates](#arena-gates) (and [Crowd reactions](#crowd-reactions))
7. [UMatchOverWidget — The End-of-Match Panel](#umatchoverwidget--the-end-of-match-panel)
8. [Match Modes & the Offline Bot](#match-modes--the-offline-bot)
9. [Full Match Walkthrough](#full-match-walkthrough)
10. [Extending the System](#extending-the-system)
11. [Known Limits & Future Work](#known-limits--future-work)

---

## Overview

The system splits along a single line: **the game mode decides, the game state publishes, the widgets react.**

| Concern | Lives in | Exists on |
|---|---|---|
| Match rules, timers, scoring, drains, rematch and preparation-skip arbitration | `AMKHGameMode` | Server only |
| Phase + countdown, scoreboard, rematch and preparation-skip ballots, Blueprint events | `AMKHGameState` | Every peer (replicated) |
| Result presentation, ballot feedback, focus handling | `UMatchOverWidget` / `UMKHPreparationSkipWidget` / `W_MatchInfo` / `AMainHUD` | Owning client only |

The reason the game mode never talks to UI directly is that `AGameMode` **does not exist on clients**. Any
state a widget needs must travel through the game state, which is why every setter on `AMKHGameState`
(`SetMatchPhase`, `StartFinalCountdown`, `SetScore`, `SetRematchVotes`, `SetPreparationSkipVotes`) is
authority-guarded and paired with a replicated property.

The complementary invariant: **the game state never decides anything.** It stores what the server pushed
and fans it out as events. All branching (who scored, whether the match is over, whether a rematch starts)
happens in the game mode.

---

## Match Phases

`EMKHMatchPhase` (declared in `MKHGameState.h`) drives which timer runs and which rules are active.

| Phase | Countdown | Meaning |
|---|---|---|
| `None` | — | Match not started; players still joining |
| `InitialPreparation` | `InitialPreparationDuration` (60s) | Equip phase before the first duel; skippable once both players are ready |
| `Duel` | `DuelDuration` (180s) | Active fight |
| `SuddenDeath` | none | Duel timer expired; a drain effect ticks both players down |
| `IntermissionPreparation` | `IntermissionPreparationDuration` (10s) | Short re-prep between rounds |
| `MatchOver` | none | A player reached `ScoreToWin` (3), or both did (draw) |

```
InitialPreparation ──> Duel ──(timer)──> SuddenDeath
                        │                      │
                        └──(death)──┬──────────┘
                                    v
                          [DeathResolveDelay]
                                    v
                              ResolveRound
                              /          \
                    score < ScoreToWin   score reached
                            v                  v
              IntermissionPreparation      MatchOver
                            │                  │
                            └──> Duel          └──(ballot passes)──> InitialPreparation
```

Both preparation phases end with a **final countdown** (`FinalCountdownDuration`, 3s): the phase does not change,
only a flag and the message on screen do. See [Final Countdown & Preparation Skip](#final-countdown--preparation-skip).

---

## AMKHGameMode — The Authoritative Orchestrator

### Participant registration

Registration hooks `RestartPlayer` rather than `PostLogin`: after `Super::RestartPlayer` the pawn is
spawned *and* possessed, which is the first moment `Controller->GetPawn()` is guaranteed valid. In an offline match
`RestartPlayer` then spawns the bot, which registers the same way (see
[Match Modes & the Offline Bot](#match-modes--the-offline-bot)).

Participants are `ACharacterBase` implementing `IMKHDuelParticipant` (`AMKHPlayerCharacter`, `AMKHDuelBot`): the game
mode, the game state events and the HUD never assume a player character. Whatever differs between a player and a bot
goes through the interface:

| `IMKHDuelParticipant` | Player | Bot |
|---|---|---|
| `ResetForNewRound(SpawnTransform)` | `Multicast_ResetToSpawnTransform` + forced `Multicast_TransitionToMovementState(Idle)` | revive (forced `Idle`, ragdoll off) + teleport |
| `GetParticipantDisplayName()` | replicated username; without one (not signed in) "You" for the local player, empty for the others | `DisplayName` ("The Abyss", localized; never "bot" in player-facing text) |
| `IsHumanParticipant()` | true | false |

Blueprints read it through `UMKHDuelParticipantLibrary` (`GetParticipantDisplayName`, `IsHumanParticipant`), null-safe.

`RegisterParticipant(Participant, StartSpot)` is idempotent (it early-outs on an already-registered character),
refuses characters that are not `IMKHDuelParticipant` and fills the first free slot of the fixed two-element `Players`
array. Each slot caches:

| Field | Role |
|---|---|
| `Participant` | The participant (player or bot) |
| `Score` | Rounds won |
| `SpawnTransform` | Where the round reset teleports them back to |
| `StartSpot` | The start spot (PlayerStart) the pawn spawned at, from `AController::StartSpot`: the player's side of the arena (`FindParticipantAtStartSpot`, used by item spawn points reserved to one side) |
| `SuddenDeathDrainHandle` | Active drain, so it can be removed surgically |
| `bDiedThisRound` | Guards double-counting a death within one round |

Registration also mirrors the slot onto the game state (`GS->RegisterParticipant`) and binds the death
event via `ASC->GenericGameplayEventCallbacks.FindOrAdd(Event::Death)`, passing the character as a bound
payload so the handler resolves the slot without re-casting the event instigator.

`TryStartMatch` fires the moment both slots are filled and the phase is still `None`, but never before the
world has begun play. `AGameMode::HandleMatchHasStarted` restarts the human players *before* dispatching the
actors' `BeginPlay`, so offline (where `RestartPlayer` also spawns the bot) both slots are full while no actor
has begun play yet. Starting `InitialPreparation` there would be missed by everything that registers in
`BeginPlay`, notably the item spawn points (`UMKHItemSpawnSubsystem`): `TryStartMatch` then defers itself to
`UWorld::OnWorldBeginPlay`. Online the second player always logs in after `BeginPlay`, so nothing is deferred.

### Start spots

The engine picks each player's start in `UpdatePlayerStartSpot` at login / seamless travel (`AController::StartSpot`),
before any pawn exists, and `RestartPlayer` then reuses that cached spot. The engine's `ChoosePlayerStart` only skips
starts *occupied by a pawn*, so with no pawn yet both players could get the same random start: the client spawned in
the host's room and the other room's reserved item spawn points found no owner (bug B5, playtest 2026-10-21).
`AMKHGameMode::ChoosePlayerStart_Implementation` therefore returns a random PlayerStart that no other controller holds
as `StartSpot` and no registered participant started at (`IsStartSpotTaken`); with none free (or a PIE
"Play From Here" start) it falls back to the engine. `FindFreeStartSpotForBot` uses the same test.

### Starting loadout

Right after registering a player, `RestartPlayer` calls `GrantStartingLoadout`, which reads `StartingLoadoutTable`
(`FMKHStartingLoadoutRow`, one item per row, set on `BP_MKHGameMode`: `Content/Makhia/Data/Inventory/StartingLoadouts/DT_MKHStartingLoadout_Player`)
and hands its items, in row order, to `UInventoryComponent::GrantStartingLoadout`. Each entry is
generated on the server by `UMKHItemSubsystem::GenerateItem` with its own `FMKHItemRarityRule` and stored in the inventory.
The rows say nothing else: on `OnStartingLoadoutGranted` the armor is worn and the quick slots filled by
`UMKHQuickSlotPreferenceComponent` (`HUDQuickSlots.md`, "Starting loadout quick slot preferences").

Current loadout: bronze helmet, chest and leggings (Fixed Common, equipped), bronze hammer, sword and hatchet (Fixed
Common, in the bag), 5 health, 5 regeneration and 5 strength potions. It runs once per match (again on a rematch, after
`ResetPlayerLoadouts`, see [The rematch ballot](#the-rematch-ballot)): between rounds the pawn is revived, never restarted. The bot is not a player controller and receives nothing.

Two traps met while building it:
- **Listen-server host order.** For the host, `RestartPlayer` runs before the controller's `BeginPlay`, so the
  inventory → equipment delegates are bound in `AMKHPlayerController::PostInitializeComponents`; bound in `BeginPlay`
  the host's armor left the bag without ever being equipped.
- **Blueprint defaults set on the CDO need a recompile.** Instances of a Blueprint class only copy the properties listed
  when the class was compiled; a default written straight on `Default__BP_MKHGameMode_C` (e.g. via script) stays on
  the CDO and spawned game modes keep the native empty array until the Blueprint is compiled and saved.

### Phase scheduling

Every phase follows the same three-line shape: `SetPhase(...)`, `ClearPhaseTimer()`, then schedule the next
transition on the single shared `PhaseTimerHandle`. One handle for every phase *and* for the death-resolve
delay is deliberate — at most one match-flow timer is ever pending, so cancelling is unambiguous.

Preparation phases chain two steps on that same handle (`SchedulePreparationEnd`): the phase timer first
leads to `StartFinalCountdown`, which then schedules `StartDuelPhase` (see
[Final Countdown & Preparation Skip](#final-countdown--preparation-skip)).

`SetPhase` writes the local authoritative `MatchPhase` and mirrors it (with the countdown length) onto the
game state in one call, keeping the two from drifting. It then forwards the phase and `CurrentRound` to the item
spawn points (`UMKHItemSpawnSubsystem::HandleMatchPhaseChanged`, see `ItemSpawnSystemArchitecture.md`), and entering
`MatchOver` destroys every world pickup (`UMKHItemSubsystem::DestroyAllPickups`) — in this order, so the spawn points
stop before their pickups disappear. No pickup outlives the match; within it, each spawn point decides whether its
pickup survives a round (see `PickupSystemArchitecture.md`).

`CurrentRound` (`GetCurrentRound()`, server) is set to 1 by `StartInitialPreparationPhase` (every match, rematches
included) and incremented by `StartIntermissionPhase`, which prepares the next round.

### Death handling and the double-KO case

`HandlePlayerDeath` only reacts during `Duel` or `SuddenDeath`. The subtle part is that it deliberately
**does not change phase on the first death**, so a second death arriving in the same frame still passes the
phase guard and still awards its point — a genuine double KO scores for both players.

The first death instead:

1. sets `bRoundResolving` (re-entrancy guard),
2. clears the phase timer,
3. schedules drain removal **on the next tick**, not immediately,
4. schedules `ResolveRound` after `DeathResolveDelay` (3s) so the death animation can play.

Step 3 is the non-obvious one. During sudden death both players are draining on identical periodic
executions; removing the drains synchronously on the first death would cancel the pending execution on the
second player, so two players on equal health would resolve as a clean win for whoever's periodic tick
happened to land first. Deferring by one tick lets both drains finish the frame, turning that race into the
draw it actually is.

### Round resolution and match end

`ResolveRound` ends the match once either score reaches `ScoreToWin`; **equal scores at that point are a
draw**, which is how a double KO on match point is reported. Otherwise it resets both participants
(`ResetParticipantsForNewRound`: apply `RoundResetEffect`, then `IMKHDuelParticipant::ResetForNewRound` — back to
`SpawnTransform`, revived) and enters the intermission.

For a player, the forced `Multicast_TransitionToMovementState(Idle, true)` is what revives the loser: `UDeadState`
refuses every ordinary transition (`CanTransitionTo` returns `false`), so only a forced transition can leave
it. Its `ExitState` restores capsule collision and hands gameplay input back to the owning client. The bot uses the
same movement state machine (it lives on `ACharacterBase`) and the same forced transitions (see below).

### The rematch ballot

`RequestRematch` is a **per-player ballot**, not a counter: votes land in a `TSet<APlayerController*>`, so
one player hammering the button cannot restart the duel alone. The threshold
(`CountRematchVotersNeeded`) is recomputed on every vote from `GameState->PlayerArray.Num()` — counted
from player states rather than from live pawns, because at match over one participant is dead and whether
its pawn is still possessed must not shift the target. It is clamped to a minimum of 1 so an empty ballot
can never auto-pass.

When the ballot passes, `RestartMatchInPlace` runs instead of `AGameMode::RestartGame`:

> No server travel means no reconnection, no reloaded level and no rebuilt HUD — the rematch is just a
> phase change back to `InitialPreparation`, which every peer already knows how to react to.

It closes the ballot, zeroes both scoreboards (local *and* replicated), clears `bRoundResolving`, the phase
timer and any lingering drain, resets the players' loadouts, revives both participants, then re-enters the initial
preparation phase.

**Loadout reset** (`ResetPlayerLoadouts`): for every player controller, `UInventoryComponent::ResetInventory` destroys
everything the player owns (bag entries and equipped items, each through `RemoveItem`, so it is unequipped and leaves
its quick slot first), then `GrantStartingLoadout` gives the `StartingLoadoutTable` items again. No loot of the previous match
survives a rematch. It runs before the revive so the round-reset effect tops up the stats of the fresh armor; the
pawns are never unpossessed at death, so the equipment visuals have their pawn. The bot gets its profile's loadout
again (`AMKHDuelBot::ResetLoadout`).

> The pause menu's "Restart" is the other path and is deliberately different: `Server_RestartMatch` calls
> `AGameMode::RestartGame`, a full unilateral level restart with no ballot.

---

## AMKHGameState — The Replicated Hub

### Atomic snapshots

The four replicated properties are structs, not loose fields:

| Property | Contents | Why grouped |
|---|---|---|
| `DuelState` | `Phase`, `PhaseEndServerTime`, `PhaseDuration`, `bFinalCountdown`, `PhaseSerial` | One `OnRep` delivers phase and timing together, so a client can never render a new phase paired with the previous phase's countdown |
| `PlayerDatas` | Per-slot character + score | Single array replication for the whole scoreboard |
| `RematchVotes` | `CurrentVotes`, `RequiredVotes` | Prevents rendering "2/0" while the second field is still stale |
| `PreparationSkipVotes` | `ReadyPlayers` (player states), `RequiredVotes` | Same as above; the player list also lets each peer tell whether its own player is ready |

`PhaseSerial` is incremented by every `SetMatchPhase`. `OnRep_DuelState` compares it with the previous snapshot:
a different serial is a phase change (phase events fire), the same serial is only a countdown update of the
current phase (`StartFinalCountdown`), which must **not** replay `OnMatchPhaseChanged` / `OnInitialPreparationStarted`.

Each setter calls `ForceNetUpdate()` so UI-critical transitions reach clients on the next net tick instead
of waiting for the actor's normal cadence.

### The authority broadcast rule

`OnRep_*` never runs on the server, so every authority-side setter broadcasts its event **manually** after
mutating state (`SetMatchPhase` → `HandlePhaseChanged()`, `StartFinalCountdown` → `OnFinalCountdownStarted`,
`SetRematchVotes` → `OnRematchVotesChanged`, `SetPreparationSkipVotes` → `OnPreparationSkipVotesChanged`).
This is why the listen-server host sees the same events as a remote client. Forgetting this call is the
classic way to make a feature that works on a client and silently does nothing on the host.

### Client-side countdown

The countdown is **not** replicated per frame. The server publishes a single deadline in server time
(`PhaseEndServerTime = GetServerWorldTimeSeconds() + Duration`), and each client computes
`GetPhaseTimeRemaining()` locally every frame against its own `GetServerWorldTimeSeconds()` (which
`AGameStateBase` already keeps synchronized). The result is a smooth timer with zero per-frame bandwidth.
`GetPhaseElapsedFraction()` derives the normalized progress for bars from the same two numbers.

### Diffing the scoreboard

A single `OnRep_PlayerDatas` covers any change to the array, so the callback diffs against
`PreviousPlayerDatas` to tell a *registration* (character pointer changed) from a *score update* (score
changed) and fires only the matching event. The snapshot is refreshed at the end of the callback.

### Late binders

`BroadcastExistingPlayerDatas()` re-fires `OnParticipantRegistered` and `OnScoreChanged` for every filled
slot. Any widget created after registration should call it right after binding, otherwise it renders an
empty scoreboard until the next change.

`BroadcastCurrentPhase()` re-fires `OnMatchPhaseChanged`, then `OnFinalCountdownStarted` when the final countdown
is running (it is state, so replaying it only re-applies what listeners show). `AMainHUD` calls it once the
overlay is on screen.

### Event surface

| Event | Fired when | Typical listener |
|---|---|---|
| `OnMatchPhaseChanged` | Any phase change | Generic phase-driven UI |
| `OnInitialPreparationStarted` | Match start **and every rematch** | `AMainHUD::OnMatchRestarted` (tears the panel down) |
| `OnDuelStarted` | Each duel begins | Round banners |
| `OnDuelTimeOver` | Sudden death begins | Warning VFX/SFX |
| `OnIntermissionPreparationStarted` | Between rounds | Prep banners |
| `OnFinalCountdownStarted` | Last `FinalCountdownDuration` seconds of a preparation (early once both players are ready) | `W_MatchInfo` ("stay focused" message) |
| `OnPreparationSkipVotesChanged` | Ready-up ballot changes (vote cast/withdrawn, ballot opened/closed) | `UMKHPreparationSkipWidget` |
| `OnParticipantRegistered` / `OnScoreChanged` | Scoreboard changes | `W_MatchInfo` |
| `OnPlayerDied` | Multicast on each death | Kill feed |
| `OnMatchOver` | Match decided | `AMainHUD::OnMatchOver` |
| `OnRematchVotesChanged` | Ballot changes | `UMatchOverWidget` |

`OnPlayerDied` and `OnMatchOver` are `NetMulticast` RPCs rather than `OnRep`s because they are *moments*,
not state: a client joining later should not replay them.

---

## Final Countdown & Preparation Skip

### The final countdown

Every preparation phase (initial and intermission) ends with a final countdown of `FinalCountdownDuration`
(3s, `Duel|Rules` on the game mode). It is **not** a phase: `MatchPhase` stays `InitialPreparation` /
`IntermissionPreparation`, so spawn points, barriers and every phase listener are unaffected (the
[arena gates](#arena-gates) stay down until the duel starts).

```
Start*Phase ──> SchedulePreparationEnd(Duration)
                  │  PhaseTimerHandle: Duration - FinalCountdownDuration
                  v
               StartFinalCountdown ──> GS->StartFinalCountdown(3)     bFinalCountdown = true
                  │  PhaseTimerHandle: 3s                              OnFinalCountdownStarted
                  v
               StartDuelPhase
```

`StartFinalCountdown` never extends a phase: it uses `min(FinalCountdownDuration, time remaining)`, so a phase
shorter than 3s enters it immediately with its own deadline (and a zero-length one goes straight to the duel).
`GS->StartFinalCountdown` only moves `PhaseEndServerTime` (the scheduled path lands on the original deadline);
`PhaseDuration` is kept, so `GetPhaseElapsedFraction` jumps close to 1 on an early start.

`W_MatchInfo` binds `OnFinalCountdownStarted` and swaps its status line for the "stay focused" message; its
countdown number keeps reading `GetPhaseTimeRemaining()` every tick, so it shows 3‑2‑1 with no extra code.

`W_MatchInfo` is a pediment hanging from the top edge of the screen (`T_MatchInfoPediment`, drawn at 1000x145,
source PNG in `SourceArt/Miscellaneous/MatchInfo`): names and titles on the flanks, scores beside the central
keystone that holds `PhaseTime`, the status line and the ready prompt below its tip. Its slot in `W_HUDOverlay`
has alignment Y = 0 so that it touches the edge. A side without a title (Training dummy) keeps its name centred
in the flank; names too long for the flank end with an ellipsis.

#### The scoreboard looks

`W_HUDOverlay` holds both looks, and the player picks one in Settings > General > Scoreboard Style:

| `EMKHScoreboardStyle` | Instance in `W_HUDOverlay` | Widget |
|---|---|---|
| `Pediment` (default) | `ScoreboardPediment` | `MatchInfo/W_MatchInfo` |
| `Epigraph` | `ScoreboardEpigraph` | `MatchInfo/Epigraph/W_MatchInfo_Epigraph`: open text, no panel |

`UHUDOverlayWidget::ApplyScoreboardStyle` collapses the look not in use. Both stay bound to the game state, so a switch
during a match is up to date; this is safe because the scoreboard only displays state. The graphs must never set the visibility of the widget itself: only
this function does, or the hidden look reappears at the next phase. The Epigraph is a duplicate of
`W_MatchInfo`: a change to the scoreboard logic goes in **both** graphs until one look is dropped.

### The preparation-skip ballot

During the initial preparation (not the intermission) both players can ready up with `IA_SkipInit` (T by
default, player mappable, mapped in `IMC_BaseMapping` and `IMC_UIOpenMapping` so it also works with the
inventory open):

```
IA_SkipInit (Started) ──> AMKHPlayerController::HandleSkipPreparation   (local pre-check: ballot open)
                      ──> Server_ToggleSkipPreparationVote
                      ──> AMKHGameMode::ToggleSkipPreparationVote        (ballot open? participant?)
                            toggle the voter in SkipPreparationVoters    (a second press withdraws the vote)
                            PublishSkipPreparationVotes ──> GS->SetPreparationSkipVotes ──> OnPreparationSkipVotesChanged
                            every participant ready ──> StartFinalCountdown (early)
```

| Moment | Ballot |
|---|---|
| `StartInitialPreparationPhase` (match start, every rematch) | Opened, empty: `RequiredVotes` = **human** participants (2 online, 1 against the bot) |
| Final countdown starts (on schedule **or** because everyone is ready) | Closed: `RequiredVotes` = 0, voters cleared |

The ballot closing at the final countdown is the whole "ready-up disappears under 3 seconds" rule: the server
ignores votes on a closed ballot and the widget hides itself. An early start therefore looks exactly like the
scheduled one — same `OnFinalCountdownStarted`, same message, 3 seconds.

Voters are stored as **player states** and replicated as such, so `IsPlayerReadyToSkip(PlayerState)` derives
"am I ready" from replicated state instead of an optimistic local flag (the problem `bLocalRematchVoteCast` has).

### UMKHPreparationSkipWidget

C++ base of `W_PreparationSkip`, placed inside `W_MatchInfo` under the status line. It binds to the game state
(falling back to `UWorld::GameStateSetEvent` on clients), replays the current ballot on bind, and is visible
(`SelfHitTestInvisible`, it sits over the inventory) only while the ballot is open.

| Situation | Prompt (`EditDefaultsOnly` format, `{Key}` = current key) | Counter |
|---|---|---|
| Nobody ready | `IdlePromptFormat` — "PRESS [T] WHEN YOU'RE READY TO FIGHT" | `0/2 READY` |
| Local player ready | `LocalReadyPromptFormat` — "READY! WAITING FOR YOUR OPPONENT... [T] TO CANCEL" | `1/2 READY` |
| Only the opponent ready | `OpponentReadyPromptFormat` — "YOUR OPPONENT IS READY! PRESS [T] TO FIGHT NOW" | `1/2 READY` |
| Both ready | hidden: the final countdown message takes over | — |

The key comes from `UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice`, which reads the player's
key profile on the device in use (so a remapped key shows even under UI focus) and falls back to the keyboard key
and then to the applied mapping contexts; the widget refreshes on `UEnhancedInputUserSettings::OnSettingsApplied`
and on a keyboard ↔ pad switch. On the controller `IA_SkipInit` is RB. `Txt_SkipPrompt` and `Txt_ReadyCount` are `BindWidgetOptional`;
`BP_OnSkipVotesUpdated` lets the Blueprint animate each change.

---

## Arena Gates

`AMKHArenaGate` (`Source/Makhia/Public/Arena/`, Blueprint `BP_ArenaGate` in `Blueprints/Arena`) is the iron
portcullis closing each preparation room in Makhion. It replaces `BP_MatchBarrier` there (that Blueprint and the
`ToggleLevelBarrier` event of `BP_MKHGameMode` stay for maps that still place it; the game mode ignores the gates).

**Every peer drives its own gate**: the actor binds to `AMKHGameState::OnMatchPhaseChanged` (through
`UWorld::GameStateSetEvent` when the game state replicates late) and nothing of the gate replicates. The state is a
pure function of the replicated phase:

| Phase | Grille (`GateMesh` + `GrilleCollision`) |
|---|---|
| `None`, `InitialPreparation`, `IntermissionPreparation` (final countdown included) | down (drops in `CloseDuration`, 1.2 s) |
| `Duel`, `SuddenDeath`, `MatchOver` | rises in `OpenDuration` (2.5 s) from the duel start, then up |

- The collision is the grille's own: `SM_MKN_Portcullis` has none, so `GrilleCollision` (`BlockAll` box ignoring
  `Camera`, 5 × 4.6 m × 20 cm, its bottom 10 cm under the floor when closed so the editor's box outline does not
  z-fight with it) is attached to `GateMesh` and rises with it. A participant walks out as soon as the grille's feet
  clear their capsule (about 1.1 s into the rise for a 1.9 m capsule), not when the grille is fully up. The camera
  ignores it: the boom does not snap in front of the see-through bars.
- Each peer moves its own grille, so the server lets a player through up to one latency earlier than their client
  sees it: at worst a small movement correction at the gate, during the first second of the duel.
- Binding applies the current state instantly (`SetGateOpen(..., bInstant = true)`): a late joiner or a client
  whose game state replicated after `BeginPlay` sees the right position without an animation. The replayed
  `BroadcastCurrentPhase` events are idempotent for the gate.
- Rising eases in and out (smoothstep), dropping accelerates; a reversal mid-travel finds the travel fraction
  that keeps the current height (`FindAlphaForLift`), so the grille never jumps. The actor ticks only while the
  grille moves.
- **Sounds** (slots in Project Settings → Makhia Audio → *Arena Gates*, `AudioSystemArchitecture.md`): loaded at
  `BeginPlay` (nothing on a dedicated server), attached to the grille. A change of direction plays the rise / drop
  sound and fades out the previous one (0.2 s); reaching the end of travel plays the raised clank / ground slam. An
  instant snap (late join, binding) plays nothing.
- `LiftHeight` (440 cm) leaves the spike tips just under the arch crown. The geometry it rises into (a chamber on the
  tribune) is in `Documentation/Handoff/MakhionHandoff.md` ("Gate portcullis").

### Match intro

`AMKHMatchIntro` (`Source/Makhia/Public/Arena/`, actor `MatchIntro` in Makhion) plays a short camera flight on
every peer for its own local player when the first initial preparation starts (once per level load, not on a
rematch, skipped on a late join), then hands the view back to the player camera it ends on. It does not touch the
match flow: the preparation countdown keeps running underneath. Holding `IA_SkipIntro` (Space / Gamepad A, 1 s)
skips it, with a "hold to skip" hint. Details: `Handoff/MakhionCinematics.md`.
The intro starts only once the local pawn is the view target (the sequence restores the view target it started
from: a client starting right after joining would get its controller back, camera stuck inside the pawn), and
`RestorePlayerView` hands the view back to the pawn if it is still not the view target at the end.
The intro hides the HUD overlay and the world widgets (overhead bar, damage numbers) through `AMainHUD::SetHUDOverlayHidden`, independently of the player's Toggle HUD
key (`HUDQuickSlots.md`).

### Crowd reactions

`AMKHCrowdDirector` (`Source/Makhia/Public/Arena/`, one per arena level, label `CrowdDirector` in Makhion) makes the
spectators react to the duel. Like the gates, **every peer runs its own** from replicated data and nothing of it
replicates: it binds to the game state (through `UWorld::GameStateSetEvent` when it replicates late) and to each
participant's `ACharacterBase::OnHealthChanged` (Health replicates to every peer, `COND_None`).

| Event | Source | Excitement |
|---|---|---|
| Duel starts (each round) | `OnDuelStarted` | `DuelStartExcitement` 0.8 |
| Sudden death | `OnDuelTimeOver` | `SuddenDeathExcitement` 0.6 |
| Hit during `Duel` / `SuddenDeath` | Health drop ≥ `MinHitHealthFraction` (4 % of max: drain ticks don't count) | `MinHitExcitement` 0.25 + lost fraction × 2.5 |
| Death | `OnPlayerDied` | `DeathExcitement` 1.0 |
| Match decided | `OnMatchOver` | `MatchOverExcitement` 1.0 |

- The level starts at `MinExcitement` (0.25) and never decays below it: the crowd keeps a base level of cheering
  (and of excited spectators) for the whole level.
- An event raises the level to at least its value and holds it `HoldSeconds` (1.5 s), then it decays at
  `DecayPerSecond` (0.25/s) back to the floor; the written value eases up at `RisePerSecond` (3/s) so the crowd does not snap. The actor
  ticks only while the value moves.
- The value goes to the scalar `Excitement` of `CrowdCollection` (`MPC_MKN_Crowd`). The crowd material (`M_MKN_Crowd`)
  blends every spectator from its idle clip to its excited clip with it; heals and round resets (health going up) are
  ignored. Crowd model: `Documentation/Handoff/MakhionHandoff.md` ("Crowd").
- The same events play the crowd sounds through the director's `CrowdAudio` (`UMKHCrowdAudioComponent`): the cheering
  loop follows the displayed excitement, each event has a one-shot (an empty slot is silent). Sound slots: Project Settings → Makhia Audio →
  Crowd (`AudioSystemArchitecture.md`).
- `AddExcitement(Level)` is Blueprint-callable for future triggers (big combos, item pickups, cinematics).
- **Density**: the director also thins the crowd on lower **Effects** quality. `ue_crowd.py place` spreads the
  spectators uniformly over 3 tiers, one instanced component per variant and tier tagged `CrowdTier<N>`; the director
  (`CrowdActor` = `MKN_Crowd`) shows tier N only when Effects quality ≥ N, at `BeginPlay` and on every
  `Scalability::OnScalabilitySettingsChanged` (the video menu's Apply). Low ≈ 1/3 (422), Medium ≈ 2/3 (779),
  High / Epic all (1167). Not `DetailMode`: the engine gives Effects Medium and High the same `r.DetailMode` (1), so
  High would have shown only two thirds. Hidden components are not rendered; the VAT textures stay loaded.
- Checked in a bot match (PIE standalone): duel start 0.8 each round, heavy hits and the death 1.0. Density checked
  in PIE through `UGameUserSettings` (as the menu): 422 / 779 / 1167 / 1167 visible on Low / Medium / High / Epic.

---

## UMatchOverWidget — The End-of-Match Panel

### Lifecycle

The panel is owned by `AMainHUD`, which binds both ends of its life at `BeginPlay`:

```
GameState->OnMatchOver              ──> AMainHUD::OnMatchOver     ──> CreateMatchOverWidget + ShowMatchResult
GameState->OnInitialPreparationStarted ──> AMainHUD::OnMatchRestarted ──> DestroyMatchOverWidget
```

Binding to `OnInitialPreparationStarted` for teardown is what makes the in-place rematch work: with no
level reload, nothing else would ever remove the panel from the viewport.

On clients the game state has usually not replicated when the HUD spawns, so `BindMatchOverEvent` falls
back to `World->GameStateSetEvent` and binds as soon as the world receives it.

Win/lose is decided by comparing **player states, not pawns** (`AMainHUD::OnMatchOver`): the losing pawn is
often already destroyed by the time the panel is built. A bot has no player state, so a bot winner always reads as a
defeat for the local player.

### C++ / Blueprint split

C++ owns the flow — visibility, ballot subscription, button actions, teardown. The Blueprint owns the look
via `SetMatchResult(bLocalPlayerWon, bIsDraw)`, a `BlueprintImplementableEvent` that fills in the headline
and accent colours. The only widget C++ reaches into is `Txt_Rematch`, bound with `BindWidgetOptional` so a
designer renaming or removing the label degrades the caption instead of breaking the panel.

### Why the RPC goes through the player controller

`UUserWidget` has **no RPC callspace resolution**, so a `UFUNCTION(Server)` declared on a widget is simply
never sent. Both widget actions therefore delegate to the controller:

| Action | Route |
|---|---|
| Rematch | `UMatchOverWidget::RequestRematch` → `AMKHPlayerController::Server_RequestRematch` → `AMKHGameMode::RequestRematch` |
| Pause-menu restart | `UMKHGameMenuWidget::RestartGame` → `AMKHPlayerController::Server_RestartMatch` → `AGameMode::RestartGame` |

### Ballot feedback

`BindRematchBallot` subscribes with `AddUniqueDynamic` (so a second `ShowMatchResult` cannot double-subscribe)
and then **immediately replays the current tally**, because the opponent may have voted before this panel
existed. `NativeDestruct` and `DestroyMatchOverPanel` both unsubscribe.

The caption reflects three states, driven by `bLocalRematchVoteCast`:

| Condition | Caption |
|---|---|
| No votes yet | `REMATCH` |
| Someone voted, not us | `REMATCH n/m` |
| We voted | `WAITING n/m` |

Return-to-lobby calls `UMKHGameInstance::LeaveToNewLobby` on both this panel and the pause menu: leaving a 1v1
match is always a solo return, so the player re-enters a fresh lobby rather than trying to rejoin the dead session —
as a listen server when logged in (always the case online), offline otherwise (e.g. after a match against the bot).

---

## Match Modes & the Offline Bot

### Choosing the mode

`EMKHMatchMode` (`GameMode/MKHMatchMode.h`) travels in the URL of the game level: `AMKHGameMode::InitGame` reads the
option `MKHMatchMode::OptionKey` (`?Mode=Bot` → `OfflineBot`). **No option means `OnlineDuel`**, so the arena opened
straight from PIE (two instances, one listen server) keeps working as a network duel without any login.

| Entry point | Result |
|---|---|
| `UMKHGameInstance::StartOfflineMatch()` (offline modes of the lobby selector after the cancellable wait of `RequestOfflineMatch`, *Restart Game* of the game menu) | `OpenLevel(Game)` without `?listen`, with `?Mode=Bot` |
| Console `MKHPlayVsBot` (development, `UFUNCTION(Exec)` on the game instance) | Same as above |
| Normal lobby flow (ready → 2 s cancellable countdown, or matchmaking → `ServerTravel`) | `OnlineDuel` |

`StartOfflineMatch` refuses while matchmaking or with other players in the lobby (the bot is a solo mode). A lobby
session hosted alone is destroyed without waiting for the answer (`DestroyCurrentSessionDetached`): nothing is created
on arrival, so the pending destroy cannot collide with anything, and the login is kept — `LeaveToNewLobby` hosts the
lobby again afterwards.

### Spawning the bot

```
RestartPlayer(player)      RegisterParticipant(player, StartSpot)          slot 0
  └ OfflineBot:            SpawnBotOpponent()
                             FindFreeStartSpotForBot   (a PlayerStart no participant took)
                             SpawnActor(BotClass)      (BP_DuelBot, AdjustIfPossibleButAlwaysSpawn)
                             RegisterParticipant(bot, BotStart)              slot 1
TryStartMatch              2 slots filled → InitialPreparation
```

`BotClass` is set on `BP_MKHGameMode` (`Duel|Bot`). Without it (or without a PlayerStart) the error is logged and the
match never starts.

### AMKHDuelBot

`AMKHDuelBot : AEnemyBase` — own ASC (Minimal replication), attributes from its `CharacterTag` through
`DA_CharacterClassInfo` (`BP_DuelBot` uses its own `Character.Enemy.DuelBot` → `GE_DefaultAttributes_DuelBot`: 50 health,
armor 0; tune the bot there, never on the shared `Character.Enemy.Primary` entry), possessed by
`AMKHDuelBotController` (see *Bot AI* below). The player's abilities damage it like any pawn (weapon and DMT
traces look for the Pawn object type, damage goes through its ASC).

- **Movement**: the bot has the player's movement framework (`ACharacterBase`: components, `UMovementStateMachine`,
  `ABP_MKHPlayerCharacter` animations) with the player's tuning in `BP_DuelBot` (walk 100, run 400, sprint as default).
  Path following drives the velocity-based states (Idle/Walking/Sprinting); a dodge takes its direction from
  `UDodgeSystemComponent::SetRequestedDodgeDirection` (see `CharacterSystemArchitecture.md`).
- **Death**: `UMKHAttributeSet::TrySendDeathEvent` sends `Event.Combat.Death` as for players; the bot listens on its own
  ASC (`HandleDeathEvent`) next to the game mode and goes through the movement state machine like `GA_Die` does for the
  player: `Multicast_TransitionToMovementState(Dead, true)`. `UDeadState` adds `State.Movement.Dead` (blocks damage
  and a second death event) and turns the capsule into a corpse on every machine; `ABP_MKHPlayerCharacter` (shared with
  the player) plays the `A_Die` animation in its Dead state. **No ragdoll**: the bot's optional ragdoll
  (`bRagdollOnDeath`, profile `RagdollCollisionProfile`, replicated through `bIsDead`) is off on `BP_DuelBot`, since it
  would replace the death animation.
- **Revive** (`ResetForNewRound`): forced `Multicast_TransitionToMovementState(Idle, true)` (tag removed, Pawn collision
  restored by `UDeadState`), mesh layout (cached at `BeginPlay`) restored, then teleport to the spawn transform.

### Bot AI

`AMKHDuelBotController : AAIController` (`Source/Makhia/Public/AI/Controller/`, server only, log `LogMKHAI`), Blueprint
`BP_DuelBotController`, set as `AIControllerClass` of `BP_DuelBot` (which faces its walking direction,
`bOrientRotationToMovement`). Assets in `Content/Makhia/Blueprints/Characters/AI` (`BP_DuelBot`, `Controller/`,
`StateTree/`, `Attribute/`). On possess the controller picks the first human `IMKHDuelParticipant` as opponent
(`GetOpponent`) and starts its `UStateTreeAIComponent` (`ST_DuelBot`, StateTree AI Component schema; logic started on
possess, not on BeginPlay, stopped on unpossess). The arena needs a `NavMeshBoundsVolume`.

```
Root
├─ T-Bag / Celebrate   [enter: bShouldTaunt]                       (see Taunt below)
├─ Wait          [enter: !bCanFight]          Delay → Root
├─ Uncrouch      [enter: crouched]
├─ Combat        Random Weighted By Utility (one consideration ^k each, Weight 1)   [on tick: !bCanFight → Root]
│  │             [on event Event.AI.ReactToAttack → React]
│  ├─ SafeApproach  [enter: band == Far, State Enabled]      Constant 1 (not sharpened)   MKH Approach Opponent
│  ├─ Attack     [enter: band != Far, bHasAttackCandidates, State Enabled]   MKH Commit Attack
│  │             Decisions.Scores.Attack
│  │  ├─ ReadChoice      Delay 0 → next selectable state
│  │  ├─ SwapWeapon      [enter: ChosenAttack.bNeedsSwap]   Activate By Input (SwapInputTag) → next selectable state
│  │  ├─ CombatApproach  [enter: Distance > ChosenAttack.EngageDistance]   MKH Approach Opponent (radius EngageDistance)
│  │  │                  → next selectable state; 3 s → Combat
│  │  └─ ActivateAbility Activate By Input (ChosenAttack.InputTag) → Combat
│  ├─ Bait       [State Enabled]   Decisions.Scores.Bait   MKH Enable Bait Awareness
│  │             Random Weighted: Dodge (forward) | Block | ExposeBack (see DuelBotAI.md)
│  └─ PlayPassive  [State Enabled]   Decisions.Scores.PlayPassive
│                  (HealUp: InstantPot / RegenPot, Recover)
└─ React         [State Enabled]   Highest Utility
   ├─ Dodge            [State Enabled, Can Activate Dodge]   Decisions.Scores.Dodge
   ├─ Block            [State Enabled]   Decisions.Scores.Block
   └─ ActivateAbility  [State Enabled, bHasCounterAttack]   Decisions.Scores.Counter   (leaf)
                       MKH Commit Attack (CounterAttackCandidates, log) + Activate By Input
                       (CounterAttack.InputTag, SwapInputTag = CounterAttack.SwapInputTag) → Combat
   (Combat > Idle [AI.State.Idle]: debug fallback, enabled only while a state is disabled)
   every Combat leaf: on completed → Combat (re-scores)
```

**Attack choice** (M8): which attack, from which weapon, is decided by the *MKH Duel Bot Attack Options* evaluator
and the *MKH Commit Attack* task with the pure library `FMKHAttackScoring`; the full model (considerations,
expertise sharpness, reading levels, punish windows, danger, tests) is in `Documentation/DuelBotAI.md`.

**State scores and tuning** (M8.1): the *MKH Duel Bot Decisions* evaluator scores every state with the pure library
`FMKHStateScoring` (Attack, Bait, PlayPassive, React > Block / Dodge / counterattack); each state has one
consideration that links its score. Generic scoring parameters live in `DT_DuelBotScoring`
(`FMKHDuelBotScoringRow`), referenced by every `DA_DuelBotProfile_*` (`ScoringTable`), read every tick through
`AMKHDuelBotController::GetScoring`; the profile keeps the personality. Debug console: `MKH.Bot.Pause`/`Resume`,
`MKH.Bot.State.*` (runtime state toggles, condition *MKH Bot State Enabled*), `MKH.Bot.Tuning.Set/Dump/Reset`,
`MKH.Debug.SetHealth`/`SetStamina`. Details: `Documentation/DuelBotAI.md`.

**Utility selection** (M5, sharpened in M8): `Combat` picks a child at random weighted by score; every *MKH Logged
Float Input* consideration of its children raises its score to k = `Expertise` x 6 (`bApplyExpertise`, `Expertise`
bound to `Tuning.Expertise`), so a novice bot spreads its choices and an expert one nearly always takes the best. A state's score is its considerations combined in order (And = min, Or = max,
Multiply) times its `Weight`; a state without considerations scores 0 (use a Constant consideration). Scores are computed **only when a
transition re-selects** `Combat`, so every leaf returns to `Combat` when it completes (commitment: an action, once
started, runs to its end). **Hard vetoes are enter conditions** (distance band, ability ready): if the best state
fails them the selection tries the next one, whereas a 0 score does not exclude a state. Score curves live in
`DT_DuelBotScoring` (X = normalized input, Y = score).

**Fair reactions** (M6): the bot sees the opponent's attack (`Ability.Attacking` on its ASC) like a human would, late
and not always. When an attack starts the evaluator rolls `ReactionChance` **once** (rolling every tick would turn any
chance into a certainty); after `ReactionTime`, if the roll passed and the distance band is Combat,
`bShouldReact` turns true and the evaluator sends the StateTree event `Event.AI.ReactToAttack` **once per attack**.
The `Combat → React` transition consumes it, so the bot reacts at most once per attack; an event no transition takes in
that tick is lost (the bot was busy: no reaction, as for a player). The transition has no condition (High priority):
a reaction during the bot's own strike follows the player's priority rules (`AbilityCancelSystem.md`; `DuelBotAI.md`, *Reactions and priority*).
After an attack ends, `SecondsSinceOpponentAttackEnded` feeds the Recovery punish window
(`DuelBotAI.md`, *Punish windows*).

**Difficulty profiles** (M7): difficulty changes how the bot decides and reacts and its loadout (`LoadoutTable`, see
*Loadout* below), **never its base attributes** (`GE_DefaultAttributes_DuelBot`), and never its aim: when the bot attacks, it always attacks towards the opponent (the controller's `SetFocus` is instant; an
aim error was ruled out on purpose). The parameters are a `FMKHDuelBotTuning` inside a `UMKHDuelBotProfile`
(`UPrimaryDataAsset`, `Source/Makhia/Public/AI/Data/`), assets `DA_DuelBotProfile_Easy/Medium/Hard` in
`Content/Makhia/Data/AI/Profiles/`.

| Field | Used by | Easy | Medium | Hard |
|---|---|---|---|---|
| `ReactionTimeRange` (s) | Perception: reaction delay rolled **once per attack** in the range (a fixed delay can be learnt and exploited) | 0.45-0.65 | 0.28-0.40 | 0.20-0.28 |
| `ReactionChance` | Perception: reaction roll per attack | 0.5 | 0.7 | 0.9 |
| `BaitReactionTimeRange` (s) | Perception: `ReactionTimeRange` while the bait awareness is on | 0.20-0.40 | 0.15-0.25 | 0.15-0.25 |
| `BaitReactionChance` | Perception: `ReactionChance` while the bait awareness is on | 0.9 | 0.99 | 0.99 |
| `Expertise` (0-1) | sharpness of the choices: k = `Expertise` x 6 | 0.3 | 0.7 | 0.9 |
| `OpponentReadingFactor` | what the bot knows about the opponent (`DuelBotAI.md`, section 4) | Low | Medium | High |
| `Aggression` (0-1) | `FMKHStateScoring`: raises the Attack and counterattack scores | 0.4 | 0.5 | 0.7 |
| `BaitWilling` (0-1) | `FMKHStateScoring`: scales the Bait score | 0.7 | 0.5 | 0.3 |
| `TauntChance` (0-1) | Context evaluator: taunt roll, **once per won round** (`bShouldTaunt`) | 0.3 | 0.7 | 1.0 |

Keep the `ReactionTimeRange` floor at 0.2 s: below it no human reacts and the bot reads as cheating.
`BaitReactionTimeRange` starts at 0.15 s on Medium and Hard: there the bot is already waiting for the attack.

- **Selection**: the travel URL option `?BotProfile=<ProfileId>` (`MKHMatchMode::BotProfileOptionKey`) is read by
  `AMKHGameMode::InitGame` into `RequestedBotProfileId`; `SpawnBotOpponent` looks it up in `BotProfiles` (set on
  `BP_MKHGameMode`, case-insensitive match on `UMKHDuelBotProfile::GetProfileId`) and calls
  `AMKHDuelBotController::SetProfile` right after the spawn (the controller already exists: `AutoPossessAI` possesses
  during the spawn). No option or an unknown id → `DefaultProfile` of `BP_DuelBotController`. The profile used is logged
  (`LogMKHAI`: `plays with profile ...`). Test: `open Makhion?Mode=Bot?BotProfile=Hard`.
- **Reaching the tree**: the evaluator copies `AMKHDuelBotController::GetTuning()` into its `Tuning` output every tick
  (default values without a profile), so tasks and considerations bind to its fields; a runtime change of the profile
  (future DDA) is picked up on the next tick.
- **Personality in the scores**: a state's `Weight` cannot be bound, so `FMKHStateScoring` multiplies `Aggression`
  (curves `States.Attack.Aggression`, `States.React.CounterAggression` of the scoring row, with floors that keep a
  cautious bot fighting) and `BaitWilling` into the state scores; the tree only links the result.
- **Not exposed to the player** (decision): there is no difficulty menu; the Random Duel adjusts it invisibly and the career
  shows it as levels (see `OfflineModes.md`).

- **MKH Duel Opponent** (`FMKHDuelBotContextEvaluator`, `Public/AI/StateTree/Evaluator/`): every tick outputs `Opponent`,
  `Distance` and `bCanFight` (opponent valid and neither side carries `State.Movement.Dead`, read through
  `IAbilitySystemInterface`, so on the player's PlayerState ASC too; the round reset removes the tag and the fight
  resumes). Conditions and tasks bind to these outputs.
- The evaluator also outputs `SelfHealthRatio`, `SelfStaminaRatio`, `OpponentHealthRatio`, `OpponentStaminaRatio`
  (attribute / max on each side's ASC) and `SecondsSinceLastAttack` (time since the bot last carried
  `Ability.Attacking`, the activation owned tag of `UMKHDamageAbility`; large at round start).
- **MKH Can Activate Ability** (`FMKHCanActivateAbilityCondition`): true when the ability bound to `InputTag` passes
  `CanActivateAbility` (granted, cooldown, cost, blocking tags), the player's rules. Used as the enter condition of the
  attack leaves. The ability is found with `UMKHAbilitySystemComponent::FindAbilitySpecForInputTag` (a quick slot
  input resolves to `GA_QuickSlot`); for a quick slot input on `AMKHDuelBot` it returns `CanUseQuickSlot` instead
  (see *Weapons and consumables*), since `GA_QuickSlot` can activate even on an empty slot.
- **MKH Logged Float Input** (`FMKHLoggedFloatInputConsideration`): the engine's Float Input consideration (input
  normalized over `Input Range`, then a response curve; empty curve = linear) plus a Visual Logger line on the bot's
  pawn (`LogMKHAI`, Verbose): `Utility <Label>: input … -> …, curve …, ^k -> score …`. With `bApplyExpertise` the
  score is raised to k = `FMKHDuelBotTuning::ComputeDecisionSharpness(Expertise)` (`Expertise` is an optional
  Parameter binding, default 1).
- The evaluator's reaction outputs: `bOpponentAttacking`, `SecondsSinceOpponentAttackStarted` (0 when not attacking),
  `SecondsSinceOpponentAttackEnded` (very large while attacking or before any attack), `bShouldReact`; parameters
  `MaxReactionDistance`; the reaction delay and chance come from the difficulty profile (`Tuning` output, see above).
  The `attack started` Visual Logger line shows the roll and the reaction time drawn. `bCanFight` also
  requires `AMKHGameState::IsDuelRunning` (Duel or SuddenDeath phase).
- **MKH Request Dodge Direction** (`FMKHRequestDodgeDirectionTask`): stops the path following (its input would bend the
  dodge) and calls `UDodgeSystemComponent::SetRequestedDodgeDirection` with a random side of the opponent, or away
  from it (`BackwardChance`); finishes in `EnterState`, so turn off "considered for completion" (the Activate By Input
  task in the same state dodges through `GA_Dodge`).
- **MKH Find Reposition Point** (`FMKHFindRepositionPointTask`): random point in a ring (400-600 cm) around `Target`,
  projected on the NavMesh, written to `Destination`; finishes in `EnterState`, so it is not considered for state
  completion (the Move To in the same state ends it).
- **Taunt after a won round** (M8): the Context evaluator outputs `bWonRound` (bot alive, opponent valid and carrying
  `State.Movement.Dead`; true from the decisive death to the round reset, which comes `DeathResolveDelay` later, and
  for as long as the field stays frozen after `MatchOver`) and `bShouldTaunt`, rolled with `Tuning.TauntChance` once
  when `bWonRound` turns true and kept until it turns false (a roll per tick would make it certain). *MKH Crouch Spam*
  (`FMKHCrouchSpamTask`) presses the player's crouch key (`UCrouchSystemComponent::CrouchPressed`, same gates) every
  `ToggleIntervalRange` (rolled per press, 0.2-0.35 s), stands up and succeeds after `MaxDuration` (4 s, 0 = until the
  state is left: the cap matters only on the match-over screen) and always stands up in `ExitState` (`ForceUncrouch`,
  headroom checked). On the server `Crouch()` sets the replicated `bIsCrouched` read by the animation and the movement
  states, so clients see it. `BP_DuelBot` needs Character Movement → *Can Crouch* (as the player's BP), otherwise only
  the animation crouches, not the capsule. Tree layout:

  ```
  Root
  ├─ Celebrate  [enter: bShouldTaunt]  [on tick: !bShouldTaunt → Root]   (first child of Root)
  │  ├─ GoToBody  Move To (Opponent, radius 60)   on completed → TBag (also when the path fails)
  │  └─ TBag      MKH Crouch Spam                  on succeeded → Wait
  ├─ Wait       [enter: !bCanFight]
  └─ Combat     [on tick: !bCanFight → Root]     (Root, not Wait, so Celebrate is tried first)
  ```
- **Weapons and consumables** (loadout, M8): the bot owns the player's three item components **on the pawn**
  (`UInventoryComponent`, `UEquipmentManagerComponent`, `UQuickSlotManagerComponent`), wired by
  `FMKHItemComponentLinks::Bind` exactly as `AMKHPlayerController` wires its own (use → equip, unequip → back to the
  inventory, inventory quick slot flags → manager). `AMKHDuelBot` implements `IInventoryInterface`,
  `IEquipmentInterface` and `IQuickSlotInterface` (the inventory list looks them up on its owner, the pawn) and
  `AMKHDuelBotController` forwards the same three to the pawn (`GA_QuickSlot` reaches them through the avatar's
  controller, like the player's).
  - **Loadout**: each difficulty profile assigns a `LoadoutTable` (`FMKHStartingLoadoutRow`, one item per row; row
    order = loadout order; today all three use `Content/Makhia/Data/Inventory/StartingLoadouts/DT_MKHStartingLoadout`;
    same row as the player's table, see *Starting loadout*). After `SetProfile`,
    `AMKHGameMode::SpawnBotOpponent` reads the active profile (`BuildBotLoadout`, `DefaultProfile` included) and calls
    `AMKHDuelBot::EquipLoadout`, which runs
    `UInventoryComponent::GrantStartingLoadout`, the player's entry point (items from `UMKHItemSubsystem`). On its
    `OnStartingLoadoutGranted` the bot uses the player's default rules (`FMKHDefaultQuickSlots`, shared with
    `UMKHQuickSlotPreferenceComponent`): the rarest armor of each type is worn, the rarest item of each kind, kinds in
    row order, goes into Weapon Primary/Secondary and Consumable First/Second/Third, except for the weapons the offline
    mode chose (`ChooseBotStartingWeapons`, passed to `EquipLoadout`; the others stay in the bag); assigned directly with `AddEntryToQuickSlot` (the bot has no owning client). The player starts unarmed and quick equips on
    the first attack (`AMKHPlayerController::EnsureWeaponEquipped`), a path the bot does not have, so the bot then uses
    its first weapon quick slot (Primary) at once. `DT_MKHStartingLoadout`: Bronze Hatchet (Primary), Bronze Hammer
    (Secondary), 5 Health, 5 Health Regen, 5 Strength potions, bronze armor. A profile without a table gives no items
    (logged as an error).
  - **Rematch**: `AMKHGameMode::ResetPlayerLoadouts` also calls `AMKHDuelBot::ResetLoadout` (inventory reset, loadout
    granted again), so potions drunk in the previous match come back, as for the player.
  - **Swap and drink**: the StateTree presses the player's quick slot input with *MKH Activate Ability By Input*
    (`Input.Ability.QuickSlot.Weapon.Primary/Secondary`, `Input.Ability.QuickSlot.Consumable.First/Second/Third`):
    `AbilityInputPressed` → `Event.QuickSlot.Use` → `GA_QuickSlot` (granted by `DA_CharacterClassInfo`,
    `Character.Enemy.DuelBot`) → `UQuickSlotManagerComponent::UseQuickSlot` → `UInventoryComponent::UseItem`. Same
    rules, costs, animations and cooldowns as the player; no bot-only path. A weapon swap is instant (no sheathe
    montage), a potion plays the drink montage and is consumed at its start. Pressing the slot of the weapon in hand
    does nothing (player's rule).
  - **Queries for the StateTree** (`AMKHDuelBot`, `BlueprintPure`, category Duel|Bot|Items): `GetActiveWeaponQuickSlot`,
    `GetOtherWeaponQuickSlot` (empty when there is nothing to swap to), `CanSwapWeapon`, `IsUsingQuickSlot`
    (`State.General.QuickSlot.Use`: a swap lasts one frame, a drink its montage), `CanUseQuickSlot(Slot)` (item bound,
    not the weapon in hand, consumable off cooldown, `GA_QuickSlot` not running nor blocked),
    `GetQuickSlotQuantity(Slot)`, `FindConsumableQuickSlot(ItemTag)` (e.g. `Item.Consumable.HealthPotion`).
  - **Debug** (not in Shipping): `MKH.Bot.Items` logs the bot's weapon and quick slots (`LogMKHAI`);
    `MKH.Bot.PressInput <InputTag>` taps an input on the bot's ASC like the task and logs the state before and after.
  - `bIsHoldingWeapon` follows the weapon slot.
  **Order**: the equipment must come after the default attributes. `AGameMode` restarts the players, and so spawns
  the bot, before the world's `BeginPlay` when the match is ready at once (`HandleMatchHasStarted` →
  `RestartPlayer`, then `NotifyBeginPlay`); the bot's default attributes are given at its `BeginPlay`
  (`AEnemyBase::InitAbilityActorInfo`) and would overwrite the Health raised by the equipment stats (bot starting
  below full health). `EquipLoadout` called before `BeginPlay` therefore keeps the loadout in `PendingLoadout` and
  grants it at `BeginPlay`; the stat effects then scale a full Health (`UMKHAttributeSet::AdjustAttributeForMaxChange`).
- **Equipment lookup**: the project reaches a character's equipment through its controller's `IEquipmentInterface`
  (damage calculation `UExecCalc_Damage::GetEquippedWeapon`, `GA_QuickSlot`, `GA_SheatheWeapon`):
  `AMKHDuelBotController` implements it (and `IInventoryInterface`, `IQuickSlotInterface`) and returns the bot's
  components. The async grants of equipment abilities and
  stats (`UMKHAbilitySystemComponent::GetWeakEquipmentManager`) look on the avatar first, then on the PlayerController.
- **MKH Activate Ability By Input** (`FMKHActivateAbilityByInputTask`): presses and releases an input tag on the
  bot's ASC (`AbilityInputPressed/Released`, the player's keys' entry point: same cooldowns, costs, queueing). Running
  while the ability runs, Succeeded when it ends, Failed if nothing starts within `StartTimeout` (logs whether an
  ability is granted for the tag); leaving the state early cancels the ability. The running ability is found with
  `FindAbilitySpecForInputTag`, so a quick slot input follows `GA_QuickSlot` (swap: Succeeded the next frame; drink:
  at the end of the montage). An input the ASC queues (e.g. pressed in an attack's input buffer window) is replayed
  by `OnActivateQueuedAbility` straight on the bot's ASC (no `AMKHPlayerController`), so it can still start within
  `StartTimeout`. `HoldDuration` > 0 keeps the input
  pressed (Block) and releases it in `Tick`, or in `ExitState` when the state is left first. The input is **latched on
  entering** the state (a binding to a live evaluator output, e.g. `Decisions.CounterAttack`, changes once the ability
  runs and would retarget the task). Optional `SwapInputTag` (Parameter): pressed first (instant weapon swap), the
  input once `GA_QuickSlot` is done; used by React > ActivateAbility. The controller focuses the opponent
  (`SetFocus`), so abilities that turn towards the control rotation aim at it.
  **Combos**: the task performs **one strike**. While it runs, it listens to `Event.Animation.ContinueCombo.Start/End`
  on the bot's ASC (sent by `AN_ContinueComboStart/End` to the mesh owner, so on the server where the bot's montage
  plays; abilities without combos never send them). When a window opens it returns Succeeded **without cancelling the
  ability** and the tree re-scores (`DuelBotAI.md`, *One strike at a time*): a new press of the same input in the
  window is the player's path (`OnAbilityActivatedAgain` → the next strike plays at the window end); otherwise the
  current strike ends the ability at the window end. A weapon swap, or an attack the priority rules refuse, waits
  for its end before the press (at most `StartTimeout`). The Light combo montages have two windows (none on the third
  strike). `LogMKHAI` + Visual Logger: `Combo window open (<input>): back to Combat`. `MaxDuration` defaults to 4 s.
- **Capsule**: `BP_DuelBot`'s capsule must match `BP_MKHPlayerCharacter`'s; a smaller one lets the attack animations
  push the meshes into each other.
- **Hysteresis**: the bot approaches to 150 cm but attacks up to 250 cm, so it does not flicker between Approach and
  Attack at the boundary.
- **Attack decisions**: every *Commit Attack* writes `Commit attack: <tag> (score, p)` and the whole candidate table
  (considerations, score, p, time to hit) to `LogMKHAI` and the Visual Logger; `MKH.Bot.DumpAttackOptions` prints the
  table and the state scores on screen without the bot acting; `MKH.Bot.Pause`, `MKH.Bot.State.Only <Name>`,
  `MKH.Debug.ApplyStatus <PerkTag> [player|bot]`, `MKH.Debug.SetStamina <v> [player|bot]` and
  `MKH.Debug.SetHealth <v> [player|bot]` put the duel in a state to test it (`Documentation/DuelBotAI.md`, Tests).
- **Debugging**: `ST_DuelBot` → Window → Debugger (records on PIE; pick the controller's instance and scrub the
  timeline); Rewind Debugger; Gameplay Debugger (`'`) for the path.
  Visual Logger (Tools → Debug → Visual Logger, record, then PIE): on the bot's row, a frame with `Utility` lines and a
  `LogStateTree` state change shows why that state won.
  On-screen state changes: tick `bPrintStateTreeChanges` on `BP_DuelBotController` (Duel|AI|Debug, also settable at
  runtime from Blueprint): every new active path ("Root > Combat > React > Dodge") is printed for
  `StateChangePrintDuration` seconds and logged in `LogMKHAI` (polled in `Tick`, no state change delegate exists;
  compiled out of Shipping).
- **Test level**: Makhion (NavMesh): in PIE run `open Makhion?Mode=Bot` (optionally `?BotProfile=Easy|Medium|Hard`).
  `Lvl_AISandbox` (`Content/Makhia/Levels/Maps/Dev`) is no longer used for the bot.
- **Animation**: `BP_DuelBot` uses `ABP_MKHPlayerCharacter`; since the movement framework moved to `ACharacterBase`
  (M3), `UMKHPlayerAnimInstance` reads any `ACharacterBase` and the bot walks animated.
- **Abilities**: Dodge, `GA_QuickSlot` and the reactions (hit, stagger, stun, die) are granted to the bot by
  `DA_CharacterClassInfo` (`Character.Enemy.DuelBot`); Block and the attacks come with the weapon. `GA_SheatheWeapon`
  is not granted (the bot never sheathes).
- **Roadmap** (polish, invisible DDA): `Documentation/Handoff/DuelBotAIHandoff.md`.

### What changes offline

| Concern | Online | Against the bot |
|---|---|---|
| Preparation-skip votes | 2 | 1 (humans only) |
| Rematch votes | `PlayerArray.Num()` = 2 | 1 (the AI controller has no player state) |
| Item spawn points reserved to a side | both sides spawn | the bot's side never spawns (`AMKHItemSpawnPoint::IsReservedToBot` → `AMKHGameMode::IsStartSpotHeldByBot`) |
| Scoreboard name | username | `GetParticipantDisplayName` for both slots (`W_MatchInfo::HandleParticipantRegistered`): "You" for a player who is not signed in, "The Abyss" for the bot |
| Return to lobby | host lobby | host lobby if logged in, offline lobby otherwise |
| Restart from the game menu | — | *Restart Game* reopens the level with `?Mode=Bot` (`UMKHGameInstance::RestartOfflineMatch`), a fresh match |

While a match against the bot is opening (`StartOfflineMatch`, until `LoadComplete`) `UMKHGameInstance::BackToLobby`
is ignored: tearing down a level run by `AMKHGameMode` logs the local controller out, and its `Logout`
(`ReturnToLobbyAfterLogout`) calls `BackToLobby(true)`, which would otherwise turn the travel into a return to the lobby.

---

## Full Match Walkthrough

```
SERVER (AMKHGameMode)                          EVERY PEER (AMKHGameState)         OWNING CLIENT (UI)
---------------------                          --------------------------         ------------------
RestartPlayer x2
  RegisterParticipant                    ──>   RegisterParticipant                 OnParticipantRegistered
  BindDeathEvent                                                                   (W_MatchInfo)
TryStartMatch
StartInitialPreparationPhase             ──>   SetMatchPhase(InitialPreparation)   OnInitialPreparationStarted
                                               PhaseEndServerTime = T+60           local countdown ticks
  ResetSkipPreparationBallot(open)       ──>   SetPreparationSkipVotes(0/2)        W_PreparationSkip shown
Server_ToggleSkipPreparationVote (x2)
ToggleSkipPreparationVote                ──>   SetPreparationSkipVotes(n/2)        OnPreparationSkipVotesChanged
StartFinalCountdown (T+57, or early at 2/2)
  ResetSkipPreparationBallot(closed)     ──>   SetPreparationSkipVotes(0/0)        W_PreparationSkip hidden
                                         ──>   StartFinalCountdown(3)              OnFinalCountdownStarted
                                                                                     W_MatchInfo "stay focused"
StartDuelPhase                           ──>   SetMatchPhase(Duel)                 OnDuelStarted
  [timer expires]
HandleDuelTimeOver                       ──>   SetMatchPhase(SuddenDeath)          OnDuelTimeOver
  ApplySuddenDeathDrain x2
  [ASC broadcasts Event.Death]
HandlePlayerDeath
  bDiedThisRound = true
  ScoringData.Score++                    ──>   SetScore(slot, n)                   OnScoreChanged
  next tick: RemoveSuddenDeathDrain      ──>   Multicast_PlayerDied                OnPlayerDied
  +3s: ResolveRound
ResolveRound (score reached)             ──>   SetMatchPhase(MatchOver)
  item spawn points stop
  DestroyAllPickups (world pickups)
                                               Multicast_MatchOver                 AMainHUD::OnMatchOver
                                                                                     CreateMatchOverWidget
                                                                                     ShowMatchResult
Server_RequestRematch (both players)
RequestRematch                           ──>   SetRematchVotes(n, m)               OnRematchVotesChanged
                                                                                     caption REMATCH/WAITING
RestartMatchInPlace
  scores zeroed                          ──>   SetScore(x, 0)                      OnScoreChanged
  participants revived
StartInitialPreparationPhase             ──>   SetMatchPhase(InitialPreparation)   OnMatchRestarted
                                                                                     DestroyMatchOverWidget
                                                                                     focus back to gameplay
```

---

## Extending the System

### Adding a new match phase

1. Add the value to `EMKHMatchPhase` in `MKHGameState.h`.
2. Add a `StartXPhase()` on the game mode following the `SetPhase` → `ClearPhaseTimer` → `SetTimer` shape,
   plus its `EditDefaultsOnly` duration property under `Duel|Rules`. A phase that leads into a duel and should
   end with the final countdown schedules itself with `SchedulePreparationEnd(Duration)` instead.
3. Add the granular `FMKHDuelPhaseSignature` event and its `case` in `AMKHGameState::HandlePhaseChanged`.
4. Re-check the phase guard in `HandlePlayerDeath` — a death during the new phase is either meaningful
   (add it to the guard) or must be handled explicitly.

### Adding replicated match state

Add it to an existing snapshot struct rather than as a loose `UPROPERTY` whenever it is read together with
fields already in one — that is the whole reason the structs exist. Then: `DOREPLIFETIME` it, guard the
setter with `HasAuthority()`, call `ForceNetUpdate()`, and **broadcast the event manually in the setter**
because `OnRep` will not run on the host.

---

## Known Limits & Future Work

- **Deaths outside `Duel`/`SuddenDeath` are swallowed.** `HandlePlayerDeath` returns early in any other
  phase. If a player can die during `InitialPreparation` or `IntermissionPreparation` (fall damage, kill-Z,
  residual DoT), the character enters `Dead` — collision off, input blocked — but no round resolves and no
  reset revives them, so the next duel starts with an unplayable corpse. Either make death impossible
  during prep phases or revive immediately when one occurs there.
- **`UDeadState::ExitState` hardcodes the `Pawn` collision profile.** `EnterState` only changes channel
  *responses*, so the pair is asymmetric: a character Blueprint using a custom capsule profile loses it
  after the first revive. Saving and restoring the responses (or reading the profile from the CDO) would
  make the round trip lossless.
- **`bLocalRematchVoteCast` is optimistic.** It is set before the RPC leaves the client, so a vote the
  server rejects (wrong phase) leaves the local button permanently captioned `WAITING`. Deriving it from
  replicated state instead would be self-correcting.
- **`Logout` ballot cleanup is currently unreachable.** A disconnect in a 1v1 sends the survivor back to the
  lobby, so the vote-removal path in `AMKHGameMode::Logout` can never affect a live match. Harmless, but
  the comment justifying it describes a scenario that no longer exists.
- **Slots are never freed.** `Players` entries are only cleared by GC when the pawn dies. Fine while a
  disconnect always ends the match; it would need explicit handling if reconnection is ever supported.
- **`AMKHGameMode::Logout` returns to the lobby on any logout** (`ReturnToLobbyAfterLogout`: frees the lobby seat, then
  `BackToLobby(true)`; skipped while the world is tearing down — PIE stop, quitting — because the game instance
  subsystems are already gone; it used to be the Blueprint `OnLogout`, which then hit `Accessed None`), including the host's own controller destroyed by a
  hard travel that did not start from `BackToLobby` (whose re-entrancy guard absorbs it). Travels that open a match
  against the bot are protected (`bOpeningOfflineMatch`, see above); any other hard travel out of a level run by this
  game mode still bounces to the lobby. The lobby flow is not affected. A remote player leaving while the host's
  loading screen is still up is reported as `MATCH-OPPONENT-LEFT` (`AMKHGameMode::Logout`, see `LoadingScreen.md`).
- **Skipping the preparation does not cancel pending spawn attempts.** Entering `Duel` does not touch spawn
  points, so a point triggered at `InitialPreparation` with a delay longer than the time actually spent in the
  preparation spawns its pickup during the duel. Accepted as is (2026-09-28).
