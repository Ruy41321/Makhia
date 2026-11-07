// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHSkinTileWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Engine/Texture2D.h"
#include "UI/Button/MKHButton.h"

#define LOCTEXT_NAMESPACE "MKHSkinTile"

// ============================================================
// Public Interface
// ============================================================

void UMKHSkinTileWidget::SetSkin(const FMKHSkinDefinition& Skin)
{
	SkinId = Skin.SkinId;

	if (IsValid(NameText))
	{
		NameText->SetText(Skin.DisplayName);
	}
	if (IsValid(PortraitImage))
	{
		PortraitImage->SetBrushFromTexture(Skin.Portrait);
		PortraitImage->SetVisibility(IsValid(Skin.Portrait) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UMKHSkinTileWidget::SetEquipped(const bool bInEquipped)
{
	ApplyEquippedLook(bInEquipped);
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHSkinTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(TileButton))
	{
		TileButton->OnHovered.AddDynamic(this, &UMKHSkinTileWidget::HandleHovered);
		TileButton->OnUnhovered.AddDynamic(this, &UMKHSkinTileWidget::HandleUnhovered);
		TileButton->OnClicked.AddDynamic(this, &UMKHSkinTileWidget::HandleClicked);
	}
	if (IsValid(EquippedText))
	{
		EquippedText->SetText(LOCTEXT("Equipped", "Equipped"));
	}
}

void UMKHSkinTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		ApplyEquippedLook(bPreviewEquipped);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSkinTileWidget::ApplyEquippedLook(const bool bShowEquipped)
{
	if (IsValid(TileButton))
	{
		TileButton->SetButtonStyle(bShowEquipped ? EquippedStyle : NormalStyle);
	}
	if (IsValid(EquippedBadge))
	{
		EquippedBadge->SetVisibility(bShowEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHSkinTileWidget::HandleHovered()
{
	OnTileHovered.Broadcast(this);
}

void UMKHSkinTileWidget::HandleUnhovered()
{
	OnTileUnhovered.Broadcast(this);
}

void UMKHSkinTileWidget::HandleClicked()
{
	OnTileClicked.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
