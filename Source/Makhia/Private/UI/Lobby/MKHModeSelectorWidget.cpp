// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHModeSelectorWidget.h"

#include "Career/MKHCareerLadder.h"
#include "Career/MKHCareerSubsystem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Engine/World.h"
#include "GameInstance/MKHGameInstance.h"
#include "Lobby/MKHLobbyGameState.h"
#include "Lobby/MKHLobbySubsystem.h"
#include "MKHLogChannels.h"
#include "Online/Auth/MKHAuthSubsystem.h"
#include "UI/Button/MKHButton.h"
#include "UI/Button/MKHButtonStyle.h"
#include "UI/HUD/MKHLobbyHUD.h"
#include "UI/Lobby/MKHCareerLevelWidget.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

// Keeps the namespace of the old mode card: its translated lines stay valid.
#define LOCTEXT_NAMESPACE "MKHModeCard"

namespace
{
	/** Number of flavour lines of the wait before an offline match (see GetFlavourText). */
	constexpr int32 NumFlavourLines = 4;

	/** Every mode, in row order. */
	constexpr EMKHLobbyGameMode AllModes[] = { EMKHLobbyGameMode::Online, EMKHLobbyGameMode::Training, EMKHLobbyGameMode::FreeDuel, EMKHLobbyGameMode::PathOfGlory };

	/** Returns the name of a mode. */
	FText GetModeTitle(const EMKHLobbyGameMode Mode)
	{
		switch (Mode)
		{
		case EMKHLobbyGameMode::Training:
			return LOCTEXT("TrainingTitle", "Training");
		case EMKHLobbyGameMode::FreeDuel:
			return LOCTEXT("SkirmishTitle", "Skirmish");
		case EMKHLobbyGameMode::PathOfGlory:
			return LOCTEXT("WayOfTheMakhaiTitle", "Way of the Makhai");
		default:
			return LOCTEXT("OnlineTitle", "Play online");
		}
	}

	/** Returns the description of a mode. */
	FText GetModeDescription(const EMKHLobbyGameMode Mode)
	{
		switch (Mode)
		{
		case EMKHLobbyGameMode::Training:
			return LOCTEXT("TrainingDescription", "A still target in the middle of the arena: test your damage and stamina damage without pressure.");
		case EMKHLobbyGameMode::FreeDuel:
			return LOCTEXT("RandomDuelDescription", "A full duel against a random opponent who adapts to you, round after round.");
		case EMKHLobbyGameMode::PathOfGlory:
			return LOCTEXT("WayOfTheMakhaiDescription", "Face every opponent, climb the ranks and claim the title of Makhai, the highest honor of the Makhion.");
		default:
			return LOCTEXT("OnlineDescription", "Find an opponent online, or duel the friends in your lobby.");
		}
	}
}

// ============================================================
// Public Interface
// ============================================================

void UMKHModeSelectorWidget::SetPlayAllowed(const bool bAllowed)
{
	bPlayAllowed = bAllowed;
	bCancelRequested = false;
	RefreshState();
}

void UMKHModeSelectorWidget::SetCancelAllowed(const bool bAllowed)
{
	bCancelAllowed = bAllowed;
	bCancelRequested = false;
	RefreshState();
}

void UMKHModeSelectorWidget::SetLobbyStatus(const bool bShow, const FText& Message)
{
	LobbyStatus = bShow ? Message : FText::GetEmpty();
	RefreshState();
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHModeSelectorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	InitializeControls();
	InitializeNavigation();
}

void UMKHModeSelectorWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySelectedMode();
	if (IsDesignTime())
	{
		for (const EMKHLobbyGameMode Mode : AllModes)
		{
			MKHLobbyUI::SetText(GetRowLabel(Mode), GetModeTitle(Mode));
		}
		MKHLobbyUI::SetText(ActionText, GetPlayLabel());
		MKHLobbyUI::SetShown(LockPanel, false);
		MKHLobbyUI::SetShown(SearchThrobber, false);
		MKHLobbyUI::SetShown(TimerText, false);
	}
}

void UMKHModeSelectorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshState();
}

void UMKHModeSelectorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Sign-in state, lobby population and matchmaking change from many places: poll while shown.
	FollowRowFocus();
	RefreshState();
}

// ============================================================
// Protected / Internal Logic - Selection
// ============================================================

void UMKHModeSelectorWidget::InitializeControls()
{
	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		if (UMKHButton* Row = GetRow(Mode); IsValid(Row))
		{
			Row->OnClicked.AddUniqueDynamic(this, &UMKHModeSelectorWidget::HandleRowClicked);
		}
		MKHLobbyUI::SetText(GetRowLabel(Mode), GetModeTitle(Mode));
	}

	for (UMKHCareerLevelWidget* Level : GetCareerLevels())
	{
		Level->OnLevelClicked.AddUObject(this, &UMKHModeSelectorWidget::HandleCareerLevelClicked);
	}

	if (IsValid(ActionButton))
	{
		ActionButton->OnClicked.AddUniqueDynamic(this, &UMKHModeSelectorWidget::HandleActionClicked);
		PlayButtonStyle = ActionButton->GetButtonStyle();
	}

	if (IsValid(LockActionButton))
	{
		LockActionButton->OnClicked.AddUniqueDynamic(this, &UMKHModeSelectorWidget::HandleLockActionClicked);
	}

	MKHLobbyUI::SetText(LockActionText, LOCTEXT("SignIn", "Sign in"));
	MKHLobbyUI::SetText(OnlineHeading, LOCTEXT("OnlineHeading", "Online"));
	MKHLobbyUI::SetText(OfflineHeading, LOCTEXT("OfflineHeading", "Offline"));
}

void UMKHModeSelectorWidget::InitializeNavigation()
{
	FCustomWidgetNavigationDelegate ToDetail;
	ToDetail.BindDynamic(this, &UMKHModeSelectorWidget::NavigateToDetail);
	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		if (UMKHButton* Row = GetRow(Mode); IsValid(Row))
		{
			Row->SetNavigationRuleCustom(EUINavigation::Right, ToDetail);
		}
	}

	FCustomWidgetNavigationDelegate ToRows;
	ToRows.BindDynamic(this, &UMKHModeSelectorWidget::NavigateToRows);
	FCustomWidgetNavigationDelegate ToAction;
	ToAction.BindDynamic(this, &UMKHModeSelectorWidget::NavigateToAction);
	FCustomWidgetNavigationDelegate FromActionUp;
	FromActionUp.BindDynamic(this, &UMKHModeSelectorWidget::NavigateFromActionUp);

	if (IsValid(ActionButton))
	{
		ActionButton->SetNavigationRuleCustom(EUINavigation::Left, ToRows);
		ActionButton->SetNavigationRuleCustom(EUINavigation::Up, FromActionUp);
	}
	if (IsValid(LockActionButton))
	{
		LockActionButton->SetNavigationRuleCustom(EUINavigation::Left, ToRows);
		LockActionButton->SetNavigationRuleCustom(EUINavigation::Down, ToAction);
	}

	const TArray<UMKHCareerLevelWidget*> Levels = GetCareerLevels();
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		if (UMKHButton* Button = Levels[Index]->GetButton(); IsValid(Button))
		{
			Button->SetNavigationRuleCustom(EUINavigation::Down, ToAction);
			if (Index == 0)
			{
				Button->SetNavigationRuleCustom(EUINavigation::Left, ToRows);
			}
		}
	}
}

void UMKHModeSelectorWidget::SelectMode(const EMKHLobbyGameMode Mode)
{
	if (Mode == SelectedMode)
	{
		return;
	}

	SelectedMode = Mode;
	ApplySelectedMode();
	OnModeChanged.Broadcast(SelectedMode);
}

void UMKHModeSelectorWidget::ApplySelectedMode()
{
	// Instances placed in another widget can load without the class default: fall back to the class default object.
	const UMKHModeSelectorWidget* Defaults = GetClass()->GetDefaultObject<UMKHModeSelectorWidget>();
	UMKHButtonStyle* Normal = IsValid(RowStyle) ? RowStyle.Get() : Defaults->RowStyle.Get();
	UMKHButtonStyle* Selected = IsValid(SelectedRowStyle) ? SelectedRowStyle.Get() : Defaults->SelectedRowStyle.Get();
	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		UMKHButton* Row = GetRow(Mode);
		UMKHButtonStyle* Style = Mode == SelectedMode ? Selected : Normal;
		if (IsValid(Row) && IsValid(Style))
		{
			Row->SetButtonStyle(Style);
		}
	}

	MKHLobbyUI::SetText(TitleText, GetModeTitle(SelectedMode));
	const bool bCareer = SelectedMode == EMKHLobbyGameMode::PathOfGlory;
	MKHLobbyUI::SetText(DescriptionText, GetModeDescription(SelectedMode));
	MKHLobbyUI::SetShown(CareerPanel, bCareer, ESlateVisibility::SelfHitTestInvisible);

	const TMap<EMKHLobbyGameMode, FSlateBrush>& Brushes = Previews.Num() > 0 ? Previews : Defaults->Previews;
	const FSlateBrush* Brush = Brushes.Find(SelectedMode);
	if (IsValid(PreviewImage) && Brush != nullptr)
	{
		PreviewImage->SetBrush(*Brush);
	}

	RefreshCareerLevels();
}

void UMKHModeSelectorWidget::FollowRowFocus()
{
	if (!UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this))
	{
		return;
	}

	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		if (const UMKHButton* Row = GetRow(Mode); IsValid(Row) && Row->HasKeyboardFocus())
		{
			SelectMode(Mode);
			return;
		}
	}
}

UMKHButton* UMKHModeSelectorWidget::GetRow(const EMKHLobbyGameMode Mode) const
{
	switch (Mode)
	{
	case EMKHLobbyGameMode::Training:
		return TrainingRow;
	case EMKHLobbyGameMode::FreeDuel:
		return FreeDuelRow;
	case EMKHLobbyGameMode::PathOfGlory:
		return PathOfGloryRow;
	default:
		return OnlineRow;
	}
}

UTextBlock* UMKHModeSelectorWidget::GetRowLabel(const EMKHLobbyGameMode Mode) const
{
	switch (Mode)
	{
	case EMKHLobbyGameMode::Training:
		return TrainingLabel;
	case EMKHLobbyGameMode::FreeDuel:
		return FreeDuelLabel;
	case EMKHLobbyGameMode::PathOfGlory:
		return PathOfGloryLabel;
	default:
		return OnlineLabel;
	}
}

TArray<UMKHCareerLevelWidget*> UMKHModeSelectorWidget::GetCareerLevels() const
{
	TArray<UMKHCareerLevelWidget*> Levels;
	for (UMKHCareerLevelWidget* Level : { CareerLevel0.Get(), CareerLevel1.Get(), CareerLevel2.Get() })
	{
		if (IsValid(Level))
		{
			Levels.Add(Level);
		}
	}
	return Levels;
}

UWidget* UMKHModeSelectorWidget::GetDetailEntryWidget() const
{
	if (UWidget* Level = GetSelectedCareerWidget())
	{
		return Level;
	}
	if (IsValid(LockActionButton) && LockActionButton->IsVisible())
	{
		return LockActionButton;
	}
	if (IsValid(ActionButton) && ActionButton->GetIsEnabled())
	{
		return ActionButton;
	}
	return GetRow(SelectedMode);
}

UWidget* UMKHModeSelectorWidget::GetSelectedCareerWidget() const
{
	if (SelectedMode != EMKHLobbyGameMode::PathOfGlory)
	{
		return nullptr;
	}

	const TArray<UMKHCareerLevelWidget*> Levels = GetCareerLevels();
	if (!Levels.IsValidIndex(SelectedCareerLevel))
	{
		return nullptr;
	}

	UMKHButton* Button = Levels[SelectedCareerLevel]->GetButton();
	return IsValid(Button) && Button->GetIsEnabled() ? Button : nullptr;
}

void UMKHModeSelectorWidget::HandleRowClicked()
{
	const bool bGamepad = UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this);
	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		const UMKHButton* Row = GetRow(Mode);
		if (IsValid(Row) && (bGamepad ? Row->HasKeyboardFocus() : Row->IsHovered()))
		{
			SelectMode(Mode);
			break;
		}
	}

	if (bGamepad)
	{
		RefreshState();
		if (UWidget* Entry = GetDetailEntryWidget(); IsValid(Entry))
		{
			Entry->SetFocus();
		}
	}
}

void UMKHModeSelectorWidget::HandleCareerLevelClicked(UMKHCareerLevelWidget* Level)
{
	const int32 Index = GetCareerLevels().IndexOfByKey(Level);
	if (Index == INDEX_NONE)
	{
		return;
	}

	SelectedCareerLevel = Index;
	RefreshCareerLevels();
	if (UMKHMenuNavigationLibrary::IsGamepadNavigationActive(this) && IsValid(ActionButton))
	{
		ActionButton->SetFocus();
	}
}

void UMKHModeSelectorWidget::RefreshCareerLevels()
{
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!IsValid(Career) || !IsValid(Skins))
	{
		return;
	}

	// Nothing chosen yet, or a choice no longer playable: the next level to play.
	if (!Career->IsLevelUnlocked(SelectedCareerLevel))
	{
		SelectedCareerLevel = Career->GetNextLevelIndex();
	}

	const TArray<UMKHCareerLevelWidget*> Levels = GetCareerLevels();
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const FMKHCareerLevel* Level = Career->FindLevel(Index);
		MKHLobbyUI::SetShown(Levels[Index], Level != nullptr, ESlateVisibility::SelfHitTestInvisible);
		if (Level == nullptr)
		{
			continue;
		}

		const bool bCompleted = Index < Career->GetCompletedLevels();
		const bool bLocked = !Career->IsLevelUnlocked(Index);
		const FText State = bCompleted ? LOCTEXT("CareerCompleted", "Completed") : (bLocked ? LOCTEXT("CareerLocked", "Locked") : LOCTEXT("CareerNext", "Next"));
		const FMKHSkinDefinition* Skin = Skins->FindSkin(Level->BotSkinId);
		Levels[Index]->SetLevel(State, Skin != nullptr ? Skin->DisplayName : FText::GetEmpty(), Level->TitleAwarded, bLocked);
		Levels[Index]->SetSelected(Index == SelectedCareerLevel);
	}
}

UWidget* UMKHModeSelectorWidget::NavigateToDetail(EUINavigation Direction)
{
	return GetDetailEntryWidget();
}

UWidget* UMKHModeSelectorWidget::NavigateToRows(EUINavigation Direction)
{
	return GetRow(SelectedMode);
}

UWidget* UMKHModeSelectorWidget::NavigateFromActionUp(EUINavigation Direction)
{
	if (UWidget* Level = GetSelectedCareerWidget())
	{
		return Level;
	}
	if (IsValid(LockActionButton) && LockActionButton->IsVisible())
	{
		return LockActionButton;
	}
	return ActionButton;
}

UWidget* UMKHModeSelectorWidget::NavigateToAction(EUINavigation Direction)
{
	return IsValid(ActionButton) && ActionButton->GetIsEnabled() ? ActionButton.Get() : GetRow(SelectedMode);
}

// ============================================================
// Protected / Internal Logic - State
// ============================================================

void UMKHModeSelectorWidget::RefreshState()
{
	EMKHLobbyGameMode RunningMode = SelectedMode;
	const bool bRunning = GetRunningMode(RunningMode);
	ApplyRunningMode(bRunning, RunningMode);

	if (bRunning)
	{
		RefreshCancelState(RunningMode);
	}
	else
	{
		RefreshPlayState();
	}

	RefreshTimer(bRunning, RunningMode);
	RefreshStatusLine(bRunning, RunningMode);
}

bool UMKHModeSelectorWidget::GetRunningMode(EMKHLobbyGameMode& OutMode) const
{
	if (const UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby) && Lobby->IsOfflineMatchPending())
	{
		OutMode = RequestedOfflineMode;
		return true;
	}

	if (bCancelAllowed && !bPlayAllowed)
	{
		OutMode = EMKHLobbyGameMode::Online;
		return true;
	}

	return false;
}

void UMKHModeSelectorWidget::ApplyRunningMode(const bool bRunning, const EMKHLobbyGameMode RunningMode)
{
	if (bRunning)
	{
		SelectMode(RunningMode);
	}

	for (const EMKHLobbyGameMode Mode : AllModes)
	{
		MKHLobbyUI::SetEnabled(GetRow(Mode), !bRunning || Mode == RunningMode);
	}

	const bool bLevelsEnabled = !bRunning;
	if (IsValid(CareerPanel))
	{
		CareerPanel->SetIsEnabled(bLevelsEnabled);
	}

	if (bRunning != bShowingCancel)
	{
		bShowingCancel = bRunning;
		bCancelRequested = false;
		SearchStartTime = -1.0;
		FlavourLineOffset = FMath::RandRange(0, NumFlavourLines - 1);
		ApplyActionLook(bRunning);
	}
}

void UMKHModeSelectorWidget::RefreshPlayState()
{
	FText Reason;
	bool bSignInAction = false;
	const bool bLocked = EvaluateLock(SelectedMode, Reason, bSignInAction);

	MKHLobbyUI::SetShown(LockPanel, bLocked, ESlateVisibility::SelfHitTestInvisible);
	MKHLobbyUI::SetText(LockReasonText, Reason);
	MKHLobbyUI::SetShown(LockActionButton, bLocked && bSignInAction);
	MKHLobbyUI::SetText(ActionText, GetPlayLabel());
	MKHLobbyUI::SetEnabled(ActionButton, !bLocked && (SelectedMode != EMKHLobbyGameMode::Online || bPlayAllowed));
}

void UMKHModeSelectorWidget::RefreshCancelState(const EMKHLobbyGameMode RunningMode)
{
	const double Now = GetRealTimeSeconds();
	if (bCancelRequested && Now - CancelRequestTime >= CancelAnswerTimeout)
	{
		bCancelRequested = false;
	}

	// Someone joining the room makes the offline match impossible: drop the request.
	UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	if (RunningMode != EMKHLobbyGameMode::Online && IsValid(Lobby) && Lobby->HasOtherPlayersInLobby())
	{
		Lobby->CancelOfflineMatchRequest();
	}

	MKHLobbyUI::SetShown(LockPanel, false);
	MKHLobbyUI::SetText(ActionText, LOCTEXT("CancelTitle", "Cancel"));
	MKHLobbyUI::SetEnabled(ActionButton, !bCancelRequested);
}

void UMKHModeSelectorWidget::ApplyActionLook(const bool bCancel)
{
	// Instances placed in another widget can load without the class default: fall back to the class default object.
	UMKHButtonStyle* CancelStyle = CancelButtonStyle;
	if (!IsValid(CancelStyle))
	{
		CancelStyle = GetClass()->GetDefaultObject<UMKHModeSelectorWidget>()->CancelButtonStyle;
	}

	UMKHButtonStyle* Style = bCancel && IsValid(CancelStyle) ? CancelStyle : PlayButtonStyle.Get();
	if (IsValid(ActionButton) && IsValid(Style))
	{
		ActionButton->SetButtonStyle(Style);
	}
}

void UMKHModeSelectorWidget::RefreshStatusLine(const bool bRunning, const EMKHLobbyGameMode RunningMode)
{
	MKHLobbyUI::SetShown(SearchThrobber, bRunning && !bCancelRequested, ESlateVisibility::HitTestInvisible);
	// A line of the lobby flow (search state, failed cancel) wins over the generic one of the running mode.
	const bool bUseLobbyLine = !LobbyStatus.IsEmpty() && !bCancelRequested;
	MKHLobbyUI::SetText(StatusText, bUseLobbyLine || !bRunning ? LobbyStatus : GetRunningStatusText(RunningMode));
}

void UMKHModeSelectorWidget::RefreshTimer(const bool bRunning, const EMKHLobbyGameMode RunningMode)
{
	// The elapsed time counts from the first tick that sees the search running.
	const double Now = GetRealTimeSeconds();
	const bool bSearching = bRunning && (RunningMode != EMKHLobbyGameMode::Online || IsMatchmaking());
	if (!bSearching)
	{
		SearchStartTime = -1.0;
	}
	else if (SearchStartTime < 0.0)
	{
		SearchStartTime = Now;
	}

	const float Countdown = bRunning && RunningMode == EMKHLobbyGameMode::Online ? GetMatchCountdownRemaining() : -1.f;
	const bool bShowTimer = !bCancelRequested && (Countdown >= 0.f || bSearching);
	MKHLobbyUI::SetShown(TimerText, bShowTimer, ESlateVisibility::HitTestInvisible);
	if (bShowTimer)
	{
		const FText Time = Countdown >= 0.f ? FText::AsNumber(FMath::CeilToInt32(Countdown)) : FormatElapsedTime(Now - SearchStartTime);
		MKHLobbyUI::SetText(TimerText, Time);
	}
}

FText UMKHModeSelectorWidget::GetRunningStatusText(const EMKHLobbyGameMode RunningMode) const
{
	if (bCancelRequested)
	{
		return LOCTEXT("Cancelling", "Cancelling...");
	}

	if (RunningMode != EMKHLobbyGameMode::Online)
	{
		return GetFlavourText(SearchStartTime < 0.0 ? 0.0 : GetRealTimeSeconds() - SearchStartTime);
	}

	if (GetMatchCountdownRemaining() >= 0.f)
	{
		return LOCTEXT("DuelStarting", "The duel is about to begin!");
	}

	return IsMatchmaking()
		? LOCTEXT("SearchingOpponent", "Searching for an opponent...")
		: LOCTEXT("ReadyWaiting", "Ready. Waiting for the other players.");
}

FText UMKHModeSelectorWidget::GetPlayLabel() const
{
	switch (SelectedMode)
	{
	case EMKHLobbyGameMode::Training:
		return LOCTEXT("ActionTrain", "Train");
	case EMKHLobbyGameMode::FreeDuel:
	case EMKHLobbyGameMode::PathOfGlory:
		return LOCTEXT("ActionFight", "Fight");
	default:
		break;
	}

	const UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	return IsValid(Lobby) && Lobby->HasOtherPlayersInLobby() ? LOCTEXT("ActionReady", "Ready") : LOCTEXT("ActionSearch", "Search");
}

FText UMKHModeSelectorWidget::GetFlavourText(const double Elapsed) const
{
	const int32 Step = FMath::FloorToInt32(Elapsed / FMath::Max(0.5f, FlavourLineDuration));
	switch ((FlavourLineOffset + Step) % NumFlavourLines)
	{
	case 0:
		return LOCTEXT("BotFlavourSand", "Raking the arena sand...");
	case 1:
		return LOCTEXT("OfflineFlavourGates", "Oiling the arena gates...");
	case 2:
		return LOCTEXT("OfflineFlavourBlade", "Your opponent is sharpening their blade...");
	default:
		return LOCTEXT("BotFlavourCrowd", "The crowd is taking its seats...");
	}
}

bool UMKHModeSelectorWidget::EvaluateLock(const EMKHLobbyGameMode Mode, FText& OutReason, bool& bOutSignInAction) const
{
	bOutSignInAction = false;
	if (Mode == EMKHLobbyGameMode::Online)
	{
		return EvaluateOnlineLock(OutReason, bOutSignInAction);
	}

	if (EvaluateOfflineLock(OutReason))
	{
		return true;
	}

	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	if (Mode == EMKHLobbyGameMode::PathOfGlory && (!IsValid(Career) || Career->GetLevelCount() == 0))
	{
		OutReason = LOCTEXT("CareerUnavailable", "The career is not available.");
		return true;
	}

	return false;
}

bool UMKHModeSelectorWidget::EvaluateOnlineLock(FText& OutReason, bool& bOutSignInAction) const
{
	const UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(this);
	if (!IsValid(Auth) || !Auth->IsOnlineAvailable())
	{
		OutReason = IsValid(Auth) ? Auth->GetOnlineUnavailableText() : LOCTEXT("OnlineUnavailable", "Online services are not available.");
		return true;
	}

	switch (Auth->GetAuthState())
	{
	case EMKHAuthState::LoggedIn:
		return false;
	case EMKHAuthState::LoggingIn:
	case EMKHAuthState::Unknown:
		OutReason = LOCTEXT("SigningIn", "Signing in...");
		return true;
	default:
		if (!Auth->IsOnlineReachable())
		{
			// The game signs back in by itself when the connection returns: nothing to press meanwhile.
			OutReason = LOCTEXT("NoInternet", "No internet connection. You can still play offline.");
			return true;
		}
		OutReason = LOCTEXT("SignInToPlayOnline", "Sign in to play online.");
		bOutSignInAction = Auth->CanStartLogin();
		return true;
	}
}

bool UMKHModeSelectorWidget::EvaluateOfflineLock(FText& OutReason) const
{
	if (IsMatchmaking())
	{
		OutReason = LOCTEXT("CancelSearchForOffline", "Cancel the search to play offline.");
		return true;
	}

	const UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	const UMKHGameInstance* GameInstance = UMKHGameInstance::GetMKHGameInstance(this);
	if ((IsValid(Lobby) && Lobby->HasOtherPlayersInLobby()) || (IsValid(GameInstance) && GameInstance->IsClient()))
	{
		OutReason = LOCTEXT("LeaveRoomForOffline", "Leave the room to play offline.");
		return true;
	}

	return false;
}

float UMKHModeSelectorWidget::GetMatchCountdownRemaining() const
{
	const UWorld* World = GetWorld();
	const AMKHLobbyGameState* LobbyGameState = IsValid(World) ? World->GetGameState<AMKHLobbyGameState>() : nullptr;
	return IsValid(LobbyGameState) ? LobbyGameState->GetMatchCountdownRemaining() : -1.f;
}

bool UMKHModeSelectorWidget::IsMatchmaking() const
{
	const UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	return IsValid(Lobby) && Lobby->GetIsMatchmaking();
}

double UMKHModeSelectorWidget::GetRealTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.0;
}

FText UMKHModeSelectorWidget::FormatElapsedTime(const double Seconds)
{
	const int32 TotalSeconds = FMath::Max(0, FMath::FloorToInt32(Seconds));

	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2).SetUseGrouping(false);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Minutes"), FText::AsNumber(TotalSeconds / 60));
	Args.Add(TEXT("Seconds"), FText::AsNumber(TotalSeconds % 60, &TwoDigits));
	return FText::Format(LOCTEXT("ElapsedTime", "{Minutes}:{Seconds}"), Args);
}

void UMKHModeSelectorWidget::HandleActionClicked()
{
	EMKHLobbyGameMode RunningMode = SelectedMode;
	if (GetRunningMode(RunningMode))
	{
		if (RunningMode != EMKHLobbyGameMode::Online)
		{
			if (UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this); IsValid(Lobby))
			{
				Lobby->CancelOfflineMatchRequest();
			}
		}
		else if (!bCancelRequested)
		{
			// Flag first: the broadcast can answer synchronously (SetPlayAllowed / SetCancelAllowed).
			bCancelRequested = true;
			CancelRequestTime = GetRealTimeSeconds();
			OnCancelRequested.Broadcast();
		}
		RefreshState();
		return;
	}

	if (SelectedMode == EMKHLobbyGameMode::Online)
	{
		OnPlayRequested.Broadcast();
		return;
	}

	StartOfflineMode();
}

void UMKHModeSelectorWidget::StartOfflineMode()
{
	UMKHLobbySubsystem* Lobby = UMKHLobbySubsystem::Get(this);
	if (!IsValid(Lobby))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHModeSelectorWidget: no lobby subsystem, cannot start the offline match."));
		return;
	}

	RequestedOfflineMode = SelectedMode;
	switch (SelectedMode)
	{
	case EMKHLobbyGameMode::Training:
		Lobby->RequestOfflineMatch(EMKHOfflineMode::Training);
		break;
	case EMKHLobbyGameMode::PathOfGlory:
		Lobby->RequestOfflineMatch(EMKHOfflineMode::Career, SelectedCareerLevel);
		break;
	default:
		Lobby->RequestOfflineMatch(EMKHOfflineMode::RandomDuel);
		break;
	}
	RefreshState();
}

void UMKHModeSelectorWidget::HandleLockActionClicked()
{
	if (AMKHLobbyHUD* LobbyHUD = AMKHLobbyHUD::GetForPlayer(GetOwningPlayer()); IsValid(LobbyHUD))
	{
		LobbyHUD->OpenLoginPanel();
	}
}

#undef LOCTEXT_NAMESPACE
