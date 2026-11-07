// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"

// ============================================================
// Public Interface
// ============================================================

bool UMKHMenuNavigationLibrary::IsGamepadNavigationActive(const UWidget* Widget)
{
	const UMKHMenuNavigationSubsystem* Navigation = UMKHMenuNavigationSubsystem::Get(Widget);
	return IsValid(Navigation) && Navigation->IsGamepadNavigationActive();
}

FVector2D UMKHMenuNavigationLibrary::GetPopupPosition(const UWidget* Anchor)
{
	if (!IsValid(Anchor))
	{
		return FVector2D::ZeroVector;
	}

	if (!IsGamepadNavigationActive(Anchor))
	{
		return UWidgetLayoutLibrary::GetMousePositionOnViewport(const_cast<UWidget*>(Anchor));
	}

	const FGeometry& Geometry = Anchor->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(const_cast<UWidget*>(Anchor), Geometry, FVector2D(Geometry.GetLocalSize().X, 0.0),
		PixelPosition, ViewportPosition);
	return ViewportPosition;
}

bool UMKHMenuNavigationLibrary::IsGamepadBackKey(const FKeyEvent& KeyEvent)
{
	return KeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right;
}

bool UMKHMenuNavigationLibrary::IsGamepadAcceptKey(const FKeyEvent& KeyEvent)
{
	return KeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom;
}
