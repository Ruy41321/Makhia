// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHSkinPanelWidget.generated.h"

class UMKHSkinTileWidget;
class UPanelWidget;
class UTextBlock;

/**
 * Appearance panel of the lobby (parent of W_SkinPanel, left side of W_LobbyOverlay): one tile per skin of the
 * catalog. Hovering a tile previews the skin on the local character, a click chooses it (saved at once and used in
 * the next match); the skin can be changed at any time. Hidden when the catalog has fewer than two skins.
 * The Blueprint owns the layout. Full model: see Documentation/CharacterSkins.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSkinPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Creates one tile per skin of the catalog in TileList. */
	void BuildTiles();

	/** Marks the tile of the chosen skin as equipped. */
	void RefreshEquipped();

	/**
	 * UMKHSkinSubsystem callback: the chosen skin changed.
	 *
	 * @param SkinId  New chosen skin.
	 */
	void HandleSelectedSkinChanged(FName SkinId);

	/**
	 * Tile callback: previews its skin.
	 *
	 * @param Tile  Hovered tile.
	 */
	void HandleTileHovered(UMKHSkinTileWidget* Tile);

	/**
	 * Tile callback: ends the preview of its skin.
	 *
	 * @param Tile  Tile the cursor left.
	 */
	void HandleTileUnhovered(UMKHSkinTileWidget* Tile);

	/**
	 * Tile callback: chooses its skin.
	 *
	 * @param Tile  Clicked tile.
	 */
	void HandleTileClicked(UMKHSkinTileWidget* Tile);

	// ============================================================
	// Properties
	// ============================================================

	/** Container of the tiles (a vertical box in W_SkinPanel). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skin Panel")
	TObjectPtr<UPanelWidget> TileList;

	/** Title of the panel ("Appearance", set by code). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skin Panel")
	TObjectPtr<UTextBlock> TitleText;

	/** Tile created for each skin (W_SkinTile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Panel")
	TSubclassOf<UMKHSkinTileWidget> TileClass;

	/** Tiles created by BuildTiles, in catalog order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHSkinTileWidget>> Tiles;

private:
	/** Handle of the UMKHSkinSubsystem::OnSelectedSkinChanged binding. */
	FDelegateHandle SelectedSkinChangedHandle;
};
