// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHSkinSubsystem.generated.h"

class UMKHSkinCatalog;
class UMKHSkinSaveGame;
struct FMKHSkinDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHSkinChangedSignature, FName /*SkinId*/);

/**
 * Skins of this machine's player: the catalog (UMKHDataSubsystem::GetSkinCatalog), the skin chosen in the lobby,
 * saved in the "Skin" slot and restored at start-up, and the skin previewed while hovering the appearance panel.
 *
 * Only the chosen skin leaves the machine: the lobby player state and, in a match, the player controller send it
 * to the server, which validates it with ResolveSkinId and replicates it. The preview stays local.
 * Full model: see Documentation/CharacterSkins.md.
 */
UCLASS()
class MAKHIA_API UMKHSkinSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the skin subsystem of the game instance of a context object.
	 *
	 * @param WorldContextObject  Any object living in a world owned by a game instance.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHSkinSubsystem* Get(const UObject* WorldContextObject);

	/** Returns the skin catalog, or null when none is configured. */
	const UMKHSkinCatalog* GetCatalog() const;

	/**
	 * Finds a skin of the catalog.
	 *
	 * @param SkinId  Identifier of the skin.
	 * @return The skin, or null when unknown. Do not store it.
	 */
	const FMKHSkinDefinition* FindSkin(FName SkinId) const;

	/**
	 * Returns the identifier itself when the catalog knows it, otherwise the default skin's one (server validation
	 * of the identifiers sent by clients).
	 *
	 * @param SkinId  Identifier to check.
	 */
	UFUNCTION(BlueprintPure, Category = "Cosmetics|Skin")
	FName ResolveSkinId(FName SkinId) const;

	/** Returns the skin chosen on this machine (always a skin of the catalog, None without catalog). */
	UFUNCTION(BlueprintPure, Category = "Cosmetics|Skin")
	FName GetSelectedSkinId() const { return SelectedSkinId; }

	/**
	 * Chooses the skin of this machine's player, saves it and ends any preview.
	 *
	 * @param SkinId  Skin to wear; unknown identifiers are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics|Skin")
	void SelectSkin(FName SkinId);

	/**
	 * Shows a skin on this machine's lobby character without choosing it (hover in the appearance panel).
	 *
	 * @param SkinId  Skin to preview; unknown identifiers are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics|Skin")
	void SetPreviewSkin(FName SkinId);

	/** Ends the preview: the lobby character shows the chosen skin again. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics|Skin")
	void ClearPreviewSkin();

	/** Returns the skin previewed now, None when there is no preview. */
	UFUNCTION(BlueprintPure, Category = "Cosmetics|Skin")
	FName GetPreviewSkinId() const { return PreviewSkinId; }

	/** Returns the skin to show on this machine's lobby character: the preview, or else the chosen skin. */
	UFUNCTION(BlueprintPure, Category = "Cosmetics|Skin")
	FName GetDisplayedSkinId() const { return PreviewSkinId.IsNone() ? SelectedSkinId : PreviewSkinId; }

	/** Broadcast when the chosen skin changes (sent to the server by the lobby player state). */
	FMKHSkinChangedSignature OnSelectedSkinChanged;

	/** Broadcast when the skin to show on this machine's lobby character changes (preview or choice). */
	FMKHSkinChangedSignature OnDisplayedSkinChanged;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Reads the "Skin" save slot; an unknown or missing skin falls back to the default one. */
	void LoadSelection();

	/** Writes the chosen skin to the "Skin" save slot. */
	void SaveSelection() const;

	/**
	 * Broadcasts OnDisplayedSkinChanged when the displayed skin is no longer the previous one.
	 *
	 * @param PreviouslyDisplayed  Skin displayed before the change.
	 */
	void BroadcastDisplayedChange(FName PreviouslyDisplayed);

	// ============================================================
	// Properties
	// ============================================================

	/** Skin chosen on this machine. */
	FName SelectedSkinId;

	/** Skin previewed on this machine, None without preview. */
	FName PreviewSkinId;
};
