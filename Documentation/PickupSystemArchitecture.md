# Pickup System & Item Data Architecture

How item instances are identified, generated, stored, equipped, dropped and picked up from the ground.
Server-authoritative, built for the 1v1 arena.

> Status: complete (2026-09-26). Verified in PIE (listen server + client): generated spawns, drops (equipped and
> partial), cross-player collection, aim selection, globally unique ItemIDs and cleanup at `MatchOver`.
> 2026-09-28: items are placed in levels through item spawn points (`Documentation/ItemSpawnSystemArchitecture.md`),
> pickups can be reserved to one player, and generation accepts a rarity rule. Pickups animate when they appear
> (drop, rise from the ground) and when they are collected (see "Animations").
> Open points are listed at the end ("Known limitations").

## Overview

```
UMKHDataSettings ──► UMKHDataSubsystem      (definitions, rarity, roll tables — see DataSettingsArchitecture.md)
                            ▲
UMKHItemSubsystem ──────────┘               (server: global ItemIDs, GenerateItem, pickup registry)
        │ FMKHItemEntry { ItemTag, ItemID, Quantity, RarityTag, EffectPackage }
        ├─► AMKHPickupItem { replicated Item + collection rights + animation state, mesh from the definition } (world pickups)
        │       ▲ placed by drops and AMKHItemSpawnPoint (server, see ItemSpawnSystemArchitecture.md)
        │       ▲ aimed at by UMKHPickupInteractionComponent (local), collected by Server_PickupItem
        │       └─ UMKHPickupAnimationComponent (every machine): drop / rise / collection animations
        ├─► FRPGInventoryEntry { Item, bIsUsed, bIsQuickSlotted*, QuickSlotTag* }   (* not replicated)
        ├─► FRPGEquipmentEntry { Item, SlotTag, runtime equip state }
        └─► UInventoryItem { Item + UI state + data derived from the definition }  (widgets only)
```

### Source map

| File (`Source/Makhia/...`) | Responsibility |
|---|---|
| `Public/Inventory/MKHItemEntry.h` | `FMKHItemEntry`, the item identity |
| `Public/Inventory/ItemTypes.h` | `FMasterItemDefinition` (static item data, incl. `PickupMesh` / `PickupMeshTransform`) |
| `Public/Data/MKHDataSettings.h`, `MKHDataSubsystem.h` | Data assets and `PickupItemClass` (see `DataSettingsArchitecture.md`) |
| `Public/Inventory/MKHItemSubsystem.h` | Item generation, global ItemIDs, pickup registry |
| `Public/Inventory/MKHItemRarityRule.h` | `FMKHItemRarityRule`, optional rarity override passed to `GenerateItem` |
| `Public/Inventory/Spawn/` | Item spawn points, groups, loot tables, spawn conditions (see `ItemSpawnSystemArchitecture.md`) |
| `Public/Inventory/InventoryComponent.h` | Inventory (`FRPGInventoryList`), use / remove / drop / split |
| `Public/Equipment/EquipmentManagerComponent.h` | Equipment (`FRPGEquipmentList`), server-only equip |
| `Public/Inventory/Pickup/MKHPickupItem.h` | World pickup actor |
| `Public/Inventory/Pickup/MKHPickupAnimationComponent.h` | Drop / rise / collection animations and their tuning |
| `Public/Inventory/Pickup/MKHPickupAnimationTypes.h` | `EMKHPickupAppearance`, `EMKHPickupAnimationType`, `FMKHPickupAnimationState` |
| `Public/Inventory/Pickup/MKHPickupInteractionComponent.h` | Local aim-based selection |
| `Public/Inventory/Pickup/MKHPickupAutoEquip.h` | Hold-to-equip rules (`FMKHPickupAutoEquip`: equip / quick slot choice) |
| `Public/UI/HUD/Pickup/MKHItemPickupWidget.h` | Parent of `W_ItemPickUp`: hold-to-equip hint |
| `Public/Player/PlayerController/MKHPlayerController.h` | `InteractAction` (press / hold), `Server_PickupItem`, `CanCollectPickup` |
| `Public/GameMode/MKHGameMode.h` | `SetPhase` → item spawn points, then (`MatchOver`) `DestroyAllPickups` |
| `Public/Inventory/InventoryItem/InventoryItem.h` | `UInventoryItem`, UI view of an item |
| `Public/UI/HUD/Inventory/MKHQuantityPopupWidget.h`, `MKHSplitStackDragOperation.h`, `InventoryDashboardWidget.h` | Drop / split UI |

Assets: `BP_ItemBase` and `BP_ItemSpawnPoint` (`/Game/Makhia/Blueprints/PickUpItem/Blueprints/`), `NS_GlowingCircle`
(`/Game/Makhia/Blueprints/PickUpItem/Material/`), `W_ItemPickUp` (`/Game/Makhia/UI/GameUI/PickupUI/`),
`W_QuantityPopup` and the inventory dashboard widgets, item DataTables in `/Game/Makhia/Data/Inventory/DataTables/`.

### Who runs what

| Operation | Where it runs |
|---|---|
| Generating items / IDs, spawning pickups, `AddItem`, `AddItemEntry`, `RemoveItem`, equip, `DestroyAllPickups` | Server only (`BlueprintAuthorityOnly` or authority checks; clients get a warning) |
| `UseItem`, `DropItem`, `SplitStackToQuickSlot`, restack, collect | Requested anywhere, executed on the server through validated Server RPCs |
| Pickup visuals (`InitializeFromItem`, `OnPickupItemInitialized`) | Every machine, derived from the replicated `Item` |
| Pickup animations (`UMKHPickupAnimationComponent`) | Every machine except a dedicated server, from the replicated `AnimationState` |
| Pickup selection (`UMKHPickupInteractionComponent`), candidate / focus state, widgets | Locally controlled PlayerController only (owning client, listen-server host) |

## Item identity: `FMKHItemEntry`

`Public/Inventory/MKHItemEntry.h`. The single source of truth for one concrete item instance, passed unchanged
between inventory, equipment, UI and world pickups.

| Field | Meaning |
|---|---|
| `ItemTag` | Which `FMasterItemDefinition` this item is an instance of. |
| `ItemID` | Unique for the whole game session across every player; `0` means "no item" (`IsValid()`). |
| `Quantity` | Stack size (always 1 for equipment). |
| `RarityTag` | Picked among the definition's `RarityTags` or rolled at generation. |
| `EffectPackage` | Rolled passive stats and abilities (equipment only). |

Everything static — name, icon, description, consumable cooldown, pickup mesh — is **not** stored in the entry:
it is read from the definition through `UMKHDataSubsystem`. Blueprints can read entries but cannot build them.

## Generation: `UMKHItemSubsystem`

`Public/Inventory/MKHItemSubsystem.h`, a `UGameInstanceSubsystem`. Only the authoritative world
(`NetMode != NM_Client`) may use it; calls on clients log a warning and return an invalid entry / ID 0.

- `GenerateItemID()` — sequential counter living on the GameInstance, so IDs never collide between players
  and survive level travel.
- `GenerateItem(ItemTag, Quantity, RarityRule = {})` — new `FMKHItemEntry` with a fresh ID. For equipment (tag under
  `Item.Equipment`) it resolves the rarity, rolls passive stats, and for weapons adds guaranteed + rolled abilities and
  assigns the dynamic skill input tags (`UMKHAbilitySystemLibrary::AssignDynamicSkillInputTag`). Unknown tags return
  an invalid entry. The rarity follows `FMKHItemRarityRule`:
  - `Default` (every caller except item spawn points): a roll weighted by the table's `RollProbability`, restricted to
    the definition's `FMasterItemDefinition::RarityTags` when that container is not empty
    (`UMKHItemSubsystem::PickDefinitionRarity` → `UEquipmentRollLibrary::RollRarityAmong`). One tag forces that rarity;
    listing every rarity but one excludes it while keeping the others' proportions. Allowed rarities that all weigh 0
    are picked uniformly; a container naming no rarity of the table falls back to the free roll with a warning;
  - `Fixed`: the given rarity; `Weighted`: a roll restricted to the listed rarities with the rule's own weights
    (`UEquipmentRollLibrary::RollRarityWeighted`);
  - a rule naming no rarity of the table falls back to `Default` with a warning. Non-equipment items (consumables)
    always take the rarity picked from their definition's `RarityTags` and ignore the rule.

Every item source uses it: `UInventoryComponent::AddItem` (grants), `AMKHPickupItem::SpawnGeneratedPickup` and item
spawn points (world spawns), and partial drops (`GenerateItemID` for the part that leaves the stack).

## Inventory

`UInventoryComponent` lives on the PlayerController and replicates a `FRPGInventoryList` (FastArray).

- `AddItem(ItemTag, NumItems)` — **server only** (`BlueprintAuthorityOnly`, no client RPC): generates the item
  through `UMKHItemSubsystem` and stores it with `AddItemEntry`. Default loadouts are granted by
  `BP_MKHPlayerController` on `BeginPlay` behind `HasAuthority`.
- `AddItemEntry(Item)` — server only, keeps the ItemID:
  1. an entry with the same ID still exists (weapons stay in the bag while equipped) → only `bIsUsed` is cleared;
  2. non-equipment → merged into an existing stack with the same tag (the incoming ID is dropped), preferring a stack
     that is **not quick-slotted** (`FindStackTarget`, which asks the quick slot manager); only when every stack of that
     tag is quick-slotted does the first of them receive it;
  3. otherwise → new entry (`AddSeparateEntry`, which never merges).
  It returns the ID of the entry now holding the item (the stack it merged into, or its own).
- `MoveQuantityToStack(SourceID, TargetID, Quantity)` — server only: moves part of a consumable stack into another
  stack of the same tag (the source goes away when emptied); `FindQuickSlottedStackID(ItemTag)` gives the first
  quick-slotted stack of a tag. Both serve the hold-to-equip pickup (see "Hold to equip").
- `UseItem(ItemID)` — client requests go through the validated `ServerUseItem`. Consumables apply their effect
  and cooldown read from the definition; equipment broadcasts `EquipmentItemUsedDelegate(FMKHItemEntry)`.
- `RemoveItem(ItemID, NumItems)` — **server only** (`BlueprintAuthorityOnly`): destroys items (no pickup). When the
  whole entry goes away it is first released (see below); equipped armor, which lives in the equipment and not in the
  bag, is unequipped back into the bag first so it can be removed.
- `DropItem(ItemID, Quantity)` — callable anywhere; clients go through `ServerDropItem` (validation only rejects
  `ItemID <= 0`; a missing item is a soft warning, not a kick). On the server (`DropItemOnServer`):
  1. compute the landing transform — `DropDistance` (100 cm) in front of the pawn along its yaw plus a random offset
     within `DropScatterRadius` (40 cm, so repeated drops do not overlap), pulled back `DropWallClearance` (30 cm) from
     any wall hit on the way (`ECC_Visibility`, ignoring the pawn and its attached equipment actors), then snapped to
     the ground with a downward trace of `DropGroundTraceDistance` (500 cm); no ground → the pawn's feet height.
     The traces are `AMKHPickupItem::FindRestLocation`, shared with the item spawn points. No pawn → the drop is refused;
  2. unequip the item if the equipment holds it (armor returns to the bag, a weapon gets `bIsUsed` cleared);
  3. build the dropped item: `Quantity <= 0`, `>=` the stack, or any equipment → the whole entry, same ItemID;
     otherwise a split part with a **new ItemID** while the stack left in the bag keeps its own;
  4. spawn it with `AMKHPickupItem::SpawnPickupFromItem` and the `Drop` appearance (thrown from the pawn, see
     "Animations") — **before** touching the inventory, so a failed spawn never loses the item;
  5. `RemoveItem` the dropped quantity.
  **UI**: every drop reaches the inventory through `UInventoryDashboardWidget::ConfirmDropItem(ItemID, Quantity)` →
  `UInventoryDashboardController::DropItem` → `UInventoryComponent::DropItem` (see `InventoryDashboard.md`).
  - Action menu "Drop" → `UInventoryDashboardWidget::RequestDropItem(Item)`: a consumable stack of more than one
    item opens the **quantity popup** in Drop mode; anything else drops whole at once. Confirming calls
    `ConfirmDropItem`.
  - Ctrl+Left click on an entry drops the whole stack (`ConfirmDropItem(ItemID, -1)`).
  - Dragging an entry with the mouse and releasing it outside the inventory window (`InventoryFrame`, the dashboard's
    1200x750 frame, `BindWidgetOptional` of `UInventoryDashboardWidget`) drops it whole, without the quantity popup,
    like Ctrl+click: the entry's `OnDragCancelled` (no widget took the drop) calls `TryDropDraggedItemOutside`.
    Released inside the window but on no slot, the item stays where it was. A drag cancelled by code
    (`CancelDragDrop`: Esc, the dashboard or action menu closing) sends an empty pointer event and never drops.
  - To drop part of a stack with the mouse: Shift+Left click opens the Split popup (below); the split part follows
    the cursor and, released outside the window, only that quantity is dropped (`NativeOnDragCancelled` of the
    dashboard, the drag's source widget). Shift is safe here: the popup and the drag that follow keep the player in
    the inventory, so Shift is never still held when Tab closes it (Shift+Tab opens the Steam overlay).
- **Quantity popup** (`UMKHQuantityPopupWidget`, layout `W_QuantityPopup`, class in `UInventoryDashboardWidget::
  QuantityPopupClass`, viewport Z order `QuantityPopupZOrder` = 50), shared by Drop and Split: slider 1..stack (starts at
  1) with "selected / stack", confirm button labelled with the action, Cancel; Enter confirms, Escape or a click outside
  the panel cancels, Left / Right arrows ±1. It closes with the dashboard (`CloseInventoryDashboard` /
  `DestroyInventoryDashboard`).

- **Releasing an item** that leaves the player entirely (`ReleaseItemBindings`): unequip through
  `EquipmentItemUnequippedDelegate` (the PlayerController forwards it to `UnEquipItemByItemID`; no-op when not equipped),
  then clear the quick slot in both places it can live: the entry flags (`RemoveEntryFromQuickSlot`, only set on the
  machine that assigned the slot — the listen-server host) and the server `UQuickSlotManagerComponent`, whose map
  replicates the removal to remote owners. A partial drop keeps the stack in its quick slot. Consuming the last item
  of a quick-slotted stack goes through `RemoveItem` too, so it frees the quick slot the same way.
- **Quick slot on entry removal** (`FRPGInventoryList::ReleaseQuickSlotOnRemoval`, in `RemoveItem` and
  `PreReplicatedRemove`): on the machine where the entry is flagged as quick-slotted (the owning client, or the
  listen-server host) the flags are cleared and `QuickSlotItemRelocatedDelegate` is broadcast before the removal, so the
  dashboard (bag, equipment weapon slots, quick slot area) and the HUD clear the slot through the same path as a manual
  removal. `PreReplicatedRemove` also drops the entry's quick slot preload handles.

## Split to quick slot

- **UI**: a left double click on an entry still quick-slots the whole stack (it replaced Shift+Left click: closing the
  inventory with Tab right after, Shift still held, opened the Steam overlay with Shift+Tab). The same double click
  equips / unequips armor and removes a quick-slotted entry from its slot (`UMKHInventoryEntryWidget::QuickEquip`).
  Bag entries are `TileView` rows: the row (`SObjectTableRow`) takes the double click for the list and never passes
  it to the entry widget, so the bag handles it (`UMKHInventoryBagWidget::HandleItemDoubleClicked`, `OnItemDoubleClicked`
  of `InventorySlots`); the equipment and quick slot entries use the entry's own `NativeOnMouseButtonDoubleClick`.
- **Dashboard first opening**: the dashboard is created the first time the inventory opens;
  `UInventoryDashboardController::BroadcastInitialValues` sends each entry already quick-slotted (starting loadout
  quick slot preferences, applied before any opening) to its quick slot, every other entry to the bag. The HUD
  has the same timing: `UMKHWeaponSkillRowWidget` gives its weapon slot the row's `QuickSlotTag` as soon as it caches
  its slots (initialization), not only in `NativePreConstruct`, otherwise `UMKHWeaponHUDWidget` finds no slot for a
  weapon quick-slotted before the HUD is constructed ("no slot for quick slot tag" in the log). The `W_ActionMenu` "Split" button (visible when
  `CanSplitItem`: a consumable with quantity > 1) calls `RequestSplitItem`, which opens the quantity popup in Split mode
  (range 1..N); Shift+Left click on a splittable entry opens the same popup. Confirming starts, on the next tick, a real UMG drag of a `UMKHSplitStackDragOperation` (source ItemID +
  quantity; payload = preview `UInventoryItem` with the split quantity; visual from `CreateItemDragVisual`, i.e.
  `DragVisualClass` = `W_EntryDragVisual`). The drag is started from code (`StartDragDropAtCursor`:
  `FUMGDragDropOp::New` + `FSlateApplication::ProcessReply(BeginDragDrop)`): the item follows the cursor and the next
  mouse release drops it (Slate ignores mouse presses while a drag is active).
- **Drop targets**: only consumable quick slots accept it — `UMKHInventoryEntryWidget::NativeOnDrop` first calls
  `TryDropSplitOnQuickSlot` (and `DragEnter` highlights through `CanDropSplitOnQuickSlot`). Everywhere else (bag,
  equipment, outside) nothing happens: the split was only virtual, so the quantity stays in the original stack.
  Closing the dashboard cancels the drag.
- **Server**: the dashboard goes through its controller (`UInventoryDashboardController::SplitStackToQuickSlot`, the
  controller is set on the widget by `SetInventoryWidget`) to `UInventoryComponent::SplitStackToQuickSlot(ItemID,
  Quantity, QuickSlotTag)`. Consumables and consumable quick slots only. Quantity covering the whole stack → the stack
  itself is assigned on the owning machine. Otherwise clients call `ServerSplitStackToQuickSlot`; `SplitStackOnServer`
  lowers the source, adds the part as a separate entry with a new ItemID and `AssignQuickSlotOnOwner`: the listen-server
  host assigns it directly, a remote owner receives `ClientAssignQuickSlot` (queued in `PendingQuickSlotAssignments` and
  applied from `PostReplicatedAdd` if the entry has not replicated yet).
- **Restack**: when a consumable stack leaves the quick slots by the player's choice — `RemoveEntryFromQuickSlot`
  (menu, drag to the bag) or pushed out by another item in `AddEntryToQuickSlot` — `RequestRestack` (client →
  `ServerRestackItem`) makes the server merge it into another stack of the same tag that is not quick-slotted
  (`RestackOnServer`); without one the stack stays as is. Drops, removals and consumption never restack.

## Equipment

`UEquipmentManagerComponent::EquipItem(const FMKHItemEntry&)` is **server only** and is reached exclusively
through `UseItem`: the equipment definition class is resolved from the item definition, never sent by a client.
Unequipping broadcasts `UnEquippedEntryDelegate`, and the owner's wiring (`FMKHItemComponentLinks::Bind`, used by
`AMKHPlayerController` and by the duel bot `AMKHDuelBot`) returns `Entry.Item` to the inventory with `AddItemEntry`,
so the item keeps its ID, rarity and rolls across equip cycles.

## World pickups: `AMKHPickupItem`

`Public/Inventory/Pickup/MKHPickupItem.h`. One item lying on the ground; the Blueprint subclass (`BP_ItemBase`,
configured as `PickupItemClass` in Project Settings -> Game -> Makhia Data) adds the widget, VFX and rarity colors.

- **Replication**: the actor replicates; `Item` (`FMKHItemEntry`) is `ReplicatedUsing = OnRep_Item` and
  `ExposeOnSpawn`. Visuals are never replicated: every machine derives them from `Item`.
- **Spawning** (server only, `BlueprintAuthorityOnly`, via `SpawnActorDeferred` so `Item` is set before `BeginPlay`
  and travels in the initial replication):
  - `SpawnGeneratedPickup(WorldContext, ItemTag, Quantity, Transform, ExclusiveCollector = nullptr, Appearance = Instant,
    DroppedBy = nullptr)` — new item from `UMKHItemSubsystem::GenerateItem`.
  - `SpawnPickupFromItem(WorldContext, Item, Transform, ExclusiveCollector = nullptr, Appearance = Instant,
    DroppedBy = nullptr)` — existing item, keeps its ID and rolls; the caller removes it from wherever it was stored.
  - `Transform` is always the resting place; `Appearance` (`Instant`, `Drop` from `DroppedBy`, `RiseFromGround`) only
    decides how it is shown getting there (see "Animations"). `Drop` without a pawn falls back to `Instant` with a warning.
  - The spawned class is `UMKHDataSubsystem::GetPickupItemClass()`; when not configured the native class is spawned
    (mesh only) with a warning.
- **Placing items in a level**: never place `BP_ItemBase` directly (it would hold no item); place an item spawn point
  (`BP_ItemSpawnPoint`, see `ItemSpawnSystemArchitecture.md`). Pickups therefore always spawn at runtime with their item
  in the initial replication (the former `InitialItemTag` / `InitialQuantity` were removed on 2026-09-28).
- **Collection rights**: `bExclusive` + `ExclusiveCollector` (`APlayerState`), replicated and set before `FinishSpawning`
  (a non-null `ExclusiveCollector` passed to the spawn functions makes the pickup exclusive). `CanBeCollectedBy(PlayerState)`
  is true for a free pickup, and for an exclusive one only for its collector — nobody once that player has left.
  `IsCollectibleByLocalPlayer()` applies it to the first local player (local UI). The rights belong to the pickup, not
  to the item: once collected, the item is a normal item (dropped again, it becomes a free pickup).
- **Placement helper**: `static FindRestLocation(World, Origin, Target, WallClearance, GroundTraceDistance, FallbackZ,
  QueryParams)` — horizontal trace from `Origin` to `Target` stopping `WallClearance` short of a blocking hit, then a
  downward trace to the ground (`FallbackZ` without ground); zero-length traces are skipped. Used by drops and spawn points.
- **Initialization** (`InitializeFromItem`, every machine): runs once from `BeginPlay` (after `Super::BeginPlay`, so
  Blueprint components are ready) and again from `OnRep_Item` only if the item changes after `BeginPlay`.
  It reads `PickupMesh` / `PickupMeshTransform` from the item definition, loads the mesh asynchronously if it is not in
  memory, applies it to the `PickupMesh` component (no collision), then fires `OnPickupItemInitialized(Item)`.
  Without a configured mesh the component keeps its default mesh.
- **Mesh transform**: `PickupMeshTransform` is composed on top of the `PickupMesh` layout authored in the Blueprint
  class defaults (`ItemTransform * BlueprintTransform`, read from the component archetype): identity keeps the Blueprint
  layout (e.g. the hover height), a row only holds per-item corrections (rotation, scale, offset in that frame).
- **Interaction range**: `InteractionSphere` (radius 250, z 10), query only, object type WorldDynamic, `Overlap` on every
  channel including the project's custom ones (`CameraObstacle`, `Weapon`), so it never blocks characters. Whether a pawn
  is inside is decided by `IsPawnInInteractionRange` (see "Interaction" below), not by overlap events.
- **`OnPickupItemInitialized`** (`BlueprintImplementableEvent`): where the Blueprint applies rarity color, dynamic
  material instances (the mesh is already set, so MIDs must be created here, not in the construction script), Niagara
  parameters and the pickup widget.
- **Lifetime and registry**: on the server each pickup registers with `UMKHItemSubsystem` in `BeginPlay` and
  unregisters in `EndPlay`. A pickup disappears when it is collected (destroyed once its collection animation is over,
  see "Animations"), when `UMKHItemSubsystem::DestroyAllPickups()`
  runs, which `AMKHGameMode::SetPhase` calls when the match enters `MatchOver` (no pickup outlives the match), or, for
  pickups of an item spawn point with `bKeepPickupBetweenRounds` off, when the next round is prepared. There is no timed
  despawn.

### Blueprint: `BP_ItemBase`

`/Game/Makhia/Blueprints/PickUpItem/Blueprints/BP_ItemBase`, child of `AMKHPickupItem`, set as `PickupItemClass` in
`DefaultGame.ini`; `/Game/Makhia/Blueprints/PickUpItem` is in `DirectoriesToAlwaysCook` because only the config
references it.

- **Components**: native `PickupRoot` (root), `VisualRoot` (child of the root, identity at rest, moved only by the
  animations), `PickupMesh` (child of `VisualRoot`; layout: z = 50, fallback mesh `SM_Axe`, no override material, so
  every item mesh keeps its own materials), `InteractionSphere` and `PickupAnimation` (`UMKHPickupAnimationComponent`,
  where the animations are tuned); Blueprint `NS_GlowingCircle` (z 70, child of `VisualRoot`, so the glow follows the
  item while it animates), `UI_Detail` (`W_ItemPickUp`, screen space, z 130, hidden by default), `Inner_icon`
  (billboard, z 120, hidden by default) and `InteractDetectionCollider` (sphere, radius 400, z 60, "icon" range). The
  widgets and ranges are attached to `PickupRoot`, not to the mesh, so neither the per-item mesh transform nor the
  animations move or scale them.
- **`OnPickupItemInitialized(InitializedItem)`** → `RarityTag = InitializedItem.RarityTag` → `ApplyRarityVisuals` →
  `RefreshInteractionWidgets` → `UI_Detail` widget cast to `W_ItemPickUp` → `UpdateItemRef(InitializedItem)`.
- **`ApplyRarityVisuals`**: `GetRarityMainColor` (`MainColor` of `UMKHDataSubsystem::FindRarityDefinitionForContext(RarityTag)`,
  white when the rarity is missing) → `PickupMesh.SetColorParameterValueOnMaterials("Color")` and `SetScalarParameterValueOnMaterials("IsPickup",
  1)` (these create a MID per material slot, or reuse it, so a second initialization does not stack MIDs) →
  `NS_GlowingCircle.SetColorParameter("ParticleColor")` → `NS_GlowingCircle.ResetSystem`. Only materials exposing those
  parameters are tinted. The construction script also calls it (editor preview).
- **Glow** (`NS_GlowingCircle`, emitter `SingleLoopingParticle`): one burst of **one immortal particle**
  (`Loop Behavior` Once, `Kill Particles When Lifetime Has Elapsed` off), simulated in **local space**, so it lives as
  long as the pickup and follows `VisualRoot` during the drop / rise / collect animations. The component does not
  auto-activate, `ResetSystem` starts it once the color is known, and the `Color` module (`User.ParticleColor`) runs in
  **Particle Update**, so the particle always follows the parameter. Before 2026-10-14 the particle lived 1 s and was
  simulated in world space: the glow vanished after a second, or stayed where the drop animation started (the
  dropping pawn's belly).
- **Glow material** (`M_PickUp_Glow`, translucent, Unlit): emissive = `ParticleColor` × `Glow Amount` (2) ×
  `EyeAdaptationInverse`, opacity = particle alpha × radial gradient × `DepthFade` (15 cm). The inverse eye adaptation
  keeps the glow equally bright on screen whatever the map exposure (Makhion uses a −2.7 EV bias, under which an
  uncompensated emissive glow is invisible); tune the strength with `Glow Amount` only. A long `DepthFade` (it was
  100 cm) erases the sprite next to floors and walls.
- **`RefreshInteractionWidgets`**, called by `OnPickupItemInitialized`, `OnInteractionStateChanged`,
  `OnPickupAnimationStarted` / `OnPickupAnimationFinished` and the begin/end overlap events of
  `InteractDetectionCollider`: `UI_Detail` visible ⇔ `IsInteractionFocused()` and `IsInteractable()`; `Inner_icon`
  visible ⇔ `InteractDetectionCollider` overlaps the local player's pawn (`GetPlayerPawn(0)`, so the other player's pawn
  never shows it), the pickup is not focused, `IsCollectibleByLocalPlayer()` and `IsInteractable()`. Result: detail
  widget on the focused pickup, icon on every other pickup within 400 cm that this player may collect, neither while
  the pickup animates or once it is collected.
- **`W_ItemPickUp.UpdateItemRef(FMKHItemEntry NewItem)`**: builds a UI view with `UInventoryItem::CreateFromItemEntry`
  (name, rarity, category = equipment slot leaf, quantity resolved from the definition through `UMKHDataSubsystem`) and
  stores it, then calls `UpdateUI`, which is also called on PreConstruct and Construct. `UpdateUI` renders texts and
  quantity, then — only when the rarity is found (`FindRarityDefinitionForContext`, which also works in the widget designer)
  — `MainBorder` color = rarity `MainColor` and `CategoryBorder` material = `MI_Square`; otherwise the designer look is
  kept. The category is empty for consumables.

## Interaction: aiming and collecting

Any player can collect any free pickup; an exclusive pickup only its collector (see "Collection rights"). Selection
is local, collection is server-authoritative.

- **Range test** — `AMKHPickupItem::IsPawnInInteractionRange(Pawn, ExtraReach)`: `InteractionSphere` against the pawn's
  upright collision capsule (`GetSimpleCollisionCylinder`: distance from the sphere center to the capsule's inner
  segment ≤ sphere radius + capsule radius + `ExtraReach`). It gives the same answer as an overlap, but it is pure
  geometry, so the client selection and the server validation apply exactly the same rule.
- **Selection** — `UMKHPickupInteractionComponent`, created by `AMKHPlayerController` (`PickupInteractionComponent`),
  runs only where the controller is locally controlled (owning client, listen-server host). Every `RefreshInterval`
  (0.1 s, a timer — no Tick, no overlap events, because turning the camera must re-rank the candidates):
  0. nothing is focused while the pawn is dead (`AMKHPlayerController::IsPawnDead`, tag `State.Movement.Dead`) or a UI
     screen has focus (`IsUIFocused`, tracked by `ChangeFocus`: inventory, game menu, end-of-match panel);
  1. candidates = every `AMKHPickupItem` whose item has replicated, that is interactable (`IsInteractable`: not
     animating on this machine and not being collected), that the local player may collect (`CanBeCollectedBy`: a
     pickup reserved to the other player is never a candidate, so it gets no focus, widget or icon) and whose range
     contains the local pawn;
  2. angles are measured between the player's view direction (`GetPlayerViewPoint`) and the direction to each
     `GetInteractionFocusPoint()` — the center of the displayed mesh, since the root lies on the ground; candidates
     beyond `MaxConeAngleDegrees` (30°) are discarded. Ranking uses the angle minus `FocusStickinessDegrees` (3°) for the
     pickup already focused (hysteresis: nearly aligned pickups do not flicker); every candidate within
     `FocusTieToleranceDegrees` (2°) of the best ranked angle counts as a tie, and the tie goes to the pickup closest to
     the pawn. No candidate in the cone → no focus, and Interact does nothing;
  3. `AMKHPickupItem::SetInteractionState(bIsCandidate, bIsFocused)` on each pickup (pickups that left the range are
     cleared first, the focused one is updated last). It fires the `BlueprintImplementableEvent`
     `OnInteractionStateChanged` only on change (see `BP_ItemBase` above for the widgets).
     `IsInteractionCandidate()` / `IsInteractionFocused()` expose the current state. This state is local and never
     replicated.
- **Collecting** — `InteractAction` (`IA_Interact`, `E` / pad `X` in `IMC_BaseMapping`, no trigger: implicit Down) →
  `HandleInteractStarted` → `Server_PickupItem(FocusedPickup)` (Server, Reliable), always on the press (a held press
  may then equip the item, see "Hold to equip"). On the server:
  1. a null pickup (already destroyed: another player was faster, or a double press) is ignored with a `Verbose` log;
  2. `CanCollectPickup`: the player has a pawn and an inventory, the pawn is not dead (`IsPawnDead`; the Dead tag is
     applied on the server too), the pickup holds a valid item, has finished appearing (`HasFinishedAppearing`, on the
     server clock: a pickup still flying or rising cannot be collected) and is not reserved to another player
     (`CanBeCollectedBy`, warning otherwise), and the pawn is in range with
     `PickupServerReachTolerance` (100 cm) of extra reach — the server's copy of the pawn lags behind the client that
     chose the pickup;
  3. `TryClaimForPickup` — only after every check, so a refused request never locks the pickup; the first claim wins;
  4. `UInventoryComponent::AddItemEntry(Item)` (keeps the ItemID; a consumable merges into the bag stack of the same
     tag, dropping the pickup's ID) — the item is the player's right away — then `StartCollect(Pawn)`: the collection
     animation plays on every machine and the server destroys the pickup when it is over (which also unregisters it).
     The receiving entry ID, the quantity and the time are kept (`LastCollectedItemID`, cleared first by every
     request, so a refused one leaves nothing to equip) for `Server_EquipLastPickup`.

## Hold to equip

Pressing Interact always collects the item at once (collection animation, item in the bag). Keeping the button held
for `HoldToEquipDuration` (0.5 s, on `AMKHPlayerController`, about the length of the collection animation) then
equips it or puts it in a quick slot; releasing earlier leaves it in the bag. Setting: Settings → General → Gameplay →
"Hold to Equip Pickups" (`UMKHGameUserSettings::bHoldToEquipPickup`, on by default, local to each machine).

**Input** (owning machine, `AMKHPlayerController`; no key in code, the hold is timed from the action's events):

| Event of `IA_Interact` | Effect |
|---|---|
| `Started` (`HandleInteractStarted`) | `Server_PickupItem(Focused)`; with the setting on and a pickup requested, `bInteractHoldPending` |
| `Triggered`, every frame (`HandleInteractHeld`) | pending and `Instance.GetElapsedTime()` ≥ 0.5 s → `Server_EquipLastPickup()` (once); a UI screen with focus or a dead pawn cancels |
| `Completed` / `Canceled` (`HandleInteractReleased`) | ends the hold |

The hold does not follow the pickup (it is already collected and hiding): it applies to the item this press
collected. The two RPCs are reliable on the same actor, so the server always handles the pickup first.

**Server** (`FMKHPickupAutoEquip`, `Public/Inventory/Pickup/MKHPickupAutoEquip.h`, pure rules): `Server_PickupItem`
`Server_EquipLastPickup` (reliable, validated, no parameter) takes the item recorded by the last `Server_PickupItem`,
only once and only within `HoldToEquipServerWindow` (2 s, lag margin), and passes its entry ID and picked quantity to
`ApplyOnServer`. Clients never send item data: only the pickup, then a bare request.

- **Armor** (equipment that is not a weapon, with an equipment class): `UseItem` → equipped; the piece it replaces
  returns to the bag (`UEquipmentManagerComponent::EquipItem` unequips the slot first).
- **Weapon** → `AssignQuickSlotOnOwner` into: the weapon quick slot holding a weapon of the **same type**
  (`UEquipmentDefinition::SlotTag`, the "one weapon per type" rule of `W_Equipment` `GetRandomWeaponSlot` /
  `UInventoryDashboardController::FindQuickSlotChoices`); else the first empty weapon slot; else the slot whose weapon
  is **not in hand** (`bIsUsed` false); else weapon slot 1. The weapon pushed out goes back to the bag; if it was in
  hand it is unequipped (`RelocateQuickSlotOccupant`) and the new weapon is not drawn: the player draws it with its
  slot key (equipping from the server would bypass the weapon swap ability).
- **Consumable**: the picked quantity moves into a quick-slotted stack of the same tag if there is one
  (`MoveQuantityToStack`; nothing to do if the pickup already merged into it); otherwise the stack that received it is
  assigned to the first empty consumable slot, else consumable slot 1 (the stack pushed out is restacked into the bag,
  as for a manual assignment).

Quick slot flags live on the owning machine, so the assignment uses the existing path: the listen-server host (and an
offline match against the bot) assigns directly, a remote client receives `ClientAssignQuickSlot`, queued until the new
entry replicates. The server's choice reads its mirror (`UQuickSlotManagerComponent`).

**Prompt** — `W_ItemPickUp` derives from `UMKHItemPickupWidget` (`Public/UI/HUD/Pickup/`): `HoldHintBox` (row under the
category) with `HoldHintText` ("Hold: equip" / "Hold: quick slot", from `FMKHPickupAutoEquip::GetAction`, `TS_Overline`).
The key itself is the prompt's `W_KeyCap` (`IA_Interact`). `AMKHPickupItem` drives it from C++ through its widget
components (`ForEachPickupWidget`): `SetHoldHint(Item)` when the pickup gains the focus (hidden with the setting off or
for items holding does nothing for). `BP_ItemBase` needs no graph for it. There is no hold progress bar: the prompt
hides as soon as the press starts the collection.

## Animations

`UMKHPickupAnimationComponent` (`PickupAnimation` on `AMKHPickupItem`). Purely cosmetic: the server decides everything
at once (the pickup is spawned at its resting place, a collected item is in the inventory immediately) and every
machine plays the animation locally from the replicated `AnimationState`.

- **What moves**: only `VisualRoot` (mesh + glow), never the actor, so interaction ranges, focus and server checks
  always use the resting place. The pose is expressed for the mesh pivot (`PickupMesh`'s location): the component
  scales and spins `VisualRoot` around it, so squash and spin happen around the item itself. Done procedurally (not
  with physics), so every machine ends exactly on the same resting place.
- **State** (`FMKHPickupAnimationState`, replicated): `Type` (`None` / `Drop` / `RiseFromGround` / `Collect`), `Pawn`
  (dropping or collecting pawn), `Origin` (server-side fallback when `Pawn` is not available on a machine),
  `ServerStartTime`. The appearance is set before the pickup finishes spawning, so it arrives with the initial
  replication and is started from `BeginPlay` (before `InitializeFromItem`); the collection arrives through
  `OnRep_AnimationState` (the server plays it directly for the listen-server host).
- **Drop** (inventory drops, `DroppedBy` = the dropping pawn): starts at the pawn's belly (capsule center,
  `DropOriginForwardOffset` 30 cm in front, `DropOriginHeightOffset`), computed on each machine from its own copy of the
  pawn; flies on a parabolic arc (`DropArcHeight` 50 cm) in `DropFlightDuration` (0.3 s) growing from `DropStartScale`
  (0.5) and spinning `DropSpinDegrees` (360°); then `DropBounceCount` (2) damped bounces (`DropBounceHeight` 20 cm,
  `DropBounceDamping` 0.35) in `DropBounceDuration` (0.2 s), with a squash at each contact (`LandingSquash` 0.25 for
  `LandingSquashDuration` 0.07 s). Total 0.5 s.
- **Rise from ground** (item spawn points): from `RiseDepth` (80 cm) below the resting place, ease-out, growing from
  `RiseStartScale` (0.3) and spinning `RiseSpinDegrees` (180°), no bounce, in `RiseDuration` (0.5 s).
- **Collect**: from where the item is seen up to `CollectAboveHeadHeight` (40 cm) above the collector's capsule in
  `CollectRiseDuration` (0.25 s), a pause of `CollectHoldDuration` (0.05 s), then down to the belly
  (`CollectTargetHeightOffset`) shrinking to `CollectEndScale` (0.1) in `CollectDescendDuration` (0.2 s), spinning
  `CollectSpinDegrees` (360°); the targets follow the collector while it moves. At the end `VisualRoot` is hidden; the
  server destroys the pickup after the animation plus `CollectDestroyMargin` (0.25 s), so clients finish it first.
- **Collection rules**: appearing pickups cannot be collected — locally `IsInteractable()` is false while an animation
  plays (no focus, no widget, no icon), and the server refuses until `HasFinishedAppearing()` (its own clock, which
  clients never run ahead of). A collected pickup (`IsBeingCollected()`) is never interactable again.
- **Late receivers**: a machine that learns about an animation more than `LateStartTolerance` (1 s) after its end
  (synchronized server clock, `AGameStateBase::GetServerWorldTimeSeconds`) does not replay it: an appearance shows the
  pickup at rest, a collection hides it. Otherwise the animation plays from its first frame.
- **Blueprint hooks**: `OnPickupAnimationStarted(Type)` / `OnPickupAnimationFinished(Type)` (on every machine that plays
  it; `BP_ItemBase` refreshes the interaction widgets), `IsPlayingPickupAnimation()`, `IsInteractable()`,
  `IsBeingCollected()`, `PickupAnimation.GetAnimationDuration(Type)`.
- **Dedicated server**: nothing is played (no tick); only the timings (`HasFinishedAppearing`, destroy delay) are used.

## HUD quick slots

- **Empty look**: `W_WeaponQuickSlotHUD` and `W_ConsumableQuickSlotHUD` cache the designer brushes of `SlotIcon` /
  `SlotBg` in `OnInitialized` (`CacheDefaultBrushes`) and restore them in `ClearQuickSlot` (`RestoreDefaultBrushes`)
  before the empty-slot icon and tint are applied, so an emptied slot looks exactly like it did at start (the rarity
  material of the previous item no longer stays on the background).
- **Cooldown display** (`W_ConsumableQuickSlotHUD`, `W_SkillSlotHUD`): the overlay follows the cooldown tag of the item
  currently in the slot. `StopCooldownDisplay` (on clear, and in the skill slot before re-reading the weapon skills)
  ends the tag listener, clears the refresh timer and hides the overlay; `SyncCooldownDisplay`, called right after a
  new listener is created, shows a cooldown that is already running (the listener only reports later changes).

## UI: `UInventoryItem`

A UObject view used only by widgets: wraps `FMKHItemEntry` plus UI state (`bIsEquipped`, quick slot) and caches
presentation data derived from the definition (name, icons, slot, stats, consumable cooldown). It never takes part
in gameplay logic. `UInventoryItem::CreateFromItemEntry(WorldContext, Item)` builds a view of an item that is not in
an inventory (used by `W_ItemPickUp`).

**Bag entries and tooltips.** The bag (`W_Bag`) is a TileView: its `W_InventoryEntry` widgets are recycled, so an
entry shows different `UInventoryItem`s over time (`NativeOnListItemObjectSet` → `InitEntry`). The entry tooltip is
its `ToolTipWidgetDelegate` bound to `GetToolTipWidget`, evaluated on every hover from the entry's current item. Never
call `SetToolTip` on an entry: it replaces the delegate with a widget built for the item of that moment, and once the
entry is recycled (drop, pickup, equip) it shows another item's tooltip (bug B2, playtest 2026-10-21). The action menu
hides the tooltip through `SetToolTipSuppressed`, which `GetToolTipWidget` reads.

## Extending the system

### Adding an item that can lie on the ground
1. Add the row to the item DataTable of its category (`DT_Consumables`, `DT_Equip_*`; a new category needs its
   table mapped to the category tag in `DA_InventoryDefinitions`).
2. Fill `PickupMesh` (a StaticMesh) and, only if the mesh needs it, `PickupMeshTransform` (identity keeps the
   `BP_ItemBase` layout; use it for per-item rotation, scale or offset).
3. For the rarity tint the mesh materials must expose a vector parameter `Color` and a scalar `IsPickup`; other
   materials are shown unchanged.

### Spawning a pickup from gameplay code
Server only (the functions are `BlueprintAuthorityOnly` and refuse client worlds with a warning):

```cpp
// New item (rarity and stats rolled now, fresh ItemID).
AMKHPickupItem::SpawnGeneratedPickup(this, ItemTag, /*Quantity*/ 1, SpawnTransform);

// Existing item: keeps ItemID and rolls; the caller removes it from where it was stored.
AMKHPickupItem::SpawnPickupFromItem(this, Item, SpawnTransform);

// Either one reserved to a single player (only that player state may collect it).
AMKHPickupItem::SpawnPickupFromItem(this, Item, SpawnTransform, OwnerPlayerState);

// Animated appearance: thrown from a pawn to SpawnTransform, or emerging from the ground there.
AMKHPickupItem::SpawnPickupFromItem(this, Item, SpawnTransform, nullptr, EMKHPickupAppearance::Drop, DroppingPawn);
AMKHPickupItem::SpawnPickupFromItem(this, Item, SpawnTransform, nullptr, EMKHPickupAppearance::RiseFromGround);
```

The same two nodes are available in Blueprint (`Items|Pickup`, `ExclusiveCollector` under the advanced pins). To put
items in a level, place item spawn points (`ItemSpawnSystemArchitecture.md`).

### Tuning the animations
Open `BP_ItemBase`, select the `PickupAnimation` component: every duration, height, scale, spin and bounce parameter
is in its Details panel (categories `Pickup Animation|Drop`, `|Rise From Ground`, `|Collect`, `|Network`). Stop PIE
before editing, then play again. Setting a duration to 0 skips that phase (all of them at 0 = no animation).
A visual that must follow the item while it animates goes under `VisualRoot`; one that must stay at the resting place
(widgets, ranges) under `PickupRoot`.

### Changing how pickups look
Visuals belong to `BP_ItemBase`: react to `OnPickupItemInitialized` (item ready, mesh applied),
`OnInteractionStateChanged` (candidate / focused) and `OnPickupAnimationStarted` / `OnPickupAnimationFinished`. MIDs created in the construction script only serve the editor
preview: at runtime the mesh is set after it, so the runtime visuals must be applied in `OnPickupItemInitialized`.
Niagara parameters read by a module of **Particle Spawn** are frozen at the particle's birth: put the module in Particle
Update or reset the system after setting the parameter (see "Glow"). An effect that must follow the item simulates
in local space; an emissive that must read the same in every map is multiplied by `EyeAdaptationInverse`.

## Testing

PIE with Net Mode "Play As Listen Server" and 2 players. Gameplay functions can be driven from a Python script run
with `py <file>` in the editor console, but **only on objects of the server world** (`UEDPIE_0_`): calling a function
that sends a Server RPC on an object of a client world makes the RPC run locally on the client and can recurse until
the editor crashes. Keys (Interact, inventory) and UMG buttons must be tested by hand.

## Known limitations

- The pickup widget shows no category for consumables.
- Item assets to check: the `cuirass` mesh (Bronze Chest) has empty material slots; `M_WeaponSet_1` and
  `MI_PROP_pot_dungeon` may not expose `Color` / `IsPickup`, so those items may not show the rarity tint.
- Exclusive pickups stay visible to the other player (only the interaction is hidden and refused).
