// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/MainHUD.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Character/CharacterBase.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/MKHGameState.h"
#include "MKHLogChannels.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "UI/HUD/GameMenu/MKHGameMenuWidget.h"
#include "UI/HUD/HUDOverlay/HUDOverlayWidget.h"
#include "UI/HUD/Inventory/InventoryDashboardWidget.h"
#include "UI/HUD/MatchOver/MatchOverWidget.h"
#include "UI/HUD/MKHOverheadBarVisibilityComponent.h"
#include "UI/HUD/Settings/MKHSettingsScreenWidget.h"
#include "UI/WidgetControllers/HUDOverlayController.h"
#include "UI/WidgetControllers/InventoryDashboardController.h"
#include "UI/WidgetControllers/MKHGameMenuController.h"

const FName AMainHUD::HideWithHUDTag(TEXT("MKH.HideWithHUD"));

AMainHUD::AMainHUD()
{
	OverheadBarVisibility = CreateDefaultSubobject<UMKHOverheadBarVisibilityComponent>(TEXT("OverheadBarVisibility"));
}

void AMainHUD::BeginPlay()
{
	Super::BeginPlay();
	
	// Every widget this HUD builds is owned by the player controller and every focus change is routed
	// through it, so without a valid one there is nothing this HUD can legally do.
	PlayerController = Cast<AMKHPlayerController>(GetOwningPlayerController());
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("AMainHUD::BeginPlay: owning player controller is not an AMKHPlayerController, HUD stays uninitialized."));
		return;
	}

	PlayerController->ChangeFocus(EFocusOption::Game);
	CreateHUDOverlayWidget();
	BindMatchOverEvent();

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMainHUD::OnActorSpawned));
	}
}

void AMainHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

UHUDOverlayController* AMainHUD::GetHUDOverlayWidgetController()
{
	if (!IsValid(HUDOverlayController))
	{
		HUDOverlayController = NewObject<UHUDOverlayController>(PlayerController, HUDOverlayControllerClass);
		HUDOverlayController->SetOwningActor(PlayerController);

		HUDOverlayController->BindCallbacksToDependencies();
	}
	return HUDOverlayController;
}

void AMainHUD::CreateHUDOverlayWidget()
{
	if (UUserWidget* Widget = CreateWidget<UHUDOverlayWidget>(PlayerController, HUDOverlayWidgetClass))
	{
		HUDOverlayWidget = Cast<UHUDOverlayWidget>(Widget);
		UHUDOverlayController* Controller = GetHUDOverlayWidgetController();
		Controller->SetHUDOverlayWidget(HUDOverlayWidget);
		Controller->BroadcastInitialValues();
		HUDOverlayWidget->AddToViewport();
		ApplyHUDOverlayOpacity();   // may be created during the match intro or while the player hides the HUD

		// The overlay's match-info sub-widget reacts only to OnMatchPhaseChanged transitions. On the
		// listen-server host the initial phase may already have been broadcast before this widget bound,
		// so replay the current phase now that it is on screen and listening.
		if (UWorld* World = GetWorld())
		{
			if (AMKHGameState* GameState = World->GetGameState<AMKHGameState>())
			{
				GameState->BroadcastCurrentPhase();
			}
		}
	}
}

void AMainHUD::SetHUDOverlayHidden(bool bHideOverlay)
{
	bOverlayHiddenByIntro = bHideOverlay;
	ApplyHUDHiddenState();
}

void AMainHUD::ToggleHUDHiddenByPlayer()
{
	bOverlayHiddenByPlayer = !bOverlayHiddenByPlayer;
	ApplyHUDHiddenState();
}

void AMainHUD::ApplyHUDHiddenState() const
{
	ApplyHUDOverlayOpacity();

	const bool bHideWidgets = IsHUDOverlayHidden();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (HidesWithHUD(**It))
		{
			SetActorWidgetsHidden(**It, bHideWidgets);
		}
	}
}

bool AMainHUD::HidesWithHUD(const AActor& Actor)
{
	return Actor.IsA<ACharacterBase>() || Actor.ActorHasTag(HideWithHUDTag);
}

void AMainHUD::SetActorWidgetsHidden(const AActor& Actor, const bool bHideWidgets)
{
	TInlineComponentArray<UWidgetComponent*> WidgetComponents(&Actor);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (IsValid(WidgetComponent))
		{
			// UpdateWidget adds/removes the screen-space widget now: the component may have stopped ticking.
			WidgetComponent->SetHiddenInGame(bHideWidgets);
			WidgetComponent->UpdateWidget();
		}
	}
}

void AMainHUD::OnActorSpawned(AActor* SpawnedActor) const
{
	if (IsValid(SpawnedActor) && IsHUDOverlayHidden() && HidesWithHUD(*SpawnedActor))
	{
		SetActorWidgetsHidden(*SpawnedActor, true);
	}
}

void AMainHUD::ApplyHUDOverlayOpacity() const
{
	if (IsValid(HUDOverlayWidget))
	{
		// Opacity, not visibility: the overlay's own widgets keep managing their visibility.
		HUDOverlayWidget->SetRenderOpacity(IsHUDOverlayHidden() ? 0.f : 1.f);
	}
}

void AMainHUD::DestroyHUDOverlayWidget()
{
	// BlueprintCallable, so it can legitimately arrive twice or before the overlay was ever created.
	if (!IsValid(HUDOverlayWidget))
	{
		return;
	}

	HUDOverlayWidget->RemoveFromParent();
	HUDOverlayWidget = nullptr;
	GetHUDOverlayWidgetController()->UnbindAllEventsFromDelegates();
	GetHUDOverlayWidgetController()->SetHUDOverlayWidget(nullptr);
}

UInventoryDashboardController* AMainHUD::GetInventoryWidgetController()
{
	if (!IsValid(InventoryDashboardController))
	{
		InventoryDashboardController = NewObject<UInventoryDashboardController>(PlayerController, InventoryDashboardControllerClass);
		InventoryDashboardController->SetOwningActor(PlayerController);

		InventoryDashboardController->BindCallbacksToDependencies();
	}

	return InventoryDashboardController;
}

void AMainHUD::ToggleGameMenu()
{
	// The settings screen sits on top of the menu, so Esc dismisses it first.
	if (IsValid(SettingsScreenWidget))
	{
		SettingsScreenWidget->RequestClose();
		return;
	}

	if (IsValid(InventoryDashboardWidget) && InventoryDashboardWidget->bIsOpen)
	{
		InventoryDashboardWidget->CloseInventoryDashboard();
		return;
	}

	// The credits and legal windows are modal over the menu: Esc closes them and leaves the menu open.
	if (IsValid(GameMenuWidget) && GameMenuWidget->IsModalWindowOpen())
	{
		GameMenuWidget->CloseModalWindows();
		return;
	}

	if (!IsValid(GameMenuWidget))
	{
		CreateGameMenuWidget();
		GameMenuWidget->OpenGameMenu();
	}
	else if (GameMenuWidget->IsOpen())
	{
		GameMenuWidget->CloseGameMenu();
	}
	else
	{
		GameMenuWidget->OpenGameMenu();
	}
}

void AMainHUD::CreateGameMenuWidget()
{
	if (UUserWidget* Widget = CreateWidget<UMKHGameMenuWidget>(PlayerController, GameMenuWidgetClass))
	{
		GameMenuWidget = Cast<UMKHGameMenuWidget>(Widget);
		UMKHGameMenuController* Controller = GetGameMenuWidgetController();
		Controller->SetGameMenuWidget(GameMenuWidget);
		BindGameMenuWidgetDelegates();
		Controller->BroadcastInitialValues();
		GameMenuWidget->AddToViewport();
	}
}

void AMainHUD::DestroyGameMenuWidget()
{
	if (IsValid(GameMenuWidget))
	{
		GameMenuWidget->DestroyGameMenu();
	}
}

UMKHGameMenuController* AMainHUD::GetGameMenuWidgetController()
{
	if (!IsValid(GameMenuController))
	{
		GameMenuController = NewObject<UMKHGameMenuController>(PlayerController, GameMenuControllerClass);
		GameMenuController->SetOwningActor(PlayerController);

		GameMenuController->BindCallbacksToDependencies();
	}
	return GameMenuController;
}

void AMainHUD::ToggleInventory()
{
	if (IsValid(GameMenuWidget) && GameMenuWidget->IsOpen())
	{
		GameMenuWidget->CloseGameMenu();
	}
	
	if (IsValid(InventoryDashboardWidget) && InventoryDashboardWidget->bIsOpen)
	{
		InventoryDashboardWidget->CloseInventoryDashboard();
		return;
	}

	if (!IsValid(InventoryDashboardWidget))
	{
		CreateInventoryWidget();
		if (!IsValid(InventoryDashboardWidget))
		{
			UE_LOG(LogMKHAbility, Warning, TEXT("AMainHUD::ToggleInventory: the inventory dashboard could not be created."));
			return;
		}
	}

	// The overlay is optional here: it only mirrors the quick-slot highlight, so a missing one
	// must not keep the dashboard from opening.
	if (IsValid(HUDOverlayWidget))
	{
		HUDOverlayWidget->OnQuickSlotActivated(MKHGameplayTags::Input::Inventory);
	}

	InventoryDashboardWidget->OpenInventoryDashboard();
}

void AMainHUD::CreateInventoryWidget()
{
	if (UUserWidget* Widget = CreateWidget<UInventoryDashboardWidget>(PlayerController, InventoryDashboardWidgetClass))
	{
		InventoryDashboardWidget = Cast<UInventoryDashboardWidget>(Widget);
		UInventoryDashboardController* Controller = GetInventoryWidgetController();
		Controller->SetInventoryWidget(InventoryDashboardWidget);
		BindInventoryWidgetDelegates();
		Controller->BroadcastInitialValues();
		InventoryDashboardWidget->AddToViewport();
	}
}

void AMainHUD::DestroyInventoryWidget()
{
	if (IsValid(InventoryDashboardWidget))
	{
		InventoryDashboardWidget->DestroyInventoryDashboard();
	}
}

void AMainHUD::BindInventoryWidgetDelegates()
{
	InventoryDashboardWidget->OnOpenInventoryDashboardDelegate.AddLambda(
		[this]()
		{
			PlayerController->ChangeFocus(EFocusOption::UI);
		});
	InventoryDashboardWidget->OnCloseInventoryDashboardDelegate.AddLambda(
		[this]()
		{
			PlayerController->ChangeFocus(EFocusOption::Game);
		});
	InventoryDashboardWidget->OnDestructionInventoryDashboardDelegate.AddLambda(
		[this]()
		{
			// This runs while the widget's destruction delegate is mid-broadcast. Clearing that delegate
			// here would free this very lambda's capture storage (use-after-free on the next line), so
			// the widget's own delegates are left alone — they die with the widget on GC.
			InventoryDashboardWidget = nullptr;

			if (IsValid(InventoryDashboardController))
			{
				InventoryDashboardController->UnbindAllEventsFromDelegates();
				InventoryDashboardController->SetInventoryWidget(nullptr);
			}
			if (IsValid(PlayerController))
			{
				PlayerController->ChangeFocus(EFocusOption::Game);
			}
		});
}

void AMainHUD::BindMatchOverEvent()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (AMKHGameState* GameState = World->GetGameState<AMKHGameState>())
	{
		GameState->OnMatchOver.AddDynamic(this, &AMainHUD::OnMatchOver);
		GameState->OnInitialPreparationStarted.AddDynamic(this, &AMainHUD::OnMatchRestarted);
		return;
	}

	// On clients the game state has usually not replicated yet when the HUD spawns, so defer the
	// binding until the world receives it.
	World->GameStateSetEvent.AddWeakLambda(this,
		[this](AGameStateBase* NewGameState)
		{
			if (AMKHGameState* MKHGameState = Cast<AMKHGameState>(NewGameState))
			{
				MKHGameState->OnMatchOver.AddDynamic(this, &AMainHUD::OnMatchOver);
				MKHGameState->OnInitialPreparationStarted.AddDynamic(this, &AMainHUD::OnMatchRestarted);
			}
		});
}

void AMainHUD::OnMatchRestarted()
{
	// No-op on the first match of a session: the panel only exists after a match has ended.
	// DestroyMatchOverWidget broadcasts the panel's destroy delegate, which restores game focus.
	DestroyMatchOverWidget();
}

void AMainHUD::OnMatchOver(ACharacterBase* WinningPlayer, bool bIsDraw)
{
	// Any menu still open would sit on top of the result panel and keep stealing the focus.
	DestroyGameMenuWidget();
	DestroyInventoryWidget();

	const APlayerState* WinningPlayerState = IsValid(WinningPlayer) ? WinningPlayer->GetPlayerState() : nullptr;

	// Comparing player states rather than pawns: the losing pawn is often already destroyed by now.
	// A winning bot has no player state, which reads as a defeat for the local player.
	const bool bLocalPlayerWon = IsValid(PlayerController)
		&& WinningPlayerState != nullptr
		&& WinningPlayerState == PlayerController->PlayerState;

	CreateMatchOverWidget();
	if (!IsValid(MatchOverWidget))
	{
		return;
	}

	MatchOverWidget->ShowMatchResult(bLocalPlayerWon, bIsDraw);
}

void AMainHUD::CreateMatchOverWidget()
{
	if (IsValid(MatchOverWidget))
	{
		return;
	}

	if (UUserWidget* Widget = CreateWidget<UMatchOverWidget>(PlayerController, MatchOverWidgetClass))
	{
		MatchOverWidget = Cast<UMatchOverWidget>(Widget);
		BindMatchOverWidgetDelegates();
		MatchOverWidget->AddToViewport();
	}
}

void AMainHUD::DestroyMatchOverWidget()
{
	if (IsValid(MatchOverWidget))
	{
		MatchOverWidget->DestroyMatchOverPanel();
	}
}

void AMainHUD::BindMatchOverWidgetDelegates()
{
	MatchOverWidget->FOnShowMatchOverDelegate.AddLambda(
		[this]()
		{
			PlayerController->ChangeFocus(EFocusOption::UI);
		});
	MatchOverWidget->FOnDestroyMatchOverDelegate.AddLambda(
		[this]()
		{
			// Never clear the panel's delegates from here: this lambda is stored inside the delegate
			// currently broadcasting, and clearing it frees this lambda's own capture (use-after-free).
			MatchOverWidget = nullptr;

			if (IsValid(PlayerController))
			{
				PlayerController->ChangeFocus(EFocusOption::Game);
			}
		});
}

void AMainHUD::BindGameMenuWidgetDelegates()
{
	GameMenuWidget->FOnOpenGameMenuDelegate.AddLambda(
		[this]()
		{
			PlayerController->ChangeFocus(EFocusOption::UI);
		});
	GameMenuWidget->FOnCloseGameMenuDelegate.AddLambda(
		[this]()
		{
			PlayerController->ChangeFocus(EFocusOption::Game);
		});
	GameMenuWidget->FOnDestroyGameMenuDelegate.AddLambda(
		[this]()
		{
			// Never clear the menu's delegates from here: this lambda is stored inside the delegate
			// currently broadcasting, and clearing it frees this lambda's own capture (use-after-free).
			GameMenuWidget = nullptr;

			if (IsValid(GameMenuController))
			{
				GameMenuController->UnbindAllEventsFromDelegates();
				GameMenuController->SetGameMenuWidget(nullptr);
			}
			if (IsValid(PlayerController))
			{
				PlayerController->ChangeFocus(EFocusOption::Game);
			}
		});
}

// ============================================================
// Settings Screen
// ============================================================

void AMainHUD::CreateSettingsWidget()
{
	if (IsValid(SettingsScreenWidget))
	{
		return;
	}

	if (!IsValid(SettingsScreenWidgetClass))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("MainHUD: SettingsScreenWidgetClass is not set."));
		return;
	}

	SettingsScreenWidget = CreateWidget<UMKHSettingsScreenWidget>(PlayerController, SettingsScreenWidgetClass);
	if (!IsValid(SettingsScreenWidget))
	{
		return;
	}

	SettingsScreenWidget->OnSettingsClosed.AddUObject(this, &AMainHUD::OnSettingsClosed);
	SettingsScreenWidget->AddToViewport();
}

void AMainHUD::DestroySettingsWidget()
{
	if (!IsValid(SettingsScreenWidget))
	{
		return;
	}

	SettingsScreenWidget->OnSettingsClosed.RemoveAll(this);
	SettingsScreenWidget->RemoveFromParent();
	SettingsScreenWidget = nullptr;
}

void AMainHUD::ToggleSettingsWidget()
{
	if (IsValid(SettingsScreenWidget))
	{
		DestroySettingsWidget();
	}
	else
	{
		CreateSettingsWidget();
	}
}

void AMainHUD::OnSettingsClosed()
{
	DestroySettingsWidget();

	// Leaving the settings hands the player back to gameplay rather than to the menu underneath.
	// CloseGameMenu already restores the game focus through FOnCloseGameMenuDelegate.
	if (IsValid(GameMenuWidget) && GameMenuWidget->IsOpen())
	{
		GameMenuWidget->CloseGameMenu();
	}
}
