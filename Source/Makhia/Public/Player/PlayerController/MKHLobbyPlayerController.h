// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MKHLobbyPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UMKHVoicePlayerComponent;

/**
 * Player controller of the lobby level.
 *
 * Gives the lobby the same Enhanced Input path as a match for the game menu: the menu mapping
 * context is registered with the input user settings, so the key the player rebound in
 * Settings -> Keyboard opens the menu here too. Login, matchmaking and the lobby overlay stay in
 * the Blueprint (BP_LobbyController). The lobby is always a menu, so the gamepad menu navigation
 * (UMKHMenuNavigationSubsystem) is active for the whole level.
 */
UCLASS()
class MAKHIA_API AMKHLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Creates the voice component. */
	AMKHLobbyPlayerController();

protected:

	//~ Begin APlayerController Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	//~ End APlayerController Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Registers every remappable context with the user settings and applies the menu one to the local player. */
	void ApplyMenuMappingContext() const;

	/** ToggleGameMenuAction handler: forwards to the lobby HUD. */
	void ToggleGameMenu();

	// ============================================================
	// Properties
	// ============================================================

	/** Mapping context active in the lobby; must map ToggleGameMenuAction (IMC_UIOpenMapping). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MenuMappingContext;

	/**
	 * Every context of the match (gameplay, UI open, dead), registered with the user settings only so the
	 * Keyboard settings page lists all the remappable actions from the lobby too. Never applied here.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> RemappableMappingContexts;

	/** Action that opens and closes the game menu (IA_ToggleGameMenu). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleGameMenuAction;

	/** Voice chat link with the server (choices sent, server decisions received). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<UMKHVoicePlayerComponent> VoiceComponent;
};
