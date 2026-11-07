// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MKHLobbyPlayerPlatform.generated.h"

class AMKHLobbyCharacterPreview;
class AMKHLobbyPlayerState;
class APlayerState;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * One slot of the lobby lineup (parent of BP_LobbyPlayerPlatform): pedestal, character preview and nameplate.
 *
 * Never replicated: UMKHLobbyLineupSubsystem tells each platform which player to show on this machine (the local
 * player always on slot 0, the central one). The platform spawns a local AMKHLobbyCharacterPreview and dresses it
 * (local player: the skin shown by UMKHSkinSubsystem, preview included; others: their replicated skin), feeds the
 * nameplate from the player's AMKHLobbyPlayerState and lights the pedestal. The components (pedestal mesh, contact
 * shadow, nameplate widget, character anchor) belong to the Blueprint and are found by name at BeginPlay.
 * Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS()
class MAKHIA_API AMKHLobbyPlayerPlatform : public AActor
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Local actor: no tick, no replication. */
	AMKHLobbyPlayerPlatform();

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a player on the platform; does nothing when that player is already shown the same way.
	 *
	 * @param PlayerState  Player to show (null empties the platform).
	 * @param bLocal       True for the local player (brighter pedestal, local nameplate frame).
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Lineup")
	void ShowPlayer(APlayerState* PlayerState, bool bLocal);

	/** Empties the platform: no character, no nameplate, dim pedestal. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Lineup")
	void ShowEmpty();

	/** Returns the lineup slot of the platform (0 = central, the local player). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Lineup")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Returns the player shown on the platform, or null. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Lineup")
	APlayerState* GetShownPlayer() const { return ShownPlayer.Get(); }

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Finds the Blueprint components by name (see the component name constants in the .cpp). */
	void ResolveComponents();

	/** Spawns the character preview on the anchor, if there is none yet. */
	void SpawnPreview();

	/** Destroys the character preview. */
	void DestroyPreview();

	/** Starts or stops following the name and ready state of the shown player. */
	void BindShownPlayer(APlayerState* PlayerState);

	/**
	 * Player state callback: the name or the ready state of the shown player changed.
	 *
	 * @param PlayerState  Player whose data changed.
	 */
	void HandleLobbyInfoChanged(AMKHLobbyPlayerState* PlayerState);

	/** Updates the nameplate, the contact shadow, the pedestal glow and the skin from the shown player. */
	void RefreshPresentation();

	/** Dresses the preview: the local player's displayed skin, or the shown player's replicated skin. */
	void RefreshPreviewSkin();

	/**
	 * UMKHSkinSubsystem callback: the skin shown on the local player's character changed (preview or choice).
	 *
	 * @param SkinId  Skin to show.
	 */
	void HandleDisplayedSkinChanged(FName SkinId);

	/** Pushes the shown player into the nameplate widget; retries shortly while the widget does not exist yet. */
	void RefreshNameplate();

	/** Returns the pedestal glow for the current state (local, other player, empty). */
	float ComputeGlow() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Lineup slot of this platform: 0 is the central one (always the local player), then 1, 2... */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Lobby|Lineup", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

	/** Character spawned on the platform (BP_LobbyCharacterPreview). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Lineup")
	TSubclassOf<AMKHLobbyCharacterPreview> PreviewClass;

	/** Pedestal material parameter driven by the platform state. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Pedestal")
	FName GlowParameter = TEXT("Glow");

	/** Pedestal glow under the local player. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Pedestal", meta = (ClampMin = "0.0"))
	float LocalGlow = 1.f;

	/** Pedestal glow under another player. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Pedestal", meta = (ClampMin = "0.0"))
	float RemoteGlow = 0.35f;

	/** Pedestal glow of an empty platform. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Pedestal", meta = (ClampMin = "0.0"))
	float EmptyGlow = 0.12f;

	/** Seconds between two attempts to reach the nameplate widget before it is created. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Nameplate", meta = (ClampMin = "0.01", Units = "s"))
	float NameplateRetryInterval = 0.1f;

	/** Character preview currently spawned (null when empty). */
	UPROPERTY(Transient)
	TObjectPtr<AMKHLobbyCharacterPreview> Preview;

	/** Pedestal mesh (Blueprint component "StaticMesh"). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PedestalMesh;

	/** Shadow under the feet, shown only with a player (Blueprint component "ContactShadow"). */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ContactShadowComponent;

	/** Nameplate widget component (Blueprint component "PlayerStatusWidget"). */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> NameplateComponent;

	/** Where the character stands (Blueprint component "PlayerSpawnTransform"). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PreviewAnchor;

private:
	/** Player shown on the platform. */
	TWeakObjectPtr<APlayerState> ShownPlayer;

	/** True when the shown player is the local one. */
	bool bShownLocal = false;

	/** Player state whose OnLobbyInfoChanged is bound. */
	TWeakObjectPtr<AMKHLobbyPlayerState> BoundPlayerState;

	/** Handle of the OnLobbyInfoChanged binding. */
	FDelegateHandle LobbyInfoChangedHandle;

	/** Retry of RefreshNameplate while the widget does not exist yet. */
	FTimerHandle NameplateRetryHandle;

	/** Handle of the UMKHSkinSubsystem::OnDisplayedSkinChanged binding. */
	FDelegateHandle DisplayedSkinChangedHandle;
};
