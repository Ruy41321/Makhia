# Duel Bot AI

How the offline duel bot (`AMKHDuelBot`, StateTree `ST_DuelBot`) decides. The controller, the tree layout, the
reactions, the difficulty profiles and the loadout are described in `MatchFlowArchitecture.md` (*Bot AI*); the
learning path and the decisions behind them in `Handoff/DuelBotAIHandoff.md`. This file covers the attack choice (M8),
the state scores, the tuning table and the debugging tools (M8.1). The behaviour from the player's side is in
`DuelBotBehaviourSpec.md`.

## 1. Principles

- **The bot plays the player's game**: same abilities, inputs, cooldowns, costs, timings and data
  (`DT_AbilityTuning`, montages). It never reads anything a player could not see or learn.
- **Difficulty is how it decides and what it carries, never its base attributes**: `FMKHDuelBotTuning` and
  `LoadoutTable` (`UMKHDuelBotProfile`).
  - `Expertise` (0-1) is **only the sharpness** of the choices: the same scores, a more or less certain pick.
  - `OpponentReadingFactor` (Low / Medium / High) is **what it knows** about the opponent (table in section 4).
  - `LoadoutTable` is **what it fights with**: each profile points at its own table (`DT_MKHStartingLoadout_AllCommon`,
    `_Rare`, `_Epic-Legg`), and a better loadout makes the bot stronger.
- **Utility, not rules**: every option gets a score from small considerations (0-1 each, multiplied); a weighted
  random draw picks one. Hard vetoes (ability not usable) are enter conditions or candidate filters, never a 0 score.
- **Testable**: the whole scoring is in pure libraries (no world access): `FMKHAttackScoring` (which attack) and
  `FMKHStateScoring` (which state), covered by the Automation Specs `Makhia.AI.AttackScoring` and
  `Makhia.AI.StateScoring`. The StateTree only links their 0-1 outputs.
- **Two tuning levels**: the difficulty profile (`DA_DuelBotProfile_*`) holds the personality and the loadout; the generic scoring
  parameters live in one DataTable row (`DT_DuelBotScoring`) that the profile references (section 8).

## 2. Pipeline

```
Context ─┐
Vitals ──┼─> Attack Options evaluator ──(AttackCandidates, AttackUtility)──> Combat chooses Attack (utility)
Perception┘     FMKHAttackCandidateBuilder (data)              │
                FMKHAttackScoring (pure)                         v
                                                         Commit Attack task (draw with p_i, freeze ChosenAttack)
                                                                 │
                         ReadChoice → SwapWeapon? → CombatApproach? → ActivateAbility → Combat
```

| Piece | File | Role |
|---|---|---|
| `FMKHAttackScoring` | `Public/AI/MKHAttackScoring.h` | Pure scoring: considerations, damage advantage, danger, choice probabilities, draw |
| `FMKHAttackCandidateBuilder` | `Public/AI/MKHAttackCandidateBuilder.h` | Game data → scoring inputs (tuning row, timing, perk rows, weapon definition) |
| *MKH Duel Bot Vitals* | `Evaluator/MKHDuelBotVitalsEvaluator` | Attributes of both sides, `DamageAdvantage` |
| *MKH Duel Bot Perception* | `Evaluator/MKHDuelBotPerceptionEvaluator` | What the bot sees: attacks, block, crowd control, punish windows, danger |
| *MKH Duel Bot Attack Options* | `Evaluator/MKHDuelBotAttackOptionsEvaluator` | Candidates of both weapons, scores, `p_i`, `AttackUtility` |
| *MKH Commit Attack* | `Task/MKHCommitAttackTask` | Draws one candidate on entering Attack (and the counter of React), logs the decision |
| *MKH Approach Opponent* | `Task/MKHApproachOpponentTask` | Walks to the opponent; outside the NavMesh, as close as it can, retrying |
| `FMKHStateScoring` | `Public/AI/MKHStateScoring.h` | Pure state scores: Attack, Bait, PlayPassive, React (Block / Dodge / counter) |
| *MKH Duel Bot Decisions* | `Evaluator/MKHDuelBotDecisionsEvaluator` | State scores, best counterattack, Bait chase tracking |
| *MKH Logged Float Input* | `StateTree/MKHLoggedFloatInputConsideration` | Links a state score, sharpened by the expertise |
| *MKH Bot State Enabled* | `Condition/MKHBotStateEnabledCondition` | Enter condition of every selectable state (runtime toggles) |

## 3. Attack candidates

Every tick the Attack Options evaluator lists the damage abilities (abilities with a `DT_AbilityTuning` row) of the
**two weapons** in the bot's item entries (`Context.CurrentWeaponEntry`, `Context.SheathedWeaponEntry`). The swap is
instant, so the sheathed weapon competes on equal terms (no swap penalty).

Vetoes:

- **Weapon in hand**: no granted spec for the `AbilityTag` (`UMKHAbilitySystemComponent::FindAbilitySpecForAbilityTag`)
  or `CanActivateAbility` false (cooldown, stamina cost, blocking state), as a key press would check.
- **Sheathed weapon**: no possible swap (`AMKHDuelBot::CanSwapWeapon`), or the skill's `CooldownTag` on the bot (it
  outlives the swap). Its input tag is the one the grant will give
  (`UMKHAbilitySystemComponent::ResolveEquipmentAbilityInputTag`, same rule as `GrantEquipmentAbility`); while its
  ability class is not loaded it is loaded asynchronously and the ability competes from the next ticks.

Each candidate (`FMKHAttackCandidate`, in `MKHAttackScoring.h`) carries `AbilityTag`, `InputTag`, `EngageDistance`
(reach x `EngageDistanceFactor` 0.8), `Reach`, `bNeedsSwap` and `SwapInputTag`, `CrowdControl` (strongest),
`TimeToHit` (of the scored strike), `Score`, `Probability` and the per consideration `Breakdown`.

### One strike at a time

A candidate is **the single strike a press performs** (`FMKHAttackCandidateBuilder::BuildStrikeCandidate`): a light is
priced by its next hit, never by the whole combo. The bot then re-decides at every combo window:

1. The strike runs; when its `ContinueCombo` window opens, *MKH Activate Ability By Input* returns Succeeded without
   cancelling the ability, and Combat re-selects.
2. During the window (`FMKHOpenComboWindow`, from `UMKHDamageAbility::IsWithinComboWindow`) the running attack competes
   with its **next strike** (damage and status of that strike, `TimeToHit` from the previous strike). The other attacks
   compete too: the running attack blocks them only until the window ends, so they are vetoed by cooldown and cost only.
3. Choosing the running attack presses its input in the window: the combo continues. Anything else (another attack,
   Bait, PlayPassive) means no press: the strike ends the ability at the window end. A swap, or an attack the priority
   rules refuse, waits for that end before its press (*Reactions and priority*).

The opponent's danger (`ComputeAbilityDanger`) still reads every strike: the bot must fear the whole combo.

### Considerations (`FMKHAttackScoring`, defaults in `FMKHAttackScoringParams`)

Final score = product of the eight. Every consideration has a floor above 0, so none alone erases a candidate.

| Consideration | Inputs | Score |
|---|---|---|
| Distance | distance, `GetEffectiveReach()` | 1 within reach, down to 0.2 at 600 cm beyond it (`DistanceCurve`) |
| Damage | weapon `BaseDamage` x the strike's `DamagePercent` x bot output x opponent incoming x (1 - armor); blocked hits x `BlockedDamageFactor` (0) | 0.3-1 relative to the strongest candidate; 1 when lethal |
| Guard break | the strike's stamina damage x (1 - opponent weapon block stability) vs opponent stamina | 1 without a block or when it breaks it; 0.3-0.8 by the share of stamina taken |
| Status effect | perk rows of the strike (Empower, Weaken, Exposed, Reinforced, Stun, Stagger) | 0.5 + 2 x relative gain of `DamageAdvantage` (shifts capped by the attributes' max) + 0.4 stun / 0.25 stagger |
| Crowd control fit | opponent stunned/staggered, time left, approach time + `TimeToHit` | miss 0.1; stun: 0.4-1, the slower attack that still lands wins; stagger: 0.4-1, the faster wins; 1 without CC |
| Cooldown economy | skill `CooldownTime` | 1 for basic attacks; a skill costs up to 0.6 (at 20 s), discounted by `PunishWindowScore` and by a lethal hit |
| Variety and loyalty | last attack started, input tag | light (`Input.Ability.Attacks.Basics.Light`): 1 in hand, `LightSwapScore` 0.8 in the sheath, never a repeat penalty; others: 0.7 for the same attack, 1 otherwise |
| Speed | the strike's `TimeToHit` (the approach is Distance's) | neutral situation: 1 up to `FastHitTime` 0.3 s, down to `MinSpeedScore` 0.6 at `SlowHitTime` 1 s (`SpeedCurve`); 1 when the opponent is crowd controlled (Crowd control fit decides there) |

**Light loyalty** (M8.1): a light combo stays on the weapon in hand; the sheathed weapon's light wins only when it is
clearly better (more than 1 / 0.8 of the held one's score). Heavy and skills keep the repeat penalty.

`TimeToHit` comes from the montage notifies (`AbilityTuning.md`, *Timing*). Status effects always count and add up;
the damage advantage they bring weighs on the will to attack through `DamageAdvantage` (never an on/off switch).

### Choice and utility

- `p_i = s_i^k / sum(s_j^k)` with `k = Expertise x 6` (`FMKHDuelBotTuning::GetDecisionSharpness`, the only place where
  Expertise becomes k). Expertise 0: uniform choice; 0.5: mostly the good ones; 1: exact argmax
  (`IsExactChoice`). A better option always has a higher chance.
- `AttackUtility = sum(p_i x s_i)`: the expected score of the choice, what attacking is worth.
- *Commit Attack* draws with `p_i` on entering Attack and freezes `ChosenAttack` until the state is left.

## 4. Perception and reading levels

| Output | Low | Medium | High |
|---|---|---|---|
| Opponent attacking, recovery after an attack | yes | yes | yes |
| `bIsOpponentBlocking` (after the reaction delay, rolled per block) | yes | yes | yes |
| Opponent stunned / staggered | yes | yes | yes |
| Crowd control time left | typical length - time seen (`TypicalStunDuration` 2 s, `TypicalStaggerDuration` 0.8 s) | same | exact (effect time left) |
| `OpponentAttackAbilityTag`, `OpponentAttackDanger` (after the reaction delay) | - | yes | yes |
| Opponent cooldowns (`OpponentAbilitiesStatus`) | - | - | yes |

### Punish windows

`PunishWindowScore` is the best open window (`PunishWindows` tells which):

| Window | Reading | Score |
|---|---|---|
| Recovery: the opponent's attack just ended | Low | 1 fading to 0 over `RecoveryWindowDuration` (0.6 s) |
| OutOfStamina: `State.General.Stamina.Out` | Low | 1 |
| LastDodge: stamina below `DodgeStaminaCost` | Medium | 0.6 |
| DangerousInCooldown: a dangerous ability (danger >= 0.7) in cooldown | High | 0.5 x (1 - danger of the abilities still usable) |

### Danger

`FMKHAttackScoring::ComputeAbilityDanger`, from the opponent's tuning row: stun 1.0, stagger 0.7; a strike whose
effective multiplier (`DamagePercent` x opponent output x bot incoming) reaches 1.8 is dangerous from 0.6 up to 1 as
it takes the bot's remaining health; below the threshold it scales down. Parameters: `Perception.Danger` of the scoring
row.

`OpponentReadyDanger` (Bait): the highest danger of the opponent's abilities ready to use. High: every ability of the
weapon in hand off cooldown; Medium: the danger of the last attack it recognised (it does not track cooldowns); Low: 0.
`OpponentAttackStaminaDamage`: the stamina damage of the recognised attack (Block score).

## 5. The tree (`ST_DuelBot`)

- **Combat**: *Try Select Children At Random Weighted By Utility*. Each child has **one** *MKH Logged Float Input*
  consideration bound to its score of the Decisions evaluator (`Scores.Attack`, `Scores.Bait`,
  `Scores.PlayPassive`), range 0-1, no curve, `bApplyExpertise` on and `Expertise` bound to `Tuning.Expertise`
  (`(a x b)^k = a^k x b^k`: linking the product sharpens exactly as the four considerations did). Weights stay 1.
  SafeApproach keeps its Constant consideration.
- Every selectable state has the enter condition *MKH Bot State Enabled* (`AI.State.<Name>`), see section 6.
- **Attack**: enter conditions band not Far, `bHasAttackCandidates`. Task: *MKH Commit Attack*. Children, in order:
  1. `ReadChoice` (Delay 0 → next selectable state). StateTree evaluates enter conditions during the selection, when
     the parent's task has not entered yet: a condition bound to `ChosenAttack` would be forced to false. From
     `ReadChoice` on, Attack is active and its siblings read the choice.
  2. `SwapWeapon` [`ChosenAttack.bNeedsSwap`]: presses `SwapInputTag` (instant swap) → next selectable state.
  3. `CombatApproach` [`Distance > ChosenAttack.EngageDistance`]: *MKH Approach Opponent* with radius `EngageDistance`
     → next selectable state; still running after 3 s → Combat.
  4. `ActivateAbility`: presses `ChosenAttack.InputTag`, one strike (*One strike at a time*) → Combat.
- **React** (*Highest Utility*, entered once per opponent attack on `Event.AI.ReactToAttack`): `Dodge`, `Block`,
  `ActivateAbility` with their Decisions scores (`Scores.Dodge`, `Scores.Block`, `Scores.Counter`; not sharpened:
  the choice is an argmax), Weights 1. `ActivateAbility` [`bHasCounterAttack`] is **one leaf**: *MKH Commit Attack*
  on `CounterAttackCandidates` (the counter alone, p = 1, for the decision log) and *MKH Activate Ability By Input*
  with `InputTag` = `CounterAttack.InputTag` and `SwapInputTag` = `CounterAttack.SwapInputTag` (swap, then the
  attack, in one task) → Combat. No child states: transitions to states inside React (`NextSelectableState`,
  `GotoState` to a sibling) failed with "Could not trigger completion transition" during the M8.1 tests; the cause in
  the engine was not found, the leaf avoids it.
- **Idle** (last Combat child) [`AI.State.Idle` enabled]: Constant 0.0001, Delay 0.5 s → Combat. Enabled only while a
  state is disabled with the console, so Combat always has a child to select (with every score 0 the weighted choice
  fails, Root fails and the tree stops). The controller also restarts a stopped tree (`RestartStoppedTree`, warning,
  at most once a second).
- **SafeApproach** [band Far]: Constant 1, Weight 1, not sharpened. It and Attack exclude each other through the band
  (Attack needs band not Far), but in Far it competes with Bait and PlayPassive, whose scores are raised to k.
- **Bait** (*Random Weighted By Utility*): *MKH Enable Bait Awareness*, children with fixed Constant values, not sharpened
  (Expertise would only make the bot repeat one bait), shared among those whose enter conditions pass:
  - `Dodge` 0.4 (forward, enter: *Can Activate* Dodge and a bound float > 0.8);
  - `Block` 0.4 (enter: *Can Activate* Block);
  - `ExposeBack` 0.2: *MKH Turn Relative To Opponent*, 180 degrees, held 1.5 s.

  Each returns to Combat on completion. The awareness switches Perception to `BaitReactionChance` and
  `BaitReactionTimeRange`. It ends when the state is left, or earlier after `MaxBaitTime` (Perception evaluator), so
  those values never outlive the state.
- **Reactions**: `Combat → React` on `Event.AI.ReactToAttack`, High priority, no condition. The Bait children repeat it.

### Reactions and priority

The bot's reactions follow the player's rules (`AbilityCancelSystem.md`), so a reaction can interrupt only some strikes:

- **Leaving a state** does not cancel an ability that holds a priority tag (`FMKHActivateAbilityByInputTask::CanCancelOnExit`).
  The exceptions are a dead bot and a task past `MaxDuration`. Without a tag (after `AN_MakeAbilityCancellable`) it cancels.
- **`React > Dodge`** has *Can Activate* (Dodge): `GA_Dodge` is blocked by any priority tag, so while one is on the
  state is not selectable and the next reaction is tried. With none selectable the event is lost and the strike goes on.
- **`React > Block`** presses at once: `GA_Block` cancels basic attacks; a skill keeps running until its tag goes
  (`AN_MakeAbilityCancellable`), then the Blocking state starts and cancels it. Test this at a normal frame rate: in a
  background PIE (about 3 fps) the notify falls between two samples and the skill seems to end at the press.
- **An attack chosen during another one** presses at once when `CanActivateAbility` allows it (a Priority First skill
  over a Second attack, or after the notify) and its own `CancelAbilitiesWithTag` interrupts the running one. Otherwise,
  and for every weapon swap, it waits for the running attack to end (`ShouldWaitForAttackEnd`, at most `StartTimeout`).

### Approach outside the NavMesh

`SafeApproach` and `CombatApproach` use *MKH Approach Opponent* (`FMKHApproachOpponentTask`), not the engine's Move To,
which fails at once when the opponent is outside the NavMesh: the tree then re-selected the state several times per
frame. The task never fails for an unreachable opponent: it walks to the nearest NavMesh point (searched within
`NearestPointExtent`), waits there and retries every `RetryInterval` (0.25 s) until the opponent is in reach or the state
is left. Replacing a node with `ObjectTools` drops its property bindings: rebind them.

### State completion of parent tasks

**Every state with tasks needs at least one task considered for completion** (*MKH Commit Attack* on Attack, *MKH Enable
Bait Awareness* on Bait: both stay Running until the state is left). The cause is an engine detail found on 2026-10-30:

- A state whose tasks are all "not considered" gets an **extra completion bit** after its task bits
  (`MakeCompletionTasksMask`), and entering the state resets only the task bits (`ResetStatus(TasksNum)`).
- That bit is shared with other states of the same depth: the second task of `PlayPassive`, `React > Dodge`,
  `React > ActivateAbility` writes it when it completes, and it stays set.
- The state then reads as completed at once. The engine starts the completion transitions from the first completed
  state (`TriggerTransitions`), so the child's own transition is skipped: `Attack → Combat` instead of
  `ReadChoice → next`, or `Could not trigger completion transition, jump back to root` for Bait, which has none.

Symptom: a `Commit attack` every frame, the bot standing still between `Attack > ReadChoice` and the Bait children; only
entering `PlayPassive` (which resets both bits) ended it. A task that finishes in `EnterState` cannot be the only task
of a parent state: make it stay Running.

## 5b. State scores (`FMKHStateScoring`, parameters `States` of the scoring row)

Each consideration is an `FMKHUtilityCurve` (input range normalized to 0-1, then a curve; empty = linear).

| State | Score |
|---|---|
| Attack | `Utility` (AttackUtility 0-0.3, floor 0.2) x `DamageAdvantage` (0.5-2, floor 0.5) x `PunishWindow` (floor 0.8) x `Aggression` (floor 0.6) x `HealthGap` (bot health ratio - opponent's: -1 → 0.5, 0 and above → 1) |
| Bait | `BaitWilling` x `Health` (bot health ratio: 0 → 0.4, 0.5 and above → 1) x max(**chase**, **danger**). Chase = `ChaseTime` (seconds since the first Attack commit not followed by an attack, 0-3 s) x the share of recent time the opponent moved away (opening faster than `RetreatSpeedThreshold` 50 cm/s, moving average over `RetreatAveragingTime` 1.5 s; forgotten after `ChaseMemory` 3 s without a commit). Danger = `Danger` curve of `OpponentReadyDanger` (0.5 → 0, 1 → 0.8) |
| PlayPassive | max(`Health` (1 at 0, 0.95 at 25%, 0.5 at 50%, 0 when full), `LowStamina` (1 at 0, 0.6 at 30%, 0 when full)) x `DamageDisadvantage` (inverse of DamageAdvantage, 0.85 when even) x `FreeWindow` (opponent's CC time left, 0-2 s, floor 0.7) |
| React > Block | the block's stamina cost (incoming stamina damage x (1 - our block stability); `AverageIncomingStaminaDamage` 25 without a recognised attack) as a share of our stamina, through `BlockStaminaShare` (0 → 1, 0.6 → 0.5, 1 → 0.1); **0 when it would break the guard** |
| React > Dodge | `DodgeHealthGap` of opponent health ratio - bot health ratio (-1 → 0.3, 0 → 0.6, 1 → 1): safer when behind; **0 without stamina for a dodge** |
| React > ActivateAbility | best candidate in reach **without approaching** (`Reach >= Distance`): stun `StunCounterValue` 1 > stagger 0.7; attacks without crowd control only when the bot can trade (health ratio >= 0.6 or ahead), `PlainCounterScale` 0.8 x damage x status scores (M8 scoring, status effects included); times `CounterHealth` and `CounterAggression` |

**Low health** (M8.3): Attack and Bait fall when the bot is low and PlayPassive rises, because the sharpening
(`score^k`) turns a small gap into a near certainty. At 20% against a full opponent PlayPassive is 0.57, against an
Attack of about 0.30-0.38 by profile. The bot still attacks when even (20% against 20%: Attack has no health penalty).

**Answering a bait** (M8.3): when the event is sent during `Bait`, `BaitCounterChance` (0.85) is rolled once per opponent
attack. If it passes and a counter is possible, Block and Dodge score 0 and the trade veto on plain attacks is lifted.
Without a counter in reach, or when the roll fails, the normal scores decide. It reads the active state rather than
`IsBaiting`, because PlayPassive enables the bait awareness too and must stay defensive.

**No slow counter against crowd control** (M8.3): Perception finds the next strike of the recognised attack (first strike
with a hit time after the time elapsed). If it stuns or staggers, candidates with `TimeToHit` >= its time left get 0: the
bot would be interrupted first. Time left, not the strike's full time, because the reaction delay has already passed.
It needs a recognised attack, so reading Low has no filter and can still be interrupted. Perception hands both values to
the controller (`SetIncomingStrike`, `SetBaitWantsCounter`), which Decisions reads in the same tick, so the tree needs no
new binding.

The Decisions evaluator logs `State scores: ...` to the Visual Logger every tick (Verbose).

## 6. Debugging

- **Log / Visual Logger** (`LogMKHAI`, on the controller): every `Commit attack: <tag> (score, p)` with the full
  candidate table; the evaluator logs the table (Verbose) when the best candidate or the count changes; Perception
  logs blocks, crowd control and punish window changes; Decisions logs the state scores (Verbose).
- **On screen**: `bPrintStateTreeChanges` on `BP_DuelBotController` (on) prints the active state path at each change
  (`StateChangePrintDuration`, not in Shipping).
- **Console** (not in Shipping, server world: an offline match):
  - `MKH.Bot.DumpAttackOptions`: prints the scoring context, the candidate table and the state scores (with chase,
    ready danger and counter) on screen and in the log, without the bot acting.
  - `MKH.Bot.Pause` / `MKH.Bot.Resume`: the StateTree pauses (tasks keep their state), the movement stops and every
    held input is released; resume continues where it was. While paused the evaluators still tick
    (`UMKHDuelBotStateTreeComponent::TickEvaluatorsOnly`: no task, no transition), so `SetHealth` and the dumps show
    the decision the bot would take. Set up a situation, then dump or resume.
  - `MKH.Bot.State.Disable <Name> [Name...]`, `Enable`, `Only <Name> [Name...]`, `Reset`, `List`: names are the
    `AI.State.` tags without the prefix: `SafeApproach`, `Attack`, `Bait`, `PlayPassive`, `React`, `React.Dodge`,
    `React.Block`, `React.ActivateAbility`. Enabling a state enables its children (`Only React` keeps the three
    reactions). `Only` disables every other one (e.g. `Only Attack React`); a disabled React means no reaction at all.
    While any state is disabled, Combat > Idle is the fallback. Session only.
  - `MKH.Bot.Tuning.Set <Field> <Value>`: session override, by reflection, of a profile tuning field (tried first:
    `Aggression 0.9`, `ReactionTimeRange (Min=0.2,Max=0.3)`) or of the scoring row (`AttackScoring.RepeatScore 0.5`,
    `States.Bait.ChaseMemory 5`, `Perception.Danger.StunDanger 0.8`); `MKH.Bot.Tuning.Dump` logs the effective values
    and the overrides, `MKH.Bot.Tuning.Reset` drops them.
  - `MKH.Debug.ApplyStatus <PerkTag> [player|bot]`: applies a status effect as a hit would (e.g.
    `StatEffect.Perk.Stun.ThunderShock`), the other duelist as the source.
  - `MKH.Debug.SetStamina <Value> [player|bot]`: sets the current stamina (regeneration goes on).
  - `MKH.Debug.SetHealth <Value> [player|bot]`: sets the current health, clamped to 1-MaxHealth (never kills).

Table columns: `Dist Dmg Guard Stat CC CD Var Spd | Score p TTH`, flags `[swap]`, `[lethal]`, `[guard break]`.

## 7. Tests

- **Automation Spec** `Makhia.AI.AttackScoring` (`Private/AI/Tests/MKHAttackScoringSpec.cpp`; Session Frontend →
  Automation, or the MCP `AutomationTestToolset`): damage advantage, long stun → heavy, short stagger → fast strike,
  single strike damage, speed, guard break and block stability, lethal hit and cooldown economy, status effects within
  the caps, light loyalty, danger, choice distribution at Expertise 0 / 0.5 (fixed seed, 20000 draws) / 1.
- **Automation Spec** `Makhia.AI.StateScoring` (`Private/AI/Tests/MKHStateScoringSpec.cpp`): utility curve, block vs
  dodge vs counter (guard break veto, block stability, average stamina damage, health gap, stun > stagger, reach,
  trade), bait from chase and from danger, PlayPassive and Attack floors.
- **Scenarios in Makhion** (`open Makhion?Mode=Bot?BotProfile=Hard`): `MKH.Bot.Pause`, restrict the states
  (`MKH.Bot.State.Only ...`), put a duelist in a state (`SetHealth`, `SetStamina`, `ApplyStatus`), then
  `MKH.Bot.DumpAttackOptions` or `MKH.Bot.Resume`. Decisions do not depend on the frame rate (unlike damage in a
  background PIE).
- **Hand playtest** per profile (Easy / Medium / Hard): responses to stun, stagger, out of stamina, last dodge
  (Medium+), dangerous abilities in cooldown (High), healing while the opponent is stunned when the bot is losing.

## 8. Tuning knobs

Two levels (decided 2026-10-29):

| Where | What |
|---|---|
| `DA_DuelBotProfile_*` (`FMKHDuelBotTuning`) | personality and skill: `Aggression`, `BaitWilling`, `TauntChance`, reaction times and chances, `Expertise` (sharpness), `OpponentReadingFactor`; plus `ScoringTable` (+ `ScoringRowName`, None = first row) and `LoadoutTable` (`DT_MKHStartingLoadout_*`, one item per row) |
| `DT_DuelBotScoring` (`FMKHDuelBotScoringRow`, `Content/Makhia/Data/AI/`), one `Default` row shared by the three profiles | `AttackScoring` (every attack consideration, `LightSwapScore`), `EngageDistanceFactor`, `Perception` (punish windows, danger, typical CC lengths), `States` (state score curves) |

For different scoring per difficulty, add a row (or a table) and point the profile at it. The controller re-reads the
row every tick (`AMKHDuelBotController::GetScoring`), so a DataTable edit during PIE applies at once; session
overrides come on top (`MKH.Bot.Tuning.Set`). The Context band distances and the Bait awareness time stay on the tree
(they are not scoring).

## 9. Known limits

- With Expertise 1 the bot is deterministic for a given situation; the variety consideration then makes it alternate
  between equivalent attacks, swapping weapons often (the swap is free by decision).
- The Bait chase also counts the counterattack's commit (it starts at once, so it never accumulates).
- The attack is abandoned only by the 3 s approach timeout (decision: no other abort for now).
