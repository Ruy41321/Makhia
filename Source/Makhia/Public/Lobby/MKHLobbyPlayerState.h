// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MKHLobbyPlayerState.generated.h"

class AMKHLobbyPlayerState;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHLobbyPlayerInfoChangedSignature, AMKHLobbyPlayerState* /*PlayerState*/);

/**
 * Player state of the lobby (parent of BP_LobbyPawnState): replicates the name and the ready state shown on the
 * nameplates and the chosen skin, the only lobby data the server keeps per player.
 *
 * The owning machine sends its signed-in username and its ready state (UMKHLobbySubsystem) to the server, which
 * replicates them to everyone and counts the ready players. It also sends its chosen skin (UMKHSkinSubsystem),
 * signed in or not; the server keeps it only when the catalog knows it. Every machine builds its own lineup from these data
 * (UMKHLobbyLineupSubsystem). Full model: see Documentation/LobbyPresentation.md.
 */
UCLASS()
class MAKHIA_API AMKHLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	//~ Begin APlayerState Interface
	virtual void ClientInitialize(AController* C) override;
	//~ End APlayerState Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/** Returns the name shown on the nameplate (empty while the player is not signed in). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	const FString& GetLobbyDisplayName() const { return LobbyDisplayName; }

	/** Returns true when the player is ready. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	bool IsLobbyReady() const { return bLobbyReady; }

	/** Returns the skin chosen by the player (None until the owning machine sent it). */
	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	FName GetLobbySkinId() const { return LobbySkinId; }

	/** Returns how many career levels the player beat: each machine turns it into the title in its own language. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	int32 GetLobbyCareerLevels() const { return LobbyCareerLevels; }

	/** Broadcast on every machine when the name, the ready state, the skin or the career levels change. */
	FMKHLobbyPlayerInfoChangedSignature OnLobbyInfoChanged;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Owning machine only, once: sends the local name and ready state and follows their changes (signed-in players only). */
	void InitLocalLobbyInfo();

	/** Owning machine only, once: sends the chosen skin and follows its changes (signed in or not). */
	void InitLocalSkin();

	/** Returns true when this player state belongs to a local player controller. */
	bool IsOwnedByLocalPlayer() const;

	/**
	 * UMKHSkinSubsystem callback: the chosen skin changed.
	 *
	 * @param NewSkinId  New skin.
	 */
	void HandleSkinChanged(FName NewSkinId);

	/**
	 * UMKHGameInstance callback: the signed-in username changed.
	 *
	 * @param NewUsername  New username.
	 */
	UFUNCTION()
	void HandleUsernameChanged(const FString& NewUsername);

	/**
	 * UMKHLobbySubsystem callback: the local ready state changed.
	 *
	 * @param bNewReady  New ready state.
	 */
	UFUNCTION()
	void HandleReadyStateChanged(bool bNewReady);

	/** Sends the first name and ready state to the server (does not count as a ready change). */
	UFUNCTION(Server, Reliable)
	void ServerInitLobbyInfo(const FString& NewName, bool bNewReady);

	/** Sends a new name to the server. */
	UFUNCTION(Server, Reliable)
	void ServerSetLobbyName(const FString& NewName);

	/** Sends a new ready state to the server. */
	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bNewReady);

	/**
	 * Sends the career levels beaten to the server, which clamps them to the existing ones. Cosmetic, and for now the
	 * player's word: with the AppID the host reads them from Steam (see Documentation/OfflineModes.md).
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetLobbyCareerLevels(int32 NewCareerLevels);

	/** Sends the chosen skin to the server, which keeps it only when the catalog knows it. */
	UFUNCTION(Server, Reliable)
	void ServerSetLobbySkin(FName NewSkinId);

	/**
	 * Server: stores a new ready state and counts it on the host (UMKHLobbySubsystem::HandlePlayerReady) only when it
	 * really changed.
	 *
	 * @param bNewReady  New ready state.
	 */
	void ApplyLobbyReady(bool bNewReady);

	/** Replication callback of the name, the ready state, the skin and the career levels. */
	UFUNCTION()
	void OnRep_LobbyInfo();

	/** Broadcasts OnLobbyInfoChanged. */
	void BroadcastLobbyInfoChanged();

	// ============================================================
	// Properties
	// ============================================================

	/** Name shown on the nameplate. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	FString LobbyDisplayName;

	/** Ready state of the player. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	bool bLobbyReady = false;

	/** Skin chosen by the player, shown on its platform by every machine but its own (which shows its preview). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	FName LobbySkinId;

	/** Career levels beaten by the player. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	int32 LobbyCareerLevels = 0;

private:
	/** True once InitLocalLobbyInfo ran on the owning machine. */
	bool bLocalInfoInitialized = false;

	/** Handle of the UMKHSkinSubsystem::OnSelectedSkinChanged binding (owning machine only). */
	FDelegateHandle SkinChangedHandle;
};
