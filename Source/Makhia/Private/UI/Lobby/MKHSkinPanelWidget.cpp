// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHSkinPanelWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "MKHLogChannels.h"
#include "UI/Lobby/MKHSkinTileWidget.h"

#define LOCTEXT_NAMESPACE "MKHSkinPanel"

// ============================================================
// Lifecycle
// ============================================================

void UMKHSkinPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(TitleText))
	{
		TitleText->SetText(LOCTEXT("Title", "Appearance"));
	}
	BuildTiles();
}

void UMKHSkinPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins) && !SelectedSkinChangedHandle.IsValid())
	{
		SelectedSkinChangedHandle = Skins->OnSelectedSkinChanged.AddUObject(this, &UMKHSkinPanelWidget::HandleSelectedSkinChanged);
	}
	RefreshEquipped();
}

void UMKHSkinPanelWidget::NativeDestruct()
{
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins))
	{
		Skins->OnSelectedSkinChanged.Remove(SelectedSkinChangedHandle);
		Skins->ClearPreviewSkin();
	}
	SelectedSkinChangedHandle.Reset();

	Super::NativeDestruct();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSkinPanelWidget::BuildTiles()
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const UMKHSkinCatalog* Catalog = IsValid(Skins) ? Skins->GetCatalog() : nullptr;
	if (!IsValid(Catalog) || !IsValid(TileList) || !TileClass)
	{
		UE_LOG(LogMKHUI, Warning, TEXT("%s: no skin catalog, tile list or tile class; the appearance panel stays hidden."), *GetName());
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TileList->ClearChildren();
	Tiles.Reset();
	for (const FMKHSkinDefinition& Skin : Catalog->GetSkins())
	{
		UMKHSkinTileWidget* Tile = CreateWidget<UMKHSkinTileWidget>(this, TileClass);
		if (!IsValid(Tile))
		{
			continue;
		}

		Tile->SetSkin(Skin);
		Tile->OnTileHovered.AddUObject(this, &UMKHSkinPanelWidget::HandleTileHovered);
		Tile->OnTileUnhovered.AddUObject(this, &UMKHSkinPanelWidget::HandleTileUnhovered);
		Tile->OnTileClicked.AddUObject(this, &UMKHSkinPanelWidget::HandleTileClicked);
		TileList->AddChild(Tile);
		Tiles.Add(Tile);
	}

	// A single skin leaves nothing to choose.
	SetVisibility(Tiles.Num() > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UMKHSkinPanelWidget::RefreshEquipped()
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const FName SelectedSkinId = IsValid(Skins) ? Skins->GetSelectedSkinId() : NAME_None;
	for (UMKHSkinTileWidget* Tile : Tiles)
	{
		if (IsValid(Tile))
		{
			Tile->SetEquipped(Tile->GetSkinId() == SelectedSkinId);
		}
	}
}

void UMKHSkinPanelWidget::HandleSelectedSkinChanged(FName SkinId)
{
	RefreshEquipped();
}

void UMKHSkinPanelWidget::HandleTileHovered(UMKHSkinTileWidget* Tile)
{
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins) && IsValid(Tile))
	{
		Skins->SetPreviewSkin(Tile->GetSkinId());
	}
}

void UMKHSkinPanelWidget::HandleTileUnhovered(UMKHSkinTileWidget* Tile)
{
	// The cursor may already be on another tile, whose preview must stay.
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins) && IsValid(Tile) && Skins->GetPreviewSkinId() == Tile->GetSkinId())
	{
		Skins->ClearPreviewSkin();
	}
}

void UMKHSkinPanelWidget::HandleTileClicked(UMKHSkinTileWidget* Tile)
{
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins) && IsValid(Tile))
	{
		Skins->SelectSkin(Tile->GetSkinId());
	}
}

#undef LOCTEXT_NAMESPACE
