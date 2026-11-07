// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Interfaces/InventoryInterface.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "MKHPlayerController.generated.h"

class AMKHPickupItem;
struct FInputActionInstance;
class UInputAction;
class UInputMappingContext;
class UInventoryDashboardWidget;
class UMKHInputConfig;
class UInventoryComponent;
class UInventoryDashboardController;
class UMKHAbilitySystemComponent;
class UMKHPickupInteractionComponent;
class UEquipmentManagerComponent;
class UQuickSlotManagerComponent;
class UMKHQuickSlotPreferenceComponent;
class UMKHVoicePlayerComponent;

/** Broadcast after the active Enhanced Input mapping context is replaced (null means "no mappings"). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMappingContextChangedSignature, const UInputMappingContext*, NewMappingContext);

/** Fired locally when a skill input is pressed, with the skill's input tag. */
DECLARE_DELEGATE_OneParam(FSkillActivatedSignature, FGameplayTag)

/** Fired locally when the skip-intro key has been held long enough. */
DECLARE_MULTICAST_DELEGATE(FMKHSkipIntroRequestedSignature)

/** Fired locally while the skip-intro key is held, with the held fraction (0 when released, 1 when it skips). */
DECLARE_MULTICAST_DELEGATE_OneParam(FMKHSkipIntroHoldSignature, float /*HoldFraction*/)

/** Which input mode and mapping context the controller runs with. */
UENUM(BlueprintType)
enum class EFocusOption : uint8
{
	Game	UMETA(DisplayName = "Game Focus"),
	UI		UMETA(DisplayName = "UI Focus")
};

/**
 * Player controller of the arena. Owns the player's inventory, equipment and quick slot components
 * (replicated) and the local pickup selection, routes Enhanced Input to the ability system and the UI,
 * and hosts the server RPCs that widgets cannot send themselves (rematch, restart, pickup).
 */
UCLASS()
class MAKHIA_API AMKHPlayerController : public APlayerController, public IAbilitySystemInterface, public IInventoryInterface, public IEquipmentInterface, public IQuickSlotInterface
{
	GENERATED_BODY()

	friend class AMainHUD;

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the inventory, equipment, quick slot, pickup interaction and voice components. */
	AMKHPlayerController();

	/** Binds the ability input actions (from MKHInputConfig) and the UI / interaction actions. */
	virtual void SetupInputComponent() override;

	/**
	 * Clears any leftover gameplay input block when a new pawn is possessed.
	 * The controller survives a seamless travel, so after a rematch it would otherwise keep the
	 * restricted "dead" mapping context applied by the previous match's death.
	 */
	virtual void AcknowledgePossession(APawn* NewPawn) override;

	/** Registers the replicated inventory component reference. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/**
	 * Applies the gameplay mapping context. The local controller also starts reporting its NAT type for the
	 * network stats overlay. Component callbacks are wired earlier, in PostInitializeComponents.
	 */
	virtual void BeginPlay() override;

	/**
	 * Wires the inventory, equipment and quick slot components to each other (and the equip events on the server)
	 * as soon as they exist: the game mode can use items (starting loadout) from RestartPlayer, which for a
	 * listen-server host runs before BeginPlay.
	 */
	virtual void PostInitializeComponents() override;

	/** Stops reporting the NAT type. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	// --- Component access ---

	/** Returns the ability system component of the controlled pawn. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** IEquipmentInterface: returns the equipment manager owned by this controller. */
	virtual UEquipmentManagerComponent* GetEquipmentManagerComponent_Implementation() const override;

	/** IInventoryInterface: returns the inventory owned by this controller. */
	virtual UInventoryComponent* GetInventoryComponent_Implementation() const override;

	/** IQuickSlotInterface: returns the quick slot manager owned by this controller. */
	virtual UQuickSlotManagerComponent* GetQuickSlotManagerComponent_Implementation() const override;

	/**
	 * Returns the project ability system component stored on the player state, caching it on first success.
	 * May return nullptr on clients before the player state has replicated.
	 */
	UMKHAbilitySystemComponent* GetMKHAbilitySystemComponent();

	// --- Input ---

	/**
	 * Forwards an ability input press to the ability system. Attack inputs first make sure a weapon is
	 * equipped (quick-equipping or queueing the attack when needed); skill inputs also fire SkillActivatedDelegate.
	 *
	 * @param InputTag Input tag of the pressed action.
	 */
	void AbilityInputPressed(const FGameplayTag& InputTag);

	/**
	 * Forwards an ability input release to the ability system.
	 *
	 * @param InputTag Input tag of the released action.
	 */
	void AbilityInputReleased(const FGameplayTag& InputTag);

	/**
	 * Enables or disables gameplay input by swapping the active mapping context.
	 * Disabling applies DeadMappingContext (which should only expose the actions that must survive,
	 * e.g. the game menu); a null DeadMappingContext blocks every input.
	 * The blocked flag is remembered, so a focus change while blocked does not silently restore
	 * the default context.
	 *
	 * @param bEnabled  True to restore the default gameplay context, false to apply the restricted one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Custom Values | Input")
	void SetGameplayInputEnabled(bool bEnabled);

	/** True while gameplay input is blocked (e.g. the character is in the Dead state). */
	UFUNCTION(BlueprintPure, Category = "Custom Values | Input")
	bool IsGameplayInputBlocked() const { return bGameplayInputBlocked; }

	// --- Match intro ---

	/**
	 * Adds or removes MatchIntroMappingContext (hold-to-skip) on top of the gameplay contexts, at a higher priority so
	 * its key does not reach the actions they map (Space would jump). It survives ChangeMappingContext. Local only.
	 *
	 * @param bActive True while a match intro plays.
	 */
	void SetMatchIntroInputActive(bool bActive);

	/** Returns the action that skips the match intro (shown by the skip hint keycap). */
	UInputAction* GetSkipIntroAction() const { return SkipIntroAction; }

	/** Returns how long the skip key must be held: the Hold trigger of SkipIntroAction, in seconds (0 if none). */
	float GetSkipIntroHoldSeconds() const;

	/** True while a UI screen has focus (inventory, game menu, end-of-match panel), see ChangeFocus. Local state. */
	UFUNCTION(BlueprintPure, Category = "Custom Values | Input")
	bool IsUIFocused() const { return CurrentFocus == EFocusOption::UI; }

	/**
	 * True while the controlled pawn is in the Dead movement state (State.Movement.Dead on its ability system).
	 * The tag is applied by the movement state machine on every machine, so this is valid on the server too.
	 */
	UFUNCTION(BlueprintPure, Category = "Custom Values | State")
	bool IsPawnDead() const;

	// --- Server requests ---

	/**
	 * Casts this player's vote on the post-match rematch ballot. The match only restarts once
	 * every participant has voted (see AMKHGameMode::RequestRematch).
	 *
	 * UI widgets must route through here because UUserWidget does not implement RPC callspace
	 * resolution, so a UFUNCTION(Server) declared on a widget would never leave the client.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestRematch();

	/**
	 * Restarts the match immediately, with no ballot. Used by the in-game pause menu, which is
	 * deliberately unilateral; the end-of-match panel votes through Server_RequestRematch instead.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RestartMatch();

	/**
	 * Toggles this player's ready-to-skip vote on the initial preparation (see
	 * AMKHGameMode::ToggleSkipPreparationVote). Sent by the SkipPreparationAction input.
	 */
	UFUNCTION(Server, Reliable)
	void Server_ToggleSkipPreparationVote();

	/**
	 * Collects a world pickup into this player's inventory, then starts its collection animation
	 * (AMKHPickupItem::StartCollect, which destroys it once over). Sent when the Interact input
	 * is pressed while UMKHPickupInteractionComponent focuses a pickup. The server re-checks everything
	 * (see CanCollectPickup) and claims the pickup, so a stale or forged request cannot collect out of
	 * range or twice; a pickup already gone is ignored. The collected item is remembered for
	 * Server_EquipLastPickup.
	 *
	 * @param Pickup The pickup to collect.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PickupItem(AMKHPickupItem* Pickup);

	/**
	 * Hold to equip: the player kept Interact held for HoldToEquipDuration after a pickup, so the item collected
	 * last is equipped or quick slotted (FMKHPickupAutoEquip::ApplyOnServer). Carries no item data: only an item
	 * collected by this player within HoldToEquipServerWindow is used, and only once.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_EquipLastPickup();

	/**
	 * Stores the NAT type measured on this machine in the player's state, so the opponent's network
	 * stats overlay can show it (see UMKHNetDiagnosticsSubsystem).
	 *
	 * @param NatType NAT type reported by the online backend on the owning client (Unknown when not reported).
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ReportNatType(EMKHNatType NatType);

	/**
	 * Stores the skin chosen on this machine in the player's state, so every machine dresses the character with it
	 * (see AMKHPlayerState::SetSkinId, which validates it).
	 *
	 * @param SkinId Skin chosen in the lobby on the owning client.
	 */
	UFUNCTION(Server, Reliable)
	void Server_ReportSkin(FName SkinId);

	/**
	 * Stores the career levels beaten on this machine in the player's state, for the title shown to the opponent
	 * (see AMKHPlayerState::SetCareerLevels, which clamps them).
	 *
	 * @param CareerLevels Levels beaten on the owning client.
	 */
	UFUNCTION(Server, Reliable)
	void Server_ReportCareerLevels(int32 CareerLevels);

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Replaces every active mapping context with the given one and broadcasts MappingContextChangedDelegate.
	 *
	 * @param NewMappingContext Context to apply; nullptr leaves no mappings at all.
	 */
	UFUNCTION(BlueprintCallable, Category = "Custom Values | Input")
	void ChangeMappingContext(const UInputMappingContext* NewMappingContext) const;

	/**
	 * Publishes the project's mapping contexts to the Enhanced Input user settings, so the key
	 * profile knows every mappable action and the settings screen has something to list.
	 * Requires bEnableUserSettings in Config/DefaultInput.ini; logs and returns otherwise.
	 */
	void RegisterMappingContextsWithUserSettings() const;

	/**
	 * Makes sure an attack input can run: quick-equips the weapon of the quick slots when unarmed, or asks
	 * for the attack to be queued while a weapon swap or a higher priority action is in flight.
	 *
	 * @param InputTag    Input tag of the attack.
	 * @param bForceQueue Set to true when the attack must be queued instead of activated right away.
	 * @return False when the attack cannot run (unarmed and nothing to equip).
	 */
	bool EnsureWeaponEquipped(const FGameplayTag& InputTag, bool& bForceQueue);

private:
	/** Wires the inventory, equipment and quick slot components to each other. */
	void BindCallbacksToDependencies() const;

	/** Server: keeps the character's bIsHoldingWeapon flag in sync with the equipped weapon slot. */
	void SubscribeToEquipEvents() const;

	/**
	 * Returns the mapping context that represents "game focus" right now: the restricted one while
	 * gameplay input is blocked, the default one otherwise.
	 */
	const UInputMappingContext* GetGameplayMappingContext() const;

	/**
	 * Switches between game and UI focus: cursor visibility, input mode and mapping context.
	 *
	 * @param NewFocus The focus to apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Custom Values | Input")
	void ChangeFocus(const EFocusOption NewFocus);

	/** ToggleInventoryAction handler: opens or closes the inventory dashboard through the HUD. */
	void ToggleInventory();

	/** ToggleGameMenuAction handler: opens or closes the game menu through the HUD. */
	void ToggleGameMenu();

	/** ToggleHUDAction handler: hides or shows the gameplay overlay through the HUD (screenshots, trailer shots). */
	void ToggleHUD();

	/**
	 * InteractAction press: asks the server to collect the focused pickup at once; with "Hold to Equip Pickups" on,
	 * a hold starts too.
	 */
	void HandleInteractStarted();

	/**
	 * InteractAction held (every frame): once held for HoldToEquipDuration, asks the server to equip the item just
	 * collected (Server_EquipLastPickup). A UI screen taking the focus or the pawn dying cancels the hold.
	 *
	 * @param Instance Action state, whose elapsed time is the hold time.
	 */
	void HandleInteractHeld(const FInputActionInstance& Instance);

	/** InteractAction release: ends the hold (the item stays in the bag when the hold was too short). */
	void HandleInteractReleased();

	/** SkipPreparationAction handler: toggles the ready-to-skip vote while the ballot is open. */
	void HandleSkipPreparation();

	/** SwitchWeaponAction press: presses the other weapon quick slot's input (see ResolveSwitchWeaponTag). */
	void HandleSwitchWeaponPressed();

	/** SwitchWeaponAction release: releases the quick slot input pressed by HandleSwitchWeaponPressed. */
	void HandleSwitchWeaponReleased();

	/**
	 * Weapon quick slot the switch button should use: the other slot than the current weapon, or the current
	 * one when the other slot is empty (so the button still draws a sheathed weapon).
	 *
	 * @return The quick slot input tag to press, or an empty tag when no weapon is quick slotted.
	 */
	FGameplayTag ResolveSwitchWeaponTag() const;

	/**
	 * SkipIntroAction Ongoing handler: reports the held fraction through OnSkipIntroHoldChanged.
	 *
	 * @param Instance The action instance (its elapsed time is the time held).
	 */
	void HandleSkipIntroHeld(const FInputActionInstance& Instance);

	/** SkipIntroAction Canceled / Completed handler: the key was released, the held fraction goes back to 0. */
	void HandleSkipIntroReleased();

	/** SkipIntroAction Triggered handler: held long enough, fires OnSkipIntroRequested. */
	void HandleSkipIntroTriggered();

	/**
	 * Server: checks that this player can collect the pickup right now — the player has a pawn and an
	 * inventory, the pawn is not dead, the pickup holds an item, has finished appearing and is not reserved
	 * to another player, and the pawn is within the pickup's interaction range plus PickupServerReachTolerance.
	 * Logs the reason when it cannot.
	 *
	 * @param Pickup The pickup requested by the client.
	 */
	bool CanCollectPickup(const AMKHPickupItem& Pickup) const;

	/** Local controller: reports the current NAT type and every later change of it. */
	void StartNatTypeReporting();

	/**
	 * Writes the NAT type into the player state: directly on the listen server, through
	 * Server_ReportNatType on a client.
	 *
	 * @param NatType NAT type measured on this machine.
	 */
	void ReportNatType(EMKHNatType NatType);

	/**
	 * Local controller: writes the skin chosen on this machine (UMKHSkinSubsystem) into the player state, directly
	 * on the listen server, through Server_ReportSkin on a client.
	 */
	void ReportSkin();

	/**
	 * Local controller: writes the career levels beaten on this machine (UMKHCareerSubsystem) into the player state,
	 * directly on the listen server, through Server_ReportCareerLevels on a client.
	 */
	void ReportCareerLevels();

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Broadcast whenever ChangeMappingContext replaces the active mapping context. */
	UPROPERTY(BlueprintAssignable)
	FMappingContextChangedSignature MappingContextChangedDelegate;

	/** Fired locally when a skill input is pressed (see AbilityInputPressed). */
	FSkillActivatedSignature SkillActivatedDelegate;

	/** Fired locally when the skip-intro key has been held for GetSkipIntroHoldSeconds (see AMKHMatchIntro). */
	FMKHSkipIntroRequestedSignature OnSkipIntroRequested;

	/** Fired locally while the skip-intro key is held and when it is released (see UMKHIntroSkipHintWidget). */
	FMKHSkipIntroHoldSignature OnSkipIntroHoldChanged;

private:
	// --- Input ---

	/** Mapping context applied while the player has game focus and gameplay input is not blocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Mapping context applied while a UI screen has focus (see ChangeFocus). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input")
	TObjectPtr<UInputMappingContext> UIOpenMappingContext;

	/** Mapping context applied while gameplay input is blocked. Should only map the actions that must
	 *  stay available (e.g. the game menu). Leave unset to block every input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input")
	TObjectPtr<UInputMappingContext> DeadMappingContext;

	/** Mapping context added on top while a match intro plays: only SkipIntroAction (see SetMatchIntroInputActive). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input")
	TObjectPtr<UInputMappingContext> MatchIntroMappingContext;

	/** Priority of MatchIntroMappingContext; the gameplay contexts use 0. */
	static constexpr int32 MatchIntroMappingPriority = 10;

	/** True while a match intro plays and MatchIntroMappingContext is applied. */
	bool bMatchIntroInputActive = false;

	/** True while gameplay input is blocked; keeps focus changes from restoring the default context. */
	bool bGameplayInputBlocked = false;

	/** Focus last applied by ChangeFocus (see IsUIFocused). */
	EFocusOption CurrentFocus = EFocusOption::Game;

	/** Maps the ability input actions to their input tags. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | Input")
	TObjectPtr<UMKHInputConfig> MKHInputConfig;

	/** Opens or closes the inventory dashboard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	/** Opens or closes the game menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> ToggleGameMenuAction;

	/** Hides or shows the gameplay overlay (IA_ToggleHUD, H by default, remappable). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> ToggleHUDAction;

	/** Collects the pickup the player is aiming at (see UMKHPickupInteractionComponent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> InteractAction;

	/** Readies up (or cancels) to skip the rest of the initial preparation (IA_SkipInit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> SkipPreparationAction;

	/** Swaps between the two weapon quick slots with one button (IA_SwitchWeapon, mapped on the controller). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> SwitchWeaponAction;

	/** Quick slot input pressed by the switch button, released with it. */
	FGameplayTag PressedSwitchWeaponTag;

	/** Held (Hold trigger on the action) to skip the match intro (IA_SkipIntro, mapped in MatchIntroMappingContext). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Category = "Custom Values | Input | Actions")
	TObjectPtr<UInputAction> SkipIntroAction;

	// --- Pickup ---

	/**
	 * Extra reach the server grants on top of a pickup's interaction range when validating
	 * Server_PickupItem: the server's copy of the pawn lags behind the client that chose the pickup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0", Units = "cm"), Category = "Custom Values | Pickup")
	float PickupServerReachTolerance = 100.f;

	/** How long Interact must stay held after a pickup to also equip / quick slot the item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0.1", Units = "s"), Category = "Custom Values | Pickup")
	float HoldToEquipDuration = 0.5f;

	/** Server: Server_EquipLastPickup is refused for an item collected longer ago than this (lag margin included). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0.1", Units = "s"), Category = "Custom Values | Pickup")
	float HoldToEquipServerWindow = 2.f;

	/** Owning machine: true from an Interact press that requested a pickup until the hold completes or ends. */
	bool bInteractHoldPending = false;

	/** Server: entry now holding the item collected last (0 once used or when the last request was refused). */
	int64 LastCollectedItemID = 0;

	/** Server: quantity the last collected pickup held. */
	int32 LastCollectedQuantity = 0;

	/** Server: world time of the last collection. */
	double LastCollectedTime = 0.0;

	// --- Components ---

	/** Cached ability system component of the player state (see GetMKHAbilitySystemComponent). */
	UPROPERTY()
	TObjectPtr<UMKHAbilitySystemComponent> MKHAbilitySystemComponent;

	/** The player's inventory (replicated FastArray, server-authoritative). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true), Replicated)
	TObjectPtr<UInventoryComponent> InventoryComponent;

	/** Voice chat link with the server (choices sent, server decisions received). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHVoicePlayerComponent> VoiceComponent;

	/** The player's equipped items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UEquipmentManagerComponent> EquipmentComponent;

	/** The player's weapon and consumable quick slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UQuickSlotManagerComponent> QuickSlotManagerComponent;

	/** Remembers the kinds of items kept in the quick slots and fills the same slots on the next starting loadout. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHQuickSlotPreferenceComponent> QuickSlotPreferenceComponent;

	/** Chooses the world pickup the player is aiming at; active only on the locally controlled controller. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHPickupInteractionComponent> PickupInteractionComponent;

	// --- Network diagnostics ---

	/** Subscription to UMKHNetDiagnosticsSubsystem::OnLocalNatTypeChanged (local controller only). */
	FDelegateHandle NatTypeChangedHandle;
};
