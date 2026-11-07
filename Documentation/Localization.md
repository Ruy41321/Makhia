# Localization

Makhia ships in **English** (native language), **Italian** and **French**. Every piece of text the
player sees is an `FText` gathered into one localization target, translated in `.po` files and
compiled into `.locres` resources that the engine loads at runtime.

> The French translation was written by Claude and must be reviewed by a native speaker before
> release. Italian and French use the informal "tu" throughout.

---

## Architecture

| Piece | Where | Role |
|---|---|---|
| Target **Game** | `Config/Localization/Game_*.ini` + `[/Script/Localization.LocalizationSettings]` in `Config/DefaultEditor.ini` | Gather / export / import / compile configuration. Same settings as the Localization Dashboard shows (Tools → Localization Dashboard). |
| Gathered text | `Content/Localization/Game/Game.manifest` | Every localizable text, with its namespace, key and source location. |
| Translations | `Content/Localization/Game/<culture>/Game.archive`, `Game.po` | `.po` is the file translators edit; the archive is what the engine compiles. |
| Runtime data | `Content/Localization/Game/<culture>/Game.locres`, `Game.locmeta` | Loaded by the engine; staged in packages through `+CulturesToStage` in `Config/DefaultGame.ini`. |
| ICU data | `InternationalizationPreset=EFIGS` in `Config/DefaultGame.ini` | Culture data for number/date formats and language names. `English` would strip Italian and French. |
| Language switch | `UMKHLocalizationLibrary` (`Source/Makhia/Public/Libraries/`) | Lists shipped languages, reads/switches the current one. |
| Settings UI | `UMKHGeneralSettingsWidget`, option **Language** | See `SettingsArchitecture.md`. |

Gather sources:

- **C++**: `Source/Makhia`, `Source/MakhiaSteam` and `Source/MakhiaDiscord` (`LOCTEXT` / `NSLOCTEXT`; the Steam and Discord modules are listed in `SearchDirectoryPaths` of `Game_Gather.ini` and in `DefaultEditor.ini`: add any new game module the same way — the dormant `MakhiaEOS` has no texts). Editor-only code (`#if WITH_EDITOR`, data validation messages) is skipped.
- **Legal documents**: namespace `MKHLegal`, generated C++ from `Documentation/Legal/*.md` (`LegalDocuments.md`);
  Steam, Discord and Valve are named directly there (exception to the `{Platform}` rule below).
- **Config**: `NSLOCTEXT` values in `Config/*.ini` (crash dialog message in `DefaultEngine.ini`, see `CrashReporting.md`; project title in `DefaultGame.ini`). `Config/Localization` is excluded.
- **Assets**: `Content/Makhia` and the external actors/objects of its maps. `Content/ThirdParty`, `Content/Developers` and `Content/Localization` are excluded.

### Language at runtime

The engine's current language is the only source of truth; nothing is stored in `UMKHGameUserSettings`.

- **First launch**: the engine picks the system language when it ships (e.g. `it-IT` → `it`), English otherwise.
- **Switch** (`UMKHLocalizationLibrary::SetLanguage`): packaged game → `SetCurrentLanguageAndLocale` saved to `GameUserSettings.ini` `[Internationalization] Language/Locale`, read back by the engine at the next start. Editor → the **game localization preview** (`EnableGameLocalizationPreview` + the *Preview Game Language* editor preference), so the editor UI never changes language.
- **Live update**: text already on screen follows the switch on its own — plain localized texts, `FText::Format` results and `AsNumber`/`AsPercent` are rebuilt by the engine. Widgets that build their rows once (settings pages) rebuild on page activation. Only text created from strings (`FromString`, BP `ToText (String)`, `Append`) would stay stale — which is why the rules below forbid it in UI.

### Known editor-preview differences

The preview only swaps *game* text. In PIE:

- numbers keep the editor's locale (e.g. `100%` instead of French `100 %`): the preview changes the language, not the locale.

Check it in a packaged build.

---

## Rules for new text

- **Key names stay English in every language** ("Tab", "Escape", "Left Shift"): `UMKHInputSettingsLibrary::GetKeyDisplayName`
  returns the source string of `FKey::GetDisplayName` as culture-invariant text, because `Engine.locres` translates
  them in a packaged build (Italian "Scheda" for Tab, bug B3 of the 2026-10-21 playtest). Gamepad buttons use the short
  Xbox labels. Never show `FKey::GetDisplayName` directly.

1. Every visible text is an **`FText`**, localizable: `LOCTEXT`/`NSLOCTEXT` in C++, a text literal in Blueprints and assets.
2. **Never** build UI text from strings: no `FText::FromString`, `FText::FromName`, BP `ToText (String)` / `Append` for sentences. Player names and other data that must not be translated are fine as culture-invariant text.
3. Sentences with values use **format arguments**: `FText::Format` / BP *Format Text* with named arguments (`{Value}`, `{Quantity}`). The format pattern is the translatable text; numbers go in as numbers (`FText::AsNumber`, `AsPercent`) so each language formats them itself. Plural-sensitive text can use `{0}|plural(one=…,other=…)` in the translation.
4. Rich text: the tags are part of the format pattern (`<StatValue>{Value} </><StatName>{Name}</>`), never concatenated.
5. **UI names never come from gameplay tags or asset names.** Item categories come from `UItemTypesToTables::CategoryNames` (`DA_InventoryDefinitions`) through `UMKHDataSubsystem::GetItemCategoryName`.
6. Designer placeholders that code overwrites (numbers, `x`, `—`, `↺`, fake names, previews of C++ text), developer-only UI (DevTool / DeviceID login) and language names are **culture invariant** (`INVTEXT`, or *Localizable* unticked in the Details panel), so they never reach the `.po` files.
7. Upper case comes from the text style (`TextTransformPolicy`) where the style applies it; do not write sources in capitals unless the style doesn't.
8. Comparing texts: never compare displayed strings to decide logic (they change with the language). If unavoidable, compare the source string (`FTextInspector::GetSourceString`), as `UMKHKeyboardSettingsWidget` does for categories.

---

## Workflow: adding or changing text

All steps work from the Localization Dashboard (target **Game**) or from the command line; the command line is:

```
UnrealEditor-Cmd.exe "<Project>/Makhia.uproject" -run=GatherText -config="<Project>/Config/Localization/Game_<Step>.ini" -unattended -nopause
```

1. **Save** every asset you changed (the gather reads packages from disk).
2. **Gather** (`Game_Gather.ini`): rebuilds the manifest and archives. Check `Game_Conflicts.txt` (same key, different text) — it must be empty.
3. **Export** (`Game_Export.ini`): writes `Content/Localization/Game/<culture>/Game.po`. New or changed entries have an empty `msgstr`.
4. **Translate** the `.po` files (it, fr), following the glossary and the style rules below.
5. **Import** (`Game_Import.ini`), then **Compile** (`Game_Compile.ini`): writes the `.locres`. Compile validates format arguments and rich text tags.
6. **Report** (`Game_GenerateReports.ini`): `Game.csv` must show the same word count for `en`, `it` and `fr` (100% translated).
7. Check the screens in PIE with the Language option (see Testing) and open the `Content/Localization/Game` files in Perforce.

The command line commandlet exits with code 1 when the editor is open, because the MCP plugin cannot bind port 8085 twice; the gather itself still succeeds (check the log and `Game.csv`).

---

## Translation style

- Informal "tu" in both languages. Short button labels: they are rendered in capitals with letter spacing.
- Keep `{placeholders}` and `<Tag>…</>` untouched; translations may reorder them.
- French typography: no-break space (U+00A0) before `: ; ! ?` and `%`, between a number and its unit (`5 s`, `50 PV`) and inside guillemets `« … »`. Italian writes `15%` and `5 s`.
- Seconds in the HUD cooldown (`{Seconds}s`) stay compact in every language.
- Fantasy proper names stay in English (*Slashy Slashy*, *AssKicker*); descriptive names are translated.

## Glossary

| English | Italiano | Français |
|---|---|---|
| Health / HP | Salute / PV | Santé / PV |
| Shield | Scudo | Bouclier |
| Armor | Armatura | Armure |
| Stamina | Stamina | Endurance |
| Base Damage | Danno base | Dégâts de base |
| Critical hit | Colpo critico | Coup critique |
| Cooldown | Ricarica | Recharge |
| Block / Blocking Stability | Parare / Stabilità in parata | Parer / Stabilité en parade |
| Dodge | Schivare | Esquiver |
| Perk (status effect) | Tratto | Atout |
| Empower | Potenziamento | Surpuissance |
| Reinforced | Rinforzo | Renforcement |
| Weaken | Indebolimento | Affaiblissement |
| Exposed | Esposizione | Exposition |
| Stagger | Sbilanciamento | Déséquilibre |
| Stun | Stordimento | Étourdissement |
| Skill | Abilità | Compétence |
| Quick slot | Slot rapido | Raccourci |
| Inventory | Inventario | Inventaire |
| Equip / Unequip | Equipaggia / Rimuovi | Équiper / Retirer |
| Drop (item) | Getta | Jeter |
| Split (stack) | Dividi | Diviser |
| Rarity: Common, Uncommon, Rare, Epic, Legendary | Comune, Non comune, Raro, Epico, Leggendario | Commun, Peu commun, Rare, Épique, Légendaire |
| Category: Hatchet, Sword, Hammer, Headwear, Chestwear, Legwear | Accetta, Spada, Martello, Testa, Torso, Gambe | Hachette, Épée, Marteau, Tête, Torse, Jambes |
| Potion | Pozione | Potion |
| Loot | Bottino | Butin |
| Pick up | Raccogli | Ramasser |
| Duel / Round / Match | Duello / Round / Partita | Duel / Manche / Match |
| Preparation phase | Fase di preparazione | Phase de préparation |
| Ready | Pronto | Prêt |
| Rematch | Rivincita | Revanche |
| Victory / Defeat / Draw | Vittoria / Sconfitta / Pareggio | Victoire / Défaite / Égalité |
| Opponent | Avversario | Adversaire |
| Lobby | Lobby | Salon |
| Lobby invite / Join | Invito alla lobby / Unisciti | Invitation au salon / Rejoindre |
| Friends / Friend request | Amici / Richiesta di amicizia | Amis / Demande d'ami |
| Add friend / Recent players | Aggiungi amico / Giocatori recenti | Ajouter un ami / Joueurs récents |
| Respond on {Platform} / {Platform} friends | Rispondi su {Platform} / Amici {Platform} | Répondre sur {Platform} / Amis {Platform} |
| In your party (friend in your lobby) | Nella tua lobby | Dans ton salon |
| Delivery (network quality) | Consegna | Livraison |
| Online / Away / Offline | Online / Assente / Offline | En ligne / Absent / Hors ligne |
| Display name | Nome visualizzato | Nom d'affichage |
| Credits (menu) | Riconoscimenti | Crédits |
| Settings / Apply / Revert / Defaults | Impostazioni / Applica / Ripristina / Valori predefiniti | Paramètres / Appliquer / Rétablir / Par défaut |
| Keyboard / Key binding slot | Tastiera / Slot | Clavier / Emplacement |
| Quality: Low, Medium, High, Epic, Custom | Bassa, Media, Alta, Epica, Personalizzata | Basse, Moyenne, Haute, Épique, Personnalisée |
| On / Off | Attivato / Disattivato | Activé / Désactivé |
| Appearance / Equipped (skin) | Aspetto / In uso | Apparence / Équipée |
| Skins: Arcanist, Oracle, Ember, Bloodcrown, Abyss | Arcanista, Oracolo, Brace, Corona di Sangue, Abisso | Arcaniste, Oracle, Braise, Couronne de Sang, Abysse |

Platform names ("Steam") are culture invariant (`INVTEXT`) and enter the texts through `{Platform}` arguments (`UMKHOnlineBackend::GetPlatformDisplayName`): never write a platform name in a translatable sentence.

Same English word, different translation by context: "Epic" is a rarity (`DT_RarityTable`, masculine in Italian: *Epico*) or a quality level (`MKHVideoSettingsWidget`, *Epica*). Every `.po` entry carries its source location to tell them apart.

---

## Adding a language

1. Localization Dashboard → target **Game** → *Add New Culture* (or add a `CulturesToGenerate` line to every `Config/Localization/Game_*.ini` and the culture to `SupportedCulturesStatistics` in `DefaultEditor.ini`).
2. `Config/DefaultGame.ini`: `+CulturesToStage=<code>`; check that `InternationalizationPreset` includes the culture (`EFIGS` covers en/fr/it/de/es; use `All` otherwise).
3. Check the fonts cover the alphabet (`Content/Makhia/UI/Fonts`, see `UITypography.md`); add a fallback font if not.
4. Gather → Export → translate → Import → Compile → Report, as above.
5. Nothing to change in code: the Language option lists whatever cultures have compiled data.

---

## Testing

- **PIE**: Settings → General → Language, or from Python `unreal.MKHLocalizationLibrary.set_language('fr')`. The choice persists across PIE sessions through the *Preview Game Language* editor preference (Editor Preferences → Region & Language); set it back to English when done.
- **Designer**: the same preference previews widgets in the chosen language.
- Screens to check for clipping (French is the longest): Settings (all pages), HUD and preparation prompt, inventory and tooltips, pickup prompt, quantity popup, match over, game menu (friend list, exit panel), lobby.
- **Packaged build**: first launch follows the system language; the choice survives a restart (`GameUserSettings.ini`); number formats are localized.
