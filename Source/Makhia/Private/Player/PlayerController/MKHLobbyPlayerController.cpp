// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Player/PlayerController/MKHLobbyPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "MKHLogChannels.h"
#include "UI/HUD/MKHLobbyHUD.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Voice/MKHVoicePlayerComponent.h"

// ============================================================
// Lifecycle
// ============================================================

AMKHLobbyPlayerController::AMKHLobbyPlayerController()
{
	VoiceComponent = CreateDefaultSubobject<UMKHVoicePlayerComponent>(TEXT("VoiceComponent"));
}

void AMKHLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		ApplyMenuMappingContext();
		if (UMKHMenuNavigationSubsystem* Navigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(Navigation))
		{
			Navigation->SetMenuInputActive(true);
		}
	}
}

void AMKHLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMKHMenuNavigationSubsystem* Navigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(Navigation))
	{
		Navigation->SetMenuInputActive(false);
	}

	Super::EndPlay(EndPlayReason);
}

void AMKHLobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (IsValid(EnhancedInput) && IsValid(VoiceComponent))
	{
		VoiceComponent->SetupInput(EnhancedInput);
	}
	if (!IsValid(EnhancedInput) || !IsValid(ToggleGameMenuAction))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("AMKHLobbyPlayerController: no Enhanced Input component or ToggleGameMenuAction is not set."));
		return;
	}

	EnhancedInput->BindAction(ToggleGameMenuAction, ETriggerEvent::Started, this, &AMKHLobbyPlayerController::ToggleGameMenu);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHLobbyPlayerController::ApplyMenuMappingContext() const
{
	if (!IsValid(MenuMappingContext))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("AMKHLobbyPlayerController: MenuMappingContext is not set."));
		return;
	}

	// Registered first so the player's rebound keys replace the defaults of the context.
	if (UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(this); IsValid(UserSettings))
	{
		TSet<UInputMappingContext*> Contexts;
		Contexts.Add(MenuMappingContext);
		for (UInputMappingContext* Context : RemappableMappingContexts)
		{
			Contexts.Add(Context);
		}
		Contexts.Remove(nullptr);

		UserSettings->RegisterInputMappingContexts(Contexts);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		Subsystem->AddMappingContext(MenuMappingContext, 0);
	}
}

void AMKHLobbyPlayerController::ToggleGameMenu()
{
	if (AMKHLobbyHUD* LobbyHUD = GetHUD<AMKHLobbyHUD>(); IsValid(LobbyHUD))
	{
		LobbyHUD->ToggleGameMenu();
	}
}
