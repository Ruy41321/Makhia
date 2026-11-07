# Steam Deck

Makhia runs on the Steam Deck and on Linux PCs through **Proton** with the Win64 build; there is no native Linux
build (decision and history: `Handoff/SteamDeckProtonHandoff.md`). This file holds the Deck-specific model; the
controller and the menu navigation are shared with Windows (`SettingsArchitecture.md`, `GamepadMenuNavigation.md`).

## Table of Contents
- [Detecting the Deck](#detecting-the-deck)
- [Handheld profile](#handheld-profile)
- [Screen and text](#screen-and-text)
- [On-screen keyboard](#on-screen-keyboard)
- [Testing without a Deck](#testing-without-a-deck)
- [Testing on the Deck before the dedicated AppID](#testing-on-the-deck-before-the-dedicated-appid)
- [Session on the Deck](#session-on-the-deck)

## Detecting the Deck

Only the online backend talks to the platform (`OnlineBackends.md`): `UMKHOnlineBackend::IsRunningOnHandheld`
on Steam returns true when the `SteamDeck=1` environment variable is set (the Steam client sets it for every game it
starts on a Deck; Proton passes it on), else `ISteamUtils::IsSteamRunningOnSteamDeck` after starting the Steam
subsystem; false elsewhere. The game reads it through `UMKHOnlineServicesSubsystem::IsRunningOnHandheld`; outside
Shipping, `-MKHHandheld` on the command line forces true.

Why both checks: in the first Deck playtest (2026-10-28) the first launch got the benchmark profile (Epic, 144 FPS,
1024×768 borderless) instead of the handheld one. The first-launch check runs in `UMKHGameInstance::OnStart`,
before the sign-in starts the Steam subsystem, so `SteamUtils()` was most likely still null (no Deck log to confirm
it; the log line `RunFirstLaunchAutoDetect: Handheld profile` confirms the fix). The environment variable needs no
Steam API; the subsystem start covers a Deck started some other way.

## Handheld profile

`UMKHGameUserSettings::StageAutoDetectedQuality(bIsHandheld)` stages, on a handheld PC, the handheld profile instead
of the hardware benchmark (whose score under Proton says little about the Deck):

| Setting | Value | Where |
|---|---|---|
| Overall quality | Medium (`HandheldQualityLevel` = 1) | `MKHGameUserSettings.h` |
| Frame rate limit | 40 FPS (`HandheldFrameRateLimit`), chosen by the user on 2026-10-22: the Deck screen can run at 40 Hz | `MKHGameUserSettings.h` |
| Window | Borderless fullscreen at the desktop resolution (1280×800 on the Deck screen) | `StageHandheldProfile` |

It runs in the same two places as the benchmark: once on the first launch (`RunFirstLaunchAutoDetect`, guarded by
`bHasAutoDetectedQuality`, from `UMKHGameInstance::OnStart`) and on **Restore defaults** of the video page. The
player can change everything afterwards on the video page, which offers 30 and 40 FPS caps for the Deck.

The values are a starting point: they are measured and tuned in the session on the Deck (below).

First Deck playtest (2026-10-28, Development package, profile not applied, so all Epic at 1024×768): a stable
**40 FPS** in Makhion matches, about 20 FPS only in the intro cinematic (whole map in view, initial streaming),
judged acceptable since nobody plays during it. Gamepad complete; the missing stick dead zone was added
(`GamepadMenuNavigation.md`, Input contexts). To retest the profile on a Deck that already ran the game, delete
`Makhia\Saved\Config\Windows\GameUserSettings.ini` in the package folder first: the profile is applied once.

## Screen and text

The UI scales with the engine default DPI curve (shortest side): at 1280×800 the scale is ~0.74, so the layout sees
a ~1730×1080 canvas, 10% narrower than the 1920×1080 it was designed on. Checked in PIE in a 1280×800 window on
2026-10-22: lobby (lineup, skin panel, mode cards, account chip), game menu with friends and voice, all settings
pages, inventory, HUD and match over fit without clipping.

A font size N renders at about N px at 800p (N × 96/72 × 0.74). Valve asks for at least 9 px at 1280×800: every
`TS_*` style is at least 10 except `TS_OverlineSmall` (9, table column headers such as Channel / Level on the Audio
page), which is borderline. Check readability on the Deck before changing it (`UITypography.md`).

Known, not Deck-specific: the inventory dashboard overlaps the HUD skill slots on the right (also at 1920×1080).

## On-screen keyboard

Not needed with Steam: no text field is reachable. `GuestNameBox` (`W_LoginPanel`) needs the `Guest` auth method,
which Steam does not offer, and `AddFriendNameBox` (`W_FriendList`) needs `bSupportsInGameFriendRequests`, false on
Steam. A new text field on a Steam screen would need `ISteamUtils::ShowFloatingGamepadTextInput` through the backend.

## Testing without a Deck

Use a **Development** package (the switch is compiled out of Shipping). A package that is not installed keeps its
`Saved` folder next to the game: `<package>\Windows\Makhia\Saved`.
1. Delete `Makhia\Saved\Config\Windows\GameUserSettings.ini` so the first-launch path runs again (the profile is
   applied only once, like the benchmark).
2. Start `Makhia\Binaries\Win64\Makhia.exe -MKHHandheld -windowed -ResX=1280 -ResY=800`. `-MKHHandheld` only picks
   the profile; on a 1920×1080 monitor "borderless at the native resolution" is still 1920×1080, so add the window
   switches to see the Deck screen size.
3. `Makhia\Saved\Logs\Makhia.log` shows `RunFirstLaunchAutoDetect: Handheld profile, overall quality 1, frame rate
   limit 40`; the video page shows Medium and 40.

Verified on 2026-10-22 with the Development package: the log line above appears and `GameUserSettings.ini` has
every `sg.*` at 1 and `FrameRateLimit=40`.

## Testing on the Deck before the dedicated AppID

Possible with the development AppID 480, as on Windows today; use a **Development** package: only non-Shipping
builds write `steam_appid.txt` (`SteamDevAppId`) next to the binary, and without it a Shipping build started as a
non-Steam game cannot initialize Steam.
1. Deck in Desktop Mode: copy the package folder (USB stick or network share).
2. Steam → *Add a Non-Steam Game* → `Makhia\Binaries\Win64\Makhia.exe` (the real binary). Properties →
   Compatibility → force a Proton version (latest stable; Experimental if it fails).
3. Back in Game Mode, start it from the library; no launch option needed (the Deck is detected for real).
4. Online works against Windows PCs that also run on 480, like the current tests.

Not testable before the AppID: the default Steam Input layout and the launch options set in Steamworks (with a
non-Steam shortcut pick the controller layout by hand: *Gamepad*), and the Verified review. None of them needs the
Deck in hand: Valve runs the review on its own Decks.

## Session on the Deck

A single session on a borrowed Deck, with a Development package (see the section above when the AppID is not there
yet; with the AppID also the Shipping build from a Steam beta branch). Check, in order:
1. **Start**: the game starts under the default Proton; DX12 through vkd3d-proton works (else try `-dx11` and note it
   for the Steamworks launch options). No launcher, no keyboard or mouse needed from start to match.
2. **Profile**: first launch picks Medium / 40 FPS / 1280×800 (log line above).
3. **Performance**: `stat fps` and `stat unit` in the lobby, a match against the bot in Makhion and an online match;
   if 40 FPS is not stable, try Low and the resolution scale, then update the constants and the table above.
4. **Input**: Steam Input default layout (gamepad) drives everything; on-screen prompts show the pad labels.
5. **Readability** of the smallest texts (settings descriptions, `TS_OverlineSmall` headers) at arm's length.
6. **Online**: Steam sign-in, overlay (Steam button), invites, a match against a Windows PC, voice chat.
7. **Suspend and resume** in the lobby and during a match; audio, intro videos, saved settings.
Write the results in `Handoff/SteamDeckProtonHandoff.md` section 8.
