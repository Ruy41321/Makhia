# Offline Mode & Authentication

How the player signs in (and stays signed in across launches), how the game decides between online and offline
play, and where the backend (Steam today) plugs in. The offline match against the bot itself is described in
`MatchFlowArchitecture.md` → "Match Modes & the Offline Bot".

> Status (2026-10-10): complete. Authentication, boot overlay, lobby hub and the offline match are in place; the
> old Blueprint login (`BP_LoginComponent`, `BML_AccountManager`, the development buttons of `W_LobbyOverlay`) has
> been removed. The online backend is Steam (`UMKHAuthProvider_Steam`); the EOS provider lives in the dormant
> `MakhiaEOS` module. History and design decisions: `Handoff/OfflineModeHandoff.md`,
> `Handoff/SteamMigrationHandoff.md`.

## Table of Contents

1. [Overview](#overview)
2. [UMKHAuthSubsystem](#umkhauthsubsystem)
3. [Providers](#providers)
4. [Start-up login and saved preferences](#start-up-login-and-saved-preferences)
5. [Game instance integration](#game-instance-integration)
   - [Lobby subsystem](#lobby-subsystem)
6. [Boot overlay](#boot-overlay)
7. [Lobby hub](#lobby-hub)
8. [Match against the bot](#match-against-the-bot)
9. [Settings and development tools](#settings-and-development-tools)
10. [How-To](#how-to)
11. [Gotchas](#gotchas)
12. [Tests](#tests)

---

## Overview

```
UI (boot overlay, mode cards, account chip, login panel, dev login panel)
        │  Login / Logout / GetAuthState / OnAuthStateChanged / OnLoginFailed
        v
UMKHAuthSubsystem (UGameInstanceSubsystem, log LogMKHAuth)
  state EMKHAuthState { Unknown, LoggingIn, LoggedIn, Offline }      user FMKHAuthUser
  preferences UMKHAuthSaveGame (slot "Auth")                          OnAuthStateChangedNative ──> UMKHGameInstance
        │  owns one instance of the auth provider class of the online backend (OnlineBackends.md)
        v
UMKHAuthProvider (Abstract)
  └── UMKHAuthProvider_Steam  account of the running Steam client (MakhiaSteam, active)
  └── UMKHAuthProvider_Null   development LAN logins (-MKHOnline=Null)
  └── UMKHAuthProvider_EOS    IOnlineIdentity of the EIK online subsystem (MakhiaEOS, dormant)
```

- **One entry point.** Widgets, controllers and the game instance only talk to `UMKHAuthSubsystem`; nothing else
  calls a platform SDK or online subsystem to sign in. The social layer (`UMKHFriendsSubsystem`) follows the
  sign-in through its own provider.
- **Backend-independent methods** (`EMKHAuthMethod`): `PlatformAccount` (the backend's own account, friends and
  presence), `Guest` (device login with a chosen name, online without friends), `Developer` (development
  credentials, never in Shipping). A provider declares what it supports (`SupportsMethod`): with Steam only
  `PlatformAccount`, so the guest and developer widgets stay in the project but are never shown.
- **Online vs offline.** `LoggedIn` = online play available (the lobby is hosted, `?listen`). Anything else =
  offline only. The match itself never depends on the auth state: a map opened without `?Mode=` is always an
  online duel (the multiplayer PIE test path keeps working without a platform sign-in).

## UMKHAuthSubsystem

| API | Notes |
|---|---|
| `TryAutoLogin()` | Called once by `UMKHGameInstance::OnStart`; see the next section. |
| `Login(Method, Params)` + `LoginWithPlatformAccount` / `LoginAsGuest(Name)` / `LoginWithDevProfile(Index)` | Interactive. Returns false when refused; refusals and failures are broadcast by `OnLoginFailed(Method, Error)`. |
| `Logout()` | State goes `Offline` at once, the backend logout follows (see [Gotchas](#gotchas)); disables the start-up login. |
| `GetAuthState`, `IsLoggedIn`, `GetUser`, `GetLastError` | `GetUser().UserId` is the backend id that names the hosted lobby session. |
| `CanStartLogin`, `IsOnlineAvailable`, `IsOnlineReachable`, `SupportsMethod`, `GetMethodDisplayName`, `GetOnlineUnavailableText`, `HasSignInChoice`, `CanSignOut` | For the UI: button states and labels (the method name, e.g. "Steam", and the "start Steam" text come from the provider). |
| `ValidateGuestName`, `GetGuestNameMaxLength`, `GetGuestNameErrorText` | Trimmed, 3–16 chars, ASCII letters, digits, space, `_`, `-` (EIK sent the name to EOS as ANSI). Used by the guest form of the EOS backend only. |
| `GetAuthErrorText(EMKHAuthError)` | Readable, localized message; the raw backend message only goes to `LogMKHAuth`. |
| `ShouldShowDevLoginButtons`, `GetDevCredentialNames` | Development buttons (flag in the settings, never in Shipping). |

State machine:

```
Unknown ──TryAutoLogin──> LoggingIn (silent) ──success──> LoggedIn ──Logout / identity lost──> Offline
   │                          │ failure / timeout                ^                                   │
   └── nothing to retry ──> Offline <────────────────────────────┼──────── Login (interactive) ───────┘
                              └── late success of a timed-out start-up login is adopted ─┘
```

- Only one login or logout runs at a time: while `LoggingIn`, `LoggedIn`, a pending backend logout or a busy
  provider, `Login` is refused with `Busy`. One exception, below.
- Results carry a request id; results of an outdated request are ignored. The only accepted late result is the
  success of the start-up login after its timeout: the player simply comes online late.
- **Login during a timed-out start-up login.** Without network a backend can take tens of seconds to give up on the
  start-up login, long after the boot overlay is gone. `CanStartLogin` ignores that pending request, so the sign-in
  buttons are available as soon as the lobby shows. A login started meanwhile is queued (`QueueLogin`, state
  `LoggingIn`) and resolved when the start-up login answers (`RunQueuedLogin`): its success is adopted when it signed
  in the identity asked for (`IsSameLogin`: same method, same guest name or dev credential); another identity is
  signed out first, then the queued login runs; after a failure the queued login runs on its own.
- `OnAuthStateChangedNative` is broadcast before the Blueprint `OnAuthStateChanged`, so the game instance has
  updated its session state when the UI reacts.

## Providers

`UMKHAuthProvider` is an abstract `UObject`: `IsBackendAvailable`, `SupportsMethod`, `GetMethodDisplayName`,
`IsBusy`, `Login(Method, Params, bSilent, Callback)`, `Logout(Callback)`, `OnIdentityLost`, `OnIdentityRestored`.
Each callback runs exactly once, possibly inline. The provider class comes from the online backend, picked in
Project Settings → Game → Makhia Online. Optional hooks describe the platform to the UI: `SignsInByDefault`,
`GetUnavailableText`, `IsBackendReachable`, `SupportsSignOut`, `GetMethodDisplayName`.

### Steam (`UMKHAuthProvider_Steam`, active)

Only `PlatformAccount`, silent at every start-up (also the first one: `SignsInByDefault`). `UserId` = SteamID64,
display name = Steam persona name. Steam not running (or `-nosteam`) → the backend is unavailable, the game starts
offline and the mode card says to start Steam and restart the game (Steam only initialises with the process).
Steam offline mode or no connection to the Steam servers → `NoNetwork`: no "Sign in" is offered, the chip says "No
internet connection", and `OnIdentityRestored` signs the lost identity back in by itself when the servers return.
There is no sign out (`SupportsSignOut` false): the account chip shows a game-menu button instead. Details and
config: `OnlineBackends.md` → Steam, `SteamIntegration.md`.

### EOS (`UMKHAuthProvider_EOS`, dormant in `MakhiaEOS`)

Drives `IOnlineIdentity::Login` of the EIK subsystem with the credential types EIK parses in
`FUserManagerEOS::Login`:

| Method | EIK credentials | Notes |
|---|---|---|
| PlatformAccount (silent) | `eas_+_EIK_LCT_PersistentAuth_+_EIK_ECT_EPIC` | Token saved by EOS at the last Epic login; EIK deletes it if invalid. |
| PlatformAccount (interactive) | persistent auth, then `eas_+_EIK_LCT_AccountPortal_+_EIK_ECT_EPIC` | Account portal = Epic login window. |
| Guest | `noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN`, `Id` = display name | EIK creates the device id on first use; same product user id afterwards. |
| Developer | `eas_+_EIK_LCT_Developer_+_EIK_ECT_EPIC`, `Id` = `DevAuthHost`, `Token` = credential name | Dev Auth Tool. |

- The user id is the product user id (NetId `EpicAccountId|ProductUserId`, or the product user id alone for a
  guest); `bHasSocialAccount` is true when the NetId has an Epic account part.
- Display name: the guest name, or `IOnlineIdentity::GetPlayerNickname` (Epic display name), falling back to the
  Dev Auth Tool credential name.
- The identity delegates are bound for the provider lifetime and filtered by the pending step, never added or
  removed during an EIK broadcast.
- EIK forwards the EOS Connect (guest) intermediate result `EOS_OperationWillRetry` as a failed login, then the final
  answer of the same call: the provider ignores it and keeps waiting (treating it as final made later answers land
  on the next login).
- EIK logout of an Epic account also deletes the persistent token (so the next launch does not sign it in again).

## Start-up login and saved preferences

`UMKHAuthSaveGame` (slot `Auth`, user 0): `LastMethod`, `GuestDisplayName`, `bAutoLogin`.

`TryAutoLogin` picks, in order:
1. Play In Editor → `Offline` (the platform login does not work in PIE: test it in Standalone Game).
2. `-AuthToken=<credential>` on the command line (not in Shipping) → silent `Developer` login. Replaces the old
   `BP_LobbyMode` "TryAutoLogin" that parsed the same switch.
3. `bAutoLoginOnBoot` and `bAutoLogin` and `LastMethod` is `PlatformAccount` or `Guest` → silent login with that
   method (guest: saved name). A `Developer` login is saved but never retried. A saved method the provider does
   not support (e.g. a guest login of the EOS days) is not retried.
4. No saved login and the provider `SignsInByDefault` (Steam) → silent login with the platform account.
5. Otherwise → `Offline` (also after an explicit sign out).

A silent login has a wall-clock timeout (`AutoLoginTimeout`, checked every frame on the core ticker: game timers
and ticker delays run on engine time, which skips the start-up and loading stalls). A success writes
`LastMethod` and `bAutoLogin = true` (and the guest name); `Logout` writes `bAutoLogin = false`. Losing the
identity without a logout keeps the preferences.

## Game instance integration

`UMKHGameInstance` binds `OnAuthStateChangedNative` in `Init` and runs `TryAutoLogin` in `OnStart`.

- `GetIsLoggedIn()` reads the subsystem (the old `bIsLoggedIn`, `OnLogin`, `OnLogout`, `SetIsLoggedIn` are gone).
- **Signed in**: `CurrentSessionName = UserId + "_LOBBY"`, `Username = DisplayName`. From the offline lobby
  (lobby map, standalone net mode) the lobby is reopened as host (`?listen`); during a match nothing travels and
  the next return to the lobby hosts it (`LeaveToNewLobby` = `BackToLobby(GetIsLoggedIn())`).
- **Signed out** (logout or identity lost): in a hosted or joined lobby → `LeaveToNewLobby` (offline lobby);
  outside a session the session name is forgotten; `FinishBackToLobby` forgets it after leaving a session.
- **Safety net** (`LoadComplete`): a signed-in player who lands in the offline lobby (a login that completed while
  the lobby was travelling, e.g. guest → account switch) gets the lobby reopened as host. Being signed in in a
  standalone lobby is never wanted.

### Lobby subsystem

`UMKHGameInstance` keeps the session state (host, session name, username), the travels (`BackToLobby`,
`LeaveToNewLobby`, `OpenLevel`, `ServerTravel`, `StartOfflineMatch`), the boot and the reaction to sign-in changes.
Everything about the lobby room and the ways a match starts lives in **`UMKHLobbySubsystem`**
(`Source/Makhia/Public/Lobby/MKHLobbySubsystem.h`, a `UGameInstanceSubsystem`, so it lives as long as the game
instance; Blueprint node *Get MKHLobby Subsystem*):

| Area | API (Blueprint categories `Lobby|Room`, `Lobby|Ready`, `Lobby|Matchmaking`, `Lobby|Bot`) |
|---|---|
| Room (host counters) | `Get/Increment/DecrementPlayersInLobby`, `Get/SetPlayersToStart`, `HasOtherPlayersInLobby` |
| Ready | `GetIsReady`, `SetIsReady` → `OnReadyStateChanged`; `HandlePlayerReady` (host) → match countdown or `OnStartMatchmakingDelegate`; `CanStartGame`; `IsMatchCountdownRunning`; `OnStartMatchDelegate` |
| Matchmaking | `Get/SetIsMatchmaking`, `Get/SetSelectedRegion` |
| Bot | `RequestOfflineMatch`, `CancelOfflineMatchRequest`, `IsOfflineMatchPending`, native `OnOfflineMatchPendingChanged` |

The game instance calls into it on the session events: `OnLobbyCreated` → `HandleLobbyHosted` (one player),
`OnLeaveSession` → `HandleSessionLeft`, `BackToLobby` / `StartOfflineMatch` → `ResetLobbyState`, and
`StartOfflineMatch` asks `IsOfflineMatchRefused`. The timings are in Project Settings → Game → **Makhia Lobby**
(`UMKHLobbySettings`): `MatchStartCountdown` (2 s), `OfflineMatchDelayMin` / `OfflineMatchDelayMax` (3-5 s).
C++ users: `AMKHLobbyPlayerState` (ready changes, see `LobbyPresentation.md` → Lineup), `AMKHLobbyGameState`,
the mode cards and the nameplate. Blueprint users: `BP_LobbyMode`, `BP_MKHGameMode`, `BP_LobbyController`,
`BP_MatchmakingComponent`, `W_LobbyOverlay`, `BP_MKHGameInstance`.

## Boot overlay

The game always starts in the lobby; `W_BootOverlay` (`UMKHBootOverlayWidget`) covers it until the start-up login
is settled, so the player never sees the offline lobby flash before the hosted one.

```
UMKHGameInstance::BootPhase
  Pending ──TryAutoLogin──> Connecting ──LoggedIn, offline lobby──> Travelling ──LoadComplete──> Done
     │                          └── Offline / timeout ─────────────────────────────────────────> Done
     └── nothing to retry (or Play In Editor) ─────────────────────────────────────────────────> Done
```

- `AMKHLobbyHUD::BeginPlay` creates the overlay (Z order 100) only while `IsBootInProgress()`. The lobby loads
  before `OnStart`, so the overlay is up at `Pending`; when there is nothing to retry it goes `Done` before the
  first frame is drawn.
- During the travel to `Lobby?listen` the old overlay dies with the old map and the new lobby creates it again
  (`Travelling`); `LoadComplete` moves to `Done`, the overlay stops catching clicks, fades out (`FadeOutDuration`)
  and removes itself.
- The status line follows the phase ("Connecting...", "Opening your lobby..."). The logo is the game name in
  `TS_Display` (culture invariant) until a logo asset exists.
- The first child of the overlay is `SolidBackdrop`, a plain black image, under the gradient (`MI_UI_Background`).
  On the first frames UI material shaders may still be compiling ("Preparing shaders"): a material-only backdrop is
  not drawn and the lobby shows through. Keep a material-free opaque layer at the bottom.

## Lobby hub

`W_LobbyOverlay` (created by `WBP_LobbyHUD`) is the hub: no main menu, every way to play starts here.

```
W_LobbyOverlay
├── DevLoginPanel   W_DevLoginPanel (UMKHDevLoginPanelWidget)  top left; only with bShowDevLoginButtons, never in Shipping
├── AccountChip     W_AccountChip   (UMKHAccountChipWidget)    top right
└── bottom right    RegionMenu, ModeSelector (W_ModeSelector, UMKHModeSelectorWidget)
AMKHLobbyHUD
├── W_LoginPanel    (UMKHLoginPanelWidget)   modal, Z 50, opened by the chip or by the locked online mode
└── W_BootOverlay   (UMKHBootOverlayWidget)  Z 100
```

**Mode selector** (`UMKHModeSelectorWidget`, `W_ModeSelector`): a list of the four modes (`EMKHLobbyGameMode`:
Online, Training, FreeDuel = Skirmish, PathOfGlory) and the detail of the chosen one: picture (`Previews`), title, description,
career levels, lock reason, status line and the action button.

- **Mouse**: a click on a row only updates the detail; the action button starts the mode.
- **Gamepad**: moving on the rows updates the detail at once (`FollowRowFocus`). Right, or A on a row, enters the
  detail: the chosen career level in the Way of the Makhai, the Sign in action on a locked online mode, otherwise the
  action button. Left goes back to the chosen row. Custom navigation rules keep this path independent of the layout.
- A locked mode stays selectable, so its reason can be read; only its action button is disabled.

Lock rules are C++ and re-evaluated every tick while shown (sign-in state, lobby population, matchmaking), like the
exit panel of the game menu:

| Mode | Locked when | Reason | Action button when playable |
|---|---|---|---|
| Online | online services unavailable | "Online services are not available." | `OnPlayRequested` → `W_LobbyOverlay` → existing ready flow (`ReadyPressedDelegate`); label "Search", "Ready" with other players in the room |
| | signing in | "Signing in..." | |
| | offline | "Sign in to play online." + **Sign in** action (login panel) | |
| Training, Skirmish, Way of the Makhai | matchmaking | "Cancel the search to play offline." | `UMKHLobbySubsystem::RequestOfflineMatch(Mode, CareerLevelIndex)`; label "Train" or "Fight" |
| | another player in the room (host or client) | "Leave the room to play offline." | |

The ready/matchmaking Blueprint flow keeps driving online play on top of the locks: `SwitchEnabledButton` calls
`ModeSelector.SetPlayAllowed` and `SetCancelAllowed`. While a mode runs (cancel allowed and play not, or an offline
request pending) it stays chosen, the other rows and the career levels are disabled, and **the action button is the
Cancel button**: label "Cancel", `CancelButtonStyle` (`BS_ModeCardCancel`). A click broadcasts `OnCancelRequested`
(→ `CancelPressedDelegate` → `BP_LobbyController.OnCancelPressed` → `BP_MatchmakingComponent.Cancel`, unchanged) or
calls `CancelOfflineMatchRequest`. After an online cancel click the button stays disabled until the lobby flow
answers, or for `CancelAnswerTimeout` (5 s) at most.

**Status line** (footer of the detail): `SearchThrobber`, `TimerText` (match countdown or elapsed search "m:ss") and
`StatusText`. `UpdateStatusPanel` of `W_LobbyOverlay` routes every line of the matchmaking flow to `SetLobbyStatus`;
a line of the lobby flow wins over the generic line of the running mode ("Searching for an opponent...", "Ready.
Waiting for the other players.", "The duel is about to begin!"), and "Cancelling..." wins over both.

**Region menu**: stays in `W_LobbyOverlay` (`SizeBox_1`), shown only while the online mode is chosen (`IsOnlineSelected`).

**Career levels** (`UMKHCareerLevelWidget`, `W_CareerLevel`): three tiles with state, opponent (`DisplayName` of the
level's skin) and awarded title; `BS_SkinTile` / `BS_SkinTileEquipped` mark the chosen one. The data comes from
`UMKHCareerSubsystem` (`OfflineModes.md`): a level not unlocked is disabled, beaten levels stay playable, and without
levels the Way of the Makhai is locked. Player-facing texts never mention the bot (immersion).

**Offline wait: a fake search before the match.** The action button calls `UMKHLobbySubsystem::RequestOfflineMatch`,
which waits a random `OfflineMatchDelayMin`..`OfflineMatchDelayMax` (3-5 s, `UMKHLobbySettings`) before
`StartOfflineMatch`. Meanwhile the status line shows the elapsed time and a flavour line that changes every
`FlavourLineDuration` (1.8 s), starting from a random one. A click calls `CancelOfflineMatchRequest`; a player joining
the room cancels it too. The wait is a timer of the lobby world, so leaving the level drops it. The level load is
covered by the loading screen (`LoadingScreen.md`).

**Countdown before a lobby match.** When every player of a room is ready (`UMKHLobbySubsystem::HandlePlayerReady`,
host) the match no longer starts at once: `BeginMatchCountdown` waits `MatchStartCountdown` (2 s, `UMKHLobbySettings`) before
`OnStartMatchDelegate`. A player un-readying (the Cancel button of the selector: outside matchmaking
`BP_MatchmakingComponent.Cancel` only clears the ready state, the player stays in the room) or leaving the room
stops it. The host mirrors the countdown on `AMKHLobbyGameState` (`GameStateClass` of `BP_LobbyMode`, replicated
end time in server world time), so on every machine the ready card shows the seconds left and "The duel is about
to begin!".

The matchmaking start (a player joining a searching host) is not delayed: from the join on, the travels are
covered by the loading screen (`LoadingScreen.md`). When the countdown runs out the host keeps it at zero, so the
clients cover their lobby with the loading screen until the travel; a refused start clears it.

**Account chip** (`UMKHAccountChipWidget`, refreshed every tick):

| State | Shows |
|---|---|
| Signed in | display name (culture invariant), method badge from the provider ("Steam", "Guest", "Developer"), **Sign out** when the backend allows it (not with Steam: the game-menu button `GameMenuButton` is shown instead); a guest also gets **Sign in with {Platform}** (`SwitchToPlatformAccount`: guest logout, then the platform login once the backend logout completed) |
| Signing in | "Signing in..." |
| Offline | "Offline" + "Not signed in" / "Online services unavailable" / "No internet connection"; **Sign in** when a login can start (`CanStartLogin`) |

**Login panel** (`UMKHLoginPanelWidget`, design of the handoff, section 4.3; with a single sign-in method the Sign in
buttons start the login directly and the panel is not opened, `AMKHLobbyHUD::OpenLoginPanel`): platform button (label
"Sign in with {Method}" from the provider) with hint, "or", guest button with hint; the guest form (name field
prefilled with the saved name, `{Count}/{Max}` counter, live validation message, Continue enabled only for a valid
name) opens on the guest button. Buttons are enabled only while `CanStartLogin`; the status line shows
"Signing in..." or the readable error of `OnLoginFailed`. Esc (HUD order: settings → login panel → credits →
menu, plus `NativeOnKeyDown` when the name field has focus), Close and a click outside close it; a successful
login closes it too (and the lobby travels).

**Dev login panel** (`UMKHDevLoginPanelWidget`): builds one `BS_MenuButtonSmall` + `TS_Button` button per
`DevCredentialNames` entry at initialization ("Dev login: <name>", culture invariant), enabled only while a login
can start. The styles are properties of the panel: set them on the instance in `W_LobbyOverlay` too (CDO defaults
set after an instance was placed do not reach it).

## Match against the bot

Full description in `MatchFlowArchitecture.md` → "Match Modes & the Offline Bot". In short:

- The offline modes of the selector call `UMKHLobbySubsystem::RequestOfflineMatch` (cancellable 3-5 s wait, see Mode selector), then
  `StartOfflineMatch`: the game level opens without `?listen`, with
  `?Mode=Bot`; `AMKHGameMode` spawns `BotClass` (`BP_DuelBot`, `AMKHDuelBot` driven by `AMKHDuelBotController`) on the
  free PlayerStart. Development shortcut: console `MKHPlayVsBot`.
- Same rules as the online duel (preparation, first to 3, in-place rematch); participants are `ACharacterBase`
  implementing `IMKHDuelParticipant`. The game menu adds *Restart Game* (`RestartOfflineMatch`) in this mode only.
- The sign-in state is untouched by the match: *Return to Lobby* (`LeaveToNewLobby`) hosts the lobby when signed
  in, opens the offline lobby otherwise. A login completed during the match does not travel; the next return to
  the lobby hosts it.

## Settings and development tools

Project Settings → Game → **Makhia Online** (`UMKHOnlineSettings`, `DefaultGame.ini`):
`BackendClass` (online backend, see `OnlineBackends.md`), `bAutoLoginOnBoot`, `AutoLoginTimeout`, `bShowDevLoginButtons`, `DevAuthHost`
(`127.0.0.1:8081`), `DevCredentialNames` (`MainRuy`, `DevRuy`); the last three only matter to the EOS backend.

Development console commands (not in Shipping, `MKHAuthSubsystem.cpp`):

| Command | Effect |
|---|---|
| `MKH.Auth.LoginGuest <Name>` | Guest login (backends that support it). |
| `MKH.Auth.LoginPlatform` | Platform account login (Steam: the running client; EOS: persistent auth, then account portal). |
| `MKH.Auth.LoginDev <Index>` | Developer login with `DevCredentialNames[Index]` (EOS Dev Auth Tool). |
| `MKH.Auth.Logout [DelaySeconds]` | Logout; the delay lets `-ExecCmds` sign out after the start-up login. |
| `MKH.Auth.Status` | Logs state, user, availability and last error. |

Testing online (Steam client running, Standalone Game, no extra switch): the start-up login signs in by itself;
`MKH.Auth.Status` logs the result. LAN tests without a platform account: `-MKHOnline=Null -nosteam
-ExecCmds="MKH.Auth.LoginPlatform"` (`OnlineBackends.md` → Testing).

Reset the saved preferences by deleting `Saved/SaveGames/Auth.sav`.

## How-To

- **Add a backend**: subclass `UMKHAuthProvider` and implement `IsBackendAvailable`,
  `SupportsMethod` (return false for `Guest` if the backend has no anonymous login: the login panel hides it),
  `IsBusy`, `Login` and `Logout` (each callback exactly once), optionally `GetMethodDisplayName` (badge and
  "Sign in with {Method}" label) and `InitializeProvider` / `ShutdownProvider` for delegate binding. Broadcast
  `OnIdentityLost` when the backend drops the user. Return the class from the backend's `GetAuthProviderClass`
  (`OnlineBackends.md` → Adding a backend); nothing else in the game changes. The friends layer has its own provider
  (`UMKHSocialProvider`).
- **Add a sign-in entry point** (a new button or screen): call `UMKHAuthSubsystem` (`Login*`, `Logout`), enable it
  with `CanStartLogin`, show errors from `OnLoginFailed` through `GetAuthErrorText`. Never call platform or EIK
  login nodes from Blueprints.
- **Add a development credential**: append the name to `DevCredentialNames`; the dev panel builds its buttons
  from the list and `MKH.Auth.LoginDev <Index>` uses the same index.
- **New lobby mode or lock rule**: lock rules live in `UMKHModeSelectorWidget` (C++, re-evaluated every tick). Add
  the `EMKHLobbyGameMode` value, its texts and rule there, then a row in `W_ModeSelector`, and localize the texts.

## Gotchas

- **Steam not running**: the Steam provider reports the backend unavailable, every login is refused with
  `BackendUnavailable` and the lobby shows "Steam is not running. Start Steam, then restart the game...". "Start Steam
  and retry" is a game restart: Steam OSS and SteamSockets only initialise at process start.
- **Logout order** (EOS). EIK's `LeaveLobby` completion dereferences the signed-in user without checking it and
  crashes if the user logged out meanwhile. `Logout` therefore goes `Offline` first; the game instance, when in a
  session, holds the backend logout (`AddLogoutHold`) and releases it in `FinishBackToLobby`, after the session is
  left (needed by EOS only).
- **Platform login does not work in PIE** (the per-world online subsystem instance differs from the one that signs
  in; editor processes never run Steam): the start-up login is skipped in PIE; test logins in Standalone Game.

## Tests

Results by period (details in the handoffs):

- 2026-09-30 / 2026-10-01 (EOS, Standalone): first run offline; guest login → lobby hosted, `Auth.sav` written;
  relaunch → silent login with the same user id; delayed logout → lobby left, offline lobby; start-up login timeout
  → offline, late success adopted; boot overlay `Pending → Connecting → Travelling → Done`; login panel and guest form
  validation; French texts fit.
- 2026-10-09 / 2026-10-10 (Steam): silent start-up login with the running client (replacing an old saved guest login),
  `-nosteam` → offline start with the "start Steam" text; no Sign out + game-menu button in the chip. Network loss in
  the lobby and start without network ("No internet connection", automatic sign-in when it returns) still to be
  retested by hand.
- PIE: lobby offline with the login buttons disabled, match against the bot and back to the offline lobby.

Still to test by hand (UMG clicks are not automatable): selector clicks (an offline mode starts the match, online starts the
ready flow), chip buttons, login panel Esc / click outside. EOS-only paths (Epic account portal, persistent token,
Dev Auth Tool, guest form) are tested again when the `MakhiaEOS` module is reactivated.
