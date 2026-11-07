// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHSkinTileWidget.generated.h"

class UImage;
class UMKHButton;
class UMKHButtonStyle;
class UTextBlock;
class UMKHSkinTileWidget;
struct FMKHSkinDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FMKHSkinTileSignature, UMKHSkinTileWidget* /*Tile*/);

/**
 * One skin of the lobby appearance panel (parent of W_SkinTile): portrait, name and an "Equipped" badge on the
 * skin in use. Reports hover and click to UMKHSkinPanelWidget, which previews and chooses the skin. The Blueprint
 * owns the layout. Full model: see Documentation/CharacterSkins.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHSkinTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows a skin: portrait and name.
	 *
	 * @param Skin  Skin of the catalog this tile stands for.
	 */
	void SetSkin(const FMKHSkinDefinition& Skin);

	/**
	 * Marks the tile as the skin in use (button style and badge).
	 *
	 * @param bInEquipped  True for the skin in use.
	 */
	void SetEquipped(bool bInEquipped);

	/** Returns the skin this tile stands for. */
	FName GetSkinId() const { return SkinId; }

	/** Broadcast when the cursor enters the tile. */
	FMKHSkinTileSignature OnTileHovered;

	/** Broadcast when the cursor leaves the tile. */
	FMKHSkinTileSignature OnTileUnhovered;

	/** Broadcast when the tile is clicked. */
	FMKHSkinTileSignature OnTileClicked;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Applies the equipped or normal look (also in the designer, through bPreviewEquipped). */
	void ApplyEquippedLook(bool bShowEquipped);

	/** Button callback: the cursor entered the tile. */
	UFUNCTION()
	void HandleHovered();

	/** Button callback: the cursor left the tile. */
	UFUNCTION()
	void HandleUnhovered();

	/** Button callback: the tile was clicked. */
	UFUNCTION()
	void HandleClicked();

	// ============================================================
	// Properties
	// ============================================================

	/** Whole tile: one MKH Button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skin Tile")
	TObjectPtr<UMKHButton> TileButton;

	/** Portrait of the skin. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skin Tile")
	TObjectPtr<UImage> PortraitImage;

	/** Name of the skin. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skin Tile")
	TObjectPtr<UTextBlock> NameText;

	/** Badge shown on the skin in use. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skin Tile")
	TObjectPtr<UWidget> EquippedBadge;

	/** Text of the badge ("Equipped", set by code). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skin Tile")
	TObjectPtr<UTextBlock> EquippedText;

	/** Button style of a skin not in use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tile")
	TObjectPtr<UMKHButtonStyle> NormalStyle;

	/** Button style of the skin in use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tile")
	TObjectPtr<UMKHButtonStyle> EquippedStyle;

	/** Designer only: previews the equipped look. */
	UPROPERTY(EditAnywhere, Category = "Skin Tile|Preview")
	bool bPreviewEquipped = false;

private:
	/** Skin this tile stands for. */
	FName SkinId;
};
