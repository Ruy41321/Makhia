# Gamepad Menu Navigation

Every menu of the game can be driven with a controller alone (Steam Deck): the lobby, the game menu and its
credits window, the settings screen (rebinding included), the inventory, the end-of-match panel, the login panel
and the travel error dialog. The mouse keeps working as before; the two can be swapped at any moment.

Built on the last-used device (`UMKHActiveInputDeviceSubsystem`, see `SettingsArchitecture.md` → "Key prompts")
and on Slate's own focus and directional navigation. No CommonUI: the existing widgets stay as they are.

---

## Pieces

| Piece | Where | Role |
|---|---|---|
| `UMKHMenuNavigationSubsystem` | `UI/Navigation/` (LocalPlayer subsystem) | Keeps the focus on the screen on top, confines navigation to it, draws the focus frame and the focused widget's tooltip, scrolls with the right stick, hides the cursor |
| `SMKHFocusHighlightLayer` | `Private/UI/Navigation/` | Hit-test invisible viewport layer (Z `HighlightLayerZOrder` = 1100, above every menu and modal — the key capture and video confirm modals sit at 1000 — and hidden while the loading screen is up) drawing the frame and hosting the tooltip |
| `UMKHMenuNavigationSettings` | Project Settings → Game → Makhia Menu Navigation | Frame colour / thickness / padding / radius, tooltip gap (0: touching), right stick speed and dead zone, check interval, layer Z order |
| `IMKHNavigableScreen` | `UI/Navigation/MKHNavigableScreen.h` | Optional C++ hook: `WantsRootFocus()` for screens that read the pad buttons themselves |
| `FMKHSelfFocusHighlightMetaData` | `UI/Navigation/MKHFocusHighlightMetaData.h` | Slate tag of widgets that show their own focus (no frame drawn around them) |
| `UMKHMenuNavigationLibrary` | `UI/Navigation/` | `IsGamepadNavigationActive`, `GetPopupPosition`, `IsGamepadBackKey`, `IsGamepadAcceptKey` (Blueprint too) |

The subsystem acts only while **a menu has the input and the gamepad is the last device used**
(`IsGamepadNavigationActive`). "Menu has the input" is set by the controllers: `AMKHLobbyPlayerController` for the
whole lobby, `AMKHPlayerController::ChangeFocus` while a UI screen is open in a match. With the mouse it does nothing.

---

## The screen on top

Checked every `FocusCheckInterval` (0.05 s):

1. **Screen on top** = the visible widget on the viewport with the highest Z order (ties: the one added last,
   tracked through `UGameViewportSubsystem::OnWidgetAdded`) that has something to focus, or whose root is focusable.
   Toasts and the HUD overlay have nothing to focus and are skipped; the boot overlay sets itself focusable so it
   blocks the lobby underneath.
2. A **modal nested inside a screen** (not on the viewport by itself) is pushed with `PushModalScreen` /
   `PopModalScreen`: today only the Credits window (`UMKHCreditsWidget::OpenCredits` / `CloseCredits`).
3. When the screen on top changes (or the pad takes over), the focus is **placed**:
   - a screen with `WantsRootFocus()` keeps the focus on its root (key capture while listening, quantity popup);
   - same screen, the pad just took over: the focus stays where it is;
   - otherwise the widget **remembered** for that screen (closing a popup brings you back where you were);
   - a focused user widget that holds focusable widgets (a container, e.g. the bag the inventory focuses on open,
     a settings page root) hands the focus to its first focusable child;
   - else the screen's **Desired Focus Widget** (Blueprint class defaults → Interaction), else the **first
     focusable widget** in layout order (collapsed / hidden / hit-test-invisible subtrees, inactive switcher pages
     and disabled widgets skipped; list views give their displayed entries), else the root.
4. A placement that does not land (the screen was added this frame and is not laid out yet) is retried for a few
   checks. Focus on the bare root of a screen is always moved on to a real widget (a screen may focus itself one frame
   after our placement, as `UMKHSettingsScreenWidget` does), unless the screen `WantsRootFocus()`.
5. Afterwards, whenever the focus leaves the screen or its widget disappears, it is placed again: when the widget
   was destroyed on the same screen (a settings page rebuilds its rows after every change), the system waits two
   checks for the new widgets to be laid out and picks the one closest to where the focus was, same row first.
6. A focused widget that becomes **disabled** (the option arrow at the last value) hands the focus to the closest
   enabled widget, same row first: Slate does not navigate from a disabled widget.
   While a combo box or context menu is open nothing is checked.

`RequestFocusInside(Container)` moves the focus into a container once it is laid out (settings LB/RB).

### Navigation inside a screen

**Vertical lists** (settings pages): inside a vertical `UScrollBox`, up/down go **row by row** in list order
(`HandleListNavigation`, a Custom navigation rule on the scroll box content): the next row with an enabled focusable
widget, the one horizontally closest to the current column, scrolled into view. The list is the first vertical box
met after leaving the row's user widget. Slate's own search keeps to the column of the focused button and skipped
rows whose button in that column was disabled (left arrow at the first value).

The root of the screen on top gets Slate navigation metadata with a **custom boundary** in the four directions:
navigation never leaves the screen (no jumping into the lobby behind the game menu), and when Slate's row/column
search finds nothing, `HandleBoundaryNavigation` picks the nearest focusable widget in that direction (distance
along the direction + twice the offset across it). So irregular layouts (lobby cards vs. the skin panel vs. the
account chip) are reachable without explicit navigation rules in the Blueprints.

Left stick and D-pad navigate; A (Accept) presses buttons; sliders follow the engine's controller lock (A to grab,
left/right to change, A to release; `StepSize` 0.05 on the settings sliders). `ScrollWhenFocusChanges =
AnimatedScroll` on the settings page lists keeps the focused row on screen.

---

## Look

- **`UMKHButton`**: with the gamepad the focus plays the part of the hover. The focused button shows the hovered
  brush and content tint and **broadcasts `OnHovered` / `OnUnhovered`** (with the hover sound), so every hover
  reaction (skin preview on the lobby tiles, mode card states) works on the pad too. The hover of the hidden cursor
  is ignored. Without a `ButtonStyle` asset the designer Style is the base. It tags its Slate button with
  `FMKHSelfFocusHighlightMetaData`.
- **Everything else** (plain `UButton`, list rows, sliders, combo boxes, text boxes, inventory entries) gets the
  **focus frame**: a rounded outline (`FocusFrameColor`, default brass) drawn around the focused widget. Screen
  roots never get one. The engine's own dashed focus rectangle is off (`RenderFocusRule=Never` in
  `DefaultEngine.ini`).
- **Tooltips**: the tooltip widget of the focused widget, or of the first descendant that has one (inventory
  entries set it on their `MainBox`), is shown next to it (right, or left when it would leave the screen). The mouse
  tooltips are unchanged.
- **Cursor**: hidden while the pad drives a menu, shown again on mouse input (only if a menu still has the input).

---

## Back and shortcuts

B (`Gamepad_FaceButton_Right`) is the pad's Esc and is handled **where the focus is**, never through an input
action (B is Dodge in game and every pad button can be rebound):

| Screen | B | Other |
|---|---|---|
| `UMKHGameMenuWidget` | Closes the credits window, else the menu | — |
| `UMKHSettingsScreenWidget` | `RequestClose()` (like Esc / Back) | LB / RB: previous / next page, focus into the page |
| `UInventoryDashboardWidget` | Closes the dashboard | — |
| `UMKHInventoryActionMenuWidget` (`W_ActionMenu`) | Closes the menu | — |
| `UMKHInventoryEntryWidget` (`W_InventoryEntry`) | — | A: opens the action menu (like right click) |
| `UMKHQuantityPopupWidget` | Cancel | A confirms, left / right change the quantity |
| `UMKHLoginPanelWidget` | Close | — |
| `UMKHTravelErrorWidget` | OK | A: OK |
| `UMKHKeyCaptureWidget` | Keyboard capture: cancel | See below |

Start (Menu) and View stay on the game menu / inventory actions of `IMC_UIOpenMapping`.

### Inventory

- Every `W_InventoryEntry` / `W_InventoryWeaponEntry` is focusable (CDO and the instances in `W_Equipment` /
  `W_WeaponLoadout` / `W_QuickSlotArea`); bag entries are `TileView` rows. The empty cells of the bag are hit test
  invisible: they never take the focus. The skill slots of the weapon rows are focusable: A takes a skill, A on another
  slot of the row places it, B puts it back (`InventoryDashboard.md`, "Skill order").
- A on an entry opens `W_ActionMenu` next to the entry (`UMKHMenuNavigationLibrary::GetPopupPosition` in its
  Construct: cursor with the mouse, top right corner of the entry with the pad). Its buttons cover quick slot,
  use / equip, split and drop, in this order, so no drag & drop is needed and a quick double A quick-slots or
  equips the item (the focus lands on the first visible button).
- **Quick slot choice with the pad**: instead of the generic "Add to QuickSlot" (automatic slot), the menu shows one
  button per slot — "QuickSlot 1/2" for weapons, "QuickSlot 1/2/3" for consumables
  (`UInventoryDashboardWidget::GetGamepadQuickSlotChoice` / `GetQuickSlotChoiceLabel`, list from
  `UInventoryDashboardController::FindQuickSlotChoices`). The slot the item already holds is left out; a weapon whose
  type is already quick-slotted only gets that slot (one weapon per type, as with the mouse). The generic button
  stays only to remove a quick-slotted item (`ShowsGenericQuickSlotButton`). With the mouse the menu is unchanged.
- **Tooltips**: entries build their tooltip through their own `ToolTipWidgetDelegate` (`GetToolTipWidget`, bound in
  C++), which Slate only evaluates on mouse hover; `FindToolTip` runs the delegate itself, so the focused entry shows
  its item tooltip next to it. It returns nothing while the action menu is open. The mouse uses the same layer through
  `ShowAnchoredToolTip`, so both tooltips stand in the same place. See
  `InventoryDashboard.md`.
- **Item leaving the bag** (dropped, equipped, quick-slotted): Slate leaves the focus on the bag `TileView` itself
  when its focused row goes. A focused list is a container for the navigation subsystem (`FindFocusableInList`):
  the focus moves to the row now closest to where it was (the last place of each screen is kept in
  `RememberedFocusRects`, so this also works when the action menu was on top), or next to an empty list. The
  focusable `W_Bag` (the dashboard's first focus target) is a container too, even when its list is empty: with no
  items in the bag the focus goes to the closest equipment / quick slot entry, never to the bag itself. This also
  applies when the bag is the screen's first focus target (`PlaceFocus` resolves the target through
  `FindFocusableInside`). Rows of items that already left a list stay among `GetDisplayedEntryWidgets` until the
  list regenerates (off screen, they refuse the focus): `ForEachShownWidget` skips them (`IsLiveListEntry`). A focused
  list never shows a tooltip (`FindToolTip`), so no row's tooltip appears without its focus.
- **Split with the pad**: there is no cursor to drag the split with, so the confirmed quantity goes straight to a
  consumable quick slot: the first empty one, else the first slot (its stack goes back to the bag)
  (`UInventoryDashboardController::FindConsumableQuickSlotForSplit`).

### Rebinding (`UMKHKeyCaptureWidget`)

Every pad button can be bound, so no button can mean "cancel". A **gamepad capture commits on release**; holding the
button for `HoldToCancelSeconds` (1 s) cancels instead (the prompt reads "Controller button (hold to cancel)").
On a keyboard capture the pad's B cancels (no pad button can be stored there anyway). The modal keeps the focus on
its root while it listens (`WantsRootFocus`); on a conflict the focus moves to Replace / Cancel.

---

## Input contexts

`IMC_UIOpenMapping` (UI open in a match, and the whole lobby) keeps the **same left stick movement mappings as
gameplay** (`Gamepad_LeftX/Y` with `Half Axis (Makhia)`) but **no gamepad jump**. `UMKHInputModifierHalfAxis` reads
zero while the gamepad drives a menu, so the stick navigates without moving the character.

Why the stick mappings must stay: when the context switches back to gameplay, Enhanced Input marks every **new**
mapping of a Boolean action whose key is held as ignored until the key is released, and an analog stick with a bit of
drift never reports "released" (its raw value never hits exactly 0). Without the stick mappings in the UI context,
closing a menu could leave forward/back dead until the menu was opened again. Mappings present in both contexts with
the same modifiers are "rebuilt", not new, and keep working. Keyboard movement in UI mode is unchanged.

**Stick dead zone** (2026-10-28, after the Steam Deck playtest showed camera and character creeping with the stick at
rest): every stick mapping starts with the engine `Dead Zone` modifier, lower threshold 0.2, upper 1.0 (values below
the lower threshold read 0, the rest is rescaled to 0–1). `Gamepad_LeftX/Y` (movement, in both contexts) use
**Axial**, `Gamepad_Right2D` (`IA_LookGamepad`, only in `IMC_BaseMapping`) uses **Radial**, placed before
`ScaleByDeltaTime`/`Scalar`. Enhanced Input ignores the `AxisConfig` dead zones of `DefaultInput.ini`, so the modifier
is the only dead zone. A new stick mapping gets the same modifier first, with the same values in both contexts (see
above: different modifiers would make the mapping "new" on the context switch).

---

## Adding a screen

- A widget added to the viewport is a screen automatically. Set its **Desired Focus Widget** if the first focusable
  widget in layout order is not the right start.
- Full-screen "click outside to close" buttons must have **Is Focusable** off (`BackdropButton` of W_LoginPanel and
  W_Credits, `CloseMenuButton` of W_GameMenu), or they would take the focus.
- A modal shown inside another screen: `PushModalScreen` when it opens, `PopModalScreen` when it closes.
- Handle B in the screen's `NativeOnKeyDown` (`UMKHMenuNavigationLibrary::IsGamepadBackKey`), next to Esc.
- A popup that reads the pad itself: implement `IMKHNavigableScreen::WantsRootFocus`.
- Prefer `UMKHButton`; any other focusable widget gets the frame.

---

## Testing

PIE + SlateInspector `PressKey` with gamepad key names (`Gamepad_DPad_Down`, `Gamepad_FaceButton_Bottom`, ...)
reaches Slate (navigation, buttons, the device subsystem) but not Enhanced Input. Typing a `py` console command is
keyboard input and flips the device back to keyboard: read the focus with a `register_slate_post_tick_callback`
logger started before the pad presses, or with screenshots.

Verified in PIE (2026-10-18): lobby (start on the offline card, skin tiles with preview on focus, account chip,
game menu open / B back to the chip), match game menu, settings (LB / RB, rows, B), inventory (bag ↔ equipment,
A → action menu next to the entry, B back to the entry with its tooltip, split 3/5 to the first quick slot).
Not verified: a real pad and the Steam Deck (stick navigation repeat, hold-to-cancel feel), the login panel,
the travel error dialog, the end-of-match panel, the friend list with entries, the key capture modal. Fixed and verified 2026-10-20: frame visible on the key
capture conflict modal (Replace / Cancel), settings open straight on the first rail entry, up/down row by row from
either option arrow, focus kept on the row when an arrow disables itself (Invert Y Off ↔ On). The stick movement fix
(Input contexts) cannot be checked with SlateInspector, which does not reach Enhanced Input: real pad test.

---

## Roadmap

- On-screen keyboard for text fields (`AddFriendNameBox`, `GuestNameBox`) through the backend
  (Steam `ShowFloatingGamepadTextInput`): milestone M4 of `Handoff/SteamDeckProtonHandoff.md`.
- Button hints in the other menus: `W_GamepadHint` (`W_KeyCap` + label, today in the inventory) is the widget to reuse.
- Option rows: left / right on the row itself instead of the `<` `>` buttons.
