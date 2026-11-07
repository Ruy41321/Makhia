// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Input/MKHInputDeviceTypes.h"
#include "Input/NavigationReply.h"
#include "Layout/SlateRect.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MKHMenuNavigationSubsystem.generated.h"

class FMKHMenuNavigationInputProcessor;
class SMKHFocusHighlightLayer;
class SWidget;
class UListViewBase;
class UPanelWidget;
class UScrollBox;
class UUserWidget;
class UWidget;

/**
 * Gamepad navigation of every menu, so the game can be played with a controller alone (Steam Deck).
 *
 * While a menu has the input (SetMenuInputActive) and the gamepad is the active device, it keeps the Slate focus
 * on the screen on top: the topmost visible widget on the viewport that has something to focus, or a modal pushed
 * over it (PushModalScreen). It confines directional navigation to that screen, with a nearest-widget fallback
 * where the engine's row/column search finds nothing, draws a frame around the focused widget, shows the focused
 * widget's tooltip next to it, scrolls with the right stick and hides the mouse cursor. With the mouse it does
 * nothing. Model: Documentation/GamepadMenuNavigation.md.
 */
UCLASS()
class MAKHIA_API UMKHMenuNavigationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Starts following the viewport widgets, the active input device and the right stick. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops the ticker and removes the input processor and the highlight layer. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the subsystem of the local player owning a widget, controller or other object.
	 *
	 * @param ContextObject  A widget, player controller or anything with a local player.
	 * @return The subsystem, or null (no local player, dedicated server).
	 */
	static UMKHMenuNavigationSubsystem* Get(const UObject* ContextObject);

	/**
	 * Tells whether a menu has the input. Set by the player controllers: always in the lobby, while a UI screen
	 * is open in a match (AMKHPlayerController::ChangeFocus).
	 *
	 * @param bActive  True while a menu has the input.
	 */
	void SetMenuInputActive(bool bActive);

	/** Returns true while the gamepad drives a menu (menu input active and gamepad is the last device used). */
	UFUNCTION(BlueprintPure, Category = "UI|Navigation")
	bool IsGamepadNavigationActive() const;

	/**
	 * Makes a widget nested inside a screen behave as a screen of its own (a modal window inside a menu): the
	 * focus stays inside it until PopModalScreen.
	 *
	 * @param Modal  Widget to treat as the screen on top while it is visible.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void PushModalScreen(UUserWidget* Modal);

	/**
	 * Removes a modal pushed with PushModalScreen; the focus goes back where it was in the screen below.
	 *
	 * @param Modal  Widget pushed before.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void PopModalScreen(UUserWidget* Modal);

	/**
	 * Moves the focus to the first focusable widget inside a container (e.g. the settings page shown after LB/RB),
	 * at the next focus check, once the container is laid out. Only acts while the gamepad drives the menu.
	 *
	 * @param Container  Widget whose subtree is searched.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestFocusInside(UWidget* Container);

	/**
	 * Mouse only: shows a tooltip on the right of a widget, top edges aligned and growing down (on the left or moved up
	 * when it would leave the screen). Stays until HideAnchoredToolTip. With the gamepad the focus decides the tooltip.
	 *
	 * @param ToolTip  Tooltip content.
	 * @param Anchor   Widget the tooltip stands next to.
	 */
	void ShowAnchoredToolTip(UWidget* ToolTip, UWidget* Anchor);

	/** Mouse only: hides the tooltip of ShowAnchoredToolTip. */
	void HideAnchoredToolTip();

	/**
	 * Records a right stick deflection; called by the input processor.
	 *
	 * @param Value  Gamepad_RightY value, positive up.
	 */
	void ReportRightStickY(float Value) { RightStickY = Value; }

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Per-frame update: highlight layer, cursor, scrolling, and the focus checks at FocusCheckInterval. */
	bool Tick(float DeltaTime);

	/** Keeps the focus on the screen on top and the highlight on the focused widget. */
	void UpdateFocus();

	/**
	 * Puts the focus back on the screen when it left it (or a placement is pending), steps into containers and
	 * remembers where the focus is.
	 *
	 * @param Screen          Screen on top.
	 * @param bScreenChanged  True when the screen just came on top.
	 * @return The widget focused after the check.
	 */
	TSharedPtr<SWidget> KeepFocusOnScreen(UUserWidget& Screen, bool bScreenChanged);

	/**
	 * Applies a RequestFocusInside once the container is on screen.
	 *
	 * @param Screen  Screen on top; the container must be inside it.
	 * @return True when the focus moved.
	 */
	bool ApplyPendingFocusRequest(UUserWidget& Screen);

	/**
	 * Gives the focus to the screen that just came on top, or after switching to the gamepad.
	 *
	 * @param Screen          Screen on top.
	 * @param Focused         Widget focused inside the screen right now, may be null.
	 * @param bScreenChanged  True when the screen just came on top (the remembered widget wins over the current one).
	 */
	void PlaceFocus(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused, bool bScreenChanged);

	/**
	 * Returns a focusable widget inside a focused container: a user widget (a settings row, a list row) or a list
	 * view (Slate leaves the focus on the list when its focused row goes, e.g. an item leaving the bag).
	 *
	 * @param Screen    Screen on top.
	 * @param Focused   Focused widget.
	 * @param NearRect  When set, the inner widget closest to it (user widget: same column; list: same row first);
	 *                  else the first one.
	 * @return The inner widget (for an empty list, or a user widget holding one, the screen widget closest to the
	 *         list), or null when Focused is not a container.
	 */
	TSharedPtr<SWidget> FindFocusableInside(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused, const FSlateRect* NearRect) const;

	/**
	 * Returns the row of a focused list to move the focus to.
	 *
	 * @param Screen    Screen on top.
	 * @param List      Focused list view.
	 * @param NearRect  When set, the row closest to it (same row first); else the first row.
	 * @return The row, the screen widget closest to the list when it is empty, or null.
	 */
	TSharedPtr<SWidget> FindFocusableInList(UUserWidget& Screen, UListViewBase& List, const FSlateRect* NearRect) const;

	/**
	 * Returns the enabled focusable widget of a screen closest to a place (the focus left on a disabled button, or
	 * on a widget that was rebuilt).
	 *
	 * @param Screen    Screen on top.
	 * @param FromRect  Absolute rectangle to measure from.
	 * @param From      Widget at that place, excluded from the search; may be null.
	 * @return The closest widget, or null when the screen has none.
	 */
	TSharedPtr<SWidget> FindNearestFocusable(UUserWidget& Screen, const FSlateRect& FromRect, const TSharedPtr<SWidget>& From) const;

	/** Returns true while the loading screen covers the viewport (the highlight layer sits above it). */
	bool IsLoadingScreenUp() const;

	/** Device callback: the next focus check places the focus again when the gamepad takes over. */
	UFUNCTION()
	void HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice);

	/** Viewport callback: remembers the order widgets were added in (ties of Z order). */
	void HandleViewportWidgetAdded(UWidget* Widget, ULocalPlayer* Player);

	/** Returns the screen the focus belongs to: topmost navigable viewport widget, or a modal pushed over it. */
	UUserWidget* FindTopScreen() const;

	/**
	 * Custom boundary of the screen on top: called when the engine finds nothing in the direction pressed.
	 *
	 * @param Direction  Direction pressed.
	 * @return The nearest focusable widget of the screen in that direction, or null to stay.
	 */
	TSharedPtr<SWidget> HandleBoundaryNavigation(EUINavigation Direction);

	/**
	 * Confines navigation to a screen and plugs in the nearest-widget fallback.
	 *
	 * @param Screen  Screen that just came on top.
	 */
	void ApplyNavigationBoundary(UUserWidget& Screen);

	/**
	 * Sets a navigation rule with a delegate on a widget in some directions.
	 *
	 * @param Widget      Widget whose navigation metadata is set.
	 * @param Directions  Directions the rule applies to.
	 * @param Rule        Custom (the delegate always decides) or CustomBoundary (it decides when Slate finds nothing).
	 * @param Delegate    Picks the widget to focus.
	 */
	void SetNavigationRule(SWidget& Widget, std::initializer_list<EUINavigation> Directions, EUINavigationRule Rule,
		const FNavigationDelegate& Delegate);

	/**
	 * Up/down inside a vertical scroll list: the next row in list order that has an enabled focusable widget, the one
	 * closest to the current column, scrolled into view. Slate's own search keeps to the column of the focused button
	 * and skips rows where that button is disabled.
	 *
	 * @param Direction  Direction pressed.
	 * @return The widget to focus; past the end of the list, HandleBoundaryNavigation's choice.
	 */
	TSharedPtr<SWidget> HandleListNavigation(EUINavigation Direction);

	/**
	 * Finds the list row holding a widget: the outermost vertical box below its scroll box, and the child of it.
	 *
	 * @param Focused       Widget inside the list.
	 * @param OutScrollBox  Scroll box holding the list.
	 * @param OutList       Vertical box holding the rows.
	 * @param OutRow        Row holding Focused.
	 * @return False when Focused is not in a vertical box inside a scroll box.
	 */
	bool FindListRow(UWidget& Focused, UScrollBox*& OutScrollBox, UPanelWidget*& OutList, UWidget*& OutRow) const;

	/**
	 * Picks the widget a screen gives the focus to: remembered, desired, first focusable, or the root.
	 *
	 * @param Screen  Screen to search.
	 * @return The widget, or null when the screen has nothing to focus.
	 */
	TSharedPtr<SWidget> FindFocusTarget(UUserWidget& Screen) const;

	/**
	 * Returns the widget last focused in a screen, if it can still take the focus.
	 *
	 * @param Screen  Screen to look up.
	 */
	TSharedPtr<SWidget> GetRememberedFocus(const UUserWidget& Screen) const;

	/**
	 * Gives the user focus to a widget with the Navigation cause.
	 *
	 * @param Widget  Widget to focus.
	 */
	void SetFocus(const TSharedPtr<SWidget>& Widget) const;

	/** Returns the widget the local player's Slate user has focused, or null. */
	TSharedPtr<SWidget> GetFocusedWidget() const;

	/**
	 * Updates the frame target, the tooltip and the scroll target after the focus moved.
	 *
	 * @param Screen   Screen on top.
	 * @param Focused  Focused widget.
	 */
	void HandleFocusMoved(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused);

	/** Hides the frame and the tooltip. */
	void ClearHighlight();

	/** Adds the highlight layer to the viewport, again after every level change. */
	void EnsureHighlightLayer();

	/** Hides the cursor while the gamepad drives a menu and shows it again on mouse input. */
	void UpdateCursorVisibility();

	/**
	 * Scrolls the scroll box under the focus with the right stick.
	 *
	 * @param DeltaTime  Frame time.
	 */
	void ApplyRightStickScroll(float DeltaTime) const;

	// ============================================================
	// Properties
	// ============================================================

	/** Widgets added to the viewport, with the order they were added in. */
	TMap<TObjectKey<UWidget>, int32> ViewportAddOrder;

	/** Next value of ViewportAddOrder. */
	int32 NextAddOrder = 0;

	/** Modals pushed over a screen, last on top. */
	TArray<TWeakObjectPtr<UUserWidget>> ModalScreens;

	/** Widget last focused in each screen. */
	TMap<TWeakObjectPtr<UUserWidget>, TWeakPtr<SWidget>> RememberedFocus;

	/** Absolute rectangle of the widget last focused in each screen, to land near it when that widget is gone. */
	TMap<TWeakObjectPtr<UUserWidget>, FSlateRect> RememberedFocusRects;

	/** Screen on top at the last focus check. */
	TWeakObjectPtr<UUserWidget> TopScreen;

	/** Widget focused at the last focus check. */
	TWeakPtr<SWidget> LastFocused;

	/** Absolute rectangle of the last widget focused inside the screen, to recover from it being destroyed. */
	FSlateRect LastFocusedRect;

	/** True once LastFocusedRect holds a rectangle. */
	bool bHasLastFocusedRect = false;

	/** Placements tried in a row without the focus landing on a widget of the screen. */
	int32 PlacementAttempts = 0;

	/** Focus checks waited since the focused widget vanished. */
	int32 FocusLostChecks = 0;

	/** Container waiting for RequestFocusInside. */
	TWeakObjectPtr<UWidget> PendingFocusContainer;

	/** Focus checks left before PendingFocusContainer is dropped. */
	int32 PendingFocusAttemptsLeft = 0;

	/** Scroll box the right stick scrolls. */
	TWeakObjectPtr<UScrollBox> ScrollTarget;

	/** True while a menu has the input. */
	bool bMenuInputActive = false;

	/** True when the next focus check must place the focus (screen changed, gamepad took over). */
	bool bPlacementPending = true;

	/** True while the cursor is hidden because the gamepad drives the menu. */
	bool bCursorHiddenForGamepad = false;

	/** Time accumulated towards the next focus check. */
	float FocusCheckAccumulator = 0.f;

	/** Last right stick vertical value. */
	float RightStickY = 0.f;

	/** Frame and tooltip layer. */
	TSharedPtr<SMKHFocusHighlightLayer> HighlightLayer;

	/** Slate pre-processor reading the right stick. */
	TSharedPtr<FMKHMenuNavigationInputProcessor> InputProcessor;

	/** Per-frame ticker. */
	FTSTicker::FDelegateHandle TickHandle;

	/** Registration of UGameViewportSubsystem::OnWidgetAdded. */
	FDelegateHandle WidgetAddedHandle;
};
