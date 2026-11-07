# Audio System Architecture

## Table of Contents

1. [Overview](#overview)
2. [Directory Structure](#directory-structure)
3. [Design Principles](#design-principles)
4. [Signal Flow](#signal-flow)
5. [Content Assets (Editor-Created)](#content-assets-editor-created)
6. [Configuration](#configuration)
7. [C++ Classes](#c-classes)
   - [UMKHAudioSettings](#umkhaudiosettings)
   - [UMKHGameUserSettings (Audio Extension)](#umkhgameusersettings-audio-extension)
   - [UMKHAudioWorldSubsystem](#umkhaudioworldsubsystem)
   - [UMKHMusicSubsystem](#umkhmusicsubsystem)
   - [UMKHAudioSettingsWidget](#umkhaudiosettingswidget)
8. [Sound Effects](#sound-effects)
9. [Runtime Sequences](#runtime-sequences)
10. [How-To Guides](#how-to-guides)
11. [Gotchas & Troubleshooting](#gotchas--troubleshooting)
12. [Roadmap](#roadmap)

---

## Overview

Makhia routes all audio through an **Audio Modulation** mixing graph whose per-category
volumes are driven by user settings and applied automatically for every gameplay world.
Background music is owned by a session-scoped subsystem that survives level travel and
crossfades between tracks.

The system is deliberately **data-driven and decoupled**: gameplay code never hardcodes
volumes or asset paths. A sound's loudness is decided entirely by the submix it is routed
to; the submix volume is decided by a Control Bus; the Control Bus value is decided by the
user's saved settings. No `SetVolumeMultiplier` calls are scattered through gameplay logic.

| Custom Class | Engine Base | Purpose |
|---|---|---|
| `UMKHAudioSettings` | `UDeveloperSettings` | Project-wide audio config: Control Bus references, level→music map, fade defaults |
| `UMKHGameUserSettings` | `UGameUserSettings` | Persisted user volumes (Master/Music/SFX/Crowd) + push to Control Buses |
| `UMKHAudioWorldSubsystem` | `UWorldSubsystem` | Re-applies volumes and starts level music on world begin play |
| `UMKHMusicSubsystem` | `UGameInstanceSubsystem` | Owns persistent music playback, crossfades, seamless travel |
| `UMKHAudioSettingsWidget` | `UMKHSettingsPageWidget` | Settings panel: four volume sliders, live apply + save (hosted by the settings shell, see `SettingsArchitecture.md`) |
| `UMKHCrowdAudioComponent` | `UActorComponent` | Arena crowd: ambience loop, excitement-driven cheering loop, reaction one-shots (on `AMKHCrowdDirector`) |
| `UMKHSoundEffectSubsystem` | `UGameInstanceSubsystem` | Plays every gameplay and UI sound effect by tag (`SFX.*`) from `DT_SoundEffects` ([Sound Effects](#sound-effects)) |
| `UMKHAnimNotify_PlaySound` | `UAnimNotify` | "Animation" route: plays a sound tag on a frame of a montage or animation |
| `UMKHGameplayCueNotify_Burst` | `UGameplayCueNotify_Burst` | "Visual effect" route: a burst cue whose sound is a sound tag |
| `UMKHSoundEventComponent` | `UActorComponent` | "Event" route: server gameplay events → sound on every machine (on `ACharacterBase`) |

Logging for this layer uses the dedicated `LogMKHAudio` category (`MKHLogChannels.h`).

The system depends on the **AudioModulation** plugin (enabled in `Makhia.uproject`) and the
`AudioModulation`, `UMG`, `Slate`, `SlateCore`, and `DeveloperSettings` build modules
(`Makhia.Build.cs`).

---

## Directory Structure

```
Source/Makhia/
├── Public/Audio/
│   ├── MKHAudioSettings.h           # UDeveloperSettings: bus refs, level→music map, fade default
│   ├── MKHAudioWorldSubsystem.h     # Per-world bootstrap (volumes + level music)
│   ├── MKHCrowdAudioComponent.h     # Arena crowd loops + reactions
│   ├── MKHMusicSubsystem.h          # Session-scoped music player
│   ├── MKHSoundEffectTypes.h        # FMKHSoundEffectRow (DT_SoundEffects row) + enums
│   ├── MKHSoundEffectSubsystem.h    # Plays sound effects by tag
│   ├── MKHAnimNotify_PlaySound.h    # "Animation" route
│   └── MKHSoundEventComponent.h     # "Event" route (on ACharacterBase)
├── Public/AbilitySystem/Cue/
│   └── MKHGameplayCueNotify_Burst.h # "Visual effect" route
├── Public/GameUserSettings/
│   └── MKHGameUserSettings.h        # User volumes (extended in this session)
├── Public/UI/HUD/Settings/
│   └── MKHAudioSettingsWidget.h     # Volume sliders panel
├── Private/Audio/
│   ├── MKHAudioSettings.cpp
│   ├── MKHAudioWorldSubsystem.cpp
│   └── MKHMusicSubsystem.cpp
├── Private/GameUserSettings/
│   └── MKHGameUserSettings.cpp
└── Private/UI/HUD/Settings/
    └── MKHAudioSettingsWidget.cpp

Content/Makhia/Sound/
├── Modulation/
│   ├── MP_Volume       # SoundModulationParameterVolume
│   ├── CB_Master       # Control Bus (Parameter = MP_Volume)
│   ├── CB_Music        # Control Bus (Parameter = MP_Volume)
│   ├── CB_SFX          # Control Bus (Parameter = MP_Volume)
│   └── CB_Crowd        # Control Bus (Parameter = MP_Volume)
├── Submixes/
│   ├── SBM_Main        # Master submix (outputs to hardware) — modulated by CB_Master
│   ├── SBM_Music       # Child of SBM_Main — modulated by CB_Music
│   ├── SBM_SFX         # Child of SBM_Main — modulated by CB_SFX
│   └── SBM_Crowd       # Child of SBM_Main — modulated by CB_Crowd (arena crowd)
├── Theme/
│   └── MainThemeLooped # Looping background track, Submix = SBM_Music
└── SFX/
    ├── DT_SoundEffects # Every sound effect: one row per SFX.* tag
    └── Settings/
        ├── SA_SoundEffects # Default attenuation of world sound effects
        ├── SCON_Voice      # One voice per character (stop oldest)
        └── SCON_Footstep   # Two footsteps per character (stop oldest)
```

---

## Design Principles

1. **A sound never knows how loud it should be.** Loudness is a property of the submix
   the sound is routed to, not of the sound instance. This keeps mixing centralized.

2. **User volume = one persistent Control Bus mix.** Volumes are pushed to Control Buses
   via a global bus mix. This leaves room to *stack* additional, temporary mixes on top
   later (music ducking under dialogue, pause menu low-pass, low-health filter) without
   ever disturbing the user's saved values.

3. **Perceptual sliders for free.** The slider value in `[0, 1]` is passed straight to the
   Control Bus. The `Volume` modulation parameter converts it to decibels internally
   (normalized `1.0` → `0 dB`, `0.0` → `MinVolume` = `-100 dB`), so the fader feels
   logarithmic without any manual `dB` math in game code.

4. **Music is client-local, never replicated.** Music selection is triggered on world
   begin play (which runs on every client), driven by a data map, and owned by the
   GameInstance. The server-only GameMode is intentionally *not* involved.

5. **Settings model has a single writer.** The audio settings widget writes directly to
   `UMKHGameUserSettings` without a WidgetController — the Model→View observer indirection
   used for gameplay data adds nothing for a global, single-writer settings object.

---

## Signal Flow

```
MainThemeLooped ─► SBM_Music ─► SBM_Main ─► audio hardware (or the Game Output Device)
                        ▲
                        └── CB_Music + CB_Master  (user Music and Master sliders)

unrouted sounds ─► SBM_SFX ─► SBM_Main ─► audio hardware
                        ▲
                        └── CB_SFX + CB_Master    (user SFX and Master sliders)

crowd sounds ─► SBM_Crowd ─► SBM_Main ─► audio hardware
                        ▲
                        └── CB_Crowd + CB_Master  (user Crowd and Master sliders)

voice (VoIP) ─► SC_Voice ─► SBM_Voice ─► SBM_Main ─► audio hardware
                        ▲
                        └── CB_Voice              (Voice Volume only: the master does not reach the voice)
```

Since the voice chat (2026-10-20, `VoiceChat.md`) the Master bus no longer modulates `SBM_Main`: it modulates every
child submix except `SBM_Voice` (a modulation destination takes a set of modulators), so the voice volume stays
independent of the master, as in Discord. A new child submix must add `CB_Master` to its Output Volume Modulation.
The voice of the other players can also play on a device of its own (voice endpoint, `VoiceChat.md`); the game output
device is chosen in the Audio page (`UMKHAudioDeviceSubsystem`).

- `SBM_Main` is declared as the engine **Master Submix**, i.e. the root that outputs to the
  audio device. Its output volume is modulated by `CB_Master`, so the Master slider affects
  everything.
- `SBM_Music`, `SBM_SFX` and `SBM_Crowd` are children of `SBM_Main`, each modulated by its own bus. The crowd has its
  own submix so it is mixed apart from the SFX (not under the SFX slider) and has its own **Crowd** slider.
- `SBM_SFX` is the **Base Default Submix**: any sound that does not explicitly set a submix
  is automatically routed here, so future SFX fall under the SFX slider with zero wiring.

---

## Content Assets (Editor-Created)

These are not code — they are assets an author creates once in the editor. The C++/config
layer references them by the exact paths listed in [Directory Structure](#directory-structure).

| Asset | Type | Key settings |
|---|---|---|
| `MP_Volume` | Sound Modulation Parameter | Class `SoundModulationParameterVolume`; defaults (`MinVolume = -100 dB`, normalized default `1.0`) |
| `CB_Master` / `CB_Music` / `CB_SFX` / `CB_Crowd` / `CB_Voice` | Sound Control Bus | `Parameter = MP_Volume` |
| `SBM_Main` | Sound Submix | `Parent = None`; no Output Volume Modulation |
| `SBM_Music` | Sound Submix | `Parent = SBM_Main`; Output Volume Modulation → `CB_Music`, `CB_Master` |
| `SBM_SFX` | Sound Submix | `Parent = SBM_Main`; Output Volume Modulation → `CB_SFX`, `CB_Master` |
| `SBM_Crowd` | Sound Submix | `Parent = SBM_Main`; Output Volume Modulation → `CB_Crowd`, `CB_Master` |
| `SBM_Voice` | Sound Submix | `Parent = SBM_Main`; Output Volume Modulation → `CB_Voice` |
| `SC_Voice` | Sound Class | `Default Submix = SBM_Voice`; `VoiPSoundClass` of `[/Script/Engine.AudioSettings]` |
| `MainThemeLooped` | Sound Wave (looping) | `Submix = SBM_Music` |

---

## Configuration

### `Config/DefaultEngine.ini`

```ini
[/Script/Engine.AudioSettings]
MasterSubmix=/Game/Makhia/Sound/Submixes/SBM_Main.SBM_Main
BaseDefaultSubmix=/Game/Makhia/Sound/Submixes/SBM_SFX.SBM_SFX
```

- `MasterSubmix` — the root submix that outputs to hardware. **Must** be set, otherwise the
  custom submix graph forms a cycle (see [Gotchas](#gotchas--troubleshooting)).
- `BaseDefaultSubmix` — implicit destination for sounds with no submix set.

### `Config/DefaultGame.ini`

```ini
[/Script/Makhia.MKHAudioSettings]
MasterVolumeControlBus=/Game/Makhia/Sound/Modulation/CB_Master.CB_Master
MusicVolumeControlBus=/Game/Makhia/Sound/Modulation/CB_Music.CB_Music
SFXVolumeControlBus=/Game/Makhia/Sound/Modulation/CB_SFX.CB_SFX
CrowdVolumeControlBus=/Game/Makhia/Sound/Modulation/CB_Crowd.CB_Crowd
SoundEffectsTable=/Game/Makhia/Sound/SFX/DT_SoundEffects.DT_SoundEffects
DefaultSoundEffectAttenuation=/Game/Makhia/Sound/SFX/Settings/SA_SoundEffects.SA_SoundEffects
LevelMusicMap=(("Makhion", "/Game/Makhia/Sound/Theme/MainThemeLooped.MainThemeLooped"),("Lvl_ThirdPerson", "/Game/Makhia/Sound/Theme/MainThemeLooped.MainThemeLooped"))
DefaultMusicFadeSeconds=2.0
```

Editable in-editor under **Project Settings → Game → Makhia Audio**.

User volumes persist to `Saved/Config/.../GameUserSettings.ini` (written by `SaveSettings()`).

---

## C++ Classes

### UMKHAudioSettings

`UDeveloperSettings` (config `Game`, section *Makhia Audio*). Holds the design-time audio
configuration so gameplay code reads references instead of hardcoding asset paths.

| Member | Type | Purpose |
|---|---|---|
| `MasterVolumeControlBus` | `TSoftObjectPtr<USoundControlBus>` | Bus driven by the Master slider |
| `MusicVolumeControlBus` | `TSoftObjectPtr<USoundControlBus>` | Bus driven by the Music slider |
| `SFXVolumeControlBus` | `TSoftObjectPtr<USoundControlBus>` | Bus driven by the SFX slider |
| `CrowdVolumeControlBus` | `TSoftObjectPtr<USoundControlBus>` | Bus driven by the Crowd slider |
| `LevelMusicMap` | `TMap<FString, TSoftObjectPtr<USoundBase>>` | Short level name → music track |
| `DefaultMusicFadeSeconds` | `float` | Default fade for fade-in/out/crossfade |
| `CrowdAmbienceLoop`, `CrowdExcitedLoop` | `TSoftObjectPtr<USoundBase>` | Crowd loops (*Crowd → Loops*) |
| `CrowdDuelStartSound`, `CrowdHitSound`, `CrowdSuddenDeathSound`, `CrowdDeathSound`, `CrowdMatchOverSound` | `TSoftObjectPtr<USoundBase>` | Crowd reaction one-shots (*Crowd → Reactions*) |
| `CrowdSubmix` | `TSoftObjectPtr<USoundSubmixBase>` | `SBM_Crowd`; a crowd sound with another Base Submix logs a warning (*Crowd → Mix*) |
| `CrowdAmbienceVolume`, `CrowdExcitedLoopMaxVolume`, `CrowdReactionVolume`, `CrowdLoopFadeInSeconds`, `CrowdHitSoundCooldownSeconds` | `float` | Crowd mix (*Crowd → Mix*) |
| `GateRaiseSound`, `GateRaisedSound`, `GateLowerSound`, `GateSlamSound` | `TSoftObjectPtr<USoundBase>` | Portcullis sounds (*Arena Gates*), played by `AMKHArenaGate` |
| `GateSoundAttenuation` | `TSoftObjectPtr<USoundAttenuation>` | Distance falloff of the gate sounds (empty = each sound's own) |
| `GateSoundVolume` | `float` | Gate sounds volume (*Arena Gates*) |
| `SoundEffectsTable` | `TSoftObjectPtr<UDataTable>` | `DT_SoundEffects`, every sound effect (*Sound Effects*) |
| `DefaultSoundEffectAttenuation` | `TSoftObjectPtr<USoundAttenuation>` | `SA_SoundEffects`, falloff of world sound effects whose row sets none |
| `Get()` | `static const UMKHAudioSettings*` | CDO accessor |

**Every audio file is reached from here** (one place): music per level, crowd loops and reactions, arena gate sounds
as slots, and every gameplay / UI sound effect through `DT_SoundEffects` ([Sound Effects](#sound-effects)).
A new sound effect is a row of that table; a new music or ambience family adds a category to this class.

### UMKHCrowdAudioComponent

Component of `AMKHCrowdDirector` (the arena actor that turns duel events into crowd excitement, see
`MatchFlowArchitecture.md`, "Crowd reactions"). Local to each peer, non-spatialized (the crowd surrounds the
player), routed to `SBM_Crowd`. The routing is the **Base Submix of each sound asset** (the engine takes the submix
from the sound, not from the component); a crowd sound routed elsewhere logs a `LogMKHAudio` warning when it plays.

| Sound | When | Volume |
|---|---|---|
| `CrowdAmbienceLoop` | `BeginPlay` to `EndPlay`, fades in over `CrowdLoopFadeInSeconds` | `CrowdAmbienceVolume` |
| `CrowdExcitedLoop` | Same, always running, same fade-in | crowd excitement × `CrowdExcitedLoopMaxVolume` (the director's `MinExcitement` keeps it audible as the base crowd sound; floor 0.001: a loop held at 0 may be culled as silent) |
| `CrowdDuelStartSound` | Each duel start | `CrowdReactionVolume` |
| `CrowdHitSound` | A hit that excites the crowd (≥ 4 % of max health), at most every `CrowdHitSoundCooldownSeconds` | 60–100 % of `CrowdReactionVolume` by hit size |
| `CrowdSuddenDeathSound`, `CrowdDeathSound`, `CrowdMatchOverSound` | Sudden death, a death, match decided | `CrowdReactionVolume` |

An empty slot is silent (no warning): the system runs before the files exist. Loops are loaded at `BeginPlay`,
one-shots on first use (`LoadSynchronous`; small files, keep them short). A loop sound that is not set to loop logs a
`LogMKHAudio` warning. Nothing plays on a dedicated server.

### UMKHGameUserSettings (Audio Extension)

Extends the engine user settings with four normalized volumes stored as `UPROPERTY(Config)`.
Setters clamp to `[0, 1]`, store the value, and apply it live to the matching Control Bus.

| Function | Purpose |
|---|---|
| `SetMasterVolume/SetMusicVolume/SetSFXVolume/SetCrowdVolume(float)` | Store + live-apply one volume to its bus |
| `GetMasterVolume/GetMusicVolume/GetSFXVolume/GetCrowdVolume() const` | Read the normalized value |
| `ApplyAudioSettings()` | Push all four volumes to their buses |
| `ApplyVolumeToControlBus(...)` *(private)* | Loads the bus and calls `SetGlobalBusMixValue` |
| `ApplySettings(bool)` *(override)* | Calls `Super` then `ApplyAudioSettings()` |
| `SetToDefaults()` *(override)* | Resets volumes to `1.0` |

`ApplyVolumeToControlBus` is a safe no-op (verbose log) when there is no active play world,
because Audio Modulation state is per audio device — the world subsystem re-applies later.
It logs a warning if a bus asset is unassigned. Volumes are pushed via
`UAudioModulationStatics::SetGlobalBusMixValue(World, Bus, LinearVolume)`.

### UMKHAudioWorldSubsystem

`UWorldSubsystem` created only for `Game`/`PIE` worlds (`ShouldCreateSubsystem`). On
`OnWorldBeginPlay` it:

1. Returns early on a dedicated server (no audio device).
2. Calls `UMKHGameUserSettings::ApplyAudioSettings()` to re-push saved volumes to the buses
   for this world's audio device.
3. Calls `StartLevelMusic(World)` — resolves the current short level name via
   `UGameplayStatics::GetCurrentLevelName(&World, /*bRemovePrefix*/ true)`, looks it up in
   `LevelMusicMap`, and asks `UMKHMusicSubsystem` to play it (or `StopMusic()` when the
   level has no entry).

This is the single bootstrap point that guarantees the mixing graph is correctly primed for
every world (initial boot, PIE, level travel), so UI code never has to worry about *when*
it is safe to apply audio settings.

### UMKHMusicSubsystem

`UGameInstanceSubsystem` that owns the current music `UAudioComponent`. Because it lives on
the GameInstance and creates the component with `bPersistAcrossLevelTransition = true`,
music continues seamlessly across level travel.

| Function | Behavior |
|---|---|
| `PlayMusicTrack(Track, FadeInSeconds = -1)` | If `Track` is already playing → no-op (seamless). Otherwise fades out the old component and creates a new persistent 2D sound faded in. `-1` fade uses the project default. |
| `StopMusic(FadeOutSeconds = -1)` | Fades out and clears the current track. |
| `GetCurrentTrack() const` | Track currently playing, or `nullptr`. |
| `IsMusicPlaying() const` | `true` if the component is valid and playing. |
| `ResolveFadeSeconds(float)` *(private)* | Returns the argument, or `DefaultMusicFadeSeconds` when negative. |

Playback uses `UGameplayStatics::CreateSound2D(..., bPersistAcrossLevelTransition = true,
bAutoDestroy = true)` followed by `FadeIn`. Crossfades let the outgoing component fade out
and auto-destroy itself.

### UMKHAudioSettingsWidget

`Abstract` subclass of `UMKHSystemWidget`. The Blueprint child only needs four `BindWidget`
sliders named exactly `MasterVolumeSlider`, `MusicVolumeSlider`, `SFXVolumeSlider`, `CrowdVolumeSlider`; all wiring
is native.

- `NativeConstruct` → `InitializeSliderValues()` (seed from saved settings) + `BindSliderEvents()`.
- `OnValueChanged` per slider → `Set<Category>Volume()` → **live** audible feedback while dragging.
- `OnMouseCaptureEnd` / `OnControllerCaptureEnd` → `HandleVolumeCaptureEnd()` → `SaveSettings()`
  (persist only on release, for both mouse and gamepad).
- `NativeDestruct` → persists any in-flight change (menu closed mid-drag) + `UnbindSliderEvents()`.

No WidgetController is used, by design (see [Design Principles](#design-principles) #5).

Under the sliders the page hosts `W_VoiceSettingsSection` (`UMKHVoiceSettingsSectionWidget`): the game output device
and the voice chat settings (master, microphone mode, non-friends, microphone volume with a level meter and open mic
sensitivity, voice volume on `CB_Voice`, voice output device, microphone in use). See `VoiceChat.md`.

---

## Sound Effects

Every gameplay and UI sound effect (voices, attacks, impacts, footsteps, dodge, potion, weapon, block, kill, menus) is
**a tag** (`SFX.*`) and **a row** of `DT_SoundEffects` whose name is that tag. Code, montages, gameplay cues and UI never
reference a sound asset: they ask `UMKHSoundEffectSubsystem` for a tag. Linking a delivered sound = dropping its files
in the row. Added 2026-11-03 (changelist 362).

### The row (`FMKHSoundEffectRow`)

| Field | Meaning |
|---|---|
| `Variants` | The takes: one is picked at random each time, never the same twice in a row. **Empty = silent on purpose** (not delivered yet, or covered by another sound, e.g. an attack whose impact VFX already sounds) |
| `Volume`, `PitchMin`, `PitchMax` | Volume multiplier and random pitch range (the SFX slider and submix apply on top) |
| `Space` | `World` (positioned, attached to the character at `Socket`) or `2D` |
| `Audience` | `Everyone`, or `LocalPlayerOnly` (only the machine where that character is the local player: kill confirmation) |
| `Socket` | Mesh socket the sound follows (`head` for voices, `hand_r` for swings and blocks); None = mesh origin |
| `Attenuation` | Falloff; empty = `DefaultSoundEffectAttenuation` (`SA_SoundEffects`: natural sound, inner 3 m, falloff 40 m) |
| `Concurrency` | How many may play at once; `SCON_Voice` (1 per character, stop oldest), `SCON_Footstep` (2 per character) |
| `TriggerEvent`, `EventRole` | "Event" route only (below) |
| `Notes` | Who plays it and when, for whoever links the files |

**No fallback to the parent tag** (user decision): a missing row logs one `LogMKHAudio` warning per tag (a typo in a
notify shows up at once); a row whose name is not a registered tag logs a warning when the game starts. The tags used by
code are native (`MKHGameplayTags::SFX`: footstep, UI); the others live in `DefaultGameplayTags.ini` with a DevComment
saying where they play.

### The four routes

Every sound reaches the subsystem by exactly one route, chosen by one question:

| Route | Rule | Who calls the subsystem | Network |
|---|---|---|---|
| **Animation** | The sound lands on a frame | `UMKHAnimNotify_PlaySound` (notify "MKH Play Sound", tag shown on the timeline) in the montage / animation; footsteps from `UMKHFootprintComponent` at each foot plant | None: every machine plays the animation (ability montages replicate, the anim blueprint runs everywhere) |
| **Visual effect** | The sound belongs to a VFX | `UMKHGameplayCueNotify_Burst` (`SoundTag` field, leave *Burst Sounds* empty): plays at the cue's attach component, else its location, else the target | The cue's own (here: `AnimNotify_GameplayCue`, local on every machine) |
| **Event** | A fact the server decides, with no animation or VFX of its own | `UMKHSoundEventComponent` on `ACharacterBase`: on the server it binds the `TriggerEvent` of every row on the character's ability system; on the event it multicasts (unreliable) the tag, every machine plays it on that character | Multicast |
| **UI** | Interface | `UMKHButton` (`SFX.UI.Click` on click, `SFX.UI.Hover` on hover / gamepad focus), `UInventoryDashboardWidget::RequestUseItem` / `RequestUnequipItem` (`SFX.UI.Equip` / `.Unequip`) | Local, 2D, plays while paused |

`EventRole` handles events sent to both characters of an exchange: `Event.Combat.BlockSuccessful` reaches the defender
and the attacker, the block row uses `Target` so only the defender plays it. Adding an event sound is a row, no code;
an event that does not exist yet needs one `HandleGameplayEvent` / `SendGameplayEventToActor` on the server.

### Current tags (2026-11-03)

| Tag | Route | Where |
|---|---|---|
| `SFX.Voice.Hurt` | Animation | `A_HitReceived_Montage`, 0.02 s (the lethal hit plays no hurt voice: `GA_HitReceived` is blocked by the Dead state) |
| `SFX.Voice.Death` | Animation | `A_Die` (sequence of the Dead state of `ABP_MKHPlayerCharacter`, players and bot; nobody ragdolls), 0.05 s |
| `SFX.Move.Dodge` | Animation | `A_DodgeFwd_Montage`, 0.02 s |
| `SFX.Item.Drink` | Animation | `A_Drink_Montage`, 1.0 s (placeholder time: move the notify to the sip) |
| `SFX.Attack.<Weapon>.<Attack>` (14) | Animation | Each attack / skill montage, one notify per strike 0.08 s before its `AN_HitScanStart` (`AN_SpawnProjectile` for ThorThrow); combos have 3 |
| `SFX.Foley.Footstep` | Animation | `UMKHFootprintComponent` (any surface; a `UMKHPhysicalMaterial` can name its own `FootstepSoundTag`) |
| `SFX.Impact.Zap`, `SFX.Impact.Shock` | Visual effect | `GC_Zap`, `GC_Shock` (ThunderShock). `GC_Flames` is a test cue: untouched |
| `SFX.Combat.Block` | Event | `Event.Combat.BlockSuccessful`, at `hand_r` |
| `SFX.Combat.Kill` | Event | `Event.Combat.EnemyKilled` (sent by `AMKHGameMode` to the scorer), 2D, LocalPlayerOnly |
| `SFX.Weapon.Sheathe` / `.Unsheathe` | Event | `Event.Weapon.Sheathed` / `.Unsheathed` (sent by `UEquipmentManagerComponent` when the weapon slot empties / fills; a swap is one unsheathe). Muted while the bot's loadout or round weapons are rebuilt (`SetWeaponEventsEnabled`) |
| `SFX.UI.Click`, `.Hover`, `.Equip`, `.Unequip` | UI | See the routes table |

Placeholders in the table today: `SFX.Voice.Hurt` = `01_REN_-_Damage_Grunt_A_Cue` (was in `GC_DamageBurst`, whose
*Burst Sounds* are now empty), `SFX.Impact.Zap` = `SFX_Vefects_Zap_Small_01_Cue` (was in `GC_Zap`). Every other row is
empty. `GC_Dodge` still holds a grunt in its graph, but that node never runs (the switch is not wired): it was never heard.

### Loading

`UMKHSoundEffectSubsystem::Initialize` loads the table and starts an async load of every variant, attenuation and
concurrency of every row (kept by a streamable handle for the session), so the first use never hitches; a row edited
while playing in the editor is loaded on demand. The event component reads the table on its own (also on a dedicated
server, where the subsystem does not exist). In the animation editor the notify plays its sound too (preview path), so
timing can be tuned by scrubbing.

### Tested (PIE vs bot, Makhion, 2026-11-03)

With every row filled with a placeholder in memory (`LogMKHAudio` Verbose): attacks of all three weapons and skills on
their strikes, hurt voice per hit, one death voice per death (player and bot), footsteps (~0.35 s apart while running),
Zap and Shock with the ThunderShock VFX, block played once (role filter drops the attacker's copy), kill only for the
scoring player, bot weapon swaps during the duel, no weapon sound at match start nor between rounds, button click and
hover. No `LogMKHAudio` warning. Not seen in that run: dodge and drink (the bot did not use them), equip / unequip from
the dashboard, a 2-machine session.

---

## Runtime Sequences

### Boot / Level Travel

```
World begins play
  └─ UMKHAudioWorldSubsystem::OnWorldBeginPlay
       ├─ (dedicated server?) → return
       ├─ UMKHGameUserSettings::ApplyAudioSettings()
       │     └─ SetGlobalBusMixValue for CB_Master / CB_Music / CB_SFX
       └─ StartLevelMusic()
             ├─ level name in LevelMusicMap? → UMKHMusicSubsystem::PlayMusicTrack(track)
             │     └─ same track already playing → no-op (seamless travel)
             └─ not found → UMKHMusicSubsystem::StopMusic()
```

### Adjusting a Volume Slider

```
Drag slider
  └─ OnValueChanged → UMKHGameUserSettings::Set<Category>Volume(v)
       └─ clamp + SetGlobalBusMixValue(bus, v)   [audible immediately]
Release slider
  └─ OnMouseCaptureEnd → SaveSettings()          [persist to GameUserSettings.ini]
```

---

## How-To Guides

### Add music for a new level

1. Import the track, set its **Submix = SBM_Music**.
2. **Project Settings → Game → Makhia Audio → Level Music Map**: add an entry keyed by the
   **short** level name (no path, no PIE prefix — e.g. `Makhion`) → the track.

That's it — `UMKHAudioWorldSubsystem` starts it on begin play. A level with no entry fades
any current music out.

### Add the crowd sounds

Done (2026-10-20): the final crowd files are in. The steps below are for replacing or adding sounds.

1. Import the WAVs into `Content/ThirdParty/Audio/Crowd/Wav/` (48 kHz; keep the reactions short, 1–4 s) and wrap each
   in a Sound Cue in `Content/ThirdParty/Audio/Crowd/Cues/`; the settings reference the cues.
2. Open the two loops (ambience murmur, cheering crowd) and tick **Looping**. On **every** crowd sound (loops,
   reactions, Sound Cues) set **Routing → Base Submix = `SBM_Crowd`**.
3. **Project Settings → Game → Makhia Audio → Crowd**: drop each file in its slot (*Loops*: `Crowd Ambience Loop`,
   `Crowd Excited Loop`; *Reactions*: duel start, hit, sudden death, death, match over). Empty slots stay silent.
   For a loop cue also tick **Looping** on its *Wave Player* node.
4. Play a match (vs the bot is enough) and balance with *Crowd → Mix* (ambience volume, excited loop maximum,
   reaction volume, hit cooldown). The values are saved in `Config/DefaultGame.ini` (check it out in Perforce first).

For variety, a slot can take a **Sound Cue** with a *Random* node over several takes (e.g. three "ooh" for hits).

**Current setup** ("excitement driven", 2026-10-20): `Crowd Excited Loop` = `Cue_CrowdAmbient`, `Crowd Death Sound` =
`Cue_CrowdCheer1`, `Crowd Match Over Sound` = `Cue_CrowdCheer`; ambience, duel start, hit and sudden death are empty, so
those events are heard only through the excited loop rising with the excitement. The director's `MinExcitement` (on the
`CrowdDirector` actor in Makhion, default 0.25) is the loop's resting level. Every crowd wave and cue has
`Base Submix = SBM_Crowd` (Crowd slider).

### Add the gate sounds

Nothing else to wire: both portcullis gates of Makhion (`AMKHArenaGate`, `BP_ArenaGate`) already play them, at the
grille (spatialized, moving with it), on every peer (`MatchFlowArchitecture.md`, "Arena Gates").

| Slot | When | Length |
|---|---|---|
| `Gate Raise Sound` | the grille starts rising (duel start) | about the rise, 2.5 s (winch, chains); cut with a 0.2 s fade if the grille drops again |
| `Gate Raised Sound` | the grille locks fully open | short clank |
| `Gate Lower Sound` | the grille starts dropping (each preparation phase) | about the drop, 1.2 s (chains running free); cut if it rises again |
| `Gate Slam Sound` | the grille hits the ground | short heavy iron slam |

1. Import the files into `Content/Makhia/Sound/Arena/` (WAV, 48 kHz, **mono**: spatialized sounds are positioned
   from mono sources). Leave **Looping** off and **Submix** empty (`SBM_SFX`, SFX slider).
2. Create a **Sound Attenuation** asset (`Content/Makhia/Sound/Arena/SA_Gate`), e.g. inner radius 800, falloff
   distance 4000 (both gates are 37 m apart: each player hears their own gate loud, the other one faint).
3. **Project Settings → Game → Makhia Audio → Arena Gates**: drop the four files and the attenuation in their slots,
   then tune `Gate Sound Volume`. Empty slots stay silent; without an attenuation (here or on the sound) the gate
   sounds play 2D.
4. Play a match vs the bot: drop at the preparation phase, rise in the last 3 s of the countdown.

### Link delivered sound effects

1. Import the WAVs into `Content/Makhia/Sound/SFX/<Family>/` (48 kHz, **mono** for world sounds; leave the Submix
   empty: `SBM_SFX`, SFX slider). No Sound Cue is needed: the variants and the random pitch are in the row.
2. Open `DT_SoundEffects`, select the row (`SFX.Voice.Hurt`, ...), add the files to **Variants** (remove the
   placeholder if there is one). Tune Volume / pitch. Save (check the table out in Perforce first).
3. Play. To tune the timing of an animation sound, open the montage: the notify sits on the **Sound** track, drag it.

### Add a new sound effect

1. **Tag**: Project Settings → GameplayTags → add `SFX.<Family>.<Name>` (DevComment: where it plays).
2. **Row**: in `DT_SoundEffects` add a row named exactly like the tag; fill Space / Socket / Notes.
3. **Route**, by the rule of [The four routes](#the-four-routes):
   - animation: add an **MKH Play Sound** notify on the montage (track *Sound*), pick the tag;
   - visual effect: make the cue's parent `MKHGameplayCueNotify_Burst` and set its **Sound Tag**;
   - server event: set the row's **Trigger Event** (and **Event Role** if both characters receive it). No code;
   - UI: `UMKHSoundEffectSubsystem::Get(this)->PlayUISound(Tag, this)` from the widget's C++ base.

### Route a new sound category under an existing slider

Route the sound's **Submix** to `SBM_Music` (music) or `SBM_SFX` (effects). Sounds with no
submix already fall under `SBM_SFX` via the Base Default Submix, so most SFX need nothing.

### Change music from gameplay (e.g. combat vs. exploration)

```cpp
if (UMKHMusicSubsystem* Music = GetGameInstance()->GetSubsystem<UMKHMusicSubsystem>())
{
    Music->PlayMusicTrack(CombatTrack); // crossfades from the current track
}
```

### Add a new user volume category (e.g. UI, Voice)

1. Create `CB_UI` + `SBM_UI` (child of `SBM_Main`, modulated by `CB_UI` **and `CB_Master`**, unless the category must
   ignore the master like the voice).
2. Add a `TSoftObjectPtr<USoundControlBus> UIVolumeControlBus` to `UMKHAudioSettings` and
   reference `CB_UI` in `DefaultGame.ini`.
3. Add `UIVolume` (`UPROPERTY(Config)`) + `Set/GetUIVolume` to `UMKHGameUserSettings`, and a
   line in `ApplyAudioSettings()`.
4. Add a `UIVolumeSlider` `BindWidget` to `UMKHAudioSettingsWidget` and wire it like the others.

---

## Gotchas & Troubleshooting

### Custom submixes are silent (even in editor preview); only "no submix" plays

**Cause:** the submix graph forms a cycle when `MasterSubmix` is not set. A submix with a
null parent implicitly sends to `BaseDefaultSubmix`; if that is `SBM_SFX` (a child of
`SBM_Main`) while `SBM_Main` itself has a null parent, `SBM_Main → SBM_SFX → SBM_Main` loops.
The engine drops the branch, so nothing routed into the custom graph reaches hardware.

**Fix:** set `MasterSubmix = SBM_Main` in `DefaultEngine.ini` / Project Settings → Audio. As
the declared master, `SBM_Main` outputs to hardware directly and no longer does an implicit
send, breaking the cycle. **Master/Base submix changes require a full editor restart.**

### Music plays but a volume slider does nothing

The sound is not routed through the expected submix. Check the **Submix** property on the
sound asset (music → `SBM_Music`). A sound with no submix goes to `SBM_SFX` (SFX slider).

### Modulation is never the cause of silence

An undriven Control Bus resolves to its parameter's default normalized value of `1.0`
(= `0 dB`, full volume). If audio is silent, the cause is routing, not the buses.

### `ApplyVolumeToControlBus` logs "No active play world"

Expected when called before a world is up (e.g. from a menu on the CDO). `UMKHAudioWorldSubsystem`
re-applies on world begin play; no action needed.

---

## Roadmap

Implemented this session: Control Bus / submix mixing graph, user volumes (Master/Music/SFX)
with persistence and live application, per-world bootstrap, session-scoped music with
crossfades and seamless travel, and the settings slider panel.

Planned:

| Phase | Work |
|---|---|
| Crowd | Done: final files, `SBM_Crowd` + Crowd slider, excitement floor (see [Add the crowd sounds](#add-the-crowd-sounds)). |
| SFX | Done (2026-11-03, system ready, files to link): `DT_SoundEffects` + four routes, see [Sound Effects](#sound-effects). |
| Mixing | Stacked bus mixes on top of the user mix: music ducking under voice, pause-menu low-pass, low-health filter — via `ActivateBusMix` / `DeactivateBusMix`. |
| Adaptive music | **MetaSound Source** with exposed `Intensity`/`Section` params driven by `SetFloatParameter`; **Quartz** for beat-synced transitions. Swap the track asset for a MetaSound — the subsystem API is unchanged. |
```
