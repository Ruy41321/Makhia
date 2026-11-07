# UI Typography

## Table of Contents

1. [Overview](#overview)
2. [Font Families and When to Use Them](#font-families-and-when-to-use-them)
3. [Style Catalogue](#style-catalogue)
4. [How It Works](#how-it-works)
5. [Rules](#rules)
6. [Exceptions](#exceptions)
7. [How-To Guides](#how-to-guides)
8. [Gotchas & Troubleshooting](#gotchas--troubleshooting)

---

## Overview

Every text in the game takes its typography from a shared **text style** asset instead of
setting a font by hand. A style fixes font family, weight, size, letter spacing, outline and
case; editing the style restyles every text that uses it.

```
Content/Makhia/UI/Fonts/
├── Faces/            FontFace assets (one per TTF weight)
├── F_Barlow          Composite fonts: named typefaces (Regular, Medium, ...) + fallback
├── F_Marcellus
├── F_JetBrainsMono
├── Styles/TS_*       UMKHTextStyle data assets: the type scale
└── DataTable/DT_TooltipText   Rich text styles (tooltips)

Source/Makhia/Public/UI/Text/
├── MKHTextStyle.h    UMKHTextStyle  (UDataAsset: Font + TextTransform)
└── MKHTextBlock.h    UMKHTextBlock  (UTextBlock + TextStyle, palette name "MKH Text")
```

Three families only, each with one job. Colours are **not** part of the system: they depend on
context (rarity, selection, state) and stay on the widgets.

---

## Font Families and When to Use Them

| Family | Faces | Role | Use for | Never use for |
|---|---|---|---|---|
| **Marcellus** | Regular | The game's voice: engraved classical serif | Screen and panel titles, section headers, dialog titles, big moments (Victory/Defeat, VS) | Body text, buttons, numbers (its figures are old-style and uneven), anything under ~13 pt |
| **Barlow Semi Condensed** | Regular, Medium, SemiBold, Bold | The workhorse: every piece of interface text | Labels, menu entries, item names, buttons, descriptions, hints, HUD text over the 3D scene, damage numbers, score | Numbers that change every frame or second (its figures are proportional and jitter) |
| **JetBrains Mono** | Medium, Bold | Data: fixed-width figures | Timers, cooldowns, live values (volume %, ping, quantities, stat values), key caps | Words and sentences |

Weight ladder inside Barlow: Regular = reading text, Medium = labels and list entries,
SemiBold = buttons, Bold = HUD over the scene and emphasis. No italics anywhere.

Case: titles, section headers, buttons, overlines, HUD prompts and key caps are shown upper case
(through the style, never typed in capitals in the text itself); everything else is sentence case.

---

## Style Catalogue

Sizes are Slate points. `sp` = letter spacing (1/1000 em). Outline colour is black.

### Marcellus

| Style | Spec | Used by |
|---|---|---|
| `TS_Display` | 78, sp 180, UPPER, outline 2 (α 0.85) | Match result title (`W_MatchOverPanel`), game name on the boot overlay (`W_BootOverlay`) |
| `TS_Versus` | 44, sp 100, UPPER | Unused since the pediment scoreboard |
| `TS_Title` | 24, UPPER | Settings screen and page titles, game menu panel titles, login panel title |
| `TS_Heading` | 17, UPPER | Dialog titles, inventory section labels (`W_Label`), lobby mode titles (`W_ModeSelector`) |
| `TS_Section` | 13, UPPER | Section headers inside a page (keybind categories, Volume) |

### Barlow Semi Condensed

| Style | Spec                                       | Used by |
|---|--------------------------------------------|---|
| `TS_ItemNameLarge` | SemiBold 20                                | Item name on the world pickup card |
| `TS_Nameplate` | SemiBold 20, sp 20                         | Player name on the lobby nameplate (`W_PlayerStatus`) and on the match scoreboard (`W_MatchInfo`) |
| `TS_ItemName` | Regular 16                                 | Item name in inventory banner and quantity popup |
| `TS_ButtonLarge` | SemiBold 20, sp 120, UPPER                 | Full-screen menu buttons (match over, game menu, lobby) |
| `TS_ButtonMedium` | SemiBold 14, sp 60, UPPER                  | Popup buttons (quantity popup), login panel buttons, card "Sign in" |
| `TS_Button` | SemiBold 11, UPPER                         | Settings action bar and dialog buttons, account chip and dev login buttons |
| `TS_LabelLarge` | Medium 16                                  | Navigation entries, context menu entries, stat names, player names in lobby, account chip name |
| `TS_Label` | Medium 14                                  | Settings row names, lock reason of a mode card |
| `TS_Status` | SemiBold 18                                | Status lines (match phase, matchmaking status, boot overlay) |
| `TS_Body` | Regular 12                                 | Hints, dialog text, footers, mode card descriptions, login panel status |
| `TS_Caption` | Regular 10                                 | Secondary descriptions under a label, login panel hints and name validation |
| `TS_Overline` | Medium 12, sp 100, UPPER                   | Small category labels (item category, "PAGES", ready state, account method badge, "or" / "Choose a name" in the login panel, dev login header) |
| `TS_OverlineSmall` | Medium 9, UPPER                            | Table column headers |
| `TS_HUD_Name` | Bold 36                                    | Score in the match HUD |
| `TS_HUD_Banner` | Bold 22, sp 120, UPPER, outline 1 (α 0.85) | Ready count during preparation |
| `TS_HUD_Prompt` | Bold 16, sp 60, UPPER, outline 1 (α 0.85)  | Action prompts ("Press T...", "Pick up", "Drop to equip") |
| `TS_Damage` | Bold 24, outline 1                         | Floating damage numbers |

### JetBrains Mono

| Style | Spec | Used by |
|---|---|---|
| `TS_HUD_Timer` | Bold 32 | Phase timer |
| `TS_HUD_Cooldown` | Bold 24 | Cooldown countdown on skill and consumable slots |
| `TS_HUD_Key` | Bold 14, UPPER | Key caps in the HUD (slot keys, pickup key, Tab hint) |
| `TS_HUD_Counter` | Bold 14 | Quantities, perk timers, ping |
| `TS_ValueLarge` | Medium 16 | Stat values and quantity in the inventory |
| `TS_Value` | Medium 12 | Setting values (option value, volume %) |
| `TS_ValueSmall` | Medium 11 | Key caps in the keybind list, section counts, guest name counter |

To find every widget using a style: *Reference Viewer* on the `TS_` asset.

---

## How It Works

- **`UMKHTextStyle`** (`Public/UI/Text/MKHTextStyle.h`) — `UDataAsset` with `Font`
  (`FSlateFontInfo`: family, typeface, size, letter spacing, outline) and `TextTransform`.
- **`UMKHTextBlock`** (`Public/UI/Text/MKHTextBlock.h`) — `UTextBlock` subclass, shown in the
  palette as **MKH Text** under *Makhia*. Its `TextStyle` property wins over `Font` and
  *Transform Policy*:
  - `SynchronizeProperties` applies the style every time the Slate widget is (re)built, at
    runtime and in the designer, so a changed style reaches every widget without resaving it;
  - `PostEditChangeProperty` also copies the style into the saved `Font` when `TextStyle` is
    picked in the Details panel, so the designer shows the real values;
  - `SetTextStyle` (BlueprintCallable) switches style at runtime.
  With no style assigned it behaves exactly like a plain Text Block.
- **Default font** — `DefaultEngine.ini`, `[SlateStyle] DefaultFontName` points at `F_Barlow`.
  It is the font every *new* UMG text widget (Text Block, Editable Text, Combo Box, Spin Box...)
  starts with, so nothing falls back to Roboto any more. Widgets whose font was left at the
  default follow this setting automatically (the default value is not saved in the asset).
- **Fallback glyphs** — every composite font has `NotoSansMath-Regular` (OFL, copied from the
  engine's editor fonts) as *Fallback Typeface*. It provides symbols missing from the three
  families, such as the ↺ reset glyph and arrows. Without it those characters render as empty
  boxes in packaged builds: the editor's `LastResort` font is disabled for cooked data.

---

## Rules

1. Every new text is an **MKH Text** with a `TS_` style. Plain Text Blocks are reserved for the
   [exceptions](#exceptions).
2. Never edit `Font` or *Transform Policy* on an MKH Text that has a style: the style overwrites
   them. Change the style, or pick another one.
3. Pick the style by **role**, not by look: a button label is `TS_Button*` even if a label style
   has the same size today.
4. Only add a new style when no existing role fits (see [How-To](#add-a-new-style)); do not
   create one-off styles to tweak a single widget's size.
5. Colour, shadow, justification, wrapping and min width stay on the widget.
6. Type text in normal case; let the style upper-case it.

---

## Exceptions

| Widget | Why it is not an MKH Text |
|---|---|
| Reset/arrow glyphs (`ResetGlyph*`, `Next/PreviousGlyph` in settings rows) | Icons drawn with a character, not text: Barlow Regular 13 / Medium 15 set directly |
| `RegionMenu` (Combo Box) in `W_LobbyOverlay` | Not a text block; font set directly (Barlow Medium 16) |
| `GuestNameBox` (Editable Text Box) in `W_LoginPanel` | Not a text block; font set in its widget style (Barlow Medium 16, as `TS_LabelLarge`), same frame as `AddFriendNameBox` |
| `AddFriendNameBox` (Editable Text Box) in `W_FriendList` | Not a text block; font set in its widget style (Barlow Medium 14, as `TS_Label`) |
| `InfoText` (Rich Text Block) in `W_InfoRow` | Rich text: styles come from `DT_TooltipText` (below) |

`DT_TooltipText` rows follow the same rules by hand: `Default`, `StatName`, `PerkDesc`,
`SkillDesc`, `Description` = Barlow Regular 12 (`TS_Body`); `PerkName`, `SkillName` = Barlow
SemiBold 12; `StatValue` = JetBrains Mono Medium 12 (`TS_Value`). If a `TS_` style they mirror
changes, update the table too.

---

## How-To Guides

### Add a text to a widget

Palette → *Makhia* → **MKH Text**, then set *Appearance → Text Style*. To convert an existing
Text Block: right click → *Replace With* → MKH Text (bindings, variable references and
animations are kept), then set the style.

### Add a new style

1. Right click in `Content/Makhia/UI/Fonts/Styles` → *Miscellaneous → Data Asset* →
   `MKHTextStyle`, named `TS_<Role>` (`TS_HUD_<Role>` for in-game HUD text).
2. Set `Font` (family, typeface name exactly as listed in the composite font, size, spacing,
   outline) and `TextTransform`.
3. Add it to the [catalogue](#style-catalogue) above.

### Add a weight to a family

1. Import the TTF into `Content/Makhia/UI/Fonts/Faces` (source files: Google Fonts, OFL).
2. Open the composite font (`F_Barlow`, ...) → *Default Font Family* → add an entry named after
   the weight (e.g. `ExtraBold`) pointing at the new face.

### Localize to a script the families do not cover

Barlow and Marcellus cover Latin (plus some Greek); JetBrains Mono also covers Cyrillic. For
Cyrillic, CJK or others add a *Sub-Font Family* to each composite font with a Noto family for the
needed character ranges. Styles and widgets need no change.

---

## Gotchas & Troubleshooting

- **A style edit does not show in an open widget designer.** The designer rebuilds the preview
  only when the widget changes or is recompiled: press *Compile* (runtime is always up to date).
- **Text shows as boxes in a packaged build but fine in the editor.** The character is missing
  from the family and from the Noto fallback; the editor hides it with `LastResort`. Pick a
  font that has it and add it as a sub-font, or draw the symbol as an image.
- **A widget shows Barlow Bold 24 unexpectedly.** It is a plain Text Block left at the project
  default font: convert it to MKH Text and give it a style.
- **Numbers jitter while they count.** They use a Barlow style: live numbers must use a
  JetBrains Mono style.
