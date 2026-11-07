# Settings Screen Architecture

## Table of Contents

1. [Overview](#overview)
2. [Directory Structure](#directory-structure)
3. [Design Principles](#design-principles)
4. [Configuration](#configuration)
5. [Content Assets (Editor-Created)](#content-assets-editor-created)
6. [C++ Classes](#c-classes)
   - [UMKHSettingsScreenWidget](#umkhsettingsscreenwidget)
   - [UMKHSettingsPageWidget](#umkhsettingspagewidget)
   - [UMKHSettingsNavButtonWidget](#umkhsettingsnavbuttonwidget)
   - [UMKHKeyboardSettingsWidget](#umkhkeyboardsettingswidget)
   - [UMKHKeybindRowWidget](#umkhkeybindrowwidget)
   - [UMKHKeybindCategoryWidget](#umkhkeybindcategorywidget)
   - [UMKHKeyCaptureWidget](#umkhkeycapturewidget)
   - [UMKHInputSettingsLibrary](#umkhinputsettingslibrary)
   - [UMKHInputUserSettings](#umkhinputusersettings)
7. [Runtime Sequences](#runtime-sequences)
8. [How-To Guides](#how-to-guides)
9. [Gotchas & Troubleshooting](#gotchas--troubleshooting)
10. [Roadmap](#roadmap)

---

## Overview

The settings screen is a **full-screen shell hosting one page at a time**. The shell owns
navigation and the shared action bar; each page owns its own settings model and persists
itself. Pages are declared as data, so adding Video or Gameplay later is one array entry
and a Blueprint — no C++ change to the shell.

The Keyboard and Controller pages are a **view over `UEnhancedInputUserSettings`**, one device family
each (see [Keyboard and controller bindings](#keyboard-and-controller-bindings)). It never caches a
binding: every row is rebuilt from the active key profile, and a rebind is written straight
through `MapPlayerKey()`. The engine owns the model; the UI only reads and writes it.

| Custom Class | Engine Base | Purpose |
|---|---|---|
| `UMKHSettingsScreenWidget` | `UMKHSystemWidget` | Full-screen shell: nav rail, page switcher, action bar |
| `UMKHSettingsPageWidget` | `UMKHSystemWidget` | Base class for one page; apply/revert/reset contract |
| `UMKHSettingsNavButtonWidget` | `UMKHSystemWidget` | One entry of the navigation rail |
| `UMKHKeyboardSettingsWidget` | `UMKHSettingsPageWidget` | Keybind list of one device (`InputDevice`), grouped by display category: `W_KeyboardSettings` and `W_ControllerSettings` |
| `UMKHKeybindRowWidget` | `UMKHSystemWidget` | One action: name plus two key slots |
| `UMKHKeybindCategoryWidget` | `UMKHSystemWidget` | Section header plus column captions |
| `UMKHKeyCaptureWidget` | `UMKHSystemWidget` | Modal that captures the next input |
| `UMKHGeneralSettingsWidget` | `UMKHSettingsPageWidget` | Language, gameplay switches, brightness, camera feel and on-screen readouts |
| `UMKHVideoSettingsWidget` | `UMKHSettingsPageWidget` | Display, quality and resolution scale; staged until Apply |
| `UMKHSliderRowWidget` | `UMKHSystemWidget` | One setting with a continuous value |
| `UMKHOptionRowWidget` | `UMKHSystemWidget` | One setting picked from a list, shown as a stepper |
| `UMKHVideoConfirmWidget` | `UMKHSystemWidget` | Countdown asking to keep a new video mode |
| `UMKHAudioSettingsWidget` | `UMKHSettingsPageWidget` | Volume sliders (see `AudioSystemArchitecture.md`) |
| `UMKHVoiceSettingsSectionWidget` | `UMKHSystemWidget` | Device and Voice chat sections under the Audio sliders, built from the shared rows (see `VoiceChat.md`) |
| `UMKHVoiceMicMeterWidget` | `UMKHSystemWidget` | Live microphone level with the open mic activation mark |
| `UMKHInputSettingsLibrary` | `UBlueprintFunctionLibrary` | Shared access to input user settings, per-device mapping lookups |
| `UMKHInputUserSettings` | `UEnhancedInputUserSettings` | Registers gamepad keys on their own device so they get their own slots |
| `UMKHInputModifierHalfAxis` | `UInputModifier` | Keeps one half of a stick axis, for the one-way movement actions |
| `UMKHActiveInputDeviceSubsystem` | `ULocalPlayerSubsystem` | Device the player used last, so key prompts follow it |

Logging for this layer uses the dedicated `LogMKHInput` category (`MKHLogChannels.h`).

Two settings backing stores exist and they are **not** interchangeable:

| Page | Backing store | Persisted to |
|---|---|---|
| Keyboard, Controller | `UEnhancedInputUserSettings` (one shared key profile) | per-local-player SaveGame slot |
| Audio | `UMKHGameUserSettings` | `GameUserSettings.ini` |

This is why each page persists itself instead of the shell saving everything at once.

---

## Directory Structure

```
Source/Makhia/
├── Public/UI/HUD/Settings/
│   ├── MKHSettingsScreenWidget.h      # Shell: rail + switcher + action bar
│   ├── MKHSettingsPageWidget.h        # Page base class (contract)
│   ├── MKHSettingsNavButtonWidget.h   # Rail entry
│   ├── MKHKeyboardSettingsWidget.h    # Keybind page
│   ├── MKHKeybindRowWidget.h          # One remappable action
│   ├── MKHKeybindCategoryWidget.h     # Section header
│   ├── MKHKeyCaptureWidget.h          # "Press any key" modal
│   └── MKHAudioSettingsWidget.h       # Volume page
├── Public/Libraries/
│   └── MKHInputSettingsLibrary.h      # Settings / key profile accessors, per-device lookups
├── Public/Input/
│   ├── MKHInputDeviceTypes.h          # EMKHInputDevice (KeyboardMouse / Gamepad)
│   ├── MKHInputUserSettings.h         # Per-device slots (UserSettingsClass)
│   ├── MKHActiveInputDeviceSubsystem.h # Last used device, drives the key prompts
│   └── MKHInputModifierHalfAxis.h     # Stick half for the one-way movement actions
└── Private/UI/HUD/Settings/           # Matching .cpp files

Content/Makhia/UI/SettingsScreen/
├── W_SettingsScreen                   # Shell
├── W_SettingsNavButton                # Rail entry
├── Pages/W_KeyboardSettings           # Keybind page (InputDevice = Keyboard & Mouse)
├── Pages/W_ControllerSettings         # Keybind page (InputDevice = Gamepad)
├── Pages/W_AudioSettings              # Volume page
├── Pages/W_VideoSettings              # Display and quality page
├── Pages/W_GeneralSettings            # Language, gameplay, display, camera, controller, interface
├── Options/W_OptionRow                # Stepper row, shared by option pages
├── Options/W_VideoConfirm             # Video mode confirmation
├── Keybinds/W_KeybindRow
├── Keybinds/W_KeybindCategory
├── Keybinds/W_KeyCapture
└── Material/
    ├── M_UI_SettingsBackdrop          # Panel background
    └── M_UI_NavHighlight              # Selected rail entry wash
```

---

## Design Principles

**Pages are data, not code.** `SettingsPages` is a `TArray<TSubclassOf<UMKHSettingsPageWidget>>`
on the shell. The rail, the switcher and the action bar routing are all built from it.

**No WidgetController.** Unlike the HUD and Inventory, settings pages talk to a global,
engine-owned model with a single writer. The Model→View observer indirection used for
gameplay data would add a layer without adding anything. This mirrors the reasoning already
documented on `UMKHAudioSettingsWidget`.

**The engine owns the keybinds.** `UMKHKeyboardSettingsWidget` holds no map of its own.
`RebuildKeybindList()` re-reads `GetPlayerMappingRows()` after every change, so the UI cannot
drift from the profile.

**Apply is the only thing that persists.** A rebind is written to the live profile
immediately so the player can feel the new key at once, but leaving the screen any other way
rolls the profile back to the state captured when the page opened.

**The screen never listens for raw keys or focus changes.** Closing is driven only by the
Back button and the game menu input action. An earlier version reacted to `Escape` and to
focus loss; both misfired on OS-level events (see [Gotchas](#gotchas--troubleshooting)).

---

## Configuration

### Enabling the user settings object

`Config/DefaultInput.ini`:

```ini
[/Script/EnhancedInput.EnhancedInputDeveloperSettings]
bEnableUserSettings=True
```

Without this `GetUserSettings()` returns null, the key profile never exists, and the Keyboard
page logs a warning and lists nothing.

The same section sets `UserSettingsClass=/Script/Makhia.MKHInputUserSettings`. Without it every
gamepad key would land in the keyboard's Secondary slot (see below).

### Registering the mapping contexts

`AMKHPlayerController::RegisterMappingContextsWithUserSettings()` runs on `BeginPlay` and
publishes `DefaultMappingContext`, `UIOpenMappingContext`, `DeadMappingContext` and
`MatchIntroMappingContext` (hold-to-skip of the match intro) to the settings object; the lobby
controller registers the same list (`BP_LobbyController.RemappableMappingContexts`). Every context the player can ever be switched to must be registered, or the
actions it holds are missing from the profile even though they exist in the game.

### Making an action remappable

An action appears in the list only if its `UInputAction` has **Player Mappable Key Settings**
assigned, with:

- **Name** — the unique mapping name, used as the row key (e.g. `IA_MoveForward`)
- **Display Name** — the player-facing label (e.g. "Move Forward")
- **Display Category** — the section it is grouped under (e.g. "Movement")

Leaving Player Mappable Key Settings at `None` is how an action is kept out of the screen.
`IA_Look` relies on this: it is a 2D axis and cannot be expressed as a keycap.

---

### Keyboard and controller bindings

Both devices live in the **same mapping contexts and the same key profile**. An action has keyboard
and gamepad mappings side by side in `IMC_BaseMapping`; `UMKHInputUserSettings` registers gamepad keys on
`FHardwareDeviceIdentifier::DefaultGamepad` (everything else stays on the invalid device, so profiles
saved before controller support still match), and the engine numbers slots **per device**. Each page
reads and writes only its device's mappings, through the `UMKHInputSettingsLibrary` helpers
(`IsMappingForDevice`, `FindMappingForSlot`, `GetHardwareDeviceIdName` for `FMapPlayerKeyArgs`).

Rules for the gamepad mappings:

- **An action is on the Controller page only if a context gives it a default gamepad key.** The engine
  applies a player key only through a context mapping of the same key type, so a gamepad key cannot be
  added to an action that has none.
- **Same gamepad key in every context that maps the action** (`IMC_BaseMapping`, `IMC_UIOpenMapping`,
  `IMC_Dead`), as already done for the keyboard: each registered context overwrites the slot's default key.
- **One slot per action** on the controller, and it is **never left empty**: for an empty gamepad slot the
  engine falls back to the context's default key. So the capture modal ignores Delete on the Controller
  page and a conflict **swaps** the two buttons instead of clearing the other action.
- **Sticks are fixed.** Movement (left stick) has `Setting Behavior = Ignore Settings`; `IA_LookGamepad` (right
  stick) has no Player Mappable Key Settings. The capture modal refuses stick directions and axes.
- **Movement uses a stick half per action**: `IA_MoveForward` / `IA_MoveRight` read `Gamepad_LeftY` /
  `Gamepad_LeftX` through `Half Axis (Makhia)`, `IA_MoveBackWard` / `IA_MoveLeft` the same axis with
  *Negative Half* ticked. `UBasicMovementComponent` reads only the magnitude of each action, so without the
  modifier pulling back would also drive forward. The stick dead zone (0.25) comes from the axis config in
  `DefaultInput.ini`.
- **Look on the right stick** is its own action, `IA_LookGamepad` (Axis2D), bound by
  `UBasicMovementComponent::LookGamepad` (`LookGamepadAction` on `BP_MKHPlayerCharacter`): `Gamepad_Right2D`
  with Scale By Delta Time and Scalar (88, 56, 1), about 220°/s yaw and 140°/s pitch at full tilt with the legacy
  2.5 input scale. No Negate, unlike the mouse: stick up already looks up. It reads the controller settings of the
  General page (Look Sensitivity, Invert Y Axis), not the mouse ones.

Default controller layout (Xbox names; `IA_Sprint` is the Walk action):

| Button | Action | Button | Action |
|---|---|---|---|
| A | Jump | LT | Block (hold) |
| B | Dodge | RT tap / hold | Light / Heavy Attack |
| X | Interact | LB (hold) | Alternate Actions |
| Y | Switch Weapon | LB + RB / LT / RT | Skill 1 / 2 / 3 |
| D-pad ↓ | Push to Talk | LB + D-pad ↓ / ↑ | Sheathe Weapon / Hide HUD |
| RB | Ready Up (preparation) | D-pad ← ↑ → | Consumable 1 / 2 / 3 |
| L3 | Walk (hold) | | |
| R3 | Crouch | View / Menu | Inventory / Game Menu |

- **Light / heavy on RT** works like the left mouse button: Tap (0.2 s) on Light Attack, Hold (0.2 s) on Heavy
  Attack, same key.
- **Chords on Alternate Actions**: `IA_AlternateActions` (`Input/Actions/General`, "Alternate Actions", LB by default, remappable on the
  Controller page, bound to nothing in code) is the action named in a *Chorded Action* trigger, next to Pressed, by the
  gamepad mappings of the three skills (LB + RB / LT / RT), Sheathe Weapon (LB + D-pad ↓) and Hide HUD (LB + D-pad ↑).
  While it is held the chord consumes the second button, so LT does not also block, RT does not also attack, D-pad ↓
  does not also talk and D-pad ↑ does not also use Consumable 2. Rebinding a chorded action changes only its second
  button; rebinding Alternate Actions moves the chord of all of them. The capture popup of a chorded action says
  "LB + … (hold to cancel)" and swallows the chord button (`BeginCapture`'s `InChordKey`, from
  `UMKHInputSettingsLibrary::GetActionKey`), so pressing the full LB + RT stores RT instead of LB. It was `IA_SkillModifier` ("Skill Modifier") until
  2026-10-21 (a key rebound under the old name falls back to LB: no migration, nobody had rebound it). Prompts and the settings rows show
  "LB + RB" through `FindChordAction()`, which reads the registered contexts (the settings screen runs under the
  UI context). Conflicts are checked only between mappings with the same chord, so RB alone does not clash with
  LB + RB.
- **Switch Weapon** (`IA_SwitchWeapon`, controller only) is handled by `AMKHPlayerController::HandleSwitchWeaponPressed`:
  it presses the other weapon quick slot's input (`ResolveSwitchWeaponTag`, from
  `UQuickSlotManagerComponent::GetQuickEquipWeaponSlotTag`), or the current one when the other slot is empty, so
  queueing and swap rules stay those of the two weapon keys. `IA_WeaponSlot1/2` have no gamepad button.
- **Ready Up** (`IA_SkipInit`) is RB alone, so it no longer shares a button with Switch Weapon. On the keyboard it is P
  (T is Push to Talk).

Not on the controller: Weapon 1 / 2. `IA_SkipIntro` already had `Gamepad_FaceButton_Bottom` (not remappable) in
`IMC_MatchIntro`.

---

## Content Assets (Editor-Created)

Every Blueprint below derives from the matching C++ class. Widgets marked `BindWidget` are
required and the Blueprint fails to compile without them; `BindWidgetOptional` ones may be
omitted.

| Blueprint | Required widgets | Optional widgets |
|---|---|---|
| `W_SettingsScreen` | `PageSwitcher`, `NavButtonContainer` | `PageTitleText`, `ApplyButton`, `RevertButton`, `DefaultsButton`, `BackButton`, `DirtyStateText` |
| `W_SettingsNavButton` | `NavButton`, `NavLabelText` | `SelectionIndicator`, `SelectionBackground` |
| `W_KeyboardSettings`, `W_ControllerSettings` | `KeybindListContainer` | — |
| `W_KeybindRow` | `ActionNameText`, `PrimaryKeyButton`, `PrimaryKeyText` | `SecondaryKeyButton`, `SecondaryKeyText`, `ResetButton` |
| `W_KeybindCategory` | `CategoryNameText` | `CategoryCountText` |
| `W_KeyCapture` | — | `ActionNameText`, `SlotNameText`, `ConflictText`, `ReplaceButton`, `CancelButton` |
| `W_AudioSettings` | `MasterVolumeSlider`, `MusicVolumeSlider`, `SFXVolumeSlider`, `CrowdVolumeSlider` | per channel: `<Ch>ValueText`, `<Ch>ResetButton`, `<Ch>TrackRow` |
| `W_VideoSettings` | `OptionListContainer` | — |
| `W_OptionRow` | `OptionNameText`, `OptionValueText`, `PreviousButton`, `NextButton` | `OptionDescriptionText`, `ResetButton` |
| `W_GeneralSettings` | `OptionListContainer` | — |
| `W_SliderRow` | `OptionNameText`, `ValueSlider` | `OptionValueText`, `OptionDescriptionText`, `TrackRow`, `ResetButton` |
| `W_VideoConfirm` | `CountdownText` | `KeepButton`, `RevertButton` |

### Materials

Both are Material Domain **User Interface** and expose their look as parameters, so tuning
never means editing the graph.

`M_UI_SettingsBackdrop` — the panel background: a flat base plus a brass glow falling from
the top centre.

| Parameter | Default | Effect |
|---|---|---|
| `GroundColor` | `#12100E` | flat base colour |
| `GlowColor` | `#C9973E` | glow tint |
| `GlowIntensity` | 0.05 | glow strength |
| `Radius` | 0.62 | how far the glow reaches |
| `SpreadX` / `SpreadY` | 1.2 / 0.9 | elliptical shape |
| `CenterX` | 0.5 | horizontal origin |
| `Softness` | 1.6 | falloff curve |

`M_UI_NavHighlight` — the wash behind the selected rail entry, fading left to right.

| Parameter | Default | Effect |
|---|---|---|
| `Color` | `#C9973E` | wash tint |
| `Intensity` | 0.26 | opacity at the left edge |
| `Falloff` | 1.8 | how quickly it fades out |

---

## C++ Classes

### UMKHSettingsScreenWidget

The shell. Builds one page instance per entry in `SettingsPages`, one rail entry per page,
and routes the action bar to whichever page is visible.

Key members:

| Member | Notes |
|---|---|
| `SettingsPages` | Page Blueprints, in rail order |
| `NavButtonClass` | Rail entry Blueprint |
| `DefaultPageIndex` | Page shown on open |
| `NavButtonSpacing` | Gap around rail entries — applied in code because entries are spawned at runtime |
| `ShowPage(int32)` | Deactivates the old page, activates the new one, refreshes header and action bar |
| `RequestClose()` | External close request; refused while the visible page is blocking |
| `DiscardPendingChanges()` | Rolls the visible page back; called by `CloseSettings()` and `NativeDestruct()` |
| `OnSettingsClosed` | Broadcast to `AMainHUD`, which owns the teardown |

`RefreshActionBar()` collapses Restore Defaults / Revert / Apply on pages that report
`UsesActionBar() == false`, and enables Apply/Revert only when the page is dirty.

`NativeOnMouseButtonDown` takes the keyboard focus back on clicks that land on empty space,
so the focus stays inside the panel while it is in use.

### UMKHSettingsPageWidget

Abstract base defining the contract between shell and page.

| Method | Default | Meaning |
|---|---|---|
| `ActivatePage()` / `DeactivatePage()` | — | Called when the page becomes visible / hidden; fire the native and Blueprint hooks |
| `ApplyChanges()` | no-op | Write the page's values to its backing store |
| `RevertChanges()` | no-op | Drop unsaved edits |
| `ResetToDefaults()` | no-op | Restore shipped defaults |
| `HasPendingChanges()` | `false` | Drives Apply/Revert and the footer label |
| `UsesActionBar()` | `true` | `false` hides the three action buttons (Audio applies live) |
| `IsBlockingClose()` | `false` | `true` refuses close requests (Keyboard while capturing) |

`OnPageDirtyStateChanged` lets a page tell the shell to re-evaluate the action bar.

### UMKHSettingsNavButtonWidget

One rail entry. `SetSelected()` drives three things at once: the label colour
(`NormalLabelColor` / `SelectedLabelColor`), the accent bar (`SelectionIndicator`), and the
gradient wash (`SelectionBackground`, shown as `HitTestInvisible` so it never eats clicks).

### UMKHKeyboardSettingsWidget

The keybind page, for the device set in `InputDevice`. `W_ControllerSettings` is a copy of
`W_KeyboardSettings` with `InputDevice = Gamepad`: rows show one column (the Secondary keycap is hidden, the
column captions collapsed), the capture modal accepts only gamepad buttons, Restore Defaults and the row
reset touch only that device, and a conflict swaps buttons (see
[Keyboard and controller bindings](#keyboard-and-controller-bindings)).

Both pages share the live profile, so **leaving a keybind page reverts its unsaved rebinds**: an Apply on
the other page would otherwise save them behind its back.

| Member | Notes |
|---|---|
| `KeybindRowClass`, `CategoryHeaderClass`, `KeyCaptureWidgetClass` | Blueprints spawned per row / section / capture |
| `ExcludedMappingNames` | Mappings never listed |
| `CategoryOrder` | Section order by display name; unlisted categories follow alphabetically |
| `MappingOrder` | Action order inside a category by mapping name; unlisted actions follow alphabetically |
| `UncategorizedLabel` | Section for actions with no display category |
| `CaptureModalZOrder` | Viewport Z-order of the capture modal |

`SavedSnapshot` holds the state captured on activation. It exists because the engine tracks
"changed from the IMC default" (`IsDirty`, `IsCustomized`) but **not** "changed since the last
save", which is what Revert needs.

`WriteMapping()` is the single write path: it fills `FMapPlayerKeyArgs`, calls `MapPlayerKey`
or `UnMapPlayerKey`, logs any failure tags on `LogMKHInput`, and calls `ApplySettings()` so
the new key is live immediately. It never calls `SaveSettings()` — only `ApplyChanges()` does.

### UMKHKeybindRowWidget

One action. Holds no state: `RefreshFromProfile()` re-reads both slots from the profile.
Keycap colour communicates the binding state:

| State | Colour property |
|---|---|
| Bound, still default | `DefaultKeyColor` (bone) |
| Bound, changed by the player | `CustomizedKeyColor` (brass) |
| Empty slot | `UnboundKeyColor` (faint) |

### UMKHKeybindCategoryWidget

Section header: label plus action count, and the `ACTION / PRIMARY / SECONDARY` captions.
The caption widths must match the keycap `SizeBox` width in `W_KeybindRow` or the columns
drift apart.

### UMKHKeyCaptureWidget

Modal overlay. Captures keyboard (`NativeOnKeyDown`), mouse (`NativeOnMouseButtonDown`) and
gamepad axes past a deflection threshold (`NativeOnAnalogValueChanged`). It only reports what
was pressed — deciding whether the key is acceptable is the page's job.

`Escape` cancels and `Delete` clears the slot, so neither can ever be bound from this screen.
On a conflict the modal stops listening and waits for Replace or Cancel; `ConflictText` and
`ReplaceButton` are collapsed until then (with the pad the focus then moves to Replace).

Gamepad captures commit on **release**: every pad button can be bound, so holding one for
`HoldToCancelSeconds` (1 s) cancels instead. On the Keyboard page the pad's B cancels. See
`GamepadMenuNavigation.md`.

### UMKHAudioSettingsWidget

Documented in `AudioSystemArchitecture.md`; only the parts that belong to this screen are noted
here. It overrides `UsesActionBar()` to `false` (volumes apply live and save on slider release),
shows a percentage readout per channel, dims it at zero, and offers a per-channel reset that
stays hidden while the channel sits at `DefaultChannelVolume`.

Each slider's visible bar is **not** drawn by `USlider`. Two `Border` widgets in a
`<Ch>TrackRow` sit behind it — brass fill and remaining track — and `RefreshTrackFill()` splits
them by setting the two slot sizes. See the slider entry under
[Gotchas](#gotchas--troubleshooting) for why.

The **Voice chat section** (`W_VoiceSettingsSection`, `VoiceChat.md`) is a separate widget placed in the page's
`ContentStack`: it builds its rows like the General page (`OptionRowClass`, `SliderRowClass`, `SectionHeaderClass`,
plus `MicMeterClass`), applies and saves at once, shows percentages (`UMKHSliderRowWidget::SetValueFormat`) and hides
the rows the online backend cannot offer (capabilities). `IA_PushToTalk` is listed on the Keyboard and Controller
pages like the other actions (keyboard T, controller D-pad down alone). Device names longer than the value box scroll
while the row is focused (see `UMKHOptionRowWidget`).

### UMKHVideoSettingsWidget

The only page that stages its edits. `StagedValues` holds what the player picked, `AppliedValues`
what `UGameUserSettings` currently has, and `HasPendingChanges()` compares the two. Nothing
reaches the renderer until Apply.

That is not a stylistic choice: a resolution the monitor cannot display would hide the very panel
needed to undo it, so video modes cannot be tried one keypress at a time the way keybinds are.
After Apply changes the resolution or window mode, `UMKHVideoConfirmWidget` counts down and calls
the engine's `RevertVideoMode()` if nobody answers.

The quality preset is derived, never stored: `RefreshPresetFromGroups()` reads the eight
scalability groups and reports **Custom** as soon as they disagree, mirroring what
`GetOverallScalabilityLevel()` returns when levels are not uniform. Stepping the preset writes all
eight; stepping one group sends the preset to Custom.

Foliage and View Distance are deliberately not offered — see the comment on `EMKHVideoOption`.
Global Illumination is not shown either: the project uses baked lighting without Lumen, so the
group changes nothing (the preset still writes it, so presets stay uniform).

Reset to defaults does not mean Epic: after `SetToDefaults()` the page calls
`UMKHGameUserSettings::StageAutoDetectedQuality()`, which runs the hardware benchmark and stages
what this machine can run, with the frame rate capped at `DefaultFrameRateLimit` (144). The same
benchmark runs once on the first launch of a packaged game (`RunFirstLaunchAutoDetect`, from
`UMKHGameInstance::OnStart`; skipped in the editor). See `PerformanceOptimization.md`.
On a handheld PC (`UMKHOnlineServicesSubsystem::IsRunningOnHandheld`) both calls stage the handheld
profile instead of the benchmark (`SteamDeck.md`). The frame rate caps offered are 30, 40, 60, 120,
144 and unlimited (30 and 40 for the Steam Deck).

### UMKHGeneralSettingsWidget

Applies and saves as it goes, so it opts out of the action bar: nothing on this page can leave
the player unable to see the panel, which is what forces the video page to stage its edits.

The page has six sections: Language, Gameplay, Display, Camera (mouse), Controller and Interface. Mouse sensitivity and Y
inversion live in `UMKHGameUserSettings` and are read live by `UBasicMovementComponent::Look()`; the values on the component are only the fallback for when
the settings are unavailable. Show FPS goes through `GEngine->SetEngineStat`, which takes an
explicit on/off — the `stat fps` console command only toggles, so a saved value would drift out
of sync with what is on screen.

**Gameplay** holds two switches, both on by default, and the scoreboard look (`EMKHGeneralOption` values appended at the
end of the enum, so the row ids of the older options do not move):

- **Scoreboard Style** (`UMKHGameUserSettings::ScoreboardStyle`, default Pediment): see "The scoreboard looks" in
  `MatchFlowArchitecture.md`. Local only, applied at once even during a match.

- **Hold to Equip Pickups** (`UMKHGameUserSettings::bHoldToEquipPickup`): keeping Interact held for half a second after a pickup
  also equips the armor or quick slots the weapon / consumable (the press always collects at once). Local only:
  `AMKHPlayerController` reads it when Interact is pressed, the pickup prompt (`UMKHItemPickupWidget`) when a pickup
  gains the focus. Off: a held press does nothing more and the hint is hidden. See "Hold to equip" in `PickupSystemArchitecture.md`.
- **Remember Quick Slots** (`bAutoQuickSlotLoadout`): the starting loadout's armor and quick slots
  (`UMKHQuickSlotPreferenceComponent`). The server decides them, so the value travels with the preferences
  (`ServerSetPreferences`) and is sent again when it changes (`OnAutoQuickSlotLoadoutChanged`). Off: the rarest armor of
  every type is worn and the quick slots start empty; the player's choices keep being recorded. See "Starting loadout quick slot preferences" in `HUDQuickSlots.md`.

**Display** holds Brightness (`UMKHGameUserSettings::Brightness`, 0.7–1.3, default 1). It multiplies the engine
display gamma (`GEngine->DisplayGamma = 2.2 × Brightness`), which the tonemapper applies as `pow(Color, 2.2 / Gamma)`:
it lifts or sinks the midtones of the 3D scene only, because the UI is drawn after tonemapping. It leaves the fixed
exposure and the baked lighting alone (`PerformanceOptimization.md`) and costs nothing. The slider applies while dragging and
saves on release; `ApplyNonResolutionSettings()` re-applies it, and the game engine calls that at startup, so the saved value
is live from the first frame. It is on this page rather than Video because the Video page stages its edits until Apply, and
brightness has to be judged while moving the slider. In the editor `DisplayGamma` is global: a brightness tried in PIE also
changes the editor viewports until it is set back.

**Controller** holds the right stick's own Look Sensitivity and Invert Y Axis
(`GamepadLookSensitivity`, `bInvertGamepadYAxis`, read by `UBasicMovementComponent::LookGamepad()`) and Vibration
(`bEnableControllerVibration`). Vibration is written to `APlayerController::bForceFeedbackEnabled` of the local
controllers when it changes (`SetControllerVibrationEnabled`) and when `AMKHPlayerController` begins play, so any
force feedback the game plays respects it. Both sensitivity sliders share `MinMouseSensitivity` / `MaxMouseSensitivity`.

**Language** is not stored in `UMKHGameUserSettings`: the engine's current language is the source
of truth, read and switched through `UMKHLocalizationLibrary`. The values are the shipped cultures
(`GetAvailableLanguages()`, native English first), each shown by its own name ("English",
"Italiano", "Français", culture invariant). Stepping the option calls `SetLanguage()`, which in a
packaged game switches language and number locale and saves them to `GameUserSettings.ini`
(`[Internationalization]`); in the editor it drives the game localization preview instead. Reset
goes back to `GetDefaultLanguage()` (system language when shipped, English otherwise). Text on
screen updates on its own — see `Documentation/Localization.md`.

**Network Stats** (Off / Compact / Detailed) is `UMKHGameUserSettings::NetStatsMode`, read live by the
`W_NetStat` overlay (`UMKHNetStatsWidget`); the console variable `mkh.NetStats` overrides it. See
`NetworkDiagnostics.md`.

### UMKHOptionRowWidget

A setting whose value comes from a list, drawn as `‹ value ›`. It knows nothing about what the
values mean: the page pushes text in and is told when the player steps. Reusable by any future
page with discrete options. The arrows disable at the ends rather than wrapping.

**Values wider than the box** (audio device names have no length limit): `OptionValueText` sits in `ValueClipBox`
(a `SizeBox` with *Clip to Bounds*, holding the Fill slot and padding the text had). The text's Overflow Policy is
Ellipsis. When the measured text (`FSlateFontMeasure`, at `SetValue`) is wider than the text's laid-out width, the
value is left aligned (the engine draws the ellipsis only on left aligned text) so its start shows with "…", the full
value becomes the tooltip of the text (mouse) and of both arrows (the pad shows the focused widget's tooltip), and
while one of its buttons has the focus (`HasFocusedDescendants()`) or, with keyboard and mouse, the cursor is over the
row (`NativeOnMouseEnter` / `NativeOnMouseLeave`: the row is not hit-testable, so `IsHovered()` stays false) the value
scrolls: no ellipsis, moved with a render translation (layout untouched), ping-pong at `MarqueeSpeed` (45 units/s)
with `MarqueeEndPause` (1.2 s) at each end. Values that fit stay centred; the row height and the box do not change.
The row ticks (`NativeTick`) for this.

### UMKHInputSettingsLibrary

`GetInputUserSettings()`, `GetActiveKeyProfile()`, `GetKeyDisplayName()`. Every accessor
returns null rather than asserting, so a misconfigured build degrades to an empty list.

`GetActionKeyDisplayName(PC, Action, Device)` returns the key of one device; on-screen prompts call
`GetActionKeyDisplayNameForActiveDevice(PC, Action)` instead (see [Key prompts](#key-prompts)).
`GetKeyDisplayName` gives gamepad buttons their short Xbox label (`A`, `RB`, `LT`, `L3`, `↑`, `View`,
`Menu`), culture invariant like the engine key names; the settings rows and conflict messages use it too.
Per-device lookups: `IsKeyForDevice`, `IsMappingForDevice`, `FindMappingForSlot`, `GetHardwareDeviceIdName`.

### Key prompts

Prompts show the keys of the device in hand. `UMKHActiveInputDeviceSubsystem` (one per local player)
registers a Slate input pre-processor, so it sees every input before the game or the UI consumes it, in
menus as well as in game. A gamepad button, or a stick or trigger past 0.5, switches to Gamepad. A key, a
mouse button, the wheel or a mouse move of 2 px or more switches back. Slate never passes its own
synthesized mouse moves to pre-processors, so only real mouse use counts. The subsystem broadcasts
`OnActiveInputDeviceChanged` only on a switch.

`UMKHKeyHintWidget` (`W_KeyCap`), `UMKHIntroSkipHintWidget` and `UMKHPreparationSkipWidget` listen to it
next to `OnSettingsApplied` and call `GetActionKeyDisplayNameForActiveDevice`. When the action has no
gamepad button (Toggle HUD, Weapon 1/2) that function falls back to the keyboard key, so the prompt is
never empty.

In PIE with several players in one process, every player switches together (the pre-processor is
global). Typing a `py` command in the editor console also counts as keyboard use.

### UMKHInputUserSettings

Set as `UserSettingsClass`. Overrides `DetermineHardwareDeviceForActionMapping`: gamepad keys go to
the default gamepad device, the rest to the invalid device. An old save of the engine class is upgraded by
the engine's own class-change path, which copies the profiles as they are.

**Renamed mappings.** A profile saves the player's keys by **mapping name** (Player Mappable Key Settings → Name):
renaming an action's mapping name drops the key a player rebound under the old name (the default comes back). After a
release, migrate such keys in `RegisterKeyMappingsToProfile` with `MapPlayerKey` on the new name.

`RegisterKeyMappingsToProfile` (after the engine's) also logs a warning for any context mapping whose key the engine does not know (`WarnAboutInvalidKeys`). That is
how `IA_PushToTalk` first showed "(": its keys were written from Python with `Key.import_text('(KeyName="V")')`,
but an `FKey` imports a bare key name, so the key became "(". From Python: `k = unreal.Key(); k.import_text('V')` (or
`k.set_editor_property('key_name', 'V')`; the `unreal.Key(key_name=...)` constructor fails in 5.8).

---

## Runtime Sequences

### Opening the screen

```
IA_ToggleGameMenu → AMKHPlayerController::ToggleGameMenu
  → AMainHUD::ToggleGameMenu → (game menu Settings button) → CreateSettingsWidget()
      → W_SettingsScreen::NativeConstruct
          BuildPages()            one widget per SettingsPages entry, added to PageSwitcher
          BuildNavigationRail()   one nav entry per page
          BindActionBar()
          ShowPage(DefaultPageIndex)
              → page->ActivatePage() → TakeSnapshot() + RebuildKeybindList()
```

### Rebinding a key

```
click keycap → UMKHKeybindRowWidget::OnSlotClicked(MappingName, Slot)
  → HandleSlotClicked: spawn W_KeyCapture, AddToViewport(CaptureModalZOrder), BeginCapture()
  → player presses a key → OnKeyCaptured
      FindConflictingMapping()
        ├─ no conflict → ApplyPendingRebind(Key)
        └─ conflict    → ShowConflict(); Replace → OnConflictConfirmed → ApplyPendingRebind(Key)
              (the previous owner's slot is cleared first: a key is never on two actions)
  → WriteMapping() → MapPlayerKey() + ApplySettings()   [live, not saved]
  → bHasUnsavedRebinds = true → NotifyDirtyStateChanged() → action bar enables Apply/Revert
  → CloseCaptureModal() → focus returns to the page → RebuildKeybindList()
```

### Leaving the screen

```
Back button / game menu input action
  → RequestClose()
      page->IsBlockingClose() ? refuse (a capture is in progress)
                              : CloseSettings()
                                  DiscardPendingChanges() → page->RevertChanges()
                                  OnSettingsClosed → AMainHUD::OnSettingsClosed
                                      DestroySettingsWidget()
                                      GameMenuWidget->CloseGameMenu()  → focus back to gameplay
```

`Apply` is the only path that reaches `SaveSettings()`.

---

## How-To Guides

### Add a new settings page (Video, Gameplay, …)

1. Subclass `UMKHSettingsPageWidget` in C++ if the page needs native logic, or reuse the base
   class if the Blueprint can do the work.
2. Create the Blueprint and set `PageDisplayName` — the rail label comes from there.
3. Override `ApplyChanges` / `RevertChanges` / `ResetToDefaults` / `HasPendingChanges`. If the
   page applies and saves on the spot, override `UsesActionBar()` to return `false` instead.
4. Add the Blueprint to `SettingsPages` on `W_SettingsScreen`. Nothing else is needed: rail
   entry, switcher slot and action bar routing are built from that array.

### Reorder the keybind list

On `W_KeyboardSettings` (Class Defaults, *Settings | Keybinds*):

- `CategoryOrder` — section display names, in the order wanted
- `MappingOrder` — mapping names, in the order wanted inside their own category

Anything not listed follows alphabetically, so new actions appear at the end of their section
until they are pinned explicitly.

### Hide an action from the screen

Preferred: leave its **Player Mappable Key Settings** at `None` — the action never enters the
key profile at all. Alternatively add its mapping name to `ExcludedMappingNames`.

### Change colours and fonts

Most colours live on the widgets themselves (Brush Color / Color and Opacity / Style → Tint).
Colours driven at runtime live in Class Defaults instead, and setting them on the widget has
no effect because the code overwrites them:

| Property | Class |
|---|---|
| `NormalLabelColor`, `SelectedLabelColor` | `W_SettingsNavButton` |
| `IdleFooterColor`, `DirtyFooterColor` | `W_SettingsScreen` |
| `DefaultKeyColor`, `CustomizedKeyColor`, `UnboundKeyColor` | `W_KeybindRow` |

Fonts: every text is an MKH Text whose typography comes from a `TS_` text style (e.g.
`TS_Title`, `TS_Label`, `TS_Value`); change the style, not the widget. Catalogue and rules in
`Documentation/UITypography.md`.

---

## Gotchas & Troubleshooting

**Hold-to-act actions need one mapping, not two.** An action held down (Block, Walk) must
have a **single** key mapping with an **empty Triggers list**. The implicit *Down* trigger
gives `Started` on press and `Completed` on release, which is what `BindAbilityActions` binds.

Two historical mistakes to avoid:

- *Two mappings* (`Pressed` + `Released`+`Negate`) produce two `FPlayerKeyMapping` entries for
  the same action, so the settings screen shows the same key in both Primary and Secondary.
  Worse, rebinding writes to one slot only, leaving press and release on different keys.
- *An explicit `Pressed` trigger* is one-shot: the action completes in the same frame it
  fires, so the release handler runs immediately instead of on key up.

**The list is empty and `LogMKHInput` warns about the key profile.** Either
`bEnableUserSettings` is off in `DefaultInput.ini`, or the mapping context was never
registered. Both are covered in [Configuration](#configuration).

**An action is missing from the list.** It has no Player Mappable Key Settings, its mapping
name is in `ExcludedMappingNames`, or its context is not registered.

**`SizeBox` dimensions are ignored.** Override properties are a value *plus* a flag:
`WidthOverride` does nothing unless `bOverride_WidthOverride` is true. The designer sets both
when the checkbox is ticked; anything writing the property directly must set both.

**Keycap columns drift apart.** Column width comes from the `SizeBox` around each keycap
(`PrimaryKeyBox` / `SecondaryKeyBox`), not from the text. `MinDesiredWidth` alone lets a long
key name widen the button, push the column sideways and wrap the label, which in turn
squeezes `RowSeparator` out of the row. Keep the box width and the category caption widths in
sync.

**A slider is invisible but still draggable.** `USlider` draws its bar and handle from the
brushes in `WidgetStyle`, and each brush needs three things to render: `DrawAs = RoundedBox`
(there is no texture), `ImageType = FullColor`, and a tint whose **alpha is not zero**. Miss any
one and the widget lays out and reacts to the mouse while drawing nothing. `SliderBarColor` and
`SliderHandleColor` are multiplied on top, so a stray value there hides the brush as well. The
audio page sidesteps the bar entirely by drawing it with two Borders and keeping only the handle
from the slider itself.

**Colours look washed out.** `FLinearColor` is linear. Typing sRGB values straight into the
struct makes everything far too bright — use the colour picker's Hex sRGB field, which
converts for you.

**Escape or the window close button behaves oddly.** The screen deliberately does not handle
raw key events or focus changes. An earlier version closed on `Escape` and on focus loss; the
first swallowed the window-close click, the second fired on alt-tab and title-bar clicks.
Closing goes through `RequestClose()` only.

**A video setting seems not to apply.** The video page stages everything; only Apply writes to
`UGameUserSettings`. Also note that two scalability groups do nothing in this project:
`foliage.DensityScale` needs instanced foliage or landscape grass (there is none), and
`r.ViewDistanceScale` / `r.SkeletalMeshLODBias` need cull distances and mesh LODs to act on.
Both are left out of the page for that reason. Effects quality, besides the engine's effects, sets how many arena
spectators are drawn (Low about a third, Medium two thirds, High / Epic all): see "Crowd reactions" in
`MatchFlowArchitecture.md`.

**A controller rebind came back by itself.** A gamepad slot was emptied: the engine then applies the
context's default key again. Pages never empty a gamepad slot (swap instead of clear); anything else writing
to the profile must do the same.

**The same key shows up on the keyboard and the controller page.** Something used
`FindCurrentMappingForSlot()`: it matches the slot only, and keyboard and gamepad both have a First slot. Use
`UMKHInputSettingsLibrary::FindMappingForSlot()` with the device.

**A rebind is lost after closing.** That is by design: only Apply writes to disk. Because
rebinds are applied live, `DiscardPendingChanges()` restores the snapshot rather than merely
skipping the save.

**Deprecated Enhanced Input overloads.** Since UE 5.6 the `FGameplayTag` profile-id overloads
(`GetCurrentKeyProfile`, `SetKeyProfile`, `ResetKeyProfileToDefault`) are deprecated. Use the
`FString` versions: `GetActiveKeyProfile()`, `SetActiveKeyProfile()`,
`ResetKeyProfileIdToDefault()`.

**Reserved property names.** `Slot`, `Visibility` and `CategoryName` are members of `UWidget`;
reusing them as parameter names fails to compile.

---

## Roadmap

- **Controller follow-ups** — button icons instead of text labels (menu navigation with the pad is done:
  `GamepadMenuNavigation.md`); actual vibration effects (the Vibration setting is in place, nothing
  plays force feedback yet).
- **Video and Gameplay pages** — both are array entries once the pages exist.
- **Mouse sensitivity and invert Y** — currently properties on `UBasicMovementComponent`;
  they belong on a Gameplay page backed by `UMKHGameUserSettings`.
- **Shared UI theme asset** — colours currently live across six widgets and two materials. A
  theme Data Asset would centralise them.
- **Hover feedback on rail entries** — the button draws nothing in any state, so unselected
  entries have no hover response.
