// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MainHUD.generated.h"

class UMKHGameMenuWidget;
class UMKHGameMenuController;
class AMKHPlayerController;
class UInventoryDashboardWidget;
class UInventoryDashboardController;
class UHUDOverlayController;
class UHUDOverlayWidget;
class UMatchOverWidget;
class ACharacterBase;
class UMKHSettingsScreenWidget;
class UMKHOverheadBarVisibilityComponent;

/**
 * HUD of the arena match, owned by the local AMKHPlayerController. Creates and tears down the match widgets
 * (overlay, inventory dashboard, game menu, end-of-match panel, settings screen), wires them to their widget
 * controllers and routes every focus change through the player controller.
 */
UCLASS()
class MAKHIA_API AMainHUD : public AHUD
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Creates the opponent overhead bar visibility component. */
	AMainHUD();

protected:
	/** Gives the focus to gameplay, creates the overlay and binds the match-over events. */
	virtual void BeginPlay() override;

	/** Stops watching spawned actors (see OnActorSpawned). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ============================================================
	// Public Interface (HUD overlay)
	// ============================================================

public:
	/** Returns the overlay widget controller, creating and binding it on first use. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Overlay")
	UHUDOverlayController* GetHUDOverlayWidgetController();

	/** Creates the gameplay overlay, feeds it its initial values and replays the current match phase. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Overlay")
	void CreateHUDOverlayWidget();

	/** Removes the gameplay overlay and unbinds its controller. Safe to call twice or before creation. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Overlay")
	void DestroyHUDOverlayWidget();

	/**
	 * Hides or shows the gameplay overlay and the world widgets (opponent overhead bar, damage numbers) without destroying
	 * them (match intro, AMKHMatchIntro).
	 * @param bHideOverlay True to hide them.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Overlay")
	void SetHUDOverlayHidden(bool bHideOverlay);

	/**
	 * Toggle HUD key (IA_ToggleHUD): hides or shows the gameplay overlay and the world widgets for
	 * screenshots and trailer shots. Independent of the match intro: they are visible only when neither hides them.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Overlay")
	void ToggleHUDHiddenByPlayer();

	/** @return True while the player has hidden the overlay with the Toggle HUD key. */
	UFUNCTION(BlueprintPure, Category = "HUD|Overlay")
	bool IsHUDHiddenByPlayer() const { return bOverlayHiddenByPlayer; }

	// ============================================================
	// Public Interface (Inventory dashboard)
	// ============================================================

	/** Opens the inventory dashboard (creating it if needed), or closes it when open. Closes the game menu first. */
	void ToggleInventory();

	/** Creates the inventory dashboard and adds it to the viewport. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Inventory")
	void CreateInventoryWidget();

	/** Destroys the inventory dashboard, if any. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Inventory")
	void DestroyInventoryWidget();

	/** Returns the inventory widget controller, creating and binding it on first use. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Inventory")
	UInventoryDashboardController* GetInventoryWidgetController();

	// ============================================================
	// Public Interface (Game menu)
	// ============================================================

	/**
	 * Esc handling: closes, in order, the settings screen, the inventory, the credits window; otherwise opens or
	 * closes the game menu.
	 */
	void ToggleGameMenu();

	/** Creates the game menu and adds it to the viewport. */
	UFUNCTION(BlueprintCallable, Category = "HUD|GameMenu")
	void CreateGameMenuWidget();

	/** Destroys the game menu, if any. */
	UFUNCTION(BlueprintCallable, Category = "HUD|GameMenu")
	void DestroyGameMenuWidget();

	/** Returns the game menu widget controller, creating and binding it on first use. */
	UFUNCTION(BlueprintCallable, Category = "HUD|GameMenu")
	UMKHGameMenuController* GetGameMenuWidgetController();

	// ============================================================
	// Public Interface (Match over)
	// ============================================================

	/** Creates the end-of-match panel and adds it to the viewport. Does nothing if it already exists. */
	UFUNCTION(BlueprintCallable, Category = "HUD|MatchOver")
	void CreateMatchOverWidget();

	/** Removes the end-of-match panel from the viewport. */
	UFUNCTION(BlueprintCallable, Category = "HUD|MatchOver")
	void DestroyMatchOverWidget();

	// ============================================================
	// Public Interface (Settings screen)
	// ============================================================

	/**
	 * Creates the full screen settings shell and adds it to the viewport.
	 * Does nothing when it is already open.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Settings")
	void CreateSettingsWidget();

	/** Removes the settings shell from the viewport. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Settings")
	void DestroySettingsWidget();

	/** Opens the settings shell, or closes it when it is already open. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Settings")
	void ToggleSettingsWidget();

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** Applies the intro and player hide flags to the overlay and to every actor that hides with the HUD. */
	void ApplyHUDHiddenState() const;

	/** Applies the overlay opacity from the intro and player hide flags. */
	void ApplyHUDOverlayOpacity() const;

	/**
	 * @param Actor The actor to test.
	 * @return True when the actor's widget components hide with the HUD: every ACharacterBase (opponent overhead
	 *         bar) and every actor tagged HideWithHUDTag (floating damage numbers, BP_DamageText).
	 */
	static bool HidesWithHUD(const AActor& Actor);

	/**
	 * Hides or shows the widget components of an actor. Uses HiddenInGame, so the Blueprint's own visibility
	 * handling is left alone.
	 *
	 * @param Actor        The actor whose widget components change.
	 * @param bHideWidgets True to hide them.
	 */
	static void SetActorWidgetsHidden(const AActor& Actor, bool bHideWidgets);

	/** World spawn callback: an actor that hides with the HUD and spawns while it is hidden (damage number, new round, late replication) starts hidden. */
	void OnActorSpawned(AActor* SpawnedActor) const;

	/** @return True when the intro or the player currently hides the overlay. */
	bool IsHUDOverlayHidden() const { return bOverlayHiddenByIntro || bOverlayHiddenByPlayer; }

	/** Routes the dashboard's open/close/destruction to focus changes and controller cleanup. */
	void BindInventoryWidgetDelegates();

	/** Routes the game menu's open/close/destruction to focus changes and controller cleanup. */
	void BindGameMenuWidgetDelegates();

	/**
	 * Subscribes to the game state events that drive the end-of-match panel: OnMatchOver to show it
	 * and OnInitialPreparationStarted to take it down again.
	 */
	void BindMatchOverEvent();

	/** Routes the end-of-match panel's show/destruction to focus changes. */
	void BindMatchOverWidgetDelegates();

	/**
	 * Match-over callback: shows the result panel and closes any menu still open.
	 *
	 * @param WinningPlayer  The winning participant, or nullptr on a draw. Only compared against the local
	 *                       player to decide between the victory and defeat headline: a bot winner has no
	 *                       player state, so it always means a defeat.
	 * @param bIsDraw        True when the match ended without a winner.
	 */
	UFUNCTION()
	void OnMatchOver(ACharacterBase* WinningPlayer, bool bIsDraw);

	/**
	 * Start-of-match callback: takes the result panel down and hands the focus back to gameplay.
	 * A rematch restarts the match in place, so nothing else would remove the panel from the viewport.
	 */
	UFUNCTION()
	void OnMatchRestarted();

	/** Settings shell callback: the player asked to leave the screen. */
	void OnSettingsClosed();

	// ============================================================
	// Properties
	// ============================================================

	/** The owning player controller; every widget is created for it and every focus change goes through it. */
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = true))
	TObjectPtr<AMKHPlayerController> PlayerController;

	// --- HUD overlay ---

	/** Class of the overlay widget controller. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | HUDOverlayWidgets")
	TSubclassOf<UHUDOverlayController> HUDOverlayControllerClass;

	/** Class of the gameplay overlay widget. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | HUDOverlayWidgets")
	TSubclassOf<UHUDOverlayWidget> HUDOverlayWidgetClass;

	/** The overlay widget controller, created on first use. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UHUDOverlayController> HUDOverlayController;

	/** The gameplay overlay on screen. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UHUDOverlayWidget> HUDOverlayWidget;

	/** True while the match intro hides the overlay: an overlay created meanwhile starts hidden too. */
	bool bOverlayHiddenByIntro = false;

	/** True while the player hides the overlay with the Toggle HUD key (screenshots, trailer shots). */
	bool bOverlayHiddenByPlayer = false;

	/** Actor tag that makes an actor's widget components hide with the HUD (see HidesWithHUD). */
	static const FName HideWithHUDTag;

	/** Handle of the world's OnActorSpawned binding (see OnActorSpawned). */
	FDelegateHandle ActorSpawnedHandle;

	/** Shows the opponent's overhead bar only when it is close or aimed at (distances and tolerance set on it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD|Overlay", meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHOverheadBarVisibilityComponent> OverheadBarVisibility;

	// --- Inventory dashboard ---

	/** Class of the inventory widget controller. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | InventoryWidgets")
	TSubclassOf<UInventoryDashboardController> InventoryDashboardControllerClass;

	/** Class of the inventory dashboard widget. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | InventoryWidgets")
	TSubclassOf<UInventoryDashboardWidget> InventoryDashboardWidgetClass;

	/** The inventory widget controller, created on first use. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UInventoryDashboardController> InventoryDashboardController;

	/** The inventory dashboard, while it exists. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UInventoryDashboardWidget> InventoryDashboardWidget;

	// --- Game menu ---

	/** Class of the game menu widget controller. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | GameMenuWidgets")
	TSubclassOf<UMKHGameMenuController> GameMenuControllerClass;

	/** Class of the game menu widget. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | GameMenuWidgets")
	TSubclassOf<UMKHGameMenuWidget> GameMenuWidgetClass;

	/** The game menu widget controller, created on first use. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHGameMenuController> GameMenuController;

	/** The game menu, while it exists. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHGameMenuWidget> GameMenuWidget;

	// --- Match over ---

	/** Class of the end-of-match panel. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | MatchOverWidgets")
	TSubclassOf<UMatchOverWidget> MatchOverWidgetClass;

	/** The end-of-match panel, while it exists. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMatchOverWidget> MatchOverWidget;

	// --- Settings screen ---

	/** Class of the settings shell. */
	UPROPERTY(EditDefaultsOnly, category = "Custom Values | SettingsWidgets")
	TSubclassOf<UMKHSettingsScreenWidget> SettingsScreenWidgetClass;

	/** The settings shell, while it is open. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TObjectPtr<UMKHSettingsScreenWidget> SettingsScreenWidget;
};
