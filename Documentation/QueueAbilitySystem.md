# Queue Ability System

> Part of the GAS layer — see `Documentation/GASArchitecture.md` for the overall ability architecture this queue plugs into.

The Queue Ability System is designed to manage character actions by buffering inputs, allowing for a more fluid user experience and facilitating seamless combo execution.

Without this system, players would be forced to frame-perfectly time their next input after the current ability ends. This often leads to "dropped" inputs or delays in action strings. The Queue Ability System solves this by capturing inputs during a specific window and executing them at the earliest possible frame.

## Core Concepts

### Input Buffer Window
Abilities and animations define an **Input Buffer Window** via Animation Notifies. During this window, any ability activation request is registered and stored in a queue rather than being ignored or failing due to the character being "busy."

### Activation Timing
Queued abilities are triggered in two main scenarios:
1.  **On Ability End**: When the current ability calls `EndAbility()`.
2.  **On Cancelation Point**: When an animation reaches a frame designated as "cancelable," allowing the next action to interrupt the recovery frames of the previous one.

### Priority Rules
The system handles multiple queued inputs using specific priority logic:
-   **Weapon Swaps**: Generally take priority and can be prepended to attack chains (e.g., Swap -> Attack).
-   **Consumables**: Clear existing attack or consumable inputs to prioritize survival/utility.
-   **Attacks**: Multiple attack inputs are usually filtered to keep only the most recent or relevant one to avoid unintended "auto-playing" sequences.

## Technical Implementation

The system is primarily implemented within the `UMKHAbilitySystemComponent`.

### Data Storage
The queue is managed using a small, stack-allocated array of gameplay tags to minimize memory overhead while providing enough depth for complex sequences like "Swap into Attack."

```cpp
/** Array of queued Input Tags to activate abilities after the end of previous ones. */
TArray<FGameplayTag, TInlineAllocator<2>> QueuedAbilityTags;
```

### Queuing Logic
The decision to queue an ability is handled by `ShouldQueueAbility`, which checks if the buffer window is active or if the action is a high-priority weapon swap.

```cpp
bool UMKHAbilitySystemComponent::ShouldQueueAbility(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag) const
{
    using namespace MKHGameplayTags;
    
    // Check if the InputBufferWindow tag is active or if it's a weapon swap
    if (!HasMatchingGameplayTag(Combat::InputBufferWindow) && 
        !(InputTag.MatchesTag(Input::WeaponQuickSlotCategory)))
    {
        return false;
    }
    
    // If the ability is already active or can be activated immediately, don't queue
    if (Spec.IsActive() || (!IsOtherWeaponAtk(InputTag) && Spec.Ability->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get())))
    {
        return false;
    }
    
    return true;
}
```

The `QueueAbility` function then organizes the tags based on their category:

```cpp
void UMKHAbilitySystemComponent::QueueAbility(const FGameplayTag& InputTag)
{	
    using namespace MKHGameplayTags;
    
    if (InputTag.MatchesTag(Input::WeaponQuickSlotCategory))
    {
        // Prioritize weapon swap and allow an attack to follow
        QueuedAbilityTags.RemoveAll([&](const FGameplayTag& Tag) {
            return Tag.MatchesTag(Input::WeaponQuickSlotCategory) || Tag.MatchesTag(Input::Attacks);
        });
        QueuedAbilityTags.Insert(InputTag, 0);
    }
    // ... further logic for Consumables and Attacks
}
```

### Execution
The activation of queued abilities is triggered by a specialized Gameplay Event (`Event::ActivateQueuedAbility`), usually broadcast when an ability ends or reaches a cancelation point in the animation.

```cpp
void UMKHAbilitySystemComponent::OnActivateQueuedAbility(const FGameplayEventData* Payload)
{
    if (QueuedAbilityTags.IsEmpty()) return;
    
    AActor* Avatar = GetAvatarActor();
    AController* Controller = Avatar ? Avatar->GetInstigatorController() : nullptr;
    
    if (AMKHPlayerController* MKHPC = Cast<AMKHPlayerController>(Controller))
    {
        // Pop the next tag and re-trigger input handling
        MKHPC->AbilityInputPressed(QueuedAbilityTags.Pop());
        return;
    }

    // AI-driven owners (the duel bot) press their inputs straight on the ability system
    if (IsValid(Controller))
    {
        AbilityInputPressed(QueuedAbilityTags.Pop(), false);
    }
}
```

The player's inputs go back through `AMKHPlayerController::AbilityInputPressed` (quick equip when unarmed, skill HUD
notification); the duel bot has no PlayerController, so its queued inputs (pressed by its StateTree tasks) are replayed
on its own ASC, the entry point those tasks use.

### Weapon Swap In Flight (client-side synchronization)

On a remote client, a weapon swap resolves through a server RPC round trip (`UseQuickSlot` → `UInventoryComponent::ServerUseItem` → unequip/equip → ability specs replicate back via the PlayerState ASC). The swap ability itself ends locally long before that round trip completes, so without extra gating the queued attack would activate the **stale spec of the removed weapon** (locally predicted, then rejected and rolled back by the server).

To prevent this, `UMKHAbilitySystemComponent` tracks a client-only *weapon swap in flight* state:

-   **Set**: `UQuickSlotManagerComponent::UseQuickSlot` calls `NotifyWeaponSwapRequested()` when a weapon-category slot is used on a non-authoritative owner (no-op on the authority, where the swap is synchronous).
-   **Gate**: while in flight, `OnActivateQueuedAbility` keeps buffered attacks queued, `ShouldQueueAbility` queues new attack inputs instead of activating stale specs, and `AMKHPlayerController::EnsureWeaponEquipped` force-queues instead of re-triggering the quick-equip flow on stale equipment data.
-   **Clear**: when the swapped weapon's specs replicate down, `OnGiveAbility` schedules a **next-tick** flush (`FlushQueuedAbilityAfterGrant`) that clears the state and retries the queued input. The deferral is mandatory: `OnGiveAbility` fires mid FastArray delta serialize, where server-removed specs are still physically inside `ActivatableAbilities` (they are deleted only after all add/change callbacks) — activating synchronously would pick the removed weapon's stale spec, whose instance is already destroyed ("instanced ability is missing").
-   **Timeout**: if the server rejects the swap, a safety timeout (`WeaponSwapTimeoutSeconds`) clears the state and discards stale queued attacks.

The state is purely local to the client (zero extra network traffic); the perceived cost is ~1 RTT on the post-swap attack, masked by the swap animation.

### Priority Tags (client → server synchronization)

Abilities that must not be interrupted while a montage plays carry a **priority tag** (`GameplayAbility.Active.Priority.First/Second`, `UMKHGameplayAbility::PriorityTag`). It is a loose gameplay tag added in `ActivateAbility` and cleared when the ability reaches its cancelable point — the `Event.Animation.MakeAbilityCancellable` anim notify (`OnMakeAbilityCancellable`) — or, failing that, in `EndAbility`. Two independent systems read it:

-   **The movement state machine** (runs independently on every machine): gates transitions such as `Attacking → Blocking` on the tag being present.
-   **Ability activation**: both `UMKHGameplayAbility::DoesAbilitySatisfyTagRequirements` (the custom Priority1/Priority2 rules) **and** the engine's base `Super::DoesAbilitySatisfyTagRequirements` when a Blueprint lists a priority tag in its `ActivationBlockedTags` (e.g. `GA_Dodge` blocks on `Priority.First`).

Because both systems run on the server too, the tag must exist on the server ASC — not just on the predicting client. So `ActivateAbility` adds it under `IsLocallyControlled() || HasAuthority()`.

**The ordering hazard.** The tag drives *input arbitration*, and only the owning client knows the exact cancelable frame (it is anim-notify driven). The server's copy of the montage runs one latency behind — even at 0 ms ping it starts one activation-RPC later — so its `MakeCancellable` notify, which clears the tag, fires *late*. Meanwhile GAS sends `ServerTryActivateAbility(queued ability)` **before** the previous ability's cancel/EndAbility reaches the server. The consequence: the server validates the queued activation while its priority tag is still set, `Super` rejects it against `ActivationBlockedTags`, and the client's predicted activation (first montage frames + predicted cooldown) is rolled back. This manifested as "the queued ability plays a few frames then vanishes with no cooldown, client-only, worse the further `MakeCancellable` sits from the montage end."

> Note: gating the *custom* priority check by `IsLocallyControlled` does **not** fix this — the rejection comes from the engine `Super` evaluating the Blueprint's `ActivationBlockedTags`, which cannot be skipped safely (it also enforces `Jumping/Falling/Dead`, required tags, etc.). The fix must make the server's tag **state** correct at validation time, not make the server ignore it.

**The fix — mirror the removal, ordered before the activation.** `UMKHGameplayAbility::RemovePriorityTag` is the single removal point. On the owning client it removes the tag locally and then calls the reliable RPC `UMKHAbilitySystemComponent::Server_RemovePriorityTag`. Both that RPC and `ServerTryActivateAbility` travel on the **same ASC actor channel**, so reliable ordering guarantees the server clears the tag *before* validating the queued activation. When the server then evaluates the queued ability, the priority tag is already gone → `Super` passes → no rejection, no rollback.

Invariants to preserve when touching this path:

-   **Single removal point.** Never call `RemoveLooseGameplayTag(PriorityTag)` directly; go through `RemovePriorityTag` so the server mirror always fires with correct ordering.
-   **Underflow guard.** Both `RemovePriorityTag` and `Server_RemovePriorityTag` are guarded by `HasMatchingGameplayTag`: on the server the tag can be cleared twice (the client's mirror RPC *plus* the server's own `MakeCancellable`/`EndAbility`), and a loose-tag count must never underflow.
-   **Symmetric validation.** With the state kept in sync, `DoesAbilitySatisfyTagRequirements` enforces the priority rules identically on client and server (no `IsLocallyControlled` special-casing).
-   **Simulated proxies** never run `ActivateAbility`, so they never hold priority tags; do not rely on them for any priority-gated logic.

### Example Scenario

| Input Sequence | Resulting Action |
| :--- | :--- |
| Attack &rarr; Attack | Attack &rarr; Attack. |
| Attack &rarr; Attack &rarr; Consumable | Attack &rarr; Consumable. |
| Attack &rarr; Attack &rarr; Weapon Swap | Attack &rarr; Weapon Swap. |
| Attack &rarr; Weapon Swap &rarr; Attack | Attack &rarr; Weapon Swap &rarr; Attack. |
| Attack &rarr; Weapon Swap &rarr; Attack &rarr; Consumable | Attack &rarr; Consumable &rarr; Weapon Swap. |
