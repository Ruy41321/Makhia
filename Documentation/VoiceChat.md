# Voice Chat

Voice between the players of a lobby and of a match, on the native VoIP of Unreal Engine: capture and codec of the
engine, packets on the voice channel of the game connection, relayed by the listen server (SteamSockets P2P/relay
with Steam, IP with the LAN backend). Decisions and history: `Handoff/VoiceChatHandoff.md`.

## Table of Contents

1. [Who hears whom](#who-hears-whom)
2. [Layers](#layers)
3. [Server rule and enforcement](#server-rule-and-enforcement)
4. [Local voice state](#local-voice-state)
5. [Devices and volumes](#devices-and-volumes)
6. [Settings](#settings)
7. [UI](#ui)
8. [Configuration and assets](#configuration-and-assets)
9. [Engine pitfalls](#engine-pitfalls)
10. [Testing](#testing)

---

## Who hears whom

Two players A and B hear each other only when **all** of these hold (`FMKHVoicePermissionPolicy::Evaluate`):

1. neither is the bot and the match is not against the bot (bots have no player controller, so no voice component;
   an offline match is standalone and the voice never connects);
2. both turned the voice chat **on** (master switch) and both machines already sent their choices;
3. neither muted the other (saved mutes, blocked platform users included);
4. the pair is **known** (same lobby party, or platform friends) **or both** allow "voice with non-friends" (default off).

The decision is symmetric. The reason seen by each player (`EMKHVoiceBlockReason`) tells the panel why the voice is
closed, never who muted whom (`Unavailable`).

- **Party** = a player who joined the host lobby through an invite or a friend's "Join"
  (`UMKHSessionSubsystem` → `UMKHLobbySubsystem::SetJoinedThroughInvite`, sent as `FMKHVoicePreferences::bJoinedAsParty`).
  The host accepts the claim only when that player **arrived while the host lobby was passive** (lobby level, not
  matchmaking: a matchmaking lobby is public, anybody could claim to be a friend). Accepted ids live in
  `UMKHLobbySubsystem::VoicePartyMemberIds`, kept through the travels of the match and forgotten with the lobby
  (`HandleLobbyHosted`, `HandleSessionLeft`, `ResetLobbyState`).
- **Friends**: in a 1v1 the listen server is one of the two players, so the host checks its **own** platform friend
  list (`UMKHFriendsSubsystem::IsFriend`), never a client's word.
- **Blocked users** (`UMKHSocialProvider::IsBlocked`, Steam `k_EFriendFlagBlocked`) are always in the muted list the
  machine sends.

## Layers

```
UI: W_VoicePanel (game menu, friends page)   W_VoiceHUD (match HUD, lobby overlay)   nameplate TalkingIcon
    W_VoiceSettingsSection (Audio settings page)
        │ only UMKHVoiceChatSubsystem / UMKHAudioDeviceSubsystem
        v
UMKHVoiceChatSubsystem (GameInstance, LogMKHVoice)      UMKHAudioDeviceSubsystem (GameInstance, LogMKHAudio)
  local state, players, settings applied                  output devices, game output swap, microphone name
        │                       │ choices up / decisions down
        │                       v
        │           UMKHVoicePlayerComponent (on AMKHLobbyPlayerController and AMKHPlayerController)
        │                       │ server
        │                       v
        │           UMKHVoiceAuthoritySubsystem (World, server) ── FMKHVoicePermissionPolicy (pure)
        │                         GameplayMutePlayer / GameplayUnmutePlayer on the player controllers
        v
UMKHVoiceProvider of the online backend (UMKHOnlineBackend::GetVoiceProviderClass)
  └─ UMKHVoiceProvider_OSS (Null and Steam): IOnlineVoice of the default online subsystem
```

- Nothing outside the provider touches `IOnlineVoice`. The UI reads `FMKHOnlineCapabilities::bSupportsVoiceChat`,
  `bSupportsVoiceOutputDevice`, `bSupportsVoiceInputDevice` (false: the engine always records from the Windows
  default device), never the backend name.
- One provider class serves both backends: `FVoiceEngineSteam` derives from the engine `FVoiceEngineImpl` and only adds
  the Steam "in game voice speaking" flag.
- The voice interface of the **default** online subsystem is used (the one the engine net code feeds). It ticks only
  while that subsystem has a session, so the voice works in hosted/joined lobbies and matches, never offline.

## Server rule and enforcement

`UMKHVoiceAuthoritySubsystem` (world subsystem, server worlds only):

- Every voice component registers on the server at `BeginPlay`; the arrival records `bArrivedInPassiveLobby`.
- Decides every pair at each registration, unregistration, new choices (`ServerSetVoicePreferences`, validated:
  at most 64 mutes, ids up to 128 chars), game mode PostLogin and **once a second** (friend list, late player state ids).
- Applies the decision on the listener's player controller mute list: `GameplayMutePlayer` (the server stops relaying
  and the client mutes the talker locally through `ClientMutePlayer`) or `GameplayUnmutePlayer`. For the host's own
  controller it also calls `ServerUnmutePlayer`, because the `ClientMutePlayer` echo added a `Muted` reason to the same
  list and would keep the host deaf (engine quirk). Hiding a row on the client would not be enough: the server never
  relays voice between players who may not hear each other, and the host never plays it.
- Writes the decision for every other player into `UMKHVoicePlayerComponent::PeerStatuses` (replicated to the owner
  only: a player controller replicates to its owner alone). Changes are logged ("Voice server: X hears / does not hear Y").

## Local voice state

`UMKHVoiceChatSubsystem`, ticking every 0.1 s:

- **Channel**: `None` (standalone world, no local voice component, alone), `Lobby` (lobby level), `Match`.
- **Players** (`GetTalkers`): the local player first, then every human player state with a unique id; for each one the
  server reason, the saved mute / platform block, and the talking state. New remote players are registered with the
  provider; their local playback is muted whenever the server closed the voice or the player muted them (double safety
  with the server filter).
- **Choices sent** (`FMKHVoicePreferences`): master switch, non-friends consent, party claim, the saved mutes of the
  players present plus the blocked ones. Sent by the component only when they change (and retried until the
  connection exists).
- **Microphone** (`ShouldTransmit`): open only with a channel, at least one other player the server opened the voice
  with, voice chat on, mode not *Disabled*, microphone not muted for the session, and *Open mic* or the push-to-talk
  key held. Nothing opens the microphone in a match against the bot.
- **Push-to-talk**: `IA_PushToTalk` (`UMKHOnlineSettings::PushToTalkAction`), bound by `UMKHVoicePlayerComponent::SetupInput`
  from both player controllers' `SetupInputComponent`. Keyboard **T**; controller **D-pad down** alone (no chord;
  Sheathe Weapon moved to LB + D-pad down). Mapped in `IMC_BaseMapping`, `IMC_UIOpenMapping` and `IMC_Dead`;
  remappable on the Keyboard and Controller pages. Ready Up (`IA_SkipInit`) was moved from T to P for it.
- The session microphone mute (`SetMicMuted`) never changes the saved mode.

Console (development): `MKH.Voice.Dump`, `MKH.Voice.PTT 0|1`, `MKH.Voice.Set Enabled|NonFriends|MicMuted 0|1`,
`MKH.Voice.Set Mode Open|PTT|Disabled`, `MKH.Voice.Set OutputVolume <0..1>`, `MKH.Voice.Set Mute <Id> 0|1`,
`MKH.Voice.Debug.AddParty <Id>` (host only, not in Shipping: the LAN backend has no invites),
`MKH.Audio.Devices`, `MKH.Audio.SetGameOutput <index|-1>`, `MKH.Audio.SetVoiceOutput <index|-1>`.

## Devices and volumes

- **Game output device**: `UMKHAudioDeviceSubsystem` reads the devices (`UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices`)
  at the first level load and whenever the Audio page opens, and swaps the mixer (`SwapAudioOutputDevice`) to the saved
  device id, or to the system default (empty id) when the saved device is not connected. The saved id is kept, so the
  device comes back when it is plugged in again.
- **Voice output device**: "Same as game" or a device of its own. Either way the voice plays on a stream of its own,
  never in the game mix: see [Voice output stream](#voice-output-stream).
- **Microphone**: always the Windows default recording device (the engine creates its capture with an empty name and
  keeps it private). The name shown comes from `Audio::FAudioCapture::GetCaptureDeviceInfo` (default device).
- **Voice volume**: the gain of the voice stream, `slider × 4` (`UMKHVoiceProvider_OSS::MaxOutputGain`), samples
  clamped to [-1, 1]. In the 2-PC test (2026-10-21) the engine voice at gain 1 was far too quiet even at 100%.
  The game mix path (`VoiPSoundClass = SC_Voice`, submix `SBM_Voice`, `CB_Voice`) only plays when the stream cannot
  open; the master volume does **not** reach it: `CB_Master` modulates the children `SBM_Music`, `SBM_SFX`, `SBM_Crowd`
  (each with its own bus too), no longer `SBM_Main`.
- **Microphone volume and activation**: `voice.MicInputGain = 2 × slider` (the middle keeps the captured level);
  `voice.SilenceDetectionThreshold` and `voice.MicNoiseGateThreshold` = `0.1 × slider` in open mic (default slider
  0.2 = 0.02), 0.01 in push-to-talk (everything said while the key is held is sent). The engine default 0.08 (the old
  `0.4 × slider`) cut normal speech in the 2-PC test.
- **Microphone level meter**: a separate capture (`IVoiceCapture` of the Voice module, `StartMicrophoneTest`) open while
  the Audio page is built; nothing is sent. Its level uses the same scale as the activation slider.

### Voice output stream

- The provider always plays the remote talkers with a stream of its own, `FMKHVoiceOutputEndpoint`
  (`Private/Online/Backend/`): a platform output interface of the audio module (WASAPI) fed by
  `IOnlineVoice::GetRemoteTalkerOutput()` (the mix of all remote talkers), and sets `voice.MuteAudioEngineOutput` so
  the voice leaves the game mix. Device: the separate voice device, else the game's
  (`UMKHAudioDeviceSubsystem::ResolveVoicePlaybackDeviceName`); with the game on "System default" the stream opens on
  the Windows default and is reopened on `UAudioDeviceNotificationSubsystem::DefaultRenderDeviceChangedNative`
  (Console role, game thread).
- **Why a stream of its own**: Unreal multiplies the whole game mix by `[Audio] UnfocusedVolumeMultiplier` (0) when the
  window loses the focus (`FApp::GetVolumeMultiplier` in `FAudioDevice::Update`), so after Alt+Tab the others were
  silent. The stream is not part of the mix: the voice keeps playing while the game sounds stay muted. The voice synth
  components keep generating (they feed the patch) even with the mix at volume 0.
- If the stream cannot open, `voice.MuteAudioEngineOutput` stays 0 and the voice plays in the game mix (muted when
  unfocused, `CB_Voice` volume).
- Diagnostics: `MKH.Voice.Dump` logs `output peak`, the loudest sample the stream played since the previous dump
  (-1 without a stream).
- **Never use the engine's `PatchRemoteTalkerOutputToEndpoint` / `DisconnectAllEndpoints`.** Its `FVoiceEndpoint`
  calls `InitializeHardware`, which registers the WASAPI interface with the Windows device notifications
  (`FWindowsMMNotificationClient`), and its destructor never unregisters it. After the endpoint is destroyed the next
  Windows default device change (or a device plugged/unplugged) calls the freed object: access violation in
  `WindowsMMDeviceEnumeration` (game and editor). It showed up when the game output was a specific device equal to
  the voice device: the endpoint was destroyed because the voice falls back to the game mix. `FMKHVoiceOutputEndpoint`
  calls `TeardownHardware` and `UnregisterDeviceChangedListener` before it is freed.
- The endpoint never follows a device change by itself (`SetIsListeningForDeviceEvents(false)`): the provider opens a
  new one. The device is matched by **name** (`GetIndexForDevice`, the default output when empty or not found), so the
  saved ids are resolved to names.

## Settings

Stored in `UMKHGameUserSettings` (`GameUserSettings.ini`), applied at start-up and live:

| Field | Default | Shown as |
|---|---|---|
| `bVoiceChatEnabled` | on | Voice Chat (also the headset button of the panel) |
| `VoiceMicMode` | Push-to-talk | Microphone Mode: Open Mic / Push-to-Talk / Disabled (Listen Only) |
| `bVoiceAllowNonFriends` | off | Voice with Non-Friends |
| `VoiceInputVolume` | 0.5 | Microphone Volume |
| `VoiceActivationThreshold` | 0.2 (engine 0.08) | Open Mic Sensitivity, and the mark of the level meter |
| `VoiceOutputVolume` | 1 | Voice Volume |
| `VoiceOutputDeviceId` | empty (same as game) | Voice Output Device (capability) |
| `GameOutputDeviceId` | empty (system default) | Game Output Device |
| `MutedVoicePlayerIds` | empty | mute buttons of the panel (kept between sessions, not reset by Defaults) |

The rows are steppers, like the rest of the settings (no combo box: pad-friendly, same row height).

## UI

- **Audio page** (`W_AudioSettings`): `W_VoiceSettingsSection` (`UMKHVoiceSettingsSectionWidget`) under the volume
  sliders builds two sections from the shared rows (`W_OptionRow`, `W_SliderRow`, `W_KeybindCategory` headers) and the
  level meter `W_VoiceMicMeter` (`UMKHVoiceMicMeterWidget`). Everything applies and saves at once.
- **Game menu, friends page**: `W_VoicePanel` (`UMKHVoicePanelWidget`) at the top of `W_FriendList`, so it follows the
  friends page: title, channel, microphone state (with the push-to-talk `W_KeyCap`), icon buttons (headset on/off,
  microphone mute, collapse arrow, `BS_IconButton`; the headset with voice chat off and the microphone while muted switch
  to the red `BS_IconButtonOff`, `ToggleOnStyle` / `ToggleOffStyle` on `W_VoicePanel`) and one `W_VoiceTalkerEntry` (`UMKHVoiceTalkerEntryWidget`)
  per player: avatar, name, "You" on the local row, speaker shown while talking (steady, no pulse), mute button, "voice unavailable"
  icon. The reason the voice is closed is on the icon tooltip and on the mute button tooltip (the pad shows only the
  focused widget's tooltip).
- **HUD**: `W_VoiceHUD` (`UMKHVoiceHUDWidget`) in `W_HUDOverlay` and `W_LobbyOverlay`, left centre: the transmitting
  microphone with the push-to-talk key, and the other players talking now (`W_VoiceHUDTalker`). Hidden without a channel.
- **Lobby nameplates** (`W_PlayerStatus`, `UMKHLobbyNameplateWidget::TalkingIcon`): speaker next to the ready chip while
  the player talks.
- Icons: Google Material Symbols Rounded (Apache 2.0, credited in `DA_Credits`), white on transparent at 64/128 px in
  `Content/Makhia/UI/Assets/CommonIcons` (`IconHeadset`, `IconHeadsetOff`, `IconMic`, `IconMicOff`, `IconSpeaker`,
  `IconSpeakerOff`, `IconVoiceUnavailable`, `IconChevronUp`, `IconChevronDown`); sources and licence in
  `SourceArt/Miscellaneous/VoiceIcons`.

## Configuration and assets

- `DefaultEngine.ini`: `[OnlineSubsystem] bHasVoiceEnabled=true`, `[Voice] bEnabled=true`,
  `[/Script/Engine.AudioSettings] VoiPSoundClass=SC_Voice`.
- `DefaultGame.ini`: `[/Script/Engine.GameSession] bRequiresPushToTalk=true` (with false the engine starts every
  player's networked voice at login, `ClientEnableNetworkVoice(true)`); `MKHAudioSettings.VoiceVolumeControlBus`;
  `MKHOnlineSettings.PushToTalkAction`.
- Assets: `CB_Voice`, `SBM_Voice`, `SC_Voice` (`Content/Makhia/Sound`), `IA_PushToTalk`, the widgets above.

## Engine pitfalls

- `IOnlineVoice::IsRemotePlayerTalking` stays true while a talker buffer exists, and `IsLocalPlayerTalking` never
  reports user 0 (`LocalUserNum << 1`): the talking state comes from `OnPlayerTalkingStateChanged`. That event toggles
  between words, so `UMKHVoiceChatSubsystem::HoldTalkingStates` keeps a talker "talking" 0.5 s after the last report:
  the icons stay on while someone speaks and disappear when they stop.
- `FOnlineVoiceImpl::RegisterLocalTalker` calls `StartNetworkedVoice`, and the Null/Steam sessions register the local
  talker on create/join: the provider closes the networked voice again after every create/join unless it transmits.
- **Voice playbacks left registered at world cleanup (crash entering a match).** `FRemoteTalkerDataImpl::Cleanup`
  (talker unregistered) only stops the playback, and the other playbacks are reset only on the next
  `PostLoadMapWithWorld`: on the seamless travel lobby -> arena the lobby world was cleaned with
  `VoipListenerSynthComponent`s still registered: ensure `Errors.Len() == 0` (`ValidateComponentsRegistration`), ensure
  in `RendererScene.cpp`, then an access violation (2-PC test, 2026-10-21). `UMKHVoiceProvider_OSS::ReleaseWorldPlaybacks`
  (`FWorldDelegates::OnWorldCleanup`, before the validation) unregisters them; the engine makes new ones in the new world.
- **Dropped playbacks stall the remote talkers' mix.** The engine drops a talker's playback
  (`UVoipListenerSynthComponent`) after 0.4 s of silence and creates a new one on the next packet; the dropped one
  stays an input of `AllRemoteTalkerAudio` until garbage collection, and that mix forwards only what every input has
  pushed. Result (2-PC test): the voice stream (and the engine's own endpoints) went silent after the first pause.
  `UMKHVoiceProvider_OSS::DetachDroppedPlaybacks` (every tick while the stream is open) moves each unregistered
  playback to a sink mix nobody reads, through `ConnectToSplitter` (the only public hook).
- `MuteRemoteTalker` does nothing for an unregistered talker: the subsystem registers every remote player and applies
  its own playback mute after the registration.
- Long device names in the steppers: `W_OptionRow` cuts them with an ellipsis, shows them in full as tooltip and scrolls
  them while the row is hovered or focused (`SettingsArchitecture.md` → `UMKHOptionRowWidget`).
- The voice drops during travels (lobby → arena → lobby) and comes back on arrival, under the loading screen.

## Testing

- **LAN, two Standalone processes** (`Documentation/OnlineBackends.md` → Testing) with
  `Extra/OnlineTests/voice_test_driver.py "<sec>=<command>|..."` next to `session_test_driver.py`: it logs
  `[MKHVoiceTest]` lines (channel, transmission, players with reason and talking state) and runs console commands at
  given times after the other player appears (`{other}` = their id). Tip: `voice.SilenceDetectionThreshold 0` makes a
  quiet room send voice. The two processes share `GameUserSettings.ini`: reset the voice settings in the schedule.
- **UI capture**: `Extra/OnlineTests/voice_ui_driver.py <StartDelay> <AudioPageIndex>` opens the game menu and the
  Audio page and takes `shot showui` screenshots (`HighResShot` leaves the UI out).
- **Steam PC + VM** (real voice, invites, friends, blocked users, separate output device): manual, see the handoff.
