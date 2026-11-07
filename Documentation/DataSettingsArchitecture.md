# Data Settings Architecture

Centralized access to the gameplay data assets shared by several systems (item definitions, rarity, equipment roll tables, world pickup class).

## Overview

```
Project Settings -> Game -> Makhia Data        (UMKHDataSettings, Config/DefaultGame.ini)
        │  soft references only
        ▼
UMKHDataSubsystem  (UGameInstanceSubsystem, server + clients)
        │  loads every asset once in Initialize(), keeps them alive (Transient UPROPERTYs)
        ▼
Typed lookups for C++ and Blueprints
```

- **`UMKHDataSettings`** (`Public/Data/MKHDataSettings.h`) — a `UDeveloperSettings` holding **only** soft references (`TSoftObjectPtr` / `TSoftClassPtr`), one named and typed field per asset, grouped by category. No `FName -> asset` maps: adding a new asset means adding a new field.
- **`UMKHDataSubsystem`** (`Public/Data/MKHDataSubsystem.h`) — the only reader of the settings. Loads the assets synchronously when the GameInstance initializes and exposes lookups. Gameplay code must not keep its own references to these assets.

## Configured assets

| Category  | Settings field         | Asset                                                        | Subsystem access |
|-----------|------------------------|--------------------------------------------------------------|------------------|
| Items     | `ItemDefinitions`      | `/Game/Makhia/Data/Inventory/DA_InventoryDefinitions`       | `FindItemDefinition` (C++), `GetItemDefinition` (BP), `GetItemTagsMatching` (BP, items under a category tag), static `FindItemDefinitionForContext` / `GetItemTagsMatchingForContext` (C++, any context) |
| Items     | `PickupItemClass`      | Blueprint subclass of `AMKHPickupItem` (`BP_ItemBase_C`)            | `GetPickupItemClass` |
| Equipment | `RarityTable`          | `/Game/Makhia/Data/Equipment/Rarity/DT_RarityTable`         | `FindRarityDefinition` (C++), `GetRarityDefinition` (BP), static `FindRarityDefinitionForContext` (BP, any context), `GetRarityTable` |
| Equipment | `EquipmentStatEffects` | `/Game/Makhia/Data/Equipment/DA_EquipmentStatEffects`       | `GetEquipmentStatEffects` |
| Equipment | `AbilityTuningTable`   | `/Game/Makhia/Data/Equipment/Abilities/DataTables/DT_AbilityTuning` | `FindAbilityTuning` (C++), `GetAbilityTuning` (BP); see `AbilityTuning.md` |

Current readers: `UMKHItemSubsystem` (item generation and rolls), `UInventoryComponent` / `UEquipmentManagerComponent` /
`UInventoryItem` / `AMKHPickupItem` (item definitions), `AMKHPickupItem::SpawnPickupFromItem` (pickup class),
`UMKHAbilitySystemLibrary::ApplyStatusEffects` (perk table inside `EquipmentStatEffects`), and every Blueprint that shows
rarity colors through `FindRarityDefinitionForContext`: `BP_ItemBase.GetRarityMainColor`, `W_ItemPickUp.UpdateUI` and
`GetRarityColors` in `W_InventoryEntry`, `W_EntryDragVisual`, `W_ItemBanner`, `W_ConsumableQuickSlotHUD`,
`W_WeaponQuickSlotHUD`. No asset references `DT_RarityTable` directly any more.

## Lookup semantics

- **Item definition**: iterates `UItemTypesToTables::TagsToTables`; for every category tag the item tag matches (`MatchesTag`), looks up the row named after the full item tag. Same rule previously used by `UInventoryComponent::GetItemDefinitionByTag`. A miss logs nothing at the DataTable level (callers log their own warning), because loot entries naming a category are looked up as an exact item first.
- **Items under a tag** (`GetItemTagsMatching`): every row, in the tables whose category contains the tag or lies under it, whose `ItemTag` matches it (the tag itself or its children); used by item spawn points for category loot entries.
- **Item category name** (`GetItemCategoryName`, static `GetItemCategoryNameForContext`): the localized text of `UItemTypesToTables::CategoryNames` (in `DA_InventoryDefinitions`) for the item tag itself or its closest parent (`Item.Equipment.Weapon.Hatchet.AssKicker` → entry `Item.Equipment.Weapon.Hatchet` → "Hatchet"); empty when nothing matches (consumables today). Cached by `UInventoryItem::GetCategoryName`, shown by the tooltip banner and the pickup prompt. UI names are never derived from gameplay tags.
- **Rarity definition**: first row of the rarity table whose `RarityTag` matches exactly (`UEquipmentRollLibrary::GetRarityByTag`).
- C++ `Find*` functions return pointers into the DataTable: use them immediately, never store them. Blueprint `Get*` functions return a copy plus a found flag.

## Usage

```cpp
if (const UMKHDataSubsystem* Data = UMKHDataSubsystem::Get(this))
{
    if (const FMasterItemDefinition* ItemDef = Data->FindItemDefinition(ItemTag))
    {
        // ...
    }
}
```

In Blueprints, use the `Get MKHDataSubsystem` node (GameInstance subsystem getter), then `Get Item Definition`; for rarity use `Find Rarity Definition For Context` (below).

`UMKHDataSubsystem::Get` returns `nullptr` where no GameInstance exists (editor preview worlds, widget designer, construction scripts in the editor): always check the result. In Blueprints, put an `IsValid` on the subsystem before calling one of its member functions, otherwise a `PreConstruct` running in the designer logs "Accessed None".

**Rarity from any context** — `UMKHDataSubsystem::FindRarityDefinitionForContext(WorldContext, RarityTag, OutDefinition)`
(static, `BlueprintPure`, the world context pin is hidden and defaults to `self`) is the node Blueprints use for rarity:
- with a reachable GameInstance (PIE, packaged game) it is exactly `FindRarityDefinition` on the subsystem;
- in the editor without a GameInstance (widget designer previews) it loads the rarity table referenced by
  `UMKHDataSettings` and applies the same lookup (`GetRarityByTag`), so design-time previews show the real rarity colors;
- the fallback is compiled only in editor builds (`WITH_EDITOR`), and no asset references the table directly.

**Items from any context** — `FindItemDefinitionForContext(WorldContext, ItemTag)` and
`GetItemTagsMatchingForContext(WorldContext, ParentTag)` (static, C++) follow the same pattern with the item tables:
subsystem when a GameInstance is reachable, otherwise (editor only) `UMKHDataSettings::ItemDefinitions` loaded on demand.
Used by the item spawn point editor preview and by the loot validation.

An empty rarity tag finds no definition (`GetRarityByTag` ignores the `NoRarity` row, whose tag is empty): callers get the default `FRarityDefinition` (empty name, white, no materials).

## Cooking

Assets referenced only by config are not guaranteed to be cooked, so `/Game/Makhia/Data` is listed in `DirectoriesToAlwaysCook` (`Config/DefaultGame.ini`, section `/Script/UnrealEd.ProjectPackagingSettings`).

## Adding a new shared asset

1. Add a `TSoftObjectPtr<T>` (or `TSoftClassPtr<T>` for classes) `Config` field to `UMKHDataSettings` in the right category, with a doc comment.
2. Add the matching `Transient` `TObjectPtr<T>` (or `TSubclassOf<T>`) to `UMKHDataSubsystem`, load it in `Initialize` via `LoadConfiguredAsset`, and expose a typed getter.
3. Assign the asset in Project Settings -> Game -> Makhia Data (writes `Config/DefaultGame.ini`).
4. Remove any per-system reference to the same asset.

## Planned migrations (not done yet)

`DA_CharacterClassInfo`, `ProjectileInfo` (GameMode) and `DA_GenericClassReference` (GameInstance) still use their own references.
