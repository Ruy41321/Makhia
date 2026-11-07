# Network Diagnostics

How the game measures the P2P connection of an online duel, logs it (`LogMKHNet`) and shows it on
screen (network stats overlay, `W_NetStat`).

---

## Why it exists

Online duels are **listen server over Steam P2P**: `SteamSocketsNetDriver` (`Config/DefaultEngine.ini`,
`IpNetDriver` as fallback). One player hosts, the other connects to `steam.<SteamID64>`. With ICE enabled
(`UMKHNetTransportProvider_Steam`) Steam first tries a **direct** connection (NAT punch-through). If either
player's NAT does not allow it (Strict NAT, CGNAT / shared IPv4 at the ISP), the traffic goes through a **Valve
relay** (SDR). The relay can be far from both players, so ping can grow a lot (a playtest Italy–France on the
EOS relay measured ~300 ms, while each player had ~30 ms to game servers in the area).

The diagnostics answer one question per match: *was the connection direct or relayed, through which relay, and
how well does it deliver?* The NAT type is shown only when the backend reports it (EOS did, Steam does not:
`FMKHOnlineCapabilities::bReportsNatType`).

---

## Architecture

| Piece | Role |
|---|---|
| `UMKHNetDiagnosticsSubsystem` (`Online/`, GameInstance subsystem) | Refreshes the transport provider every second, samples ping, writes `LogMKHNet` (including every route change). `GetNetStats()` returns an `FMKHNetStatsSnapshot` (route, relay location, delivery quality, NAT types and whether the backend reports them). |
| `UMKHNetTransportProvider` (`Online/Backend/`) | Transport of the online backend (`OnlineBackends.md`): route of a connection, relay location, delivery quality, local NAT type. The base class is plain IP (always Direct, no NAT); `UMKHNetTransportProvider_Steam` (`MakhiaSteam`) is the active one, `_EOS` (`MakhiaEOS`, dormant) holds the EOS P2P code. |
| `MKHNetDiagnosticsTypes.h` | `EMKHNatType`, `EMKHConnectionPath`, `EMKHNetStatsMode`, `FMKHNetStatsSnapshot`. |
| `AMKHPlayerState::NatType` (replicated) | NAT type of each machine, so each player sees the opponent's. Carried over seamless travel by `CopyProperties`. |
| `AMKHPlayerController::Server_ReportNatType` | The local controller reports its NAT type at BeginPlay and on every change (`OnLocalNatTypeChanged`); the listen server writes it directly. |
| `UMKHNetStatsWidget` (`UI/HUD/HUDOverlay/`) → `W_NetStat` | The overlay, top left of `W_HUDOverlay`. C++ owns values and colours, the Blueprint owns the layout. |
| `UMKHGameUserSettings::NetStatsMode` | User setting *Network Stats*: Off / Compact / Detailed (General page, Interface section). |

### Steam (`UMKHNetTransportProvider_Steam`)

At creation it enables private and public ICE candidates (`k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable`).
The SteamSockets net driver keeps its connection handles private, so `FMKHSteamConnectionTracker` (private, fed by
`SteamNetConnectionStatusChangedCallback_t`) maps the remote SteamID64 to the Steam connection. From it:

- **Route**: `k_nSteamNetworkConnectionInfoFlags_Relayed` → Direct / Relayed.
- **Relay location**: the data center of the relay (`m_idPOPRelay`, e.g. "fra"), shown as "Relay (fra)".
- **Delivery quality**: `GetConnectionRealTimeStatus` (local and remote, shown in the Detailed overlay as *Delivery*).

The connection is matched to the game connection by the remote address `steam.<SteamID64>`. No NAT type.

### EOS registration (`UMKHNetTransportProvider_EOS`, dormant)

The subsystem ticks once per second (`FTSTicker`). When an EOS product user is signed in
(`EOS_Connect_GetLoggedInUserByIndex(0)`), it adds three notifications on the P2P interface for
every socket — *connection established* (carries `NetworkType`: Direct / Relayed), *interrupted*,
*closed* (carries the reason) — and starts `EOS_P2P_QueryNATType`. A sign-out or a different user
drops the notifications and registers again. EOS types never leave the `.cpp`: callbacks go through
`FMKHNetDiagnosticsEosCallbacks`.

### Matching the route to the game connection

SteamSockets connections have a remote address `steam.<SteamID64>` (EIK: `EOS:<ProductUserId>:<Socket>`). The
route of the game connection is the last one the transport reported for that user. A plain IP connection
(IpNetDriver fallback: LAN, PIE without sign-in) is always **Direct**. Until the transport reports, the route is
Unknown ("—").

### Measurements

Read from the game `UNetConnection` (client: `ServerConnection`; host: the first client
connection — duels are 1v1):

- **Ping**: `AvgLag` (average over the last second). Not `PlayerState` ping, which also counts the
  host's frame time.
- **Jitter**: `GetAverageJitterInMS()`.
- **Packet loss**: `GetIn/OutLossPercentage().GetAvgLossPercentage()`, rolling average in [0, 1].

On the host every value describes the connection to the client.

---

## Log (`LogMKHNet`)

Always on, independent of the overlay. Lines to look for after a playtest:

```
LogMKHNet: Route to steam.<SteamID64>:7777: Direct.
LogMKHNet: Route to steam.<SteamID64>:7777: Relayed via fra.
LogMKHNet: Connection to steam.<SteamID64> ended: ping avg 287 ms, max 340 ms over 412 s, route Relayed.
```
(The EOS transport also logs `Listening to P2P connections of <puid>`, `Local NAT type: Moderate` and
`P2P connection established with <puid>: Relayed (local NAT Moderate)`. Steam P2P details: `LogSteamSocketsAPI`.)

Testers find the log in `%LOCALAPPDATA%/Makhia/Saved/Logs/Makhia.log` (packaged game).

---

## Overlay

- **Compact**: four signal bars, ping, a **HOST** / **CLIENT** chip (gold on the host, dark on
  the client) and a red **RELAY** chip only while the traffic is relayed.
- **Detailed**: compact row plus Jitter, Packet loss (in / out), Connection (Direct / Relay with the
  relay location, e.g. "Relay (fra)"), Delivery (transport quality, when the backend measures it), Your NAT
  and Opponent NAT (hidden when the backend does not report NAT types).
- Hidden when the mode is Off and whenever there is no connection to another player (vs bot,
  offline, lobby).
- Thresholds and colours are `EditDefaultsOnly` on `W_NetStat`: 4 bars up to 50 ms, green up to
  80 ms, yellow up to 150 ms, red above. Strict NAT is shown in yellow.
- Refreshed by a 0.5 s timer, not by tick: a collapsed widget does not tick and could never show
  itself again.
- Console variable `mkh.NetStats` overrides the setting: `-1` user setting (default), `0` off,
  `1` compact, `2` detailed.

---

## Testing

PIE cannot test it: editor processes never run Steam, so PIE uses `IpNetDriver` (always Direct, no
relay). Test with two packaged (Development) builds, one per Steam account, on two machines or on a PC and the
test VM (`Handoff/MultiplayerTestVM_Prompt.md`), both signed in:

1. Settings → General → Network Stats → Detailed on both.
2. Start an online duel: the host shows **HOST**, the other **CLIENT**; ping, route (and relay location) and
   delivery quality fill in within a few seconds (`mkh.NetStats 2` forces the detailed view).
3. After the match, check `LogMKHNet` in both logs.

---

## If the route is Relayed

Not a code problem. In order of cost:

1. The player with the more open NAT hosts (with EOS the NAT types were shown; with Steam try swapping host and guest).
2. UPnP on the router, or a public / dedicated IPv4 from the ISP (CGNAT is common on recent fibre).
3. Dedicated servers in a European data centre: no NAT, no relay, fair latency for both players.
