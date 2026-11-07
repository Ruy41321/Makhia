# Armor Mitigation

How the defender's **Armor** reduces the damage that reaches **Health**.

This document is the single source of truth for the armor model: `GASArchitecture.md` and
`DamageAndStatusEffects.md` describe *where* it sits in the damage pipeline and point here for *how*
it is computed.

---

## 1. Design intent

Armor is a **permanent rating, not a pool**. Hits never damage it: as long as a piece is equipped it
keeps protecting at full strength, and all the remaining damage goes to Health. This keeps duels
readable (one bar to watch) and makes the armor you pick up a stable, predictable advantage.

It replaces the old Shield pool (a second bar that absorbed hits and wore out, with a shield-break
mechanic). The `Shield` / `MaxShield` attributes, the shield bar in the HUD and the pool maths are gone;
the Shield potion has no effect for now (to be redesigned or removed with the consumables balance).

---

## 2. Where Armor comes from

| Source | Amount | Mechanism |
|---|---|---|
| Starting value | `0` | `GE_DefaultStartingAttributes` overrides Armor to 0 (also DuelBot and Enemy_Primary) |
| Helmet (Headwear) | `20` | `UEquipmentDefinition::BaseArmorGiven` on the definition asset |
| Leggings (Legwear) | `30` | idem |
| Chest (Chestwear) | `50` | idem |
| Rolled bonus | per roll | `StatEffect.UniqueTier.ArmorUp` row → `GE_Equipment_Armor` (Add (Base) on Armor, level = rolled value) |

A full set gives **100 Armor**. `BaseArmorGiven` is applied on equip by
`UMKHAbilitySystemComponent::ApplyAndTrackBaseArmor`: an infinite `UMKHEquipmentArmorEffect` with the
amount as SetByCaller `Combat.Data.Armor`. Its handle is stored in the entry's `GrantedHandles`, so
unequipping removes it together with the rolled stats. Server only, like the rest of the equipment.

Base armor and rolled armor are separate infinite effects whose `Add (Base)` modifiers sum on the same
aggregator: a helmet (20) with a +5 Armor roll gives 25.

**Never reach `Armor` through a property redirect.** During the migration a `MaxShield → Armor`
`PropertyRedirects` made the old assets load, but an `FGameplayAttribute` resolved through it got its own
aggregator, separate from the one of `UMKHEquipmentArmorEffect`: whichever effect changed last overwrote
Armor (helmet 20 + roll 5 showed 5). The modifiers were reassigned to `Armor` and resaved, and the
redirect was removed; only the unused `W_Vitals_backup` still references `MaxShield`.

---

## 3. The formula

```
Reduction   = Armor / (Armor + ArmorForHalfReduction)        // ArmorForHalfReduction = 100
FinalDamage = Damage * (1 - Reduction)
```

- **Diminishing returns**: every point is worth a bit less than the previous one, and the reduction
  never reaches 100 %. Bonus armor from future rolls/buffs therefore cannot make a player immune.
- **Effective health**: `EffectiveHP = Health / (1 - Reduction) = Health * (1 + Armor / 100)`.
  Each point of Armor is worth exactly **+1 % effective health**, which makes the tuning linear and
  easy to reason about even though the reduction is not.

### Values

| Armor | Reduction | Effective HP (100 Health) | Hits to kill vs. no armor |
|---|---|---|---|
| 0 | 0 % | 100 | ×1.0 |
| 20 (helmet) | 17 % | 120 | ×1.2 |
| 30 (legs) | 23 % | 130 | ×1.3 |
| 50 (chest) | 33 % | 150 | ×1.5 |
| 70 (chest + legs) | 41 % | 170 | ×1.7 |
| 100 (full set) | 50 % | 200 | ×2.0 |
| 150 (full + rolls) | 60 % | 250 | ×2.5 |
| 200 | 67 % | 300 | ×3.0 |

---

## 4. Where it lives in the code

| What | Where |
|---|---|
| Attribute | `UMKHAttributeSet::Armor` (replicated, clamped to `>= 0`) |
| Formula + tuning constant | `UMKHAttributeSet::CalculateArmorDamageReduction` (static, BlueprintPure), `ArmorForHalfReduction` in `MKHAttributeSet.cpp` |
| HUD / tooltip read-out | `UMKHAttributeSet::GetArmorDamageReduction`, `W_StatsArea::SetArmorValue` ("100 (-50%)") |
| Pipeline step | `UExecCalc_Damage::ApplyArmorMitigation`, step 4 after the defensive multiplier |
| Application to Health | `UMKHAttributeSet::HandleIncomingDamage` (whole `IncomingDamage`, rounded) |
| Base armor on equip | `UMKHAbilitySystemComponent::ApplyAndTrackBaseArmor`, `UMKHEquipmentArmorEffect`, `FRPGEquipmentEntry::GetBaseArmor` |

Armor is captured on the **target without snapshot**, so it is read when the hit executes: equipping
or losing a piece mid-fight applies immediately. Because mitigation runs inside the execution,
`IncomingDamage` (and the `Event.HitReceived` / `HitInflicted` magnitude) is already the post-armor
damage that Health loses.
