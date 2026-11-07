# Inventory Dashboard

How the inventory screen (skin portrait, armor, weapon rows with their skills, consumable quick slots, bag, stats, action menu) is built.
C++ owns the whole flow; the Widget Blueprints own only the layout, the instance values (slot tags) and the item
tooltip. Drop / split mechanics and the item model are in `PickupSystemArchitecture.md`, the gamepad rules in
`GamepadMenuNavigation.md` ("Inventory").

## Classes

All in `Source/Makhia/Public/UI/HUD/Inventory/`.

| Class | Blueprint | Role |
|---|---|---|
| `UInventoryDashboardWidget` | `W_InventoryDashboard` | Root: routes inventory changes to the panels and every player request to the controller; owns the action menu, the quantity popup and the split drag. |
| `UMKHInventoryPanelWidget` (abstract) | — | Base of the three panels: dashboard reference, entries of its own widget tree, drop highlight. |
| `UMKHInventoryBagWidget` | `W_Bag` | Bag `TileView` (`InventorySlots`): items neither equipped nor quick-slotted. |
| `UMKHInventoryEquipmentWidget` | `W_Equipment`, `W_WeaponLoadout` | Two instances of one class. `W_Equipment`: skin portrait, armor slots, `W_StatsArea`, pad hints. `W_WeaponLoadout`: one row per weapon quick slot. |
| `UMKHInventoryWeaponSkillsWidget` | `W_InventoryWeaponSkills` | The three `W_SkillSlotHUD` of a weapon row (icon, key, cooldown), plus the pad's skill modifier key. Handles the skill reorder (below). |
| `UMKHGamepadHintWidget` | `W_GamepadHint` | `W_KeyCap` + label for a menu button ("A Actions"), collapsed on keyboard and mouse. `SetHintText` / `SetHintEnabled` for hints that follow a state. |
| `UMKHInventoryQuickSlotAreaWidget` | `W_QuickSlotArea` | The three consumable quick slots. |
| `UMKHInventoryStatsWidget` | `W_StatsArea` | Health "current/max" and armor "rating (-reduction%)", live from the owning pawn's ASC. |
| `UMKHInventoryEntryWidget` | `W_InventoryEntry` | One slot (bag row, equipment slot or quick slot): look and player input. |
| `UMKHInventoryActionMenuWidget` | `W_ActionMenu` | Context menu of an entry. |
| `UMKHInventorySortMenuWidget` | `W_BagSortMenu` | Menu of the bag sort criteria. |
| `UMKHInventoryEntryDragOperation` | — (was `DD_Entry`) | Drag of a whole item; the source entry hides its content meanwhile. |
| `UMKHSkillDragOperation` | — | Drag of a skill between the slots of one weapon row; carries no item data. |
| `UMKHItemDragVisualWidget` | `W_EntryDragVisual` | Item under the cursor while dragging (item or split). |
| `UMKHDropHighlightWidget` | `W_FadedVFX` | "Drop here" overlay of a panel or entry (`SetHighlightActive`). |

`UInventoryDashboardController` (`UI/WidgetControllers/`) feeds the dashboard and is its only way to the
server-authoritative components: `UseItem`, `UnequipItem`, `AddToQuickSlot`, `RemoveFromQuickSlot`, `DropItem`,
`SplitStackToQuickSlot`, plus the quick slot choice (`FindQuickSlotForItem`, `FindQuickSlotChoices`,
`FindConsumableQuickSlotForSplit`). No Blueprint dispatcher is involved any more (the old `W_InventoryDashboard`
dispatchers and the `BP_InventoryDashboardController` relay graph were removed).

## Data flow

```
UInventoryComponent / UEquipmentManagerComponent (replicated lists)
  └─► UInventoryDashboardController  (UInventoryItem views, dynamic delegates)
        └─► UInventoryDashboardWidget::On*  ──►  Bag / Equipment / QuickSlotArea  ──►  UMKHInventoryEntryWidget
Entry / panel / action menu  ──►  UInventoryDashboardWidget::Request*  ──►  controller  ──►  components (Server RPCs)
```

| Dashboard event | Goes to |
|---|---|
| `OnInventoryItemChanged` / `OnInventoryItemRemoved` | Bag `SyncItem` (copy into the row showing the ItemID, or a new row) / `RemoveItemByID` |
| `OnItemEquipped` | Armor only: Equipment `ShowItem` (weapons show through their quick slot) |
| `OnItemUnequipped` | Armor slots only: Equipment `ClearItem(SlotTag, 0)` |
| `OnQuickSlotItemRelocated` | Bag (quick-slotted → leaves, back → returns), then weapons → WeaponLoadout, consumables → QuickSlotArea |
| `OnQuickSlotItemChanged` | Weapons → WeaponLoadout `ShowItem`, consumables → QuickSlotArea `SyncItem` |
| `OnQuickSlotItemRemoved` | QuickSlotArea `RemoveItemByID` |

The panels give the dashboard to their entries in `NativeOnInitialized` (bag rows when the `TileView` generates them),
so items broadcast before the first construction are shown.

## Layout

Centred window (never full screen), `MI_UI_InventoryWindow` as background: left column `W_Equipment`, right column
"Weapons" (`WeaponLoadout`), "Quick Slot" (`QuickSlotArea`), "Inventory" (`Bag`).

- **Portrait**: `SkinPortrait` shows the `Portrait` of `UMKHSkinSubsystem::GetSelectedSkinId`, with no skin name.
- **Weapon rows**: `W_InventoryWeaponEntry` is the wide entry (same class, `bUseWeaponHudIcon`, `GetWeaponHudIcon`),
  followed by its `W_KeyCap` and `W_InventoryWeaponSkills`. The entry's parent widget is the row.
- **Weapon rows**: both always at full opacity; only the HUD dims the weapon not in hand (`HUDQuickSlots.md`).
- **Empty cells of the bag**: `EmptySlotGrid` is decorative (`M_UI_SlotGrid` in a hit test invisible canvas), never list
  items, so focus and drag see only real items. It never moves: `InventorySlots` scrolls by whole rows (`bEnableFixedLineOffset`
  with offset 0, no right click scrolling, no overscroll) and is exactly three rows high, so rows always land on the cells.
  `EntryWidth` / `EntryHeight` (96) and the grid's `Columns` must agree.
- **Right edge**: the right column is as wide as the bag: nine tiles (864) plus the 20 the list reserves for the scroll bar (thickness 16 + padding) and 2 of margin, so the bar stands
  next to the last tile. With less the list fits only eight tiles per line. `SortButton` has a right padding that puts it on the visible edge of the last tile.
- **Pad hints**: `W_GamepadHint` in `W_Equipment`, visible only with the pad.

`W_ToolTip_EquipmentEntry` keeps its own layout, independent from this window.

## Entries

Instance values on the entries placed in `W_Equipment` / `W_WeaponLoadout` / `W_QuickSlotArea`: `SlotTag` (armor
`Equipment.Slot.Armor.*` or quick slot `Input.Ability.QuickSlot.*`), `bIsEquipEntry` (equipment panels),
`bIsQuickSlot` (accepts quick-slottable drops). Bag rows have neither.

- **Look**: icon, rarity `MI_Banner` background, frame tinted with the rarity `MainColor`, quantity above one.
  Empty: no icon, transparent background, grey frame (`EmptyFrameTint`). An entry never filled keeps its designer look.
- **Input**: right click / gamepad A → action menu; Ctrl+left → drop the whole item; Shift+left on a splittable stack →
  split popup; left drag → `UMKHInventoryEntryDragOperation`; left double click → `QuickEquip` (armor equip / unequip,
  quick slot add / remove). Bag rows: the `TileView` row takes the double click, so the bag handles it
  (`HandleItemDoubleClicked`, same rule).
- **Drops on an entry**: only quick slot entries take them — first a split (`TryDropSplitOnQuickSlot`), else an entry
  drag of a not equipped weapon (equipment panel) or consumable (quick slot area). Anything else bubbles to the panel.
  **One weapon per type**: `UInventoryDashboardWidget::ResolveQuickSlotDropTarget` sends a weapon whose type is already
  quick-slotted in the other weapon slot to that slot (it replaces the weapon there); the other slot does not light up.
- **Empty look on construct**: an entry that never received an item gets the empty look in `NativeConstruct`, never the
  designer placeholder of `W_InventoryEntry` (sword icon, quantity "x").
- **Bag layout**: the `TileView` uses `LeftAligned` with fixed tiles, so rows stay on the grid of empty cells whether or
  not the items fill a row and whether or not the scroll bar is shown.
- **Tooltip**: built by the dashboard Blueprint (`MakeItemToolTip` → `W_ToolTip_EquipmentEntry`, still a Blueprint), never
  while the action menu is open on any entry; never call `SetToolTip` on an entry. It stands next to the entry: top left
  corner on the entry's right side, growing down, on the left or moved up only when it would leave the screen.
  - *Mouse*: not a Slate tooltip, which would follow the cursor. `NativeOnMouseEnter` / `Leave` →
    `UInventoryDashboardWidget::SetHoveredEntry` / `ClearHoveredEntry` → `RefreshHoverToolTip` →
    `UMKHMenuNavigationSubsystem::ShowAnchoredToolTip` (anchor: the entry's `SlotFrame`). Also hidden during a drag and
    when the dashboard closes; it comes back when the action menu closes over the same entry.
  - *Gamepad*: `GetToolTipWidget`, bound to the entry's own `ToolTipWidgetDelegate` and read by the navigation subsystem
    for the focused entry. It returns nothing with the mouse, so Slate never shows it.

## Panels as drop targets

| Panel | Highlight on drag enter | Drop |
|---|---|---|
| Bag | equipped armor or quick-slotted consumable (a quick-slotted weapon keeps the current highlight) | armor → unequip; weapon / consumable → leave the quick slot |
| Equipment, WeaponLoadout | any equipment not equipped | armor → use (equip); weapon → quick slot (`FindQuickSlotForItem`) |

## Bag sorting

`SortButton` (icon on the bag's title row in `W_InventoryDashboard`) or gamepad Y opens `W_BagSortMenu`
(`UMKHInventorySortMenuWidget`, on the viewport, `SortMenuClass`). The menu covers the screen, so a click outside its buttons
closes it, like Esc, B and Y. The choice goes `RequestBagSort` → `UMKHInventoryBagWidget::SetSortMode`, which reorders the rows
of the `TileView` and keeps them in order when items arrive. The order lives only on this screen: the replicated list has
no order to keep, so no RPC is involved. The rules are pure, in `FMKHBagSorting` (spec `Makhia.Inventory.BagSorting`).

| `EMKHBagSortMode` | First | On a tie | Last tie |
|---|---|---|---|
| `Category` | Swords, hatchets, hammers, helmets, chests, legs, consumables | Rarity from the highest | ItemID from the lowest |
| `RarityAscending` | Rarity from the lowest | Category | ItemID from the lowest |
| `RarityDescending` | Rarity from the highest | Category | ItemID from the lowest |

The height of a rarity is its row position in `DT_RarityTable` (`UMKHDataSubsystem::GetRarityRank`); an item with no rarity
counts as the lowest. The first opening of the dashboard sorts by `Category`, unless a criterion is already set; before
that (`None`) the bag keeps the order of arrival.

## Skill order

The keys stay on the slots, the skills move: `FMKHSkillLayout` (pure rules, spec `Makhia.Inventory.SkillLayout`) swaps the `SkillInputTag` of the skills on the item. The third slot can stay empty on a weapon with fewer skills, and any slot can be the empty one.

- **Display**: `UMKHSkillQuickSlotWidget` shows the skill standing on its input (`SkillIndex`-th slot of `FMKHSkillLayout::GetSkillSlots`), on the HUD too, never the n-th skill of the list.
- **Input**: `UMKHInventoryWeaponSkillsWidget` turns on `EnableReorder` on its slots; the HUD row does not, so there they take no input. Mouse: drag a filled slot on another slot of the same weapon (`UMKHSkillDragOperation`), with the swap shown on hover through `ShowPreviewIcon`; other rows ignore it. Pad: A takes, arrows move, A places, B cancels; the hold ends if the focus leaves the row. `ReorderHighlight` (`W_FadedVFX`, collapsed unless active) lights the slots. `AcceptHint` and `CancelHint` (`W_GamepadHint` in `W_InventoryWeaponSkills`, right of the slots) follow the state: "Take skill" on a focused filled slot, "Place here" and "Cancel" during a hold, nothing on keyboard and mouse (`SetHintEnabled`).
- **Request**: slot → row → `UInventoryDashboardWidget::RequestSwapWeaponSkills` → `UInventoryDashboardController::SwapWeaponSkills` (records the choice) → `UInventoryComponent::SwapWeaponSkills` → `ServerSwapWeaponSkills` (ItemID and two tags, never item data).
- **Server**: refused while one of the two skills runs, so a key release always reaches its ability. Changes the inventory entry; for a weapon in hand also the equipped copy (`UEquipmentManagerComponent::ApplyItemSkillLayout`) and the granted specs in place (`UMKHAbilitySystemComponent::RefreshEquippedSkillInputs`): no regrant, so cooldowns and instances stay. The cooldown follows the skill because it is keyed by the skill, not the input.
- **Memory**: every skill moved is remembered by `AbilityTag` (`FMKHSkillSlotPreference`, see `HUDQuickSlots.md`) and applied to each weapon entering the inventory (`FRPGInventoryList::AddSeparateEntry`) and, at every change, to the weapons already owned (`UInventoryComponent::SetSkillLayout`), except the weapon just arranged and those with a skill running. It does not depend on "Remember Quick Slots". When two skills of a weapon claim one slot the first in the weapon's order wins; pushed skills take the slots left. A dropped weapon carries its order until someone else picks it up, then their memory applies.

## Quick slot choice (one-click actions)

`UInventoryDashboardController::FindQuickSlotForItem`: consumable → first empty consumable slot, else the first;
weapon → the slot of a quick-slotted weapon of the same type, else the first weapon slot when empty, else the second.
It reads the quick slot manager, not the widgets. The gamepad menu offers one button per slot instead
(`FindQuickSlotChoices`).

## Action menu

Created by `UInventoryDashboardWidget::OpenActionMenu` (class `ActionMenuClass`, viewport Z order `ActionMenuZOrder`),
placed with `UMKHMenuNavigationLibrary::GetPopupPosition`. Main action: "Unequip" on equipment entries, "Equip" for
equipment, "Use" otherwise, hidden for weapons. Generic quick slot button: "Add / Remove QuickSlot" (weapons and
consumables; with the pad only to remove). Split (splittable stacks), Drop. `CloseActionMenu` also cancels any drag in
progress; a click on the dashboard background, the bag releasing a row and the dashboard closing close the menu.

## Dashboard defaults (`W_InventoryDashboard`)

`ActionMenuClass` = `W_ActionMenu`, `DragVisualClass` = `W_EntryDragVisual`, `QuantityPopupClass` = `W_QuantityPopup`.
Bound widgets: `Bag`, `Equipment`, `WeaponLoadout`, `QuickSlotArea` (required), `InventoryFrame` (optional, drag-out drop area).
Tab (preview key down) and gamepad B close the dashboard.

## Look

`W_ActionMenu` and `W_QuantityPopup` share `MI_UI_InventoryWindow`. Action menu buttons: `UMKHButton` + `BS_ActionMenuItem`
(no fill until hover or pad focus, then the brass button look). Popup buttons: `BS_MenuButtonSmall`.

## Text

Action menu and stats texts are C++ `LOCTEXT` (`MKHInventoryActionMenu`, `MKHInventoryStats`); numbers go through
`FText::AsNumber` / `AsPercent`.
