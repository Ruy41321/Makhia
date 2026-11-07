# HUD Quick Slots and Keycaps

How the HUD shows quick slots (weapons, weapon skills, consumables) and the key that triggers them.

## Keycap: `UMKHKeyHintWidget` / `W_KeyCap`

`W_KeyCap` (`Content/Makhia/UI/GameUI/HUD/HUDOverlay/Hint`) is the **only** widget that prints a key on screen.
Its parent `UMKHKeyHintWidget` reads the key through `UMKHInputSettingsLibrary::GetActionKeyDisplayNameForActiveDevice`:

- the player's **Primary** key from the Enhanced Input profile, on the device in use (Settings → Keyboard or
  Settings → Controller), whatever mapping context is active (UI open, dead, …); gamepad buttons show their
  short label (`Y`, `RB`, `↑`), chorded ones with their chord (`LB + RB`);
- the keyboard key when the action has no gamepad button;
- fallback for non player-mappable actions: the first key the applied contexts map to the action.

It refreshes on construct, on `UEnhancedInputUserSettings::OnSettingsApplied` (every rebind) and on
`UMKHActiveInputDeviceSubsystem::OnActiveInputDeviceChanged` (keyboard ↔ pad), and unbinds on destruct. See
"Key prompts" in `SettingsArchitecture.md`. Never query keys in Blueprint (`QueryKeysMappedToAction`, `OnControlMappingsRebuilt`): place a `W_KeyCap`.

| Property | Use |
|---|---|
| `InputAction` | Action whose key is shown. Set on the instance, or at runtime with `SetInputAction`. |
| `bShowBackground` | `false` collapses `KeyBackground` when the host draws its own frame (pickup prompt). |
| `GamepadInputAction` | Shown instead of `InputAction` while a gamepad is in use. The weapon slot's `KeyCap` (`W_WeaponQuickSlotHUD`) uses `IA_SwitchWeapon`, so both weapon rows show `Y` (`IA_WeaponSlot1/2` have no gamepad button). |
| `bShowChord` | `false` shows only the mapping's own button (`RB` instead of `LB + RB`). Off on the skill slots' `KeyCap` (`W_SkillSlotHUD`): a chord does not fit under a 70 px slot. |
| `bGamepadOnly` | Collapsed unless a gamepad is in use. |
| `FixedGamepadKey` | Gamepad button shown as it is, for menu buttons that are not input actions (A, B). `W_GamepadHint` sets it from its `GamepadKey`. |

`W_WeaponSkillRow` shows the skill chord once, as `KeyCap_SkillModifier` (`IA_AlternateActions`, `bGamepadOnly`) at the
left of the three skills: a `SizeBox` 28 px high with 21 px top padding centres it on the skill icons (the skill
slot is 70×102, the icon area its top 70 px). Collapsed on keyboard and mouse, so the row keeps
its keyboard layout.

Bound widgets: `KeyText` (label), `KeyBackground` (backdrop), both optional.
At design time the placeholder text of `KeyText` is kept.

Users: the three quick slots (as `KeyCap`), `W_InverntoryHint` (`IA_ToggleInventory`), `W_ItemPickUp`
(`IA_Interact`, no background inside the outlined border), and indirectly the preparation skip prompt, which uses
the same library call inside a sentence.

## Quick slots: `UMKHQuickSlotWidget`

```
UMKHQuickSlotWidget (abstract, C++)          item binding, filled/empty look, rarity bg, keycap, cooldown
├── UMKHWeaponQuickSlotWidget      → W_WeaponQuickSlotHUD     icon = GetWeaponHudIcon
├── UMKHConsumableQuickSlotWidget  → W_ConsumableQuickSlotHUD icon = GetIcon, quantity, consumable cooldown
└── UMKHSkillQuickSlotWidget       → W_SkillSlotHUD           skill standing on the SkillIndex-th skill slot: icon + cooldown
```

C++ owns the flow; the Blueprints own only the layout and the flash (`W_FlashVFX`, in `BP_PlayActivationFlash`).

### API

| Function | Effect |
|---|---|
| `SetSlotItem(Item)` | Shows the item and follows `OnItemUpdatedDelegate` until reset. |
| `ResetSlot()` | Drops the item, restores the empty look, stops the cooldown display. |
| `RefreshSlot()` | Re-renders from the current item. |
| `SetInputAction(Action)` | Changes the action and forwards it to `KeyCap`. |
| `IsSlotFilled()` | True when there is content (the flash is white when filled, red when empty). |
| `GetQuickSlotTag()` / `SetQuickSlotTag(Tag)` | Quick slot the widget displays. |
| `GetSlotItem()` / `GetSlotItemID()` | Item shown, null / 0 when empty. |
| `PlayActivationFlash()` | Plays `BP_PlayActivationFlash`. |

Subclass hooks: `HasSlotContent`, `ResolveSlotIcon`, `ResolveCooldownTag`, `ResolveDisplayedQuantity`.
Blueprint hooks: `BP_OnSlotRefreshed(bFilled)`, `BP_OnCooldownFinished` (skill and consumable slots call
`PlayActivationFlash` on it), `BP_PlayActivationFlash` (each slot plays its `W_FlashVFX`: weapon `PlayHoleWhite` /
`PlayHoleRed`, skill `PlayFullWhite` / `PlayFullRed`, consumable `PlayFullWhite` / `PlayHoleRed`, white when
`IsSlotFilled`).

### Look

- **Filled**: icon from `ResolveSlotIcon`, white tint; with `bUseRarityBackground` `SlotBg` gets the rarity's
  `MI_Square` material; `QuantityText` visible only for quantities above 1.
- **Empty**: the designer brushes of `SlotIcon`/`SlotBg` are restored (cached in `NativeOnInitialized`), then
  `EmptyIconTexture` + `EmptyIconTint` and, with `bUseRarityBackground`, `EmptyBackgroundTint`.
  Weapon: `T_sword_185`, consumable: `T_Bedroll` (both 0,0,0,0.4); skill: `Fade` tinted 0.1/0.9, no rarity background.
- The empty look is applied on construct, so an unassigned slot never shows the designer placeholder.

### Cooldown

`StartCooldownTracking` registers `RegisterGameplayTagEvent(Tag, NewOrRemoved)` on the owning player's ASC
(`UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPlayer())`). While the tag is present a timer
(`CooldownRefreshInterval`, 0.1 s) updates `CDProgress` (remaining / duration from
`UMKHAbilitySystemComponent::GetCooldownRemainingForTag`) and `CDTime` (seconds, rounded up). When the tag goes away
the overlay is hidden and `BP_OnCooldownFinished` fires. A cooldown already running when the slot is filled is
picked up immediately.

### Bound widgets

`SlotIcon` (required), `SlotBg`, `KeyCap`, `QuantityText`, `CDOverlay`, `CDProgress`, `CDTime` (optional).

### Properties set by the containers

| Property | Set by |
|---|---|
| `InputAction`, `QuickSlotTag` | Instance values in `W_ConsumablesHUD`; for the weapon slot `W_WeaponSkillRow` pushes its row values (`QuickSlotTag`, `WeaponInputAction`, instance values in `W_WeaponHUD`) in PreConstruct. |
| `SkillIndex` | `UMKHWeaponSkillRowWidget`: 0, 1, 2 in tree order. |
| `InputTag` (skill) | Instance values in `W_WeaponSkillRow`, matched exactly by the row's skill flash. |

## Containers: `UMKHQuickSlotContainerWidget`

```
UMKHQuickSlotContainerWidget (abstract, C++)   slots by tag, AddItem / RemoveItemByID / SyncItem / PlayActivationFlash
├── UMKHConsumablesHUDWidget  → W_ConsumablesHUD   its 3 W_ConsumableQuickSlotHUD
├── UMKHWeaponSkillRowWidget  → W_WeaponSkillRow   weapon slot + 3 skill slots, active / inactive look
└── UMKHWeaponHUDWidget       → W_WeaponHUD        its 2 rows, active weapon slot
```

The container Blueprints have no graph: they own the layout and the instance values only.

| Function | Effect |
|---|---|
| `AddItem(Item)` | Removes the ItemID from the slot it was in, then, if the item is still quick slotted, shows it in the slot whose `QuickSlotTag` is exactly the item's. |
| `RemoveItemByID(ItemID)` | Empties the slot showing the item; false when none does. |
| `SyncItem(Item)` | `CopyFrom` into the item already shown (its `OnItemUpdatedDelegate` refreshes every slot showing it), or `AddItem` when no slot shows it. |
| `PlayActivationFlash(Tag)` | Default: flashes the slot whose tag is exactly `Tag`. |

Subclass hooks: `GatherQuickSlots` (default: every `UMKHQuickSlotWidget` of the widget tree, nested user widgets
excluded), `AssignSlotItem`, `ClearSlotItem`.

- **Row** (`UMKHWeaponSkillRowWidget`): routes only on its weapon slot; showing or clearing a weapon does the same on
  the three skill slots. Flash: a tag matching the weapon slot's tag flashes it unless its weapon is already
  equipped (pressing the key of the weapon in hand does nothing); otherwise, only while the row is active, the skill
  slot whose `InputTag` is exactly the tag.
- **Weapon block** (`UMKHWeaponHUDWidget`): its slots are the rows' weapon slots, an item goes through its row.
  Skill tags (`Input.Ability.Attacks.Skill.*`) flash the active row (the first row when there is none); weapon slot
  tags flash the row with that exact tag.
- **Overlay** (`UHUDOverlayWidget`, `W_HUDOverlay`): `WeaponHUD` / `ConsumablesHUD` are `BindWidgetOptional`.
  `OnItemQuickSlotted` / `OnItemChanged` go to the weapon block for `Item.Equipment.Weapon.*`, to the consumables
  otherwise; `OnItemRemoved` tries the consumables, then the weapons. `OnQuickSlotActivated`: `Input.Inventory` →
  `InventoryHint->PlayActivationFlash()`, consumable category → consumables, everything else (weapon slots,
  skills) → weapon block.

## Inventory hint: `UMKHInventoryHintWidget`

`W_InventoryHint` (`Content/Makhia/UI/GameUI/HUD/HUDOverlay/Hint`, instance `InventoryHint` in `W_HUDOverlay`, a
`BindWidgetOptional` of `UHUDOverlayWidget`) shows the inventory icon (`SlotIcon`) over a `W_KeyCap` (`KeyCap`,
`IA_ToggleInventory`), both set in the designer. `PlayActivationFlash()` calls `BP_PlayActivationFlash`, where the
Blueprint plays `W_FlashVFX` `PlayFullWhite`; it runs when the inventory opens (`AMainHUD::ToggleInventory` →
`OnQuickSlotActivated(Input.Inventory)`).

No HUD widget uses a Blueprint interface for the flash any more: `W_QuickSlot` and `W_FlashInterface` were deleted
(2026-10-21). New flashing widgets expose a C++ `PlayActivationFlash` + `BP_PlayActivationFlash` pair.

## Active weapon

Two states only: **active** (today's look) and **inactive**.

- **Rule** (`UHUDOverlayController::ComputeActiveWeaponSlotTag`, the only place): the quick slot of the weapon in the
  equipment `Equip.WeaponSlot` (`UQuickSlotManagerComponent::FindQuickSlotTagByItemID`); with no weapon equipped
  the `QuickEquipWeaponSlotTag` (the slot the unarmed attack equips on the fly), when it holds a weapon; else the first
  filled weapon slot; with no weapon quick slotted, none.
- **Refresh**: on equip / unequip of the weapon slot (`EquipmentEntryDelegate`, `UnEquippedEntryDelegate`), on
  `OnQuickEquipWeaponSlotChanged` (set locally or `OnRep_QuickEquipWeaponSlotTag`), on `OnQuickSlotsChanged`
  (local change or `OnRep_QuickSlotTagMap`) and on quick slot item relocations / removals. The refresh runs on the
  next tick, once per tick: the equipment (PlayerState) and the quick slots (PlayerController) replicate in any
  order and the unequip is broadcast before the entry leaves the list, so the value settles by itself. The
  controller broadcasts `HUDActiveWeaponSlotChangedDelegate` on change, and always from `BroadcastInitialValues`
  (a new overlay starts with every row active).
- **Rows**: `UMKHWeaponHUDWidget::SetActiveWeaponSlot` marks each row; a row is inactive only when it holds a weapon
  and is not the active slot (one weapon: it is active; no weapon: every row looks as today).
- **Look** (`UMKHWeaponSkillRowWidget`, `EditDefaultsOnly`): the whole row (weapon slot, its skills and keycaps) is
  multiplied by `InactiveTint` (`ColorAndOpacity`, **linear**: 0.3 ≈ 55% perceived brightness) and scaled by
  `InactiveScale` (0.85) around `InactiveScalePivot` (1, 0.5: the weapon side, toward the screen edge, stays put).
  Render transform and colour only, so the layout never moves. The blend lasts `TransitionDuration` (0.15 s,
  smoothstep, driven in `NativeTick` only while it runs). No material, negligible GPU cost.

## Hiding the HUD: `IA_ToggleHUD`

`IA_ToggleHUD` (`Content/Makhia/Input/Actions/UI`, **H** in `IMC_BaseMapping`, remappable under *General* as
"Hide HUD") hides the whole gameplay overlay `W_HUDOverlay` and the world widgets (the opponent's `OverheadBar` of
`BP_MKHPlayerCharacter`, the floating damage numbers of `BP_DamageText`) for player screenshots and trailer shots, and shows them again on the next
press. The match intro hides the same things.

- `AMKHPlayerController::ToggleHUDAction` → `AMainHUD::ToggleHUDHiddenByPlayer`.
- `AMainHUD` keeps two flags, `bOverlayHiddenByIntro` (`SetHUDOverlayHidden`, match intro) and
  `bOverlayHiddenByPlayer`: the overlay is visible only when neither is set, so the intro ending never brings back
  a HUD the player hid. Hiding uses render opacity, never visibility, so the overlay's own widgets keep managing
  their visibility (an overlay recreated meanwhile starts hidden too).
- World widgets: every `UWidgetComponent` of every `ACharacterBase` and of every actor carrying the actor tag
  `MKH.HideWithHUD` (set on `BP_DamageText`) gets `SetHiddenInGame` (+ `UpdateWidget`, the component may have stopped
  ticking); such actors spawned while hidden (damage numbers, new round, late replication) are caught through the
  world's `OnActorSpawned`. `HiddenInGame` leaves the Blueprint's own `SetVisibility` handling alone. To make another
  world widget hide with the HUD, add the same tag to its actor (pickups are deliberately left visible).
- Only gameplay focus maps the key: menus, inventory, settings and the end-of-match panel are not affected and
  stay usable. The state lasts until the next press or the next level load (new HUD).

## Opponent overhead bar

`W_OverheadBar.PlayerRef` is an `ACharacterBase` (the health and stamina delegates live there), set by the owner after
`CreateWidget` and before `SetWidget`, so the same widget serves players and the duel bot. The perks bar
(`W_PerksBar.InitPlayerRef`) also takes an `ACharacterBase`: it binds to the `StatEffect.Perk` tags of the character's
ASC at once (the bot's ASC lives on the actor), and only when the ASC is not ready yet it waits for
`AMKHPlayerCharacter::OnPlayerCharacterReady` (players, ASC on the PlayerState).

The bar keeps its binding handle (`PerkTagBinding`) and releases it on destruct, because the ASC of a player outlives the widget (it stays on the PlayerState); a new construct binds again. The perks bar shows every active `StatEffect.Perk.*` tag that has a row in `DT_PerkDefinitions` (icon and buff/debuff
from its `UiDataTag` row in `DT_PerkUiData`). Status effects get the tag dynamically (`ApplyStatusEffects`); a
consumable effect grants it with a "Grant Tags to Target Actor" component, e.g. `GE_StrengthPotion` →
`StatEffect.Perk.Empower.StrengthPotion` (row display only: duration and magnitude stay on the GE).

The opponent's `OverheadBar` (`W_OverheadBar`, screen-space widget component of `BP_MKHPlayerCharacter`, created only
on non-local characters, and of `BP_DuelBot`, created at `BeginPlay`) is not always on screen: `UMKHOverheadBarVisibilityComponent`
(`Source/Makhia/Public/UI/HUD/`, default subobject `OverheadBarVisibility` of `AMainHUD`, so it runs only for the local
viewer) shows it when either holds:

- **Proximity**: the opponent is within `ProximityRevealDistance` (default 500 cm) of the local pawn.
- **Aim**: a sphere of `AimToleranceRadius` (default 0.5 cm = 5 mm; 0 = exact line) swept from the camera centre along
  the view direction, up to `MaxAimDistance` (50 m), touches the opponent's body. The sweep is component-level
  (`SweepComponent` on the mesh's physics asset bodies, capsule as fallback), so channel responses do not matter. With
  `bRequireLineOfSight` a line trace on `OcclusionChannel` (Visibility) from the camera to the touched point must be
  clear: aiming through a wall reveals nothing.

Once revealed the bar stays for `RevealLingerTime` (1 s: a millimetre tolerance would otherwise flicker as the
crosshair sweeps over the body), fading in over `FadeInTime` (0.1 s) and out over `FadeOutTime` (0.3 s). The component
drives the user widget's **render opacity**, so it combines with the Toggle HUD / intro `HiddenInGame` above without
either overriding the other. All values are editable on the component in `BP_MainHUD`'s defaults.

## Starting loadout quick slot preferences

Each match starts with the same `StartingLoadoutTable` (`AMKHGameMode`), which lists only items: the rows carry no
"equip" or "quick slot" choice. `UMKHQuickSlotPreferenceComponent` (`Source/Makhia/Public/QuickSlot/`, default subobject
`QuickSlotPreferenceComponent` of `AMKHPlayerController`, so never on the duel bot) remembers which **kind** of item
the player keeps in each quick slot and which armor types they took off, and applies both when the next loadout is
granted. First match ever: the default rules below.

**Kind** (`GetItemKindTag`): the tag right below `Item.Equipment.Weapon` or `Item.Consumable`, never the concrete item
(rarity, rolls): `Item.Equipment.Weapon.Sword.BronzeSword` → `Item.Equipment.Weapon.Sword`;
`Item.Consumable.HealthPotion` stays as it is. A new weapon must sit under a kind tag
(`Item.Equipment.Weapon.<Kind>.<Item>`) to be remembered by kind.

**Recording** (owning machine: client, listen-server host or offline player):

- Listens to `InventoryList.QuickSlotItemRelocatedDelegate`. Only an entry **taking** a quick slot records
  (`bIsQuickSlotted`): explicit assignment, split into a slot, swap, the server's assignment. The same kind remembered
  in a slot that is empty now is forgotten (the item moved). A weapon kind is remembered in one slot only (one weapon
  per type in the quick slots, `InventoryDashboard.md`).
- **A slot emptied by hand** (`UInventoryDashboardController::RemoveFromQuickSlot`: menu, drag to the bag, double click)
  forgets that slot's preference: it starts empty until the player fills it again. Other removals change nothing, so
  the slot comes back next match: a stack used up, a dropped item, the rematch reset.
- **Armor taken off by hand** (`UInventoryDashboardController::UnequipItem`) adds its type
  (`FMKHDefaultQuickSlots::GetArmorType`) to `UnequippedArmorTypes`, so that type starts in the bag. Wearing a piece of
  the type by any path, pickups included (`EquipmentEntryDelegate`), removes it. A dropped piece changes nothing.
- Committed on the next tick (a swap records twice, saved once): `AsyncSaveGameToSlot` to the `QuickSlotPreferences`
  slot (`UMKHQuickSlotPreferenceSaveGame`, per machine, like `Skin`) and sent to the server. Every commit also sets
  `bDefaultsApplied`, which ends the first match's defaults for good, even if the player then empties everything.
  Saves from before the flag read false: their categories with no preference get the defaults one last time.

**Server**:

- `ServerSetPreferences` (reliable, validated) carries only `FMKHQuickSlotPreference` pairs (quick slot tag, kind tag), the armor types, `bDefaultsApplied` and the "Remember Quick Slots" flag (below):
  at most 8 of each, unique slots, weapon kinds in weapon slots, consumable kinds in consumable slots. Never item data. The
  owner sends them at `BeginPlay` (empty list included) and again on every change, so a rematch uses the latest ones.
- Bound to `UInventoryComponent::OnStartingLoadoutGranted` from `InitializeComponent` (the first grant can run before
  `BeginPlay`). The loadout waits for the preferences, or the preferences for the loadout, whichever comes last.
- **Armor**: `ApplyArmor`, once the preferences are there (online the armor arrives after the loadout by the RPC's
  latency): the rarest piece of each type not in `UnequippedArmorTypes`, the first in row order on a tie, through
  `UseItem` (`FMKHDefaultQuickSlots::PickArmorToEquip`, also used by the duel bot, with nothing skipped).
- For each preference whose slot is still empty on the server's `UQuickSlotManagerComponent` mirror: the inventory
  item of that kind that is not quick slotted nor picked for another slot; several → the rarest
  (`IsRarer`: more `NumPassiveStats + NumActiveAbilities` on its rarity row, then lower `RollProbability`), then the
  biggest stack; none → the slot stays empty (the preference is kept). A weapon kind already applied to another slot
  is skipped (saves from before the one-weapon-per-type rule may name it twice). Assigned through
  `UInventoryComponent::AssignQuickSlotOnOwner` (host directly, remote client through `ClientAssignQuickSlot`, queued
  until the entry replicates), so the usual flow (flags, manager mirror, HUD, quick-equip slot) runs unchanged.
- **Loadout defaults** (first match ever, `!bDefaultsApplied`): `OnStartingLoadoutGranted` carries the IDs of the loadout's
  items, in row order. After the preferences, each category (weapons, consumables) that no saved preference targets
  gets the rarest item of each kind in its free slots, kinds in row order (`ApplyDefaultQuickSlots`; the rule is
  `FMKHDefaultQuickSlots::PickForCategory`, shared with the duel bot, see `MatchFlowArchitecture.md`, *Bot AI*). So two
  weapons of different types (one weapon per type) and three consumables. They are ordinary quick slot adds on the
  owning machine, so they become the player's preferences from then on.
- Rematch: the previous match's items are removed before `GrantStartingLoadout` runs again, so the broadcast finds
  empty slots and applies the preferences again.

**Skill order**: `SkillSlotPreferences` (`FMKHSkillSlotPreference`: `AbilityTag`, skill slot; at most 64, the oldest goes first) travels with the other choices in `ServerSetPreferences`. `NotifySkillSlotChosenByPlayer` records each skill the player moves. The server applies them through `UInventoryComponent::SetSkillLayout`, whatever the "Remember Quick Slots" setting and whichever arrives first between loadout and preferences. Model in `InventoryDashboard.md`, "Skill order".

**Setting "Remember Quick Slots"** (Settings → General → Gameplay, `UMKHGameUserSettings::bAutoQuickSlotLoadout`, on by
default):

- It travels with the preferences: `ServerSetPreferences(Preferences, bApplyEnabled)`. The owner resends both when the
  setting changes (`OnAutoQuickSlotLoadoutChanged`), so a change made mid-match applies to the next loadout (rematch
  included).
- **Off**: the rarest armor of every type is worn, `UnequippedArmorTypes` ignored, and the quick slots start empty (no
  saved preferences, no defaults). Choice: the player keeps nothing from the last match, but never starts a duel with
  no armor unless they take it off.
- Recording goes on while off (the owning machine still saves every assignment), so turning it back on uses the
  player's latest arrangement instead of an old one.
