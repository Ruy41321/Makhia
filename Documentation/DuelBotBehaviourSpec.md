# Duel Bot: Behaviour Spec

What the bot does in a duel and why, from the player's side. How it is built is in `DuelBotAI.md`.

## What I want from the bot

I want an opponent that feels like another person in the arena: fair, readable, and beatable by getting better.

- It plays by the player's rules: same moves, cooldowns, costs and timings.
- It knows only what a player could see or learn.
- It never gets extra health or damage on its base attributes.
- It always attacks towards the player. It makes mistakes in its choices, not in its aim.
- The player never sees the word "bot" or a difficulty menu.

## The actions

### Approach
- **Goal:** close the distance when the opponent is far.
- **When:** the opponent is in the far distance band.
- **The player sees:** the bot walking towards them. If they stand off the NavMesh, it gets as close as it can and waits.
- **How to beat it:** use the approach to prepare, or hit first with a ranged ability.

### Attack
- **Goal:** pick the single strike that is worth most right now, then decide again at the next combo window.
- **When:** it is not far, and attacking scores well: it has a good attack, a damage advantage, an open punish window, and health not far below the opponent's.
- **Prefers:**
  - strikes in reach, high damage, lethal hits and guard breaks;
  - useful status effects;
  - a slow, strong hit on a long stun, and a fast hit on a short stagger;
  - a weapon swap when the other weapon has a better option.
- **Punishes:** the end of an attack, an opponent out of stamina, a last dodge, and dangerous abilities on cooldown (the last two only at higher reading levels).
- **The player sees:** normal player attacks and combos that can stop halfway when something better comes up.
- **How to beat it:** dodge or block on reaction, keep stamina for a dodge, and don't leave openings after an attack.

### React
- **Goal:** answer the opponent's attack once per attack, with a human delay. Sometimes it doesn't react at all.
- **Options:**
  - **Block:** when the hit costs little stamina. Never when it would break the guard.
  - **Dodge:** more likely when it is behind on health. Needs stamina.
  - **Counter:** a strike already in reach, with stun better than stagger. A plain attack only when it can afford the trade. Never a counter too slow to land before an incoming stun or stagger.
- **Limit:** it can cancel a strike only where a player can.
- **The player sees:** a reaction that arrives late, varies and sometimes misses.
- **How to beat it:** feint, change timing, or attack while it is committed to a strike.

### Bait
- **Goal:** provoke the opponent, either to draw out their strong moves or to make a retreating opponent commit.
- **When:** it keeps trying to attack while the opponent backs away, or the opponent has dangerous abilities ready.
- **Options:** a forward dodge, a block, or turning its back for a moment.
- **While baiting:** it reacts faster and usually answers an attack with a counter.
- **The player sees:** an opening that looks too good.
- **How to beat it:** don't take the bait, or bait back and punish its counter.

### Play passive
- **Goal:** recover when it is losing.
- **When:** health or stamina is low and it is at a damage disadvantage. A stunned opponent gives it time to heal.
- **Options:** drink an instant or regeneration potion, or recover stamina.
- **The player sees:** the bot stops pressing and looks after itself.
- **How to beat it:** press the advantage before it recovers.

### Taunt
- **Goal:** give the bot some attitude.
- **When:** it won the round, rolled once per round.
- **The player sees:** the bot walks to the body and crouch-spams.
- **Note:** this can annoy someone who is losing, so its chance is a tuning value.

## Difficulty

Difficulty changes how well the bot decides, how much it knows and what it carries.

| | Easy | Medium | Hard |
|---|---|---|---|
| Reaction time | 0.45-0.65 s | 0.28-0.40 s | 0.20-0.28 s |
| Chance to react | 50% | 70% | 90% |
| Choice quality (Expertise) | 0.3 | 0.7 | 0.9 |
| Reads the opponent's | state only | + current attack | + cooldowns |
| Aggression | 0.4 | 0.5 | 0.7 |
| Bait willingness | 0.7 | 0.5 | 0.3 |
| Taunt chance | 30% | 70% | 100% |
| Loadout | all Common | Rare | Epic and Legendary |

Reaction time never goes below 0.2 s outside Bait, because no human reacts faster and it would look like cheating. While baiting it can reach 0.15 s, because it is already waiting for the attack.

The player will never choose a difficulty: the game will adapt without saying so in the Random Duel; the career shows its levels (see `OfflineModes.md`).

## Personality

Personality comes from `Aggression`, `BaitWilling` and `TauntChance`. Easy baits a lot and attacks less; Hard presses and baits less.
