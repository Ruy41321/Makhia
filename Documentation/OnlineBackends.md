# Online Backends

How the game talks to an online platform (EOS, Steam, LAN) through one common interface, so the backend can be
swapped without touching gameplay, UI or Blueprints.

> Status (2026-10-10): migration EOS → Steamworks done (see `Handoff/SteamMigrationHandoff.md`). Backend selection,
> auth / session / social / transport providers (Steam, Null, EOS dormant), `UMKHSessionSubsystem`,
> `BP_MatchmakingComponent` facade, UI driven by capabilities, **Steam backend active** (sign-in, lobbies,
> matchmaking, SteamSockets P2P, friends, presence, avatars, recent players, invites, overlay, route and quality
> diagnostics). EOS lives in the dormant `MakhiaEOS` module, the EIK plugin is removed. Practical Steam guide (setup,
> AppID 480 limits, PC + VM tests, dedicated AppID): `SteamIntegration.md`. To do: dedicated AppID and security (M7).

## Table of Contents

1. [Layers](#layers)
2. [Choosing the backend](#choosing-the-backend)
3. [Capabilities](#capabilities)
4. [Sessions and matchmaking](#sessions-and-matchmaking)
5. [Backends](#backends)
6. [Adding a backend](#adding-a-backend)
7. [Steam](#steam)
8. [Testing](#testing)

See also `SteamIntegration.md` (Steam setup and tests) and `Source/MakhiaEOS/README.md` (reactivating EOS).

---

## Layers

```
UI / Blueprints (W_LobbyOverlay, BP_MatchmakingComponent facade, friends panel, net stats)
        │ only game subsystems
        v
UMKHAuthSubsystem   UMKHSessionSubsystem   UMKHFriendsSubsystem   UMKHNetDiagnosticsSubsystem
        │ each creates its provider from the backend
        v
UMKHOnlineBackend (Abstract, chosen by UMKHOnlineServicesSubsystem)
  ├─ UMKHAuthProvider          sign-in (Documentation/OfflineModeArchitecture.md)
  ├─ UMKHSessionProvider       host/update/search/join/destroy lobby, send invites, open slots
  ├─ UMKHSocialProvider        friends, presence, friend requests, lobby invites received, avatars, recent
  │                            players, platform overlay (none: no friends)
  └─ UMKHNetTransportProvider  route direct/relayed + relay location, delivery quality, NAT type (base: plain IP)
```

Rules:
- Nothing outside a backend and its providers includes an online subsystem interface or a platform SDK.
- Public headers of the game module only use the neutral types of `Online/Backend/MKHOnlineTypes.h`
  (`FMKHLobbySessionSettings`, `FMKHLobbySearchQuery`, `FMKHLobbySearchResult`, `FMKHOnlineCapabilities`,
  `EMKHMatchmakingRegion`). Backend data travels in the opaque `FMKHLobbySearchResult::Handle`.
- Every provider completion runs exactly once, possibly inline.

## Choosing the backend

`UMKHOnlineServicesSubsystem` (GameInstance subsystem, log `LogMKHOnline`) creates the backend at start-up:

1. `-MKHOnline=<Name>` on the command line (not in Shipping), matched against `GetBackendName()` ("EOS", "Null", ...).
2. Project Settings → Game → Makhia Online → `BackendClass` (`Config/DefaultGame.ini`).
3. `UMKHOnlineBackend_Null` when the class cannot be loaded (e.g. its module is not compiled).

The auth and session subsystems declare the dependency (`InitializeDependency`), so the backend exists before them.

## Capabilities

`FMKHOnlineCapabilities` (read with `UMKHOnlineServicesSubsystem::GetCapabilities`, Blueprint pure) tells the UI
what to show instead of testing the backend name:

| Field | EOS | Null | Steam |
|---|---|---|---|
| `bSupportsRegionSelection` | yes (the EIK lobby search never used it) | no | no (Steam filters by distance) |
| `bSupportsInGameFriendRequests` | yes | no | no (Steam overlay) |
| `bHasPlatformOverlay` | yes | no | yes |
| `bReportsNatType` | yes | no | no |
| `bSupportsAvatars` | no | no | yes |
| `bSupportsRecentPlayers` | no | no | yes (Steam coplay list) |

Where the UI reads them: `W_LobbyOverlay.UpdateSelectedRegion` hides the region menu and searches with
`NoSelection` without region support; `UMKHFriendListWidget` / `UMKHFriendEntryWidget` hide the add-friend section
and the accept/reject buttons without in-game friend requests, and with a platform overlay show instead a
"Respond on {Platform}" button on received requests, an "Add friend" button on recent players and a
"{Platform} friends" button in the header (`UMKHOnlineBackend::GetPlatformDisplayName`); the avatar of an entry and
the recent players section only appear with their capability; `UMKHNetStatsWidget` hides the NAT rows when NAT
types are not reported and the delivery row when the transport does not measure it; `UMKHDevLoginPanelWidget` needs
the provider to support the `Developer` method.

The backend also answers one platform question outside the capabilities: `UMKHOnlineBackend::IsRunningOnHandheld`
(Steam: `ISteamUtils::IsSteamRunningOnSteamDeck`, also under Proton; Null and the base class: false), read through
`UMKHOnlineServicesSubsystem::IsRunningOnHandheld`, where `-MKHHandheld` forces true outside Shipping. It picks the
handheld video profile (`SteamDeck.md`).

Auth providers also describe themselves (no capability field needed): `SignsInByDefault` (the platform session
exists outside the game, so the first start-up signs it in without a saved login), `GetUnavailableText` (what the
player can do while the backend is unavailable, shown by the mode card and the login panel), `IsBackendReachable`
(false without network: no sign-in is offered, the chip and the mode card say "No internet connection", and a login
that failed with `NoNetwork` runs again by itself on `OnIdentityRestored`), `SupportsSignOut` (false with Steam: the
account chip shows no Sign out, only the icon button that opens the game menu) and the supported methods. With the platform account as the only method (`UMKHAuthSubsystem::HasSignInChoice` false) the "Sign in"
buttons start the sign-in at once instead of opening the login panel (`AMKHLobbyHUD::OpenLoginPanel`).

## Sessions and matchmaking

`UMKHSessionSubsystem` (log `LogMKHSession`) owns the lobby flow that used to live in `BP_MatchmakingComponent`;
the component is now a facade (events forward to the subsystem, `OnSessionEvent` is relayed to its dispatchers).

- **Lobby**: every signed-in player hosts a 2-slot lobby named `<UserId>_LOBBY` when the lobby level opens as
  listen server (`HandleLobbyLevelStarted`). Attributes (`FMKHLobbyKeys`, string values): `SessionName`,
  `IsMatchmaking` ("1"/"0"), `MatchmakingTime` (unix time, "No" when passive).
- **Matchmaking** (`StartMatchmaking`): the lobby is advertised with `IsMatchmaking=1` and the start time, then a
  search runs every `UMKHLobbySettings::MatchmakingSearchInterval` seconds. Results are filtered
  (`FilterSearchResults`: newest lobby per owner, recently unreachable lobbies skipped) and checked
  (`CanJoinLobby`): not the own lobby, one free slot, and **join priority** — the player who started later joins
  the one who started earlier; in the same second the player whose opponent has the smaller session name joins
  (string comparison, valid for any id format). Only the first joinable lobby of a search is joined.
- **Join**: own lobby destroyed → `JoinLobby` → `ClientTravel` to the connect string. A failure re-hosts the own
  lobby and reports `MM-JOIN-FAILED`.
- **Host start** (`StartMatch`): loading screen, lobby back to passive, `ServerTravel` to the arena.
- **Invites accepted outside the game** (overlay, platform chat, game started by an invite): the provider
  broadcasts `OnLobbyInviteAccepted`, the subsystem leaves the own lobby and joins. Outside a ready lobby (start-up
  from an invite before the own lobby is hosted, travel, match) the invite is kept (`DeferredInvite`) and joined as
  soon as the lobby is ready (`HandleHostLobbyComplete`, `HandleLobbyLevelStarted`).
- **Invites received in the game** come from the social provider (`UMKHFriendsSubsystem` lists them); accepting one
  resolves its lobby (`UMKHSocialProvider::ResolveLobbyInvite`) and joins it through `JoinInvitedLobby`, the same
  path as an invite accepted in the overlay.
- **Recent players**: when a match starts and at every duel (`AMKHGameState`, phases `InitialPreparation` and `Duel`:
  on a client the opponent's player state may arrive after the preparation phase) every machine reports its human
  opponents, skipping its own user id, to the social provider (`UMKHFriendsSubsystem::ReportMatchOpponents` →
  `ReportPlayedWith`).
- **Activity shared with friends**: every 2 s `UMKHFriendsSubsystem` builds the local `FMKHPlayerActivity` (lobby with
  `UMKHLobbySubsystem` player count, or match against the bot / another player with `AMKHGameState::GetRound` and
  the scores, own score first) and hands it to `UMKHSocialProvider::SetLocalActivity` when it changed; friends read it
  back in `FMKHFriendInfo::Activity`. Steam: rich presence keys `mkh_act` (`lobby`, `offline`, `online`),
  `mkh_lobby` ("1/2"), `mkh_round`, `mkh_score` ("2-1"), cleared when the service stops. Steam's own friend list
  cannot show them with AppID 480 (`steam_display` needs localization tokens set up on the dedicated AppID, M7).
- **Leaving a networked level**: `UMKHGameInstance::ReleaseWorldNetDriver` shuts the net driver of the current
  level down on the next tick (never inside the engine's net code or while a level loads: a shutdown during
  `UWorld::Listen` crashed the client). It runs as soon as the player decides to leave (`AbandonLobby`, `BackToLobby`,
  before the session destroy, which takes seconds for a Steam lobby with members) and before every
  `OpenLevelWithOptions`, so the other player learns it at once. The travel then waits until 0.15 s after the
  shutdown (`TravelAfterNetworkRelease`): SteamSockets closes its sockets one tick late, and a new lobby listening on
  P2P port 7777 too early fails with `NET-LISTEN-FAILED`. A delayed travel is dropped when another travel already
  left the level (two return paths after a disconnection).
- **Teardown**: `UMKHGameInstance` destroys the session through `DestroyCurrentSession` (return to the lobby, with
  a timeout) and `DestroyCurrentSessionDetached` (offline match, shutdown).

## Backends

| Backend | Module | Online subsystem | Notes |
|---|---|---|---|
| `UMKHOnlineBackend_EOS` | `MakhiaEOS` (**dormant**: not built, EIK plugin removed) | `EIK` | Epic account / guest / Dev Auth login, EOS lobbies. Reactivation: `Source/MakhiaEOS/README.md` |
| `UMKHOnlineBackend_Steam` | `MakhiaSteam` (**active**, Win64 only) | `STEAM` | Account of the running Steam client, Steam lobbies, SteamSockets P2P. See [Steam](#steam) |
| `UMKHOnlineBackend_Null` | `Makhia` | `NULL` | Development: LAN lobbies, one id per process, logins refused in Shipping |

`UMKHSessionProvider_OSS` implements the session provider on any engine online subsystem (`IOnlineSession`);
a backend subclass only names the subsystem and adjusts the settings (`ConfigureSessionSettings`,
`ConfigureSearch`, `ReadSearchResult`). The Null subsystem neither filters searches by attribute nor
re-advertises after an update: the LAN lobby is always advertised and the base provider filters the results.

## Adding a backend

1. Subclass `UMKHOnlineBackend` (name, capabilities, provider classes).
2. Auth: subclass `UMKHAuthProvider`. Sessions: subclass `UMKHSessionProvider_OSS` when the platform has an
   engine online subsystem, otherwise `UMKHSessionProvider`.
3. Select it in Project Settings → Makhia Online, or test it with `-MKHOnline=<Name>`.

## Steam

Module `Source/MakhiaSteam` (depends on `Makhia`, never the opposite; Steamworks types stay in its `Private/`
files). Plugins `OnlineSubsystemSteam` and `SteamSockets` in `Makhia.uproject`; the module is listed for Win64 only
(`.uproject` and both targets).

- **Config** (`DefaultEngine.ini`): `[OnlineSubsystem] DefaultPlatformService=Steam`; `[OnlineSubsystemSteam]`
  `SteamDevAppId=480` (Spacewar, shared by every Steamworks developer, until the game has its own AppID),
  `bRelaunchInSteam=false`, `bAllowP2PPacketRelay=true`, `bUseSteamNetworking=true`; `NetDriverDefinitions` →
  `SteamSocketsNetDriver` with `IpNetDriver` as fallback; PacketHandler `SteamAuthComponentModuleInterface` (the
  host checks the Steam ticket of every player that connects; it passes through when Steam is not running).
  `DefaultGame.ini`: `BackendClass=/Script/MakhiaSteam.MKHOnlineBackend_Steam`.
- **Steam must run when the game starts**: `SteamAPI_Init`, the Steam subsystem and the SteamSockets socket
  subsystem only start with the process. Without Steam (or with `-nosteam`) the backend reports itself
  unavailable, the game starts offline and the mode card says to start Steam and restart the game. Editor
  processes never run Steam (Play In Editor is offline and uses `IpNetDriver`). Non-Shipping builds write
  `steam_appid.txt` next to the executable at start-up.
- **Sign-in** (`UMKHAuthProvider_Steam`): only `PlatformAccount`, silent at every start-up (also the first one,
  `SignsInByDefault`); `UserId` = SteamID64, display name = Steam persona name. Steam offline mode → `NoNetwork`.
  Signing out only takes the game offline (and keeps the start-up login off until the next sign-in). Losing the
  Steam servers reports `OnIdentityLost` (the game goes offline); when they come back the provider reports
  `OnIdentityRestored` and `UMKHAuthSubsystem` signs the same identity back in silently, unless the player signed
  out meanwhile. Closing the Steam client quits the game (engine behaviour).
- **Lobbies** (`UMKHSessionProvider_Steam` on `UMKHSessionProvider_OSS`): Steam lobbies, *friends-only* while
  passive (friends see it: "Join game" in the Steam friend list, and the in-game invite lookup), *public* while
  matchmaking; lobby searches only return public lobbies. Extra attributes `MKH_Game=Makhia` and `MKH_NetVersion=<network version>` are
  required by every search (Steam filters on its backend), so AppID 480 lobbies of other games and incompatible
  builds never show up; Steam also drops lobbies with another `BuildUniqueId`. The owner is identified by the
  `SessionName` attribute (`<SteamID64>_LOBBY`) as on every backend. Searches use Steam's default distance filter
  (no region selector). Join → connect string `steam.<SteamID64 of the owner>:7777`.
- **Transport** (`UMKHNetTransportProvider_Steam`): at creation it enables private and public ICE candidates
  (`k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable`), so connections go direct when the NATs allow it and through
  a Valve relay otherwise. The SteamSockets net driver keeps its connection handles private, so
  `FMKHSteamConnectionTracker` (private, fed by `SteamNetConnectionStatusChangedCallback_t`) maps the remote SteamID64
  to the Steam connection; from it the provider reads the route (`k_nSteamNetworkConnectionInfoFlags_Relayed`), the
  relay data center (`m_idPOPRelay`, e.g. "fra") and the delivery quality (`GetConnectionRealTimeStatus`, local and
  remote). `UMKHNetDiagnosticsSubsystem` logs every route change (`LogMKHNet`: "Route to ...: Relayed via fra").
- **Social** (`UMKHSocialProvider_Steam`, straight on `ISteamFriends`: the Steam subsystem has no requests, avatars,
  coplay or received invites): follows the game sign-in (`UMKHAuthSubsystem`). Friend list = friends, requests
  received and sent (names: Steam nickname when set, else persona name; **In your party** when the friend's lobby
  is one the local player is a member of, refreshed on `LobbyChatUpdate_t`; otherwise the friend's **activity**,
  e.g. "Makhia: In lobby 1/2", "Makhia: In online match - Round 3 - 2 : 1" with the friend's score first);
  requests are answered in the overlay
  ("Respond on Steam" opens the profile), Steam has no API for them. **In Makhia** = playing the same AppID *and*
  rich presence `mkh=1` (set while the service runs: AppID 480 is shared). Avatars: medium Steam avatar (64x64)
  converted once into a transient texture, refreshed on `AvatarImageLoaded_t` / avatar change. Recent players =
  Steam coplay list of this AppID without friends or pending requests (most recent 10), fed by `SetPlayedWith`;
  "Add friend" opens the overlay `friendadd` prompt. Lobby invites received in game: `LobbyInvite_t` (other AppID
  480 games filtered out); accepting one finds the sender's lobby with `FindFriendSession` (the Steam subsystem
  cannot find a lobby by id), retried for a few seconds while a matchmaking search holds the session search; Steam
  has no decline. Steam notifications arrive on the online thread (`SteamAPI_RunCallbacks`) and are handled on the
  game thread (`FMKHSteamSocialCallbacks`).
- **Invites outside the game**: accepted in the overlay or the Steam chat → `GameLobbyJoinRequested_t` → the Steam
  subsystem's `OnSessionUserInviteAccepted`. Game not running: Steam starts it with `+connect_lobby <LobbyId>` and
  the Steam subsystem reports the invite as soon as the session provider binds; the session subsystem keeps it
  until the own lobby is hosted. With AppID 480 Steam would start Spacewar, not Makhia: test the cold start by
  passing `+connect_lobby <LobbyId>` by hand. No `connect` rich presence is needed: lobbies are joined by id.
- Not yet: dedicated AppID, `SteamAPI_RestartAppIfNecessary`, Shipping `steam_appid` handling (M7, see
  `SteamIntegration.md` → Dedicated AppID and `Handoff/SteamMigrationHandoff.md`).

## Testing

Driver script: `Extra/OnlineTests/session_test_driver.py`, run inside a Standalone process with
`-ExecCmds="py <path>/session_test_driver.py <ReadyDelay> [leave|bot|guest]"`. It logs `[MKHTest]` lines with the
level and the session/lobby state, presses Ready `<ReadyDelay>` seconds after the own lobby is hosted (`bot`: starts
the match against the bot instead), `leave` leaves the match 8 s after reaching the arena, `guest` signs in as a
guest once the start-up login is over (EOS).

Social driver: `Extra/OnlineTests/social_test_driver.py` (`-ExecCmds="py <path>/social_test_driver.py [menu] [overlay]"`)
logs `[MKHSocialTest]` lines with the friends service state, how many entries per relation and presence, the recent
players and the avatars loaded (no player names); `menu` opens the lobby game menu (friend list) after 6 s, `overlay`
opens the platform friend list after 20 s.

- **LAN, two processes on one PC** (no platform account needed):
  `UnrealEditor.exe Makhia.uproject /Game/Makhia/Levels/Maps/Lobby -game -windowed -MKHOnline=Null -nosteam -ExecCmds="MKH.Auth.LoginPlatform,py <driver> 4"`
  twice (`IpNetDriver`; the second process listens on 7778). `-nosteam` is required while Steam is the default
  platform: otherwise the game connections use SteamSockets and the LAN IP addresses cannot be reached.
  From Git Bash prefix the command with `MSYS_NO_PATHCONV=1`, or the map path becomes a Windows path.
  **Same-PC limit of the Null subsystem**: a process uses its single LAN beacon either to host or to search, and two
  beacons on the same port of one PC do not both receive the answers, so usually only one of the two processes
  sees the other. The join only happens when the process that sees the other is also the one with join priority
  (it started searching later): give the two processes different Ready delays and retry with them swapped if
  nothing happens. Two PCs on the same LAN do not have this limit.
- **Steam, one PC**: Standalone with the Steam client running (no `-MKHOnline`): silent sign-in, lobby hosted
  (`SteamSocketsNetDriver ... listening on 7777`), matchmaking searches (`STEAM: Found N lobbies`), overlay with
  Shift+Tab. Two Steam processes on one PC share the same account: a two-player test needs two PCs or PC + VM with
  two Steam accounts (`Handoff/MultiplayerTestVM_Prompt.md`).
- **EOS** (only after reactivating `MakhiaEOS`): Standalone Game (logins do not work in Play In Editor). Without a saved Epic token or the Dev Auth Tool,
  use the `guest` driver step (EOS Connect device id): it hosts, advertises and searches real EOS lobbies. Two
  processes on one PC share the same device id, so a two-player EOS test needs two Epic accounts (Dev Auth Tool).

Results of 2026-10-09 (M1): LAN matchmaking, join, host start, arena, client leaving (host shows
`NET-CONNECTION-LOST`, both back in their own hosted lobby), match against the bot from a hosted lobby, EOS guest
lobby host + advertise + search: all working.

Results of 2026-10-09 (M3, EIK removed): EOS backend loaded from `MakhiaEOS` before putting it to rest (guest login,
lobby, P2P listener, NAT, matchmaking search); then without EIK: match against the bot, LAN matchmaking, join, arena,
host leaving (client shows `NET-CONNECTION-LOST`, both back in their own hosted lobby).

Results of 2026-10-09 (M4, Steam): one PC with the Steam client: silent sign-in (also replacing an old EOS guest
login saved in `Auth.sav`), Steam lobby with the game/version attributes, listen on SteamSockets, matchmaking
advertise + repeated searches; `-nosteam` with the Steam backend: offline start, plain IP; LAN Null with `-nosteam`:
matchmaking, join, arena, host leaving, both back in their own hosted lobby. Not yet tested: two Steam accounts.

Results of 2026-10-09 (M5, Steam social): one PC with the Steam client: friends service ready (friends and a sent
request, presences, every avatar loaded at 64x64), friend panel with avatars and the "Steam friends" button, Steam
overlay opened on the friend list; packaged Development build online. LAN Null: matchmaking, join, arena, route
logged as Direct, both back in their lobby. Not yet tested (two Steam accounts, PC + VM): "In Makhia" presence,
invites in game / from the overlay / "Join game" / cold start, recent players, Steam route and delivery quality.
