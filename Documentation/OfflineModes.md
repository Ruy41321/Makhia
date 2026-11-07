# Offline Modes

Three ways to play alone, all on the arena level with `?Mode=Bot` (`EMKHMatchMode::OfflineBot`). The mode travels in the
URL, so the game mode, the game menu's *Restart Game* and the end-of-match panel all read the same request.

| Mode (`EMKHOfflineMode`) | Name (it) | URL | Opponent | Difficulty |
|---|---|---|---|---|
| `RandomDuel` | Schermaglia (Skirmish) | `?Mode=Bot` | Random skin, never the player's | Starts Medium, follows the player each round |
| `Training` | Allenamento | `?Mode=Bot?OfflineMode=Training` | Default skin (Arcanist), stands still | None |
| `Career` | Via del Makhai (Way of the Makhai) | `?Mode=Bot?OfflineMode=Career?CareerLevel=<i>` | Skin of the level | Profile of the level |

`?BotProfile=<Id>` still forces a profile for tests and turns the difficulty adjustment off.

## Starting a mode

```
UI -> UMKHLobbySubsystem::RequestOfflineMatch(Mode, CareerLevelIndex)   cancellable wait; a locked level is refused
   -> UMKHGameInstance::StartOfflineMatch(FMKHOfflineMatchRequest)      loading screen, URL options
   -> AMKHGameMode::InitGame -> ReadOfflineRequest                      a locked level becomes the next one to play
```

`UMKHGameInstance::RestartOfflineMatch` reopens the level with `AMKHGameMode::GetOfflineRequest`, so it keeps mode and
level. A test override (`?BotProfile=`) is not carried over.

**PIE default**: the arena opened with no `?Mode=` in a standalone PIE world (Net Mode *Play Standalone*) starts in
Training (`AMKHGameMode::InitGame`). With Net Mode *Play As Listen Server* it stays an online duel, also with one client.
An explicit `?Mode=` always wins, and packaged builds are unaffected.

The bot's name is always the name of its skin (`AMKHDuelBot::SetOpponentSkin`, before it registers as a participant).

## Training

There is no Blueprint of its own: `AMKHGameMode::SpawnBotActor` spawns `BP_DuelBot` deferred, replaces its controller
with a plain `AAIController` (so no StateTree runs) and adds `UMKHTrainingDummyComponent`. This keeps the dummy's
movement, animation and attributes aligned with the bot with nothing to maintain.

- **Spot**: the actor tagged `TrainingDummySpotTag` (`TrainingDummySpot`) in the level; without one, a free start (logged).
- **Match flow**: `StartTrainingPhase` enters `Duel` with no timer. No preparation, intro, rounds, sudden death or end;
  the item spawn points stay idle (`SetPhase` does not forward the phase to `UMKHItemSpawnSubsystem`, which keeps
  seeing no running match; otherwise points triggered by `Duel`, like the rare centre one, would spawn on the dummy).
  The player leaves from the game menu.
- **Items of the player**: `TrainingLoadoutTable` on the game mode (`DT_MKHStartingLoadout_Training`: the six bronze pieces
  from common to epic, the three exclusive legendary weapons (Champion, AssKicker, Mjolnir) and the potions; no legendary armor), or `StartingLoadoutTable` when it is not set.
- **Items of the dummy**: `TrainingDummyLoadoutTable` (`DT_MKHStartingLoadout_AllCommon`), holding
  `TrainingDummyWeapon` (the sword), so the damage and the guard are the ones of a real opponent.

Phases of the dummy:

```
Guarding --Event.Combat.GuardBreak--> Open --Event.Combat.LethalHitPrevented--> (next tick) full health and stamina --> Guarding
```

The stamina regenerates by itself in `Open`, since the regeneration only stops while blocking. In `Guarding` the
component retries `StartBlocking` every tick until the block is up. The controller focuses the player, and blocking
uses the controller rotation, so the guard always faces the player.

`State.General.Unkillable` (loose tag, only the dummy has it) makes `UMKHAttributeSet::TrySendDeathEvent` send
`Event.Combat.LethalHitPrevented` instead of the death.

## Random Duel: difficulty adjustment

`UMKHDifficultyDirector` (owned by the game mode, created only in this mode) adds up the damage each side takes in the
round, from `Event.Combat.HitReceived` (`EventMagnitude`: the damage after armor). The rules are pure, in
`FMKHDifficultyAdjustment` (spec `Makhia.AI.DifficultyAdjustment`):

```
score = (dealt - taken) / (dealt + taken)        -1..+1, 0 when nobody was hit
score > RaiseThreshold (0.2)    next round one level harder
score < -LowerThreshold (0.2)   next round one level easier
otherwise                       same level
```

The hardest level is final for the match: once reached it never lowers, so a player cannot throw a round to face an
easier opponent in the next one. Lower levels still move both ways.

- Applied in `ResolveRound`, before the reset, with `AMKHDuelBotController::SetProfile`: the bot plays the new profile
  from the next round. Nothing is shown to the player.
- **The loadout stays the one of the starting difficulty for the whole match** (decision): only the behaviour changes.
- A rematch goes back to the starting difficulty, then re-grants the loadout. Every match starts from Medium; nothing
  is saved.
- Thresholds and profile order: `DifficultySettings` on the game mode. `BP_MKHGameMode` uses 0.2; the C++ default stays 0.35.

Absolute damage rather than health left, by the user's choice: the health depends on the potions drunk, the damage does
not. A share rather than a difference, so the same thresholds hold for short and long rounds and for any loadout.
Blocked hits and the sudden-death drain deal no hit, so they do not count. Examples: dealt 100 and taken 65 scores 0.21,
just over the threshold; dealt 100 and taken 48 scores 0.35 (about twice the damage).

To tune it: `DifficultySettings` on `BP_MKHGameMode` (`Duel|Bot`), then read the `MKHDifficultyDirector` lines of
`LogMKHAI` after each round (damage dealt and taken, score, level change).

## Career

`UMKHCareerSubsystem` reads the levels from `DA_CareerLadder` (`UMKHDataSettings::CareerLadder`) and keeps how many were
beaten in the local save slot `Career`. A level is playable once the previous one is beaten; beaten levels stay playable.

| Level | Profile | Bot skin | Weapons | Title won |
|---|---|---|---|---|
| 0 | Easy | Abyss | Sword, hatchet | Veterano |
| 1 | Medium | Ember | Sword, hammer | Gladiatore |
| 2 | Hard | Oracle | Hatchet, hammer | Makhai |

Starting title: Tirone. Makhai stays the same in every language.

The title is on the lobby nameplate of every player: `AMKHLobbyPlayerState` replicates the levels beaten
(`LobbyCareerLevels`, sent by the owning machine, clamped by the server) and each machine turns them into text with
`UMKHCareerSubsystem::GetTitleForCompletedLevels`, in its own language. For now it is the player's word.

The same number reaches the other places the title appears:

- **Duel scoreboard**: `AMKHPlayerState::CareerLevels` (reported by `AMKHPlayerController::ReportCareerLevels`, clamped by
  the server, kept over a seamless travel), shown under each name by `W_DuelTitle` (`UMKHDuelTitleWidget`) in `W_MatchInfo`.
  A bot shows the title of its difficulty (`IMKHDuelParticipant::GetParticipantCareerLevels`): in the career the one its
  level awards (Easy Veterano, Medium Gladiatore, Hard Makhai), in the Random Duel always Gladiatore
  (`RandomDuelTitleCareerLevels`), whatever the difficulty adjustment does. The training dummy has none.
- **Friend list**: `FMKHPlayerActivity::CareerLevels`, in the rich presence key `mkh_career`; `UMKHFriendEntryWidget` puts
  the title in front of the status of a friend in the game ("Veterano - Makhia: In lobby 1/2").

- `AMKHGameMode::RecordCareerResult` records the win before `MatchOver`, so the end panel can read it.
- `UMatchOverWidget`: after a won level with another one to play, the rematch button reads **NEXT CHALLENGE** and calls
  `UMKHGameInstance::StartNextCareerLevel`. After a defeat, a draw or the last level it stays a normal rematch.

## Weapons of the bot

Every loadout table holds all three weapons; `MKHBotWeapons::Apply` (game mode) leaves quick slotted only the chosen ones,
first in the loadout and in order. The bot holds the first; the others stay in the bag, and it never uses them.

| Mode | Weapons |
|---|---|
| Random Duel | Two different ones at random, each round: sword-hatchet, sword-hammer or hatchet-hammer (`ApplyWeaponsForNextRound`, among the items it owns) |
| Career | `BotWeapons` of the level: Easy sword-hatchet, Medium sword-hammer, Hard hatchet-hammer |
| Training | `TrainingDummyWeapon`: the sword |

`?BotProfile=` keeps the weapons flagged in the table. The rarity stays the one of the profile's table.

## Waiting for the AppID

Steam stats and achievements need the game's own AppID (see `SteamIntegration.md`). Then:

- The title follows the local progress; `OnLevelCompleted` is the hook for Steam. The host will read it from Steam instead of trusting the client.
- The progress moves from the local save to Steam by changing `UMKHCareerSubsystem` alone, importing the local value.
- Skin unlocks are not implemented: every skin is still selectable. Planned: `Bloodcrown` after 5 PvP wins.
