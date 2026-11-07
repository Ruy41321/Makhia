# Loading Screen & Travel Errors

How every travel is covered, so the player never sees a level "in passing" (the opponent's lobby, the lobby
before a match starts) nor the freeze of a blocking map load, and how an error that sends the player back to the
lobby is shown to them with a code for the developer. Lobby flow and match start: `OfflineModeArchitecture.md`
and `MatchFlowArchitecture.md`; the lobby lineup that the screen waits for: `LobbyPresentation.md`.

---

## Pieces

| Piece | Path | Role |
|---|---|---|
| `UMKHLoadingScreenSubsystem` | `Source/Makhia/Public/UI/Loading/MKHLoadingScreenSubsystem.h` (GameInstance subsystem, log `LogMKHLoading`) | Shows/hides the screen, follows the travel to its destination, hands the screen to the MoviePlayer during blocking loads, collects travel errors. |
| `SMKHLoadingScreen` | `Source/Makhia/Public/UI/Loading/SMKHLoadingScreen.h` | The screen itself, **pure Slate**: same look as `W_BootOverlay` (black base, `MI_UI_Background`, game name in `TS_Display`, divider, throbber, status line in `TS_Status`). |
| `UMKHLoadingScreenSettings` | Project Settings → Game → **Makhia Loading Screen** (`[/Script/Makhia.MKHLoadingScreenSettings]`, `DefaultGame.ini`) | Look (background, styles, colours), fades, minimum display time, timeouts. |
| `EMKHTravelError`, `UMKHTravelErrorLibrary` | `Source/Makhia/Public/UI/Loading/MKHTravelError.h` | Error list, developer codes, player titles and sentences (`MKHTravelError` namespace). |
| `UMKHTravelErrorWidget` / `W_TravelError` | `Source/Makhia/Public/UI/Lobby/MKHTravelErrorWidget.h`, `Content/Makhia/UI/LobbyUI/W_TravelError` | Modal shown in the lobby: title, sentence, "Error code: …", OK (Esc / Enter close it). Layout copied from `W_LoginPanel`. |
| `AMKHLobbyHUD` | `TravelErrorWidgetClass`, `LobbyNoticeWidgetClass` (set on `WBP_LobbyHUD`) | Opens the modal and the lobby toasts once no loading screen covers the lobby. |
| `UMKHLobbyNoticeWidget` / `W_LobbyNotice` | `Source/Makhia/Public/UI/Lobby/MKHLobbyNoticeWidget.h`, `Content/Makhia/UI/LobbyUI/W_LobbyNotice` | Non-blocking toast at the top of the lobby ("Luigi left the lobby"): fades in (0.25 s), stays 4 s, fades out (0.6 s), never takes focus or clicks. Frame = nameplate brush `MI_UI_Nameplate`, text `TS_Body`. |
| Lobby notices | `UMKHLobbySubsystem::PostLobbyNotice` / `PostPlayerLeftNotice`, `OnLobbyNoticePosted` | One pending notice that survives a return to the lobby (`bShowAfterTravel` keeps it for the next lobby). Texts in `MKHLobbyNotice`. |

Why Slate and not UMG: during a blocking `LoadMap` the game thread is frozen and only the MoviePlayer's loading
thread draws; a UMG widget there is not thread safe. One Slate class is used twice: one instance in the game
viewport (`AddViewportWidgetContent`, Z order 1000, survives map changes because it belongs to the viewport client,
not to a world) and a fresh one for each blocking load (`IGameMoviePlayer::OnPrepareLoadingScreen`). Both look the
same, so the hand-over is invisible. The style assets stay loaded in the subsystem while the loading thread draws.

## Lifecycle

```
ShowLoadingScreen(Reason, Destination) ─ fade in (0.2 s) ─> covered ─> pending travels run (RunWhenCovered)
        │                                                       │
        │                       blocking LoadMap: MoviePlayer plays SMKHLoadingScreen (throbber keeps turning)
        │                       seamless travel: the viewport screen stays up, the game keeps ticking
        v                                                       v
  destination reached (map loaded since shown + world playing + ready) for HoldAfterReady (0.3 s),
  and up for at least MinimumDisplayTime (1 s) ─> fade out (0.35 s) ─> OnLoadingScreenHidden
```

| Destination | Reached when |
|---|---|
| `Lobby` | lobby level, local player controller, lineup built (`UMKHLobbyLineupSubsystem::IsLineupReady`) |
| `Game` | game level (a lobby on the way does not count), local pawn possessed, and in an online duel both players arrived |
| `Any` | the next level with the same rules (lobby → lineup, other level → pawn) |

A map must have finished loading since the destination was set (`PostLoadMapWithWorld`, also broadcast at the end
of a seamless travel): a screen shown in the lobby for a return to the lobby does not hide before the reload.
During the start-up boot nothing is shown (the boot overlay covers it), and a dedicated server never shows it.
Play In Editor has no MoviePlayer: there the screen only freezes during a blocking load. Test in Standalone.

## Who shows it

| Moment | Where | Reason (status line) | Destination |
|---|---|---|---|
| Matchmaking: the joiner found a lobby | `BP_MatchmakingComponent.SearchSession`, after `JoiningStartDelegate` | `MatchFound` "Opponent found! Entering the arena..." | `Game` |
| Host: match starts (matchmaking player arrived, or friends countdown over) | `BP_MatchmakingComponent.StartMatch` → `ShowMatchLoadingScreen` | `MatchFound` while matchmaking, else `MatchStarting` | `Game` |
| Clients of a friends lobby: countdown over | `UMKHLoadingScreenSubsystem::UpdateLobbyCountdown` (reads `AMKHLobbyGameState`) | `MatchStarting` | `Game` |
| Match against the bot / Restart | `UMKHGameInstance::StartOfflineMatch` | `OfflineMatch` "Preparing the arena..." | `Game` |
| Back to the lobby (end of match, leave, disconnect, error) | `UMKHGameInstance::BackToLobby` | `ReturningToLobby` | `Lobby` |
| Any other `OpenLevel` / `ServerTravel` of the game instance | `UMKHGameInstance::ShowLoadingScreenForTravel` | `OpeningLobby` (lobby) or `Loading` | `Lobby` / `Game` |
| Any other map load or seamless travel (e.g. joining a friend's lobby) | `PreLoadMapWithContext`, `OnSeamlessTravelStart`, MoviePlayer prepare | `Loading` | `Any` |

`UMKHGameInstance::OpenLevelWithOptions` and `ServerTravel` run the travel through `RunWhenCovered`, so the screen
is fully opaque before the level changes (a freeze never shows a half-faded lobby). The host keeps the lobby
countdown at zero when the match starts (`UMKHLobbySubsystem::HandleMatchCountdownFinished`) and clears it when the
start is refused; a client whose screen was shown by the countdown hides it again when the countdown is cleared
before any travel.

## Travel errors

An error is recorded once (the first one is the cause), logged with its engine detail and, while the screen is
up, turns the screen into "Returning to the lobby..." (destination `Lobby`). The return itself is done by the
existing flows (`UMKHOnlineSession::HandleDisconnect` → `BackToLobby`, `BP_MKHGameInstance` network/travel error
events, `BP_MKHGameMode.OnLogout`) or, for the timeouts, by `LeaveToNewLobby`. In the lobby, `AMKHLobbyHUD` opens
`W_TravelError` as soon as no loading screen is up (`OnLoadingScreenHidden`, `OnTravelErrorReported`, HUD begin
play). The modal consumes the error.

| Code | Source | Player title / sentence (en) |
|---|---|---|
| `NET-CONNECTION-LOST` | `ENetworkFailure::ConnectionLost`, `FailureReceived` (not in a lobby); host side, the client leaving during the match | Connection lost / The connection with your opponent was lost. You are back in your lobby. |
| `NET-CONNECTION-TIMEOUT` | `ConnectionTimeout` | Connection lost / Your opponent stopped responding. Check your connection and try again. |
| `NET-CONNECT-FAILED` | `PendingConnectionFailure` | Couldn't join the match / Couldn't connect to your opponent. Try searching again. |
| `NET-VERSION-MISMATCH` | `OutdatedClient`, `OutdatedServer` | Couldn't join the match / Your opponent has a different version of the game… |
| `NET-LISTEN-FAILED` | `NetDriverListenFailure` (the lobby could not be hosted) | You are playing offline / Your lobby couldn't be opened online… |
| `NET-FAILURE` | any other network failure | Connection lost / generic sentence |
| `TRAVEL-LOAD-FAILED` | `ETravelFailure::NoLevel`, `LoadMapFailure`, `PackageMissing` | The match couldn't start / The arena couldn't be loaded… |
| `TRAVEL-FAILED` | any other travel failure | The match couldn't start / generic sentence |
| `MM-JOIN-FAILED` | `BP_MatchmakingComponent.OnJoinFailure`, outside matchmaking | Couldn't join the match / Couldn't join your opponent's lobby… |
| `LOBBY-START-FAILED` | `BP_MatchmakingComponent.StartMatch`, `SetPassiveLobby` failed (host); also clears the lobby countdown | The match couldn't start / generic sentence |
| `MATCH-OPPONENT-LEFT` | `AMKHGameMode::Logout` of a remote player while the host's screen is up | Your opponent left / Your opponent left before the match started. |
| `MATCH-START-TIMEOUT` | a level loaded on the way to a match waited `MatchStartTimeout` (15 s): the joiner in the host's lobby, the host in the arena without the opponent | The match couldn't start / The match took too long to start… |
| `TRAVEL-TIMEOUT` | the screen stayed up `TravelTimeout` (60 s) | The match couldn't start / generic sentence |

- **Not an error:**
  - a network failure once the match is over (`EMKHMatchPhase::MatchOver`): the opponent leaving after the end is
    the normal end of a match;
  - **the host leaving a lobby** (`FailureReceived` / `ConnectionLost` while in a lobby and not on the way to a
    match): it becomes the toast "<host> left the lobby" in the player's own lobby (`IsLobbyHostLeaving`,
    `PostHostLeftNotice`; the host is the other player with the lowest `PlayerId`). The host sees "<name> left the
    lobby" when a player leaves its room (`AMKHLobbyGameState::RemovePlayerState`, outside travels and loading
    screens; the server's ownerless inactive copy of the leaving player is skipped);
  - **a join failure while matchmaking** (`IsMatchmakingRetry`): the search returned a lobby that is already gone
    (EOS `404 session_not_found`; Steam: the lobby closed between search and join); the matchmaking flow hosts again and searches again by itself, so the screen just
    fades out, the lobby is remembered as unreachable and the search goes on. Most of these failures are now
    avoided before the join, see [Matchmaking search results](#matchmaking-search-results).
- **The remote player leaving a match** (`AMKHGameMode::ReportRemotePlayerLeft`, before the Blueprint `OnLogout`):
  while the host's screen still goes to the arena → `MATCH-OPPONENT-LEFT`; during the match → `NET-CONNECTION-LOST`
  on the host (a client leaving a listen server raises no network failure there); nothing once the match is over
  or when this machine is the one leaving (`UMKHGameInstance::IsReturningToLobby`: its own teardown logs the
  opponent out).
- `MM-JOIN-FAILED` outside matchmaking and `LOBBY-START-FAILED` happen in the player's own lobby before any travel:
  the screen just fades out and the modal opens there.
- A closing connection repeats its failure several times: only the first error is kept and logged as a warning,
  the others are `Verbose`.
- Network failures are reported for any level (the user wanted the modal also for a connection lost during a
  match), not only while loading.
- The log line (`LogMKHLoading: Warning: Travel error <CODE> (loading screen up/down, <reason> -> <destination>):
  <engine detail>`) is what to ask a player for together with the code.

Texts follow `Localization.md`: titles and sentences in `MKHTravelError`, status lines in `MKHLoadingScreen`;
the code itself is culture invariant; the designer placeholders of `W_TravelError` are `INVTEXT`.

## Matchmaking search results

Some backends keep a destroyed lobby in their search index for a while (EOS: the logs show 10-20 s; Steam removes
it almost at once, but the filters below apply to every backend). A player who just came back
from a match hosts a new lobby with the same session name (`<UserId>_LOBBY`), so a search can return both the old
lobby and the new one, and the old one, with an older `MatchmakingTime`, also wins the join priority (`HasJoinPriority`). Joining
it fails with `404 session_not_found` after the player already left their own lobby.

`UMKHSessionSubsystem` (`Source/Makhia/Public/Online/MKHSessionSubsystem.h`, see `OnlineBackends.md`) cleans the
results in `HandleSearchComplete` before trying them:

| Step | Function | Rule |
|---|---|---|
| Filter | `FilterSearchResults` | Keeps only the most recent lobby of each owner (same `SessionName`, highest `MatchmakingTime`) and drops the lobbies that failed to be joined in the last `UnreachableLobbyMemory` seconds (60 s, Project Settings → Makhia Lobby). Every result is logged (`LogMKHSession`: lobby id, owner, `MatchmakingTime`, players). |
| One join per search | `UMKHLobbySubsystem::TryChooseJoinTarget` (after `CanJoinLobby` → Joinable) | Only the first joinable lobby of a search is joined. The destroy of the own lobby is asynchronous while the result loop is not: without this a search with two joinable lobbies started two joins. |
| Join target | `JoinTarget` member | The join reads the chosen lobby from it once the asynchronous destroy of the own lobby completes. |
| Remember a failure | `UMKHLobbySubsystem::MarkJoinTargetUnreachable` (called by `ReportTravelError(MM-JOIN-FAILED)` while matchmaking) | The failed lobby is skipped by the next searches. |

A stale lobby can still be tried when it is the only result for its owner (the owner's new lobby not indexed yet);
then the join fails once, silently, and is not retried.

## Settings

| Setting | Default | Notes |
|---|---|---|
| `BackgroundMaterial`, `TitleStyle`, `StatusStyle` | `MI_UI_Background`, `TS_Display`, `TS_Status` | Same as `W_BootOverlay`. They are referenced only by config: they are cooked because `W_BootOverlay` uses them too; a new asset here must be referenced elsewhere or added to `DirectoriesToAlwaysCook`. |
| `TitleColor`, `DividerColor`, `StatusColor` | boot overlay colours | Linear colours. |
| `FadeInDuration` / `FadeOutDuration` | 0.2 s / 0.35 s | Fade-in delays the travels started through `RunWhenCovered`. |
| `MinimumDisplayTime`, `HoldAfterReady` | 1 s, 0.3 s | Wall clock (`FPlatformTime`): the first frame after a load is very long. |
| `MatchStartTimeout`, `TravelTimeout` | 15 s, 60 s | See the error table. |

## How-To

- **New way to start a match or a travel:** call `UMKHLoadingScreenSubsystem::ShowLoadingScreen(Reason,
  Destination)` before it starts (Blueprint: *Show Loading Screen*), and start the travel through
  `UMKHGameInstance::OpenLevel` / `ServerTravel` (they wait for the cover). A new status line is an
  `EMKHLoadingReason` value plus its text in `GetReasonText`.
- **New error:** add an `EMKHTravelError` value, its code/title/sentence in `UMKHTravelErrorLibrary`, call
  `ReportTravelError` (Blueprint: *Report Travel Error*) where it happens, translate (it, fr) and add it to the table above.
- **Test:** two Standalone processes with the LAN backend (`-MKHOnline=Null -nosteam`, see `OnlineBackends.md` →
  Testing), or a Steam PC + VM pair; `LogMKHLoading` logs every show, MoviePlayer hand-over, fade-out and error.

## Tests (2026-10-08, Standalone, EIK disabled; the LAN Null backend replaced that setup)

- Match against the bot from the lobby: screen up from the click (MoviePlayer during the blocking load) until the
  bot match is ready, ~2.2-2.7 s. Return with a simulated `NET-CONNECTION-LOST`: screen during the reload, modal in
  the lobby (Italian texts checked).
- Host + client, `ServerTravel` (seamless) from the lobby: host covered from the travel to the arena with both
  players, client covered from the seamless travel start; the client's join (blocking) also goes through the
  MoviePlayer.
- Fixes from the user tests (2026-10-08), Standalone host + client: host leaves the friends lobby → client
  toast "Host ha abbandonato la lobby" once, in its own lobby, no modal; client leaves → host toast "Joiner ha
  abbandonato la lobby" once; client leaves the match → host modal `NET-CONNECTION-LOST`, client nothing; host
  leaves the match → host nothing, client `NET-CONNECTION-LOST`; join failure while matchmaking → no modal.
- Matchmaking join, host start and return to the lobby were tested afterwards on the LAN backend (2026-10-09); lobbies and
  invites with two Steam accounts (2026-10-10, user test). The Steam matchmaking join and the real network failures and timeouts of Steam are still to test by
  hand (matrix in `Handoff/SteamMigrationHandoff.md`).
