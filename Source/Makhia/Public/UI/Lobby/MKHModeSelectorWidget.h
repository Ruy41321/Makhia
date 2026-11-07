// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MKHModeSelectorWidget.generated.h"

class UImage;
class UMKHButton;
class UMKHButtonStyle;
class UMKHCareerLevelWidget;
class UTextBlock;

/** Way to play offered by the lobby mode selector. */
UENUM(BlueprintType)
enum class EMKHLobbyGameMode : uint8
{
	/** Online duel: the ready / matchmaking flow of the lobby. Needs a signed-in player. */
	Online,
	/** Offline practice on a still target. Needs the player alone in the lobby. */
	Training,
	/** Skirmish (EMKHOfflineMode::RandomDuel). The value name stays: W_ModeSelector saves it in Previews. Needs the player alone in the lobby. */
	FreeDuel,
	/** Offline career, level by level. Needs the player alone in the lobby. */
	PathOfGlory
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMKHModeSelectorActionSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMKHModeSelectorChangedSignature, EMKHLobbyGameMode, Mode);

/**
 * Mode selection of the lobby (parent of W_ModeSelector): a list of the four modes, the detail of the chosen one and
 * the action button that starts it, or cancels it while it runs.
 *
 * Mouse: a click on a row only updates the detail; the action button starts the mode. Gamepad: moving on the rows
 * updates the detail at once; right enters the detail (career levels, action button), left goes back to the rows.
 *
 * The online flow stays in W_LobbyOverlay: OnPlayRequested / OnCancelRequested go to its ready and cancel flow, which
 * answers with SetPlayAllowed / SetCancelAllowed and reports its lines through SetLobbyStatus.
 * The Blueprint owns the layout. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHModeSelectorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Broadcast when the action button asks to play online. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|ModeSelector")
	FMKHModeSelectorActionSignature OnPlayRequested;

	/** Broadcast when the action button asks to cancel the online flow. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|ModeSelector")
	FMKHModeSelectorActionSignature OnCancelRequested;

	/** Broadcast when another mode is chosen. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|ModeSelector")
	FMKHModeSelectorChangedSignature OnModeChanged;

	/**
	 * Lets the lobby flow allow or forbid online play on top of the lock rules (e.g. lobby not hosted yet).
	 *
	 * @param bAllowed  True to allow play.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|ModeSelector")
	void SetPlayAllowed(bool bAllowed);

	/**
	 * Lets the lobby flow offer cancel (ready state, matchmaking running). While cancel is allowed and play is not,
	 * the online mode counts as running and the action button cancels it.
	 *
	 * @param bAllowed  True to offer cancel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|ModeSelector")
	void SetCancelAllowed(bool bAllowed);

	/**
	 * Shows or hides a line of the lobby flow in the status line of the detail.
	 *
	 * @param bShow    True to show the line.
	 * @param Message  Line to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|ModeSelector")
	void SetLobbyStatus(bool bShow, const FText& Message);

	/** Returns the mode shown in the detail. */
	UFUNCTION(BlueprintPure, Category = "Lobby|ModeSelector")
	EMKHLobbyGameMode GetSelectedMode() const { return SelectedMode; }

	/** Returns true while the online mode is shown in the detail (W_LobbyOverlay shows the region menu only then). */
	UFUNCTION(BlueprintPure, Category = "Lobby|ModeSelector")
	bool IsOnlineSelected() const { return SelectedMode == EMKHLobbyGameMode::Online; }

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic - Selection
	// ============================================================

	/** Binds rows, career levels and action button, and sets the texts that never change. */
	void InitializeControls();

	/** Sets the custom gamepad navigation between the rows and the detail. */
	void InitializeNavigation();

	/**
	 * Chooses the mode shown in the detail.
	 *
	 * @param Mode  Mode to show.
	 */
	void SelectMode(EMKHLobbyGameMode Mode);

	/** Copies the texts and the picture of the chosen mode into the detail and marks its row. */
	void ApplySelectedMode();

	/** Gamepad: the focus on a row chooses its mode. */
	void FollowRowFocus();

	/** Returns the row of a mode, null when it is not bound. */
	UMKHButton* GetRow(EMKHLobbyGameMode Mode) const;

	/** Returns the label of a row, null when it is not bound. */
	UTextBlock* GetRowLabel(EMKHLobbyGameMode Mode) const;

	/** Returns the bound career levels, in order. */
	TArray<UMKHCareerLevelWidget*> GetCareerLevels() const;

	/** Returns the first widget of the detail that can take the focus, the chosen row when none can. */
	UWidget* GetDetailEntryWidget() const;

	/** Returns the chosen career level, null outside the career. */
	UWidget* GetSelectedCareerWidget() const;

	/** Row callback: a click (or A) on the row under the cursor or the focus. */
	UFUNCTION()
	void HandleRowClicked();

	/**
	 * A career level was clicked: it becomes the chosen one; with the gamepad the focus moves to the action button.
	 *
	 * @param Level  The clicked level.
	 */
	void HandleCareerLevelClicked(UMKHCareerLevelWidget* Level);

	/** Refreshes the career levels from UMKHCareerSubsystem. */
	void RefreshCareerLevels();

	/** Navigation: from a row into the detail. */
	UFUNCTION()
	UWidget* NavigateToDetail(EUINavigation Direction);

	/** Navigation: from the detail back to the chosen row. */
	UFUNCTION()
	UWidget* NavigateToRows(EUINavigation Direction);

	/** Navigation: from the action button up to the chosen career level. */
	UFUNCTION()
	UWidget* NavigateFromActionUp(EUINavigation Direction);

	/** Navigation: from a career level down to the action button. */
	UFUNCTION()
	UWidget* NavigateToAction(EUINavigation Direction);

	// ============================================================
	// Protected / Internal Logic - State
	// ============================================================

	/** Applies the running mode, then refreshes the action button, lock and status of the chosen mode. */
	void RefreshState();

	/**
	 * Returns the mode that is running and can be cancelled.
	 *
	 * @param OutMode  The running mode.
	 * @return True when a mode is running.
	 */
	bool GetRunningMode(EMKHLobbyGameMode& OutMode) const;

	/** While a mode runs: keeps it chosen and disables the other rows and the career levels. */
	void ApplyRunningMode(bool bRunning, EMKHLobbyGameMode RunningMode);

	/** Evaluates the lock rules of the chosen mode and updates action button and lock panel. */
	void RefreshPlayState();

	/** Running mode: drops an offline request someone joining made impossible, updates the cancel look. */
	void RefreshCancelState(EMKHLobbyGameMode RunningMode);

	/** Updates the look of the action button: cancel or play. */
	void ApplyActionLook(bool bCancel);

	/** Updates the status line: running mode first, then the lobby flow. */
	void RefreshStatusLine(bool bRunning, EMKHLobbyGameMode RunningMode);

	/**
	 * Updates the timer of the status line: match countdown, elapsed search time, or nothing.
	 *
	 * @param bRunning     True while a mode runs.
	 * @param RunningMode  The running mode.
	 */
	void RefreshTimer(bool bRunning, EMKHLobbyGameMode RunningMode);

	/** Returns the status line of the running mode (searching, ready, countdown, flavour, cancelling). */
	FText GetRunningStatusText(EMKHLobbyGameMode RunningMode) const;

	/** Returns the label of the action button of the chosen mode. */
	FText GetPlayLabel() const;

	/**
	 * Returns a flavour line of the wait before an offline match; lines rotate every FlavourLineDuration.
	 *
	 * @param Elapsed  Seconds since the request.
	 */
	FText GetFlavourText(double Elapsed) const;

	/**
	 * Evaluates the lock rules of a mode.
	 *
	 * @param Mode              Mode to evaluate.
	 * @param OutReason         Why the mode is locked.
	 * @param bOutSignInAction  True when the lock offers the "Sign in" action.
	 * @return True when the mode is locked.
	 */
	bool EvaluateLock(EMKHLobbyGameMode Mode, FText& OutReason, bool& bOutSignInAction) const;

	/** Online rules of EvaluateLock: online services and sign-in state. */
	bool EvaluateOnlineLock(FText& OutReason, bool& bOutSignInAction) const;

	/** Offline rules of EvaluateLock: matchmaking and other players in the lobby. */
	bool EvaluateOfflineLock(FText& OutReason) const;

	/** Returns the seconds left before the lobby match starts, negative when no countdown runs. */
	float GetMatchCountdownRemaining() const;

	/** Returns true while the matchmaking search is running. */
	bool IsMatchmaking() const;

	/** Returns the current real time of the world, 0 without a world. */
	double GetRealTimeSeconds() const;

	/**
	 * Formats an elapsed time as "m:ss".
	 *
	 * @param Seconds  Elapsed time in seconds.
	 * @return The localizable formatted time.
	 */
	static FText FormatElapsedTime(double Seconds);

	/** Action button callback: starts or cancels the chosen mode. */
	UFUNCTION()
	void HandleActionClicked();

	/** Starts the chosen offline mode through UMKHLobbySubsystem::RequestOfflineMatch. */
	void StartOfflineMode();

	/** Lock action callback: opens the login panel. */
	UFUNCTION()
	void HandleLockActionClicked();

	// ============================================================
	// Properties - Content
	// ============================================================

	/** Picture of each mode in the detail. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector")
	TMap<EMKHLobbyGameMode, FSlateBrush> Previews;

	/** Look of a row not chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector")
	TObjectPtr<UMKHButtonStyle> RowStyle;

	/** Look of the chosen row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector")
	TObjectPtr<UMKHButtonStyle> SelectedRowStyle;

	/** Look of the action button while it cancels (BS_ModeCardCancel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector")
	TObjectPtr<UMKHButtonStyle> CancelButtonStyle;

	/** Seconds the action button stays disabled after a cancel click when the lobby flow does not answer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector", meta = (ClampMin = "0.5"))
	float CancelAnswerTimeout = 5.f;

	/** Seconds each flavour line of the wait before an offline match stays in the status line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|ModeSelector", meta = (ClampMin = "0.5"))
	float FlavourLineDuration = 1.8f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Row of the online mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMKHButton> OnlineRow;

	/** Row of the training. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMKHButton> TrainingRow;

	/** Row of the random duel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMKHButton> FreeDuelRow;

	/** Row of the career. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMKHButton> PathOfGloryRow;

	/** Label of OnlineRow. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OnlineLabel;

	/** Label of TrainingRow. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrainingLabel;

	/** Label of FreeDuelRow. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FreeDuelLabel;

	/** Label of PathOfGloryRow. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PathOfGloryLabel;

	/** Heading over the online row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OnlineHeading;

	/** Heading over the offline rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OfflineHeading;

	/** Picture of the chosen mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	/** Title of the chosen mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Description of the chosen mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Career levels; shown only for the career. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CareerPanel;

	/** First career level. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHCareerLevelWidget> CareerLevel0;

	/** Second career level. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHCareerLevelWidget> CareerLevel1;

	/** Third career level. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHCareerLevelWidget> CareerLevel2;

	/** Reason and sign-in action of a locked mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockPanel;

	/** Why the mode is locked. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockReasonText;

	/** "Sign in" action of the locked online mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHButton> LockActionButton;

	/** Label of LockActionButton. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LockActionText;

	/** Status line of the lobby and of the running mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Match countdown or elapsed search time. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	/** Shown while a mode runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SearchThrobber;

	/** Starts the chosen mode, or cancels it while it runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMKHButton> ActionButton;

	/** Label of ActionButton: changes with the mode and while it runs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;

private:
	/** Look of ActionButton while it plays, read from the button when the widget is initialized. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHButtonStyle> PlayButtonStyle;

	/** Mode shown in the detail. */
	EMKHLobbyGameMode SelectedMode = EMKHLobbyGameMode::Online;

	/** Offline mode of the last request, the one that runs while the request is pending. */
	EMKHLobbyGameMode RequestedOfflineMode = EMKHLobbyGameMode::FreeDuel;

	/** Chosen career level; INDEX_NONE until the first RefreshCareerLevels picks the next one to play. */
	int32 SelectedCareerLevel = INDEX_NONE;

	/** False while the lobby flow forbids online play (SetPlayAllowed). */
	bool bPlayAllowed = true;

	/** True while the lobby flow offers cancel (SetCancelAllowed). */
	bool bCancelAllowed = false;

	/** True after a cancel click, until the lobby flow answers or CancelAnswerTimeout runs out. */
	bool bCancelRequested = false;

	/** True while the action button has the cancel look. */
	bool bShowingCancel = false;

	/** Real time at which the running mode was first seen, negative when none runs. */
	double SearchStartTime = -1.0;

	/** Real time of the last cancel click. */
	double CancelRequestTime = 0.0;

	/** First flavour line of the wait, picked at random when it starts. */
	int32 FlavourLineOffset = 0;

	/** Last line of the lobby flow, empty when none is shown. */
	FText LobbyStatus;
};
