// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "MKHActivateAbilityByInputTask.generated.h"

class AAIController;
class UMKHAbilitySystemComponent;
struct FStateTreeExecutionContext;
struct FStateTreeTransitionResult;
template <typename T> struct TStateTreeInstanceDataStructRef;

/**
 * Instance data of FMKHActivateAbilityByInputTask: the bot's controller, the input tag to "press" and the runtime state
 * of the ability started by the press.
 */
USTRUCT()
struct FMKHActivateAbilityByInputTaskInstanceData
{
	GENERATED_BODY()

	// ============================================================
	// Properties
	// ============================================================

	/** Controller of the bot running the tree; bound automatically from the AI schema context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Input tag pressed on the bot's ability system, as the player's key would (e.g. Input.Ability.Attacks.Basics.Light). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Input.Ability"))
	FGameplayTag InputTag;

	/**
	 * Optional weapon quick slot input pressed first (instant swap to the sheathed weapon), then InputTag once the swap
	 * is done; empty for no swap. Lets one state swap and attack (React > ActivateAbility).
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Input.Ability.QuickSlot.Weapon"))
	FGameplayTag SwapInputTag;

	/** Seconds to wait for the ability to start (it may be queued behind a running one) before failing. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float StartTimeout = 0.5f;

	/** Seconds after which the task fails even if the ability is still running (safety net); covers a full 3-strike combo. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1"))
	float MaxDuration = 4.f;

	/** Seconds the input stays pressed before the release, for hold abilities like Block; 0 for a tap. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
	float HoldDuration = 0.f;

	/** Seconds since the press. */
	float ElapsedTime = 0.f;

	/** True while the input is pressed and not yet released (HoldDuration > 0). */
	bool bInputHeld = false;

	/** True between Event.Animation.ContinueCombo.Start and .End on the bot's ability system. */
	bool bComboWindowOpen = false;

	/** True while the press waits for the bot's running attack to end (it was chosen in a combo window). */
	bool bAttackEndPending = false;

	/** InputTag latched on entering the state: a binding that changes later (live evaluator output) never retargets the task. */
	FGameplayTag PressedInputTag;

	/** SwapInputTag latched on entering the state, for the same reason. */
	FGameplayTag PressedSwapInputTag;

	/** True from the swap press until InputTag is pressed. */
	bool bSwapPending = false;

	/** Spec of the ability seen active after the press; invalid until it starts. */
	FGameplayAbilitySpecHandle ActiveSpecHandle;

	/** Ability system the combo window delegates are bound to. */
	TWeakObjectPtr<UMKHAbilitySystemComponent> ComboEventsAbilitySystem;

	/** Handle of the ContinueCombo.Start gameplay event delegate. */
	FDelegateHandle ComboStartHandle;

	/** Handle of the ContinueCombo.End gameplay event delegate. */
	FDelegateHandle ComboEndHandle;
};

/**
 * StateTree task of the duel bot: presses and releases an input tag on the bot's UMKHAbilitySystemComponent (a tap,
 * or a hold of HoldDuration for abilities like Block), the same entry point as the player's keys (AbilityInputPressed / AbilityInputReleased), so the bot uses the player's
 * abilities with their cooldowns, costs and queueing. Running while the ability runs, Succeeded when it ends, Failed
 * when nothing starts within StartTimeout (not granted, on cooldown, out of stamina) or after MaxDuration. Leaving the
 * state early releases a held input and cancels the ability, unless a priority tag protects it (CanCancelOnExit).
 * Combos: the task performs one strike. When a ContinueCombo window opens it succeeds without cancelling the ability, so
 * the tree re-scores: a new press of this input in the window chains the next strike, anything else lets the strike end
 * the attack. A weapon swap, or an attack the priority rules refuse, waits for the running attack to end before its press.
 *
 * Tree layout and learning plan: see Documentation/Handoff/DuelBotAIHandoff.md.
 */
USTRUCT(meta = (DisplayName = "MKH Activate Ability By Input", Category = "MKH"))
struct MAKHIA_API FMKHActivateAbilityByInputTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Type of the per-state data this task reads and writes. */
	using FInstanceDataType = FMKHActivateAbilityByInputTaskInstanceData;

	// ============================================================
	// Public Interface
	// ============================================================

	/** @return The struct describing FInstanceDataType, used by the StateTree to allocate the instance data. */
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Presses and releases InputTag on the bot's ability system and starts listening to the combo window events.
	 *
	 * @param Context     Execution context of the running StateTree, gives access to the instance data.
	 * @param Transition  Transition that entered the state (unused).
	 * @return Running while waiting for the ability, Failed without a valid input tag or ability system.
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/**
	 * Follows the ability started by the press and hands the choice back to the tree in an open combo window.
	 *
	 * @param Context    Execution context of the running StateTree.
	 * @param DeltaTime  Seconds since the last tick.
	 * @return Running while the ability runs, Succeeded when it ended or a combo window opened, Failed on timeout.
	 */
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/**
	 * Stops listening to the combo window events and, when CanCancelOnExit allows it, cancels the ability still running.
	 *
	 * @param Context     Execution context of the running StateTree.
	 * @param Transition  Transition leaving the state (unused).
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

protected:
	/**
	 * Gets the ability system of the pawn driven by a controller.
	 *
	 * @param AIController  Controller of the bot.
	 * @return The pawn's UMKHAbilitySystemComponent, or null.
	 */
	static UMKHAbilitySystemComponent* GetPawnAbilitySystem(const AAIController* AIController);

	/**
	 * Releases the input pressed in EnterState, as the player lifting the key.
	 *
	 * @param AbilitySystem  Bot's ability system.
	 * @param Data           Instance data holding the input tag; bInputHeld is cleared.
	 */
	static void ReleaseHeldInput(UMKHAbilitySystemComponent& AbilitySystem, FMKHActivateAbilityByInputTaskInstanceData& Data);

	/**
	 * Presses the latched input (combo events bound, released at once for a tap) and looks for the ability it starts.
	 *
	 * @param Context        Execution context of the running StateTree.
	 * @param AbilitySystem  The bot's ability system.
	 */
	void PressInput(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const;

	/**
	 * Swap first: waits for the swap (GA_QuickSlot) to end, then presses the latched input.
	 *
	 * @param Context        Execution context of the running StateTree.
	 * @param AbilitySystem  The bot's ability system.
	 * @return Running while waiting or once the input is pressed.
	 */
	EStateTreeRunStatus TickPendingSwap(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const;

	/**
	 * Finds the active ability an input tag drives (UMKHAbilitySystemComponent::FindAbilitySpecForInputTag, as the
	 * player's input lookup does: a quick slot input follows GA_QuickSlot).
	 *
	 * @param AbilitySystem  Ability system to search.
	 * @param InputTag       Input tag the ability is bound to.
	 * @return Handle of the active spec, invalid when none is running.
	 */
	static FGameplayAbilitySpecHandle FindActiveSpecForInputTag(const UMKHAbilitySystemComponent& AbilitySystem, const FGameplayTag& InputTag);

	/**
	 * Binds the ContinueCombo.Start/End gameplay events of the bot's ability system to the window flags of this task.
	 *
	 * @param Context        Execution context, used to build a relocation-safe reference to the instance data.
	 * @param AbilitySystem  Bot's ability system, the target of the anim notifies' events.
	 */
	void BindComboWindowEvents(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const;

	/**
	 * Removes the combo window delegates and resets the window state.
	 *
	 * @param Data  Instance data holding the delegate handles.
	 */
	static void UnbindComboWindowEvents(FMKHActivateAbilityByInputTaskInstanceData& Data);

	/**
	 * Gameplay event callback: only marks the window as opened or closed (Tick acts on it).
	 *
	 * @param DataRef  Relocation-safe reference to the instance data.
	 * @param bOpened  True for ContinueCombo.Start, false for ContinueCombo.End.
	 */
	static void HandleComboWindowEvent(const TStateTreeInstanceDataStructRef<FMKHActivateAbilityByInputTaskInstanceData>& DataRef, bool bOpened);

	/**
	 * Whether the press must wait for the bot's running attack to end: a weapon swap always does, an attack or a dodge
	 * while the priority rules refuse it (CanActivateAbility); Block and the other inputs are pressed at once.
	 *
	 * @param AbilitySystem  The bot's ability system.
	 * @param Data           Instance data with the latched inputs.
	 * @return True while another attack runs and the latched input needs it to end.
	 */
	static bool ShouldWaitForAttackEnd(const UMKHAbilitySystemComponent& AbilitySystem, const FMKHActivateAbilityByInputTaskInstanceData& Data);

	/**
	 * Whether leaving the state may cancel the ability it started: not while a priority tag protects it, as for the
	 * player, unless the bot is dead or the task timed out (MaxDuration).
	 *
	 * @param AbilitySystem  The bot's ability system.
	 * @param Data           Instance data with the elapsed time.
	 * @return True when the running ability may be cancelled.
	 */
	static bool CanCancelOnExit(const UMKHAbilitySystemComponent& AbilitySystem, const FMKHActivateAbilityByInputTaskInstanceData& Data);

	/**
	 * Starts the latched inputs: the swap press when there is one (the input follows in TickPendingSwap), else the input.
	 *
	 * @param Context        Execution context of the running StateTree.
	 * @param AbilitySystem  The bot's ability system.
	 */
	void StartInputs(FStateTreeExecutionContext& Context, UMKHAbilitySystemComponent& AbilitySystem) const;
};
