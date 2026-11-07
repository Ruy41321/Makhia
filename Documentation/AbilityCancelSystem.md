# Ability Cancel System

The Ability Cancel System manages ability interruptions using priority-based logic. It allows high-priority actions to interrupt low-priority ones and ensures a smooth gameplay experience by allowing abilities to become "cancellable" during specific animation windows, such as recovery frames.

## Core Concepts

### Priority Tags
The system utilizes priority tags to determine interruption eligibility. These tags are applied to the Ability System Component (ASC) when an ability is active:
- **Priority 1 (High)**: Highest priority level. It cannot be interrupted by any other ability.
- **Priority 2 (Medium)**: Standard priority level. It can only be interrupted by abilities with **Priority 1**.

**Rule**: An ability can only interrupt an active one if its priority is **strictly higher**. Abilities with equal priority cannot interrupt each other.

### Dynamic Cancellability
Through the `AN_MakeAbilityCancellable` Anim Notify, an active ability can voluntarily remove its priority tag from the ASC. This makes the character vulnerable to interruption by any incoming ability (regardless of priority), which is essential for "canceling" recovery animations into new actions.

### Dodge and Block
- **`GA_Dodge`** (Priority 1) also has `GameplayAbility.Active.Priority` in its `ActivationBlockedTags`: any priority tag blocks it, so it cannot interrupt an attack before `AN_MakeAbilityCancellable`.
- **`GA_Block`** has no priority tag: it starts at once and cancels basic attacks (`CancelAbilitiesWithTag` = `GameplayAbility.Type.Attacks.Basic`). A skill keeps running until its tag is removed; the movement state then enters Blocking, which cancels the running abilities (`UBlockingSystemComponent::ApplyBlockingStateModifiers`).

The duel bot follows the same rules: see `DuelBotAI.md`, *Reactions and priority*.

---

## Technical Implementation

### Interruption Logic
The system overrides `DoesAbilitySatisfyTagRequirements` to evaluate the priority state of the ASC before allowing an ability to activate.

```cpp
bool UMKHGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, ...) const
{
    // Base class logic checks (ActivationBlockedTags, etc.)
    if (!Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, ...)) return false;

    if (PriorityTag.IsValid())
    {
        if (PriorityTag == MKHGameplayTags::Ability::Priority1)
        {
            // Blocked if a Priority 1 ability is already running
            if (AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1)) return false;
        }
        else if (PriorityTag == MKHGameplayTags::Ability::Priority2)
        {
            // Blocked if Priority 1 or Priority 2 is already running
            if (AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1) ||
                AbilitySystemComponent.HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority2)) return false;
        }
    }
    return true;
}
```

### State Management
When an ability starts, it grants its `PriorityTag` to the owner's ASC as a **loose tag**. Loose tags are ideal here because they can be removed dynamically by gameplay events without needing to modify the ability's permanent tag container.

```cpp
void UMKHGameplayAbility::ActivateAbility(...)
{
    // ...
    if (PriorityTag.IsValid())
    {
        GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(PriorityTag);
        BindMakeCancellable(); // Listen for the AN_MakeAbilityCancellable notify
    }
    // ...
}
```

### Interaction with Anim Notifies
The system listens for the `MKHGameplayTags::Event::MakeAbilityCancellable` event. When received, it removes the active `PriorityTag`, effectively clearing the way for subsequent abilities.

```cpp
void UMKHGameplayAbility::OnMakeAbilityCancellable(FGameplayEventData Payload)
{
    if (PriorityTag.IsValid())
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->RemoveLooseGameplayTag(PriorityTag);
            TryActivateQueuedAbility(); // Integration with the Queue Ability System
        }
    }
}
```

---

## Summary of Workflow
1. **Ability Activation**: The ability checks if the current priority on the character allows it to start.
2. **Tag Application**: If successful, it applies its `PriorityTag` (e.g., `Priority 2` for a basic attack).
3. **Active Phase**: The ability runs; other `Priority 2` abilities are blocked.
4. **New Player Input**: The player attempts to activate another `Priority 2` ability, which fails due to the active tag but get registered in the QueueAbilitySystem.
5. **Cancellable Window**: An `AN_MakeAbilityCancellable` notify fires in the Montage, removing the `Priority 2` tag and notifying the ASC to TryActivateQueuedAbility.
6. **Seamless Transition**: The player old input of a `Priority 2` ability automatically activates, allowing for smooth combo transitions without needing to time inputs perfectly.
