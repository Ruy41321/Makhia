// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "MKHPlayerState.generated.h"

class UMKHAttributeSet;
class UMKHAbilitySystemComponent;

/**
 * Player state of a human player. Owns the player's ability system component and attribute set
 * (Mixed replication), the NAT type the owning machine reported, shown by the network stats overlay, and the
 * skin it chose in the lobby, worn by its character on every machine (see Documentation/CharacterSkins.md).
 */
UCLASS()
class MAKHIA_API AMKHPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the ability system component and the attribute set. */
	AMKHPlayerState();

	/** Registers the replicated NAT type and skin. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Carries the NAT type and the skin over to the player state that replaces this one after a seamless travel. */
	virtual void CopyProperties(APlayerState* PlayerState) override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** IAbilitySystemInterface: returns the player's ability system component. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Returns the player's ability system component as the project type. */
	UFUNCTION(BlueprintPure)
	UMKHAbilitySystemComponent* GetRPGAbilitySystemComponent() const;

	/** Returns the player's attribute set. */
	UFUNCTION(BlueprintPure)
	UMKHAttributeSet* GetRPGAttributeSet() const;

	/** NAT type of this player's machine, as reported by it (Unknown until measured). */
	UFUNCTION(BlueprintPure, Category = "Network")
	EMKHNatType GetNatType() const { return NatType; }

	/**
	 * Server: stores the NAT type reported by the owning machine (see AMKHPlayerController::Server_ReportNatType).
	 *
	 * @param NewNatType NAT type measured by the online backend on the owning machine (Unknown when not reported).
	 */
	void SetNatType(EMKHNatType NewNatType);

	/** Skin worn by this player's character (None until the owning machine reported it). */
	UFUNCTION(BlueprintPure, Category = "Cosmetics")
	FName GetSkinId() const { return SkinId; }

	/**
	 * Server: stores the skin reported by the owning machine (see AMKHPlayerController::Server_ReportSkin) when the
	 * catalog knows it, and dresses the character.
	 *
	 * @param NewSkinId  Skin chosen on the owning machine.
	 */
	void SetSkinId(FName NewSkinId);

	/** Career levels beaten by this player, for its title: each machine turns them into text (0 until reported). */
	UFUNCTION(BlueprintPure, Category = "Career")
	int32 GetCareerLevels() const { return CareerLevels; }

	/**
	 * Server: stores the career levels reported by the owning machine (see AMKHPlayerController::Server_ReportCareerLevels),
	 * clamped to the existing ones. Cosmetic, and for now the player's word (see Documentation/OfflineModes.md).
	 *
	 * @param NewCareerLevels  Levels beaten, as reported.
	 */
	void SetCareerLevels(int32 NewCareerLevels);

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Replication callback of the skin: dresses the character on this machine. */
	UFUNCTION()
	void OnRep_SkinId();

	/** Puts SkinId on the pawn's skin component, when there is a pawn. */
	void ApplySkinToPawn() const;

	// ============================================================
	// Properties
	// ============================================================

	/** Ability system component of the player (Mixed replication). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHAbilitySystemComponent> MKHAbilitySystemComponent;

	/** Attribute set of the player. */
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMKHAttributeSet> MKHAttributeSet;

	/** NAT type of the owning machine, replicated so the opponent's overlay can show it. */
	UPROPERTY(Replicated)
	EMKHNatType NatType = EMKHNatType::Unknown;

	/** Skin chosen by the player in the lobby, replicated so every machine dresses its character. */
	UPROPERTY(ReplicatedUsing = OnRep_SkinId)
	FName SkinId;

	/** Career levels beaten by the player, replicated so the opponent's scoreboard can show its title. */
	UPROPERTY(Replicated)
	int32 CareerLevels = 0;
};
