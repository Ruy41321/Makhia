// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Navigation/MKHMenuNavigationSubsystem.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/IUserListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ListView.h"
#include "Components/ListViewBase.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "MKHLogChannels.h"
#include "SMKHFocusHighlightLayer.h"
#include "Types/NavigationMetaData.h"
#include "UI/Navigation/MKHFocusHighlightMetaData.h"
#include "UI/Navigation/MKHMenuNavigationSettings.h"
#include "UI/Navigation/MKHNavigableScreen.h"
#include "UI/Loading/MKHLoadingScreenSubsystem.h"

namespace MKHMenuNavigation
{
	/** Focus checks a deferred RequestFocusInside waits for the container to be laid out. */
	constexpr int32 PendingFocusAttempts = 10;

	/** Focus checks a placement is retried while the screen's widgets are not laid out yet. */
	constexpr int32 PlacementAttempts = 10;

	/** Focus checks to wait after the focused widget vanished, before placing the focus again. */
	constexpr int32 FocusLostWaitChecks = 2;

	/** Whether a widget is drawn, takes input and is enabled (its own flags only). */
	bool IsShown(const UWidget& Widget)
	{
		const ESlateVisibility Visibility = Widget.GetVisibility();
		return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden
			&& Visibility != ESlateVisibility::HitTestInvisible && Widget.GetIsEnabled();
	}

	/**
	 * Whether a displayed list entry still shows an item of its list. The row of an item that left the list (an item
	 * dropped from the bag) stays among the displayed entries until the list regenerates, off screen.
	 */
	bool IsLiveListEntry(const UListViewBase& List, const UUserWidget& Entry)
	{
		const UListView* ObjectList = Cast<UListView>(&List);
		if (!IsValid(ObjectList) || !Entry.Implements<UUserListEntry>())
		{
			return true;
		}
		const TObjectPtr<UObject>* Item = ObjectList->ItemFromEntryWidget(Entry);
		return Item != nullptr && ObjectList->GetIndexForItem(*Item) != INDEX_NONE;
	}

	/**
	 * Visits every shown widget of a subtree (the root included): enters user widgets, only the active page of a
	 * switcher and the displayed entries of a list. The visitor returns false to stop.
	 */
	bool ForEachShownWidget(UWidget* Widget, TFunctionRef<bool(UWidget&)> Visitor)
	{
		if (!IsValid(Widget) || !IsShown(*Widget))
		{
			return true;
		}
		if (!Visitor(*Widget))
		{
			return false;
		}

		if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			return ForEachShownWidget(UserWidget->GetRootWidget(), Visitor);
		}
		if (UListViewBase* List = Cast<UListViewBase>(Widget))
		{
			for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
			{
				if (IsValid(Entry) && IsLiveListEntry(*List, *Entry) && !ForEachShownWidget(Entry, Visitor))
				{
					return false;
				}
			}
			return true;
		}
		if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget))
		{
			return ForEachShownWidget(Switcher->GetActiveWidget(), Visitor);
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (!ForEachShownWidget(Panel->GetChildAt(Index), Visitor))
				{
					return false;
				}
			}
		}
		return true;
	}

	/** Whether a widget can take the focus. Lists and scroll boxes are skipped: their entries and children take it. */
	bool IsFocusCandidate(const UWidget& Widget)
	{
		if (Widget.IsA<UListViewBase>() || Widget.IsA<UScrollBox>())
		{
			return false;
		}
		const TSharedPtr<SWidget> SlateWidget = Widget.GetCachedWidget();
		return SlateWidget.IsValid() && SlateWidget->SupportsKeyboardFocus() && SlateWidget->IsEnabled();
	}

	/** First focusable widget of a subtree, in layout order. */
	TSharedPtr<SWidget> FindFirstFocusable(UWidget* Root)
	{
		TSharedPtr<SWidget> Found;
		ForEachShownWidget(Root, [&Found](UWidget& Widget)
		{
			if (IsFocusCandidate(Widget))
			{
				Found = Widget.GetCachedWidget();
				return false;
			}
			return true;
		});
		return Found;
	}

	/**
	 * Focusable widget of a subtree horizontally closest to a rectangle (keeps the column when moving between rows);
	 * the first focusable one when no rectangle is given or none of them has been laid out yet.
	 */
	TSharedPtr<SWidget> FindFocusableNear(UWidget* Root, const FSlateRect* NearRect);

	/** Whether Ancestor is Widget or one of its Slate parents. */
	bool IsSelfOrDescendantOf(const TSharedPtr<SWidget>& Widget, const TSharedPtr<SWidget>& Ancestor)
	{
		if (!Ancestor.IsValid())
		{
			return false;
		}
		for (TSharedPtr<SWidget> Current = Widget; Current.IsValid(); Current = Current->GetParentWidget())
		{
			if (Current == Ancestor)
			{
				return true;
			}
		}
		return false;
	}

	/** Whether a widget and every Slate parent of it are enabled. */
	bool IsEnabledInHierarchy(const TSharedPtr<SWidget>& Widget)
	{
		for (TSharedPtr<SWidget> Current = Widget; Current.IsValid(); Current = Current->GetParentWidget())
		{
			if (!Current->IsEnabled())
			{
				return false;
			}
		}
		return Widget.IsValid();
	}

	/** Whether a widget is currently laid out in a window (not collapsed, not on an inactive switcher page). */
	bool IsOnScreen(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return false;
		}
		FWidgetPath Path;
		return FSlateApplication::Get().GeneratePathToWidgetUnchecked(Widget.ToSharedRef(), Path) && Path.IsValid();
	}

	/** Absolute rectangle of a widget; false when it has no size yet. */
	bool GetAbsoluteRect(const SWidget& Widget, FSlateRect& OutRect)
	{
		const FGeometry& Geometry = Widget.GetPaintSpaceGeometry();
		const FVector2f Size = Geometry.GetAbsoluteSize();
		if (Size.X < 1.f || Size.Y < 1.f)
		{
			return false;
		}
		OutRect = FSlateRect(Geometry.GetAbsolutePosition(), Geometry.GetAbsolutePosition() + Size);
		return true;
	}

	/**
	 * Distance score of a candidate in a direction, lower is better: distance along the direction plus twice the
	 * offset across it. False when the candidate is not in that direction.
	 */
	bool ScoreCandidate(const FSlateRect& From, const FSlateRect& To, EUINavigation Direction, double& OutScore)
	{
		const FVector2f Delta = To.GetCenter() - From.GetCenter();
		double Along = 0.0;
		double Across = 0.0;
		switch (Direction)
		{
		case EUINavigation::Left:	Along = -Delta.X; Across = FMath::Abs(Delta.Y); break;
		case EUINavigation::Right:	Along = Delta.X; Across = FMath::Abs(Delta.Y); break;
		case EUINavigation::Up:		Along = -Delta.Y; Across = FMath::Abs(Delta.X); break;
		case EUINavigation::Down:	Along = Delta.Y; Across = FMath::Abs(Delta.X); break;
		default:					return false;
		}
		if (Along < 1.0)
		{
			return false;
		}
		OutScore = Along + 2.0 * Across;
		return true;
	}

	TSharedPtr<SWidget> FindFocusableNear(UWidget* Root, const FSlateRect* NearRect)
	{
		if (NearRect == nullptr)
		{
			return FindFirstFocusable(Root);
		}

		TSharedPtr<SWidget> Best;
		float BestDistance = TNumericLimits<float>::Max();
		ForEachShownWidget(Root, [&](UWidget& Widget)
		{
			FSlateRect Rect;
			const TSharedPtr<SWidget> Candidate = Widget.GetCachedWidget();
			if (IsFocusCandidate(Widget) && GetAbsoluteRect(*Candidate, Rect))
			{
				const float Distance = FMath::Abs(Rect.GetCenter().X - NearRect->GetCenter().X);
				if (Distance < BestDistance)
				{
					Best = Candidate;
					BestDistance = Distance;
				}
			}
			return true;
		});
		return Best.IsValid() ? Best : FindFirstFocusable(Root);
	}

	/**
	 * Focusable widget of a subtree closest to a rectangle: same row first (vertical overlap), then the closest, so
	 * the other button of the row beats the next row. Excluded and the widgets holding it (a focusable bag around an
	 * empty list) are skipped. Null when none has been laid out yet.
	 */
	TSharedPtr<SWidget> FindNearestFocusableIn(UWidget* Root, const FSlateRect& FromRect, const TSharedPtr<SWidget>& Excluded)
	{
		TSharedPtr<SWidget> Best;
		float BestDistance = TNumericLimits<float>::Max();
		ForEachShownWidget(Root, [&](UWidget& Widget)
		{
			const TSharedPtr<SWidget> Candidate = Widget.GetCachedWidget();
			FSlateRect CandidateRect;
			if (!IsSelfOrDescendantOf(Excluded, Candidate) && IsFocusCandidate(Widget) && GetAbsoluteRect(*Candidate, CandidateRect))
			{
				const FVector2f Delta = CandidateRect.GetCenter() - FromRect.GetCenter();
				const bool bSameRow = CandidateRect.Top < FromRect.Bottom && CandidateRect.Bottom > FromRect.Top;
				const float Distance = FMath::Abs(Delta.X) + 3.f * FMath::Abs(Delta.Y) + (bSameRow ? 0.f : 100000.f);
				if (Distance < BestDistance)
				{
					Best = Candidate;
					BestDistance = Distance;
				}
			}
			return true;
		});
		return Best;
	}

	/** First focusable widget of a subtree, skipping Excluded and the widgets holding it. */
	TSharedPtr<SWidget> FindFirstFocusableExcept(UWidget* Root, const TSharedPtr<SWidget>& Excluded)
	{
		TSharedPtr<SWidget> Found;
		ForEachShownWidget(Root, [&Found, &Excluded](UWidget& Widget)
		{
			if (IsFocusCandidate(Widget) && !IsSelfOrDescendantOf(Excluded, Widget.GetCachedWidget()))
			{
				Found = Widget.GetCachedWidget();
				return false;
			}
			return true;
		});
		return Found;
	}

	/** First shown list view of a subtree, or null. */
	UListViewBase* FindListIn(UWidget* Root)
	{
		UListViewBase* Found = nullptr;
		ForEachShownWidget(Root, [&Found](UWidget& Widget)
		{
			Found = Cast<UListViewBase>(&Widget);
			return Found == nullptr;
		});
		return Found;
	}

	/** Parent of a widget, crossing from a user widget's root to the user widget itself. */
	UWidget* GetParentAcrossUserWidgets(const UWidget& Widget)
	{
		if (UPanelWidget* Parent = Widget.GetParent())
		{
			return Parent;
		}
		const UWidgetTree* Tree = Cast<UWidgetTree>(Widget.GetOuter());
		return IsValid(Tree) ? Cast<UWidget>(Tree->GetOuter()) : nullptr;
	}

	/** Whether a widget and all its parents up to the viewport are shown; also returns the top-level widget. */
	bool IsShownUpToTopLevel(UWidget& Widget, UWidget*& OutTopLevel)
	{
		UWidget* Current = &Widget;
		while (true)
		{
			if (!IsShown(*Current))
			{
				return false;
			}
			UWidget* Parent = GetParentAcrossUserWidgets(*Current);
			if (!IsValid(Parent))
			{
				OutTopLevel = Current;
				return true;
			}
			Current = Parent;
		}
	}

	/** Widget of a screen whose Slate widget is the given one, or null. */
	UWidget* FindWidgetFor(UUserWidget& Screen, const TSharedPtr<SWidget>& SlateWidget)
	{
		UWidget* Found = nullptr;
		ForEachShownWidget(&Screen, [&Found, &SlateWidget](UWidget& Widget)
		{
			if (Widget.GetCachedWidget() == SlateWidget)
			{
				Found = &Widget;
				return false;
			}
			return true;
		});
		return Found;
	}

	/** Tooltip widget of a widget or, for a user widget, of its first descendant that has one. */
	UWidget* FindToolTip(UWidget* Widget)
	{
		// A focused list is not a place: its rows hold the tooltips, and none of them has the focus.
		if (!IsValid(Widget) || Widget->IsA<UListViewBase>())
		{
			return nullptr;
		}
		UWidget* Found = nullptr;
		ForEachShownWidget(Widget, [&Found](UWidget& Candidate)
		{
			// A "Tool Tip Widget" binding (inventory entries) is only evaluated by Slate on mouse hover: run it here.
			Found = Candidate.ToolTipWidgetDelegate.IsBound() ? Candidate.ToolTipWidgetDelegate.Execute() : Candidate.GetToolTip();
			return !IsValid(Found);
		});
		return Found;
	}
}

/** Reads the right stick for UMKHMenuNavigationSubsystem without consuming it. */
class FMKHMenuNavigationInputProcessor : public IInputProcessor
{
public:
	explicit FMKHMenuNavigationInputProcessor(UMKHMenuNavigationSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	{
		if (InAnalogInputEvent.GetKey() == EKeys::Gamepad_RightY)
		{
			if (UMKHMenuNavigationSubsystem* Subsystem = Owner.Get())
			{
				Subsystem->ReportRightStickY(InAnalogInputEvent.GetAnalogValue());
			}
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("MKHMenuNavigationProcessor"); }

private:
	TWeakObjectPtr<UMKHMenuNavigationSubsystem> Owner;
};

// ============================================================
// Lifecycle
// ============================================================

void UMKHMenuNavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UMKHActiveInputDeviceSubsystem* DeviceSubsystem = Collection.InitializeDependency<UMKHActiveInputDeviceSubsystem>();
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	if (IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &UMKHMenuNavigationSubsystem::HandleActiveInputDeviceChanged);
	}
	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get(); IsValid(ViewportSubsystem))
	{
		WidgetAddedHandle = ViewportSubsystem->OnWidgetAdded.AddUObject(this, &UMKHMenuNavigationSubsystem::HandleViewportWidgetAdded);
	}

	InputProcessor = MakeShared<FMKHMenuNavigationInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMKHMenuNavigationSubsystem::Tick));
}

void UMKHMenuNavigationSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);

	if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get(); IsValid(ViewportSubsystem))
	{
		ViewportSubsystem->OnWidgetAdded.Remove(WidgetAddedHandle);
	}
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = ULocalPlayer::GetSubsystem<UMKHActiveInputDeviceSubsystem>(GetLocalPlayer()); IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &UMKHMenuNavigationSubsystem::HandleActiveInputDeviceChanged);
	}

	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (HighlightLayer.IsValid() && HighlightLayer->IsParentValid() && IsValid(LocalPlayer) && IsValid(LocalPlayer->ViewportClient))
	{
		LocalPlayer->ViewportClient->RemoveViewportWidgetContent(HighlightLayer.ToSharedRef());
	}
	HighlightLayer.Reset();

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHMenuNavigationSubsystem* UMKHMenuNavigationSubsystem::Get(const UObject* ContextObject)
{
	const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(ContextObject);
	if (const UWidget* Widget = Cast<UWidget>(ContextObject))
	{
		LocalPlayer = Widget->GetOwningLocalPlayer();
	}
	else if (const APlayerController* PlayerController = Cast<APlayerController>(ContextObject))
	{
		LocalPlayer = PlayerController->GetLocalPlayer();
	}
	return IsValid(LocalPlayer) ? LocalPlayer->GetSubsystem<UMKHMenuNavigationSubsystem>() : nullptr;
}

void UMKHMenuNavigationSubsystem::SetMenuInputActive(bool bActive)
{
	bMenuInputActive = bActive;
	bPlacementPending = true;
	if (!bActive)
	{
		ClearHighlight();
	}
}

bool UMKHMenuNavigationSubsystem::IsGamepadNavigationActive() const
{
	const UMKHActiveInputDeviceSubsystem* DeviceSubsystem = ULocalPlayer::GetSubsystem<UMKHActiveInputDeviceSubsystem>(GetLocalPlayer());
	return bMenuInputActive && IsValid(DeviceSubsystem) && DeviceSubsystem->GetActiveInputDevice() == EMKHInputDevice::Gamepad;
}

void UMKHMenuNavigationSubsystem::PushModalScreen(UUserWidget* Modal)
{
	if (!IsValid(Modal))
	{
		return;
	}
	ModalScreens.Remove(Modal);
	ModalScreens.Add(Modal);
}

void UMKHMenuNavigationSubsystem::PopModalScreen(UUserWidget* Modal)
{
	ModalScreens.Remove(Modal);
	ModalScreens.RemoveAll([](const TWeakObjectPtr<UUserWidget>& Entry) { return !Entry.IsValid(); });
}

void UMKHMenuNavigationSubsystem::RequestFocusInside(UWidget* Container)
{
	if (!IsValid(Container) || !IsGamepadNavigationActive())
	{
		return;
	}
	PendingFocusContainer = Container;
	PendingFocusAttemptsLeft = MKHMenuNavigation::PendingFocusAttempts;
}

void UMKHMenuNavigationSubsystem::ShowAnchoredToolTip(UWidget* ToolTip, UWidget* Anchor)
{
	if (HighlightLayer.IsValid() && IsValid(ToolTip) && IsValid(Anchor) && !IsGamepadNavigationActive())
	{
		HighlightLayer->SetToolTip(ToolTip->TakeWidget(), Anchor->GetCachedWidget());
	}
}

void UMKHMenuNavigationSubsystem::HideAnchoredToolTip()
{
	if (HighlightLayer.IsValid() && !IsGamepadNavigationActive())
	{
		HighlightLayer->SetToolTip(nullptr, nullptr);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

bool UMKHMenuNavigationSubsystem::Tick(float DeltaTime)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}

	EnsureHighlightLayer();
	UpdateCursorVisibility();

	// With the mouse (or under the loading screen) only the highlight goes; the screen on top is kept, so taking
	// the pad back leaves the focus where it is.
	if (!IsGamepadNavigationActive() || IsLoadingScreenUp())
	{
		if (LastFocused.IsValid() && HighlightLayer.IsValid())
		{
			HighlightLayer->SetFrameTarget(nullptr);
			HighlightLayer->SetToolTip(nullptr, nullptr);
		}
		LastFocused.Reset();
		return true;
	}

	ApplyRightStickScroll(DeltaTime);

	FocusCheckAccumulator += DeltaTime;
	if (FocusCheckAccumulator >= GetDefault<UMKHMenuNavigationSettings>()->FocusCheckInterval)
	{
		FocusCheckAccumulator = 0.f;
		UpdateFocus();
	}
	return true;
}

void UMKHMenuNavigationSubsystem::UpdateFocus()
{
	// An open combo box or context menu owns the focus until it closes.
	if (FSlateApplication::Get().AnyMenusVisible())
	{
		return;
	}

	UUserWidget* Screen = FindTopScreen();
	const bool bScreenChanged = Screen != TopScreen.Get();
	if (bScreenChanged)
	{
		TopScreen = Screen;
		bPlacementPending = true;
		PlacementAttempts = 0;
		// The last focused place belongs to the previous screen.
		bHasLastFocusedRect = false;
		if (IsValid(Screen))
		{
			ApplyNavigationBoundary(*Screen);
		}
	}
	if (!IsValid(Screen))
	{
		ClearHighlight();
		return;
	}

	if (ApplyPendingFocusRequest(*Screen))
	{
		bPlacementPending = false;
	}

	const TSharedPtr<SWidget> Focused = KeepFocusOnScreen(*Screen, bScreenChanged);
	if (Focused != LastFocused.Pin())
	{
		LastFocused = Focused;
		HandleFocusMoved(*Screen, Focused);
	}
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::KeepFocusOnScreen(UUserWidget& Screen, bool bScreenChanged)
{
	TSharedPtr<SWidget> Focused = GetFocusedWidget();
	const TSharedPtr<SWidget> Root = Screen.GetCachedWidget();
	const bool bFocusInside = MKHMenuNavigation::IsSelfOrDescendantOf(Focused, Root) && MKHMenuNavigation::IsOnScreen(Focused);

	// Focus lost on the same screen (its rows were rebuilt): give the new widgets a moment to be laid out.
	if (!bFocusInside && !bPlacementPending && bHasLastFocusedRect && FocusLostChecks < MKHMenuNavigation::FocusLostWaitChecks)
	{
		++FocusLostChecks;
		return Focused;
	}
	FocusLostChecks = 0;

	if (bPlacementPending || !bFocusInside)
	{
		PlaceFocus(Screen, bFocusInside ? Focused : nullptr, bScreenChanged);
		const TSharedPtr<SWidget> Placed = GetFocusedWidget();

		// A screen added this frame is not laid out yet and its widgets refuse the focus: try again next check.
		const IMKHNavigableScreen* NavigableScreen = Cast<IMKHNavigableScreen>(&Screen);
		const bool bWantsRoot = NavigableScreen != nullptr && NavigableScreen->WantsRootFocus();
		const bool bPlaced = bWantsRoot || (Placed != Root && MKHMenuNavigation::IsSelfOrDescendantOf(Placed, Root));
		bPlacementPending = !bPlaced && ++PlacementAttempts < MKHMenuNavigation::PlacementAttempts;
		UE_LOG(LogMKHUI, Verbose, TEXT("MenuNavigation: placed focus on %s in %s (changed %d, attempt %d, placed %d)"),
			Placed.IsValid() ? *Placed->GetTypeAsString() : TEXT("nothing"), *Screen.GetName(), bScreenChanged, PlacementAttempts, bPlaced);
		if (!bPlacementPending)
		{
			PlacementAttempts = 0;
		}
		return Placed;
	}
	// The bare root (a screen may focus itself one frame late, after our placement): move on to a real widget,
	// unless the screen reads the pad on its root.
	if (Focused == Root)
	{
		const IMKHNavigableScreen* NavigableScreen = Cast<IMKHNavigableScreen>(&Screen);
		if (NavigableScreen == nullptr || !NavigableScreen->WantsRootFocus())
		{
			PlaceFocus(Screen, Focused, false);
		}
		return GetFocusedWidget();
	}

	// The focused widget was disabled under the focus (an option arrow at the last value): Slate does not navigate
	// from a disabled widget, so the focus moves to the nearest enabled one.
	if (!MKHMenuNavigation::IsEnabledInHierarchy(Focused))
	{
		FSlateRect FocusedRect;
		const TSharedPtr<SWidget> Nearest = MKHMenuNavigation::GetAbsoluteRect(*Focused, FocusedRect)
			? FindNearestFocusable(Screen, FocusedRect, Focused) : nullptr;
		if (Nearest.IsValid())
		{
			SetFocus(Nearest);
			Focused = Nearest;
		}
	}

	// Navigation landed on a container (a list row holding buttons): go on to its first focusable widget.
	if (Focused != LastFocused.Pin())
	{
		// Same column as before when possible (right arrow of one row to the right arrow of the next).
		if (const TSharedPtr<SWidget> Inner = FindFocusableInside(Screen, Focused, bHasLastFocusedRect ? &LastFocusedRect : nullptr); Inner.IsValid())
		{
			SetFocus(Inner);
			Focused = Inner;
		}
	}
	RememberedFocus.Add(&Screen, Focused);
	bHasLastFocusedRect = MKHMenuNavigation::GetAbsoluteRect(*Focused, LastFocusedRect);
	if (bHasLastFocusedRect)
	{
		RememberedFocusRects.Add(&Screen, LastFocusedRect);
	}
	return Focused;
}

bool UMKHMenuNavigationSubsystem::ApplyPendingFocusRequest(UUserWidget& Screen)
{
	UWidget* Container = PendingFocusContainer.Get();
	if (!IsValid(Container) || PendingFocusAttemptsLeft-- <= 0)
	{
		PendingFocusContainer.Reset();
		return false;
	}

	const TSharedPtr<SWidget> Target = MKHMenuNavigation::FindFirstFocusable(Container);
	if (!MKHMenuNavigation::IsSelfOrDescendantOf(Target, Screen.GetCachedWidget()) || !MKHMenuNavigation::IsOnScreen(Target))
	{
		return false;
	}

	SetFocus(Target);
	PendingFocusContainer.Reset();
	return true;
}

void UMKHMenuNavigationSubsystem::PlaceFocus(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused, bool bScreenChanged)
{
	const TSharedPtr<SWidget> Root = Screen.GetCachedWidget();
	const IMKHNavigableScreen* NavigableScreen = Cast<IMKHNavigableScreen>(&Screen);
	if (NavigableScreen != nullptr && NavigableScreen->WantsRootFocus())
	{
		if (!Focused.IsValid())
		{
			SetFocus(Root);
		}
		return;
	}

	// A focused user widget that holds focusable widgets (the bag a screen focuses on open, a page root) is a
	// container, not a place to stay.
	const TSharedPtr<SWidget> Inner = Focused.IsValid() && Focused != Root ? FindFocusableInside(Screen, Focused, nullptr) : nullptr;
	const bool bLeafFocus = Focused.IsValid() && Focused != Root && !Inner.IsValid();

	// Same screen (the gamepad took over): the focus stays where it is.
	if (bLeafFocus && !bScreenChanged)
	{
		return;
	}

	// Back on a screen: where the player left it, before anything the screen focused by itself.
	if (const TSharedPtr<SWidget> Remembered = GetRememberedFocus(Screen); Remembered.IsValid())
	{
		if (Remembered != Focused)
		{
			SetFocus(Remembered);
		}
		return;
	}

	// The remembered widget went while another screen was on top (an item dropped from the action menu left the
	// bag): the widget now at its place, e.g. the next item of the bag.
	if (const FSlateRect* RememberedRect = RememberedFocusRects.Find(&Screen); RememberedRect != nullptr && !bLeafFocus)
	{
		if (const TSharedPtr<SWidget> AtRememberedPlace = FindNearestFocusable(Screen, *RememberedRect, nullptr); AtRememberedPlace.IsValid())
		{
			SetFocus(AtRememberedPlace);
			return;
		}
	}

	if (Inner.IsValid())
	{
		SetFocus(Inner);
		return;
	}
	if (bLeafFocus)
	{
		return;
	}

	// Same screen, focused widget gone (a page rebuilt its rows): the widget now closest to where it was.
	const TSharedPtr<SWidget> Nearest = !bScreenChanged && bHasLastFocusedRect ? FindNearestFocusable(Screen, LastFocusedRect, nullptr) : nullptr;
	const TSharedPtr<SWidget> Candidate = Nearest.IsValid() ? Nearest : FindFocusTarget(Screen);
	// The first focusable widget may be a container (the focusable bag): go on to what it holds.
	const TSharedPtr<SWidget> CandidateInner = Candidate.IsValid() && Candidate != Root ? FindFocusableInside(Screen, Candidate, nullptr) : nullptr;
	const TSharedPtr<SWidget> Target = CandidateInner.IsValid() ? CandidateInner : Candidate;
	UE_LOG(LogMKHUI, Verbose, TEXT("MenuNavigation: focus target %s (nearest %d, inner %d)"), Target.IsValid() ? *Target->GetTypeAsString() : TEXT("none"),
		Nearest.IsValid(), CandidateInner.IsValid());
	SetFocus(Target);
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::FindNearestFocusable(UUserWidget& Screen, const FSlateRect& FromRect, const TSharedPtr<SWidget>& From) const
{
	return MKHMenuNavigation::FindNearestFocusableIn(Screen.GetRootWidget(), FromRect, From);
}

bool UMKHMenuNavigationSubsystem::IsLoadingScreenUp() const
{
	const UMKHLoadingScreenSubsystem* Loading = UMKHLoadingScreenSubsystem::Get(GetLocalPlayer());
	return IsValid(Loading) && Loading->IsLoadingScreenActive();
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::FindFocusableInside(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused, const FSlateRect* NearRect) const
{
	UWidget* Container = MKHMenuNavigation::FindWidgetFor(Screen, Focused);
	if (UListViewBase* List = Cast<UListViewBase>(Container))
	{
		return FindFocusableInList(Screen, *List, NearRect);
	}
	UUserWidget* UserContainer = Cast<UUserWidget>(Container);
	if (!IsValid(UserContainer))
	{
		return nullptr;
	}
	if (const TSharedPtr<SWidget> Inner = MKHMenuNavigation::FindFocusableNear(UserContainer->GetRootWidget(), NearRect); Inner.IsValid())
	{
		return Inner;
	}

	// A focusable panel around an empty list (the bag with no items) is still a container, not a place to stay.
	UListViewBase* List = MKHMenuNavigation::FindListIn(UserContainer->GetRootWidget());
	return IsValid(List) ? FindFocusableInList(Screen, *List, NearRect) : nullptr;
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::FindFocusableInList(UUserWidget& Screen, UListViewBase& List, const FSlateRect* NearRect) const
{
	const TSharedPtr<SWidget> Row = NearRect != nullptr ? MKHMenuNavigation::FindNearestFocusableIn(&List, *NearRect, nullptr) : nullptr;
	if (Row.IsValid())
	{
		return Row;
	}
	if (const TSharedPtr<SWidget> FirstRow = MKHMenuNavigation::FindFirstFocusable(&List); FirstRow.IsValid())
	{
		return FirstRow;
	}

	// Empty list (the last item left the bag): the closest widget around it, else (not laid out yet) the first one
	// outside it.
	const TSharedPtr<SWidget> ListWidget = List.GetCachedWidget();
	FSlateRect ListRect;
	const TSharedPtr<SWidget> Nearest = ListWidget.IsValid() && MKHMenuNavigation::GetAbsoluteRect(*ListWidget, ListRect)
		? FindNearestFocusable(Screen, ListRect, ListWidget) : nullptr;
	return Nearest.IsValid() ? Nearest : MKHMenuNavigation::FindFirstFocusableExcept(Screen.GetRootWidget(), ListWidget);
}

void UMKHMenuNavigationSubsystem::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	bPlacementPending = true;
}

void UMKHMenuNavigationSubsystem::HandleViewportWidgetAdded(UWidget* Widget, ULocalPlayer* Player)
{
	if (!IsValid(Widget))
	{
		return;
	}

	// Re-adding a widget brings it to the top of its Z order, like the engine does.
	ViewportAddOrder.Add(Widget, NextAddOrder++);
	for (auto It = ViewportAddOrder.CreateIterator(); It; ++It)
	{
		if (It.Key().ResolveObjectPtr() == nullptr)
		{
			It.RemoveCurrent();
		}
	}
}

UUserWidget* UMKHMenuNavigationSubsystem::FindTopScreen() const
{
	const UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
	if (!IsValid(ViewportSubsystem))
	{
		return nullptr;
	}

	struct FCandidate
	{
		UUserWidget* Widget;
		int32 ZOrder;
		int32 AddOrder;
	};
	TArray<FCandidate> Candidates;
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	for (const TPair<TObjectKey<UWidget>, int32>& Entry : ViewportAddOrder)
	{
		UUserWidget* Widget = Cast<UUserWidget>(Entry.Key.ResolveObjectPtr());
		if (!IsValid(Widget) || !Widget->IsInViewport() || !MKHMenuNavigation::IsShown(*Widget)
			|| (Widget->GetOwningLocalPlayer() != nullptr && Widget->GetOwningLocalPlayer() != LocalPlayer))
		{
			continue;
		}
		Candidates.Add({ Widget, ViewportSubsystem->GetWidgetSlot(Widget).ZOrder, Entry.Value });
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.ZOrder != B.ZOrder ? A.ZOrder > B.ZOrder : A.AddOrder > B.AddOrder;
	});

	UUserWidget* Top = nullptr;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Candidate.Widget->IsFocusable() || MKHMenuNavigation::FindFirstFocusable(Candidate.Widget->GetRootWidget()).IsValid())
		{
			Top = Candidate.Widget;
			break;
		}
	}
	if (!IsValid(Top))
	{
		return nullptr;
	}

	for (int32 Index = ModalScreens.Num() - 1; Index >= 0; --Index)
	{
		UWidget* TopLevel = nullptr;
		UUserWidget* Modal = ModalScreens[Index].Get();
		if (IsValid(Modal) && MKHMenuNavigation::IsShownUpToTopLevel(*Modal, TopLevel) && TopLevel == Top)
		{
			return Modal;
		}
	}
	return Top;
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::HandleBoundaryNavigation(EUINavigation Direction)
{
	UUserWidget* Screen = TopScreen.Get();
	const TSharedPtr<SWidget> From = GetFocusedWidget();
	FSlateRect FromRect;
	if (!IsValid(Screen) || !From.IsValid() || !MKHMenuNavigation::GetAbsoluteRect(*From, FromRect))
	{
		return nullptr;
	}
	if (From == Screen->GetCachedWidget())
	{
		return MKHMenuNavigation::FindFirstFocusable(Screen->GetRootWidget());
	}

	TSharedPtr<SWidget> Best;
	double BestScore = TNumericLimits<double>::Max();
	MKHMenuNavigation::ForEachShownWidget(Screen->GetRootWidget(), [&](UWidget& Widget)
	{
		const TSharedPtr<SWidget> Candidate = Widget.GetCachedWidget();
		FSlateRect CandidateRect;
		double Score = 0.0;
		if (MKHMenuNavigation::IsFocusCandidate(Widget)
			&& !MKHMenuNavigation::IsSelfOrDescendantOf(From, Candidate) && !MKHMenuNavigation::IsSelfOrDescendantOf(Candidate, From)
			&& MKHMenuNavigation::GetAbsoluteRect(*Candidate, CandidateRect)
			&& MKHMenuNavigation::ScoreCandidate(FromRect, CandidateRect, Direction, Score) && Score < BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
		return true;
	});
	return Best;
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::HandleListNavigation(EUINavigation Direction)
{
	UUserWidget* Screen = TopScreen.Get();
	const TSharedPtr<SWidget> From = GetFocusedWidget();
	UWidget* FocusedWidget = IsValid(Screen) ? MKHMenuNavigation::FindWidgetFor(*Screen, From) : nullptr;
	UScrollBox* ScrollBox = nullptr;
	UPanelWidget* List = nullptr;
	UWidget* Row = nullptr;
	if (!IsValid(FocusedWidget) || !FindListRow(*FocusedWidget, ScrollBox, List, Row))
	{
		return HandleBoundaryNavigation(Direction);
	}

	FSlateRect FromRect;
	const bool bHasFromRect = MKHMenuNavigation::GetAbsoluteRect(*From, FromRect);
	const int32 Step = Direction == EUINavigation::Up ? -1 : 1;
	for (int32 Index = List->GetChildIndex(Row) + Step; Index >= 0 && Index < List->GetChildrenCount(); Index += Step)
	{
		UWidget* NextRow = List->GetChildAt(Index);
		if (const TSharedPtr<SWidget> Target = MKHMenuNavigation::FindFocusableNear(NextRow, bHasFromRect ? &FromRect : nullptr); Target.IsValid())
		{
			ScrollBox->ScrollWidgetIntoView(NextRow, true, EDescendantScrollDestination::IntoView);
			return Target;
		}
	}

	// Past the end of the list: the rest of the screen.
	return HandleBoundaryNavigation(Direction);
}

bool UMKHMenuNavigationSubsystem::FindListRow(UWidget& Focused, UScrollBox*& OutScrollBox, UPanelWidget*& OutList, UWidget*& OutRow) const
{
	// The list is the first vertical box met after leaving the row's user widget (a row has vertical boxes of its own,
	// and the list may sit in an outer one). Without a user widget row, the innermost vertical box.
	UPanelWidget* InnermostList = nullptr;
	UWidget* InnermostRow = nullptr;
	bool bLeftRowWidget = false;
	UWidget* Child = nullptr;
	for (UWidget* Current = &Focused; IsValid(Current); Child = Current, Current = MKHMenuNavigation::GetParentAcrossUserWidgets(*Current))
	{
		if (UVerticalBox* VerticalBox = Cast<UVerticalBox>(Current))
		{
			if (bLeftRowWidget && !IsValid(OutList))
			{
				OutList = VerticalBox;
				OutRow = Child;
			}
			else if (!IsValid(InnermostList))
			{
				InnermostList = VerticalBox;
				InnermostRow = Child;
			}
		}
		bLeftRowWidget |= Current->IsA<UUserWidget>();

		if (UScrollBox* ScrollBox = Cast<UScrollBox>(Current))
		{
			OutScrollBox = ScrollBox;
			if (!IsValid(OutList))
			{
				OutList = InnermostList;
				OutRow = InnermostRow;
			}
			return IsValid(OutList) && IsValid(OutRow);
		}
	}
	return false;
}

void UMKHMenuNavigationSubsystem::ApplyNavigationBoundary(UUserWidget& Screen)
{
	const TSharedPtr<SWidget> Root = Screen.GetCachedWidget();
	if (!Root.IsValid())
	{
		return;
	}

	SetNavigationRule(*Root, { EUINavigation::Left, EUINavigation::Right, EUINavigation::Up, EUINavigation::Down },
		EUINavigationRule::CustomBoundary, FNavigationDelegate::CreateUObject(this, &UMKHMenuNavigationSubsystem::HandleBoundaryNavigation));
}

void UMKHMenuNavigationSubsystem::SetNavigationRule(SWidget& Widget, std::initializer_list<EUINavigation> Directions, EUINavigationRule Rule,
	const FNavigationDelegate& Delegate)
{
	TSharedPtr<FNavigationMetaData> NavigationMetaData = Widget.GetMetaData<FNavigationMetaData>();
	if (!NavigationMetaData.IsValid())
	{
		NavigationMetaData = MakeShared<FNavigationMetaData>();
		Widget.AddMetadata(NavigationMetaData.ToSharedRef());
	}

	for (const EUINavigation Direction : Directions)
	{
		NavigationMetaData->SetNavigationCustom(Direction, Rule, Delegate);
	}
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::FindFocusTarget(UUserWidget& Screen) const
{
	if (UWidget* Desired = Screen.GetDesiredFocusWidget(); IsValid(Desired))
	{
		if (const TSharedPtr<SWidget> Target = MKHMenuNavigation::FindFirstFocusable(Desired); Target.IsValid())
		{
			return Target;
		}
	}
	if (const TSharedPtr<SWidget> Target = MKHMenuNavigation::FindFirstFocusable(Screen.GetRootWidget()); Target.IsValid())
	{
		return Target;
	}
	return Screen.IsFocusable() ? Screen.GetCachedWidget() : nullptr;
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::GetRememberedFocus(const UUserWidget& Screen) const
{
	const TWeakPtr<SWidget>* Entry = RememberedFocus.Find(&Screen);
	const TSharedPtr<SWidget> Remembered = Entry != nullptr ? Entry->Pin() : nullptr;
	if (!Remembered.IsValid() || !Remembered->SupportsKeyboardFocus() || !Remembered->IsEnabled()
		|| !MKHMenuNavigation::IsSelfOrDescendantOf(Remembered, Screen.GetCachedWidget()) || !MKHMenuNavigation::IsOnScreen(Remembered))
	{
		return nullptr;
	}
	return Remembered;
}

void UMKHMenuNavigationSubsystem::SetFocus(const TSharedPtr<SWidget>& Widget) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const TSharedPtr<const FSlateUser> SlateUser = IsValid(LocalPlayer) ? LocalPlayer->GetSlateUser() : nullptr;
	if (Widget.IsValid() && SlateUser.IsValid())
	{
		FSlateApplication::Get().SetUserFocus(SlateUser->GetUserIndex(), Widget, EFocusCause::Navigation);
	}
}

TSharedPtr<SWidget> UMKHMenuNavigationSubsystem::GetFocusedWidget() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const TSharedPtr<const FSlateUser> SlateUser = IsValid(LocalPlayer) ? LocalPlayer->GetSlateUser() : nullptr;
	return SlateUser.IsValid() ? SlateUser->GetFocusedWidget() : nullptr;
}

void UMKHMenuNavigationSubsystem::HandleFocusMoved(UUserWidget& Screen, const TSharedPtr<SWidget>& Focused)
{
	if (!HighlightLayer.IsValid())
	{
		return;
	}

	// Roots of screens get no frame (they are the whole screen), nor widgets that show their own focus.
	const bool bIsScreenRoot = Focused == Screen.GetCachedWidget();
	const bool bDrawFrame = Focused.IsValid() && !bIsScreenRoot && !Focused->GetMetaData<FMKHSelfFocusHighlightMetaData>().IsValid();
	HighlightLayer->SetFrameTarget(bDrawFrame ? Focused : nullptr);

	UWidget* FocusedWidget = bIsScreenRoot ? nullptr : MKHMenuNavigation::FindWidgetFor(Screen, Focused);
	UWidget* ToolTip = MKHMenuNavigation::FindToolTip(FocusedWidget);
	HighlightLayer->SetToolTip(IsValid(ToolTip) ? ToolTip->TakeWidget() : TSharedPtr<SWidget>(), Focused);

	ScrollTarget.Reset();
	UWidget* ScrollChild = nullptr;
	for (UWidget* Current = FocusedWidget; IsValid(Current); ScrollChild = Current, Current = MKHMenuNavigation::GetParentAcrossUserWidgets(*Current))
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(Current))
		{
			ScrollTarget = ScrollBox;
			// Up/down in a vertical list goes row by row (HandleListNavigation), not by Slate's column search.
			const TSharedPtr<SWidget> ChildWidget = IsValid(ScrollChild) ? ScrollChild->GetCachedWidget() : nullptr;
			if (ChildWidget.IsValid() && ScrollBox->GetOrientation() == Orient_Vertical)
			{
				SetNavigationRule(*ChildWidget, { EUINavigation::Up, EUINavigation::Down }, EUINavigationRule::Custom,
					FNavigationDelegate::CreateUObject(this, &UMKHMenuNavigationSubsystem::HandleListNavigation));
			}
			return;
		}
	}
	MKHMenuNavigation::ForEachShownWidget(&Screen, [this](UWidget& Widget)
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(&Widget))
		{
			ScrollTarget = ScrollBox;
			return false;
		}
		return true;
	});
}

void UMKHMenuNavigationSubsystem::ClearHighlight()
{
	bHasLastFocusedRect = false;
	if (HighlightLayer.IsValid())
	{
		HighlightLayer->SetFrameTarget(nullptr);
		HighlightLayer->SetToolTip(nullptr, nullptr);
	}
	TopScreen.Reset();
	LastFocused.Reset();
	ScrollTarget.Reset();
	bPlacementPending = true;
}

void UMKHMenuNavigationSubsystem::EnsureHighlightLayer()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* ViewportClient = IsValid(LocalPlayer) ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (!IsValid(ViewportClient) || !ViewportClient->GetGameViewportWidget().IsValid())
	{
		return;
	}

	if (!HighlightLayer.IsValid())
	{
		HighlightLayer = SNew(SMKHFocusHighlightLayer);
	}
	// Same layer as the menus (AddToViewport), above them. The viewport drops it on level change: add it back.
	if (!HighlightLayer->IsParentValid())
	{
		ViewportClient->AddViewportWidgetContent(HighlightLayer.ToSharedRef(), GetDefault<UMKHMenuNavigationSettings>()->HighlightLayerZOrder);
	}
}

void UMKHMenuNavigationSubsystem::UpdateCursorVisibility()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController = IsValid(LocalPlayer) ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (IsGamepadNavigationActive())
	{
		if (PlayerController->bShowMouseCursor)
		{
			PlayerController->SetShowMouseCursor(false);
			bCursorHiddenForGamepad = true;
		}
	}
	else if (bCursorHiddenForGamepad)
	{
		bCursorHiddenForGamepad = false;
		if (bMenuInputActive)
		{
			PlayerController->SetShowMouseCursor(true);
		}
	}
}

void UMKHMenuNavigationSubsystem::ApplyRightStickScroll(float DeltaTime) const
{
	const UMKHMenuNavigationSettings* Settings = GetDefault<UMKHMenuNavigationSettings>();
	UScrollBox* ScrollBox = ScrollTarget.Get();
	if (FMath::Abs(RightStickY) < Settings->RightStickDeadZone || !IsValid(ScrollBox))
	{
		return;
	}

	const float NewOffset = ScrollBox->GetScrollOffset() - RightStickY * Settings->RightStickScrollSpeed * DeltaTime;
	ScrollBox->SetScrollOffset(FMath::Clamp(NewOffset, 0.f, ScrollBox->GetScrollOffsetOfEnd()));
}
