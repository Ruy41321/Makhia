// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Button/MKHButton.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Audio/MKHSoundEffectSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/MKHActiveInputDeviceSubsystem.h"
#include "UI/Button/MKHButtonStyle.h"
#include "UI/Navigation/MKHFocusHighlightMetaData.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "MKHButton"

// ============================================================
// Lifecycle
// ============================================================

void UMKHButton::SynchronizeProperties()
{
	ApplyButtonStyle();
	Super::SynchronizeProperties();
}

void UMKHButton::ReleaseSlateResources(bool bReleaseChildren)
{
	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = ULocalPlayer::GetSubsystem<UMKHActiveInputDeviceSubsystem>(GetOwningLocalPlayer()); IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.RemoveDynamic(this, &UMKHButton::HandleActiveInputDeviceChanged);
	}
	OnReceivedFocus.Unbind();
	OnLostFocus.Unbind();
	bHasFocus = false;

	Super::ReleaseSlateResources(bReleaseChildren);
}

#if WITH_EDITOR
void UMKHButton::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Mirrors the style into the saved properties too, so the designer shows the real look.
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMKHButton, ButtonStyle))
	{
		ApplyButtonStyle();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

const FText UMKHButton::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Makhia");
}
#endif

// ============================================================
// Public Interface
// ============================================================

void UMKHButton::SetButtonStyle(UMKHButtonStyle* InButtonStyle)
{
	ButtonStyle = InButtonStyle;
	ApplyButtonStyle();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

TSharedRef<SWidget> UMKHButton::RebuildWidget()
{
	// Bound on live instances only: a binding made on a designer template would be saved in the asset.
	if (IsDesignTime())
	{
		return Super::RebuildWidget();
	}

	OnHovered.AddUniqueDynamic(this, &UMKHButton::HandleHovered);
	OnUnhovered.AddUniqueDynamic(this, &UMKHButton::HandleUnhovered);
	OnClicked.AddUniqueDynamic(this, &UMKHButton::HandleClicked);
	OnReceivedFocus.BindUObject(this, &UMKHButton::HandleReceivedFocus);
	OnLostFocus.BindUObject(this, &UMKHButton::HandleLostFocus);

	if (UMKHActiveInputDeviceSubsystem* DeviceSubsystem = ULocalPlayer::GetSubsystem<UMKHActiveInputDeviceSubsystem>(GetOwningLocalPlayer()); IsValid(DeviceSubsystem))
	{
		DeviceSubsystem->OnActiveInputDeviceChanged.AddUniqueDynamic(this, &UMKHButton::HandleActiveInputDeviceChanged);
		bGamepadActive = DeviceSubsystem->GetActiveInputDevice() == EMKHInputDevice::Gamepad;
	}

	// Without an asset the designer Style is the base; captured once, since the state looks overwrite it.
	if (!bHasBaseStyle && !IsValid(ButtonStyle))
	{
		BaseStyle = GetStyle();
		bHasBaseStyle = true;
	}

	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	if (MyButton.IsValid())
	{
		MyButton->AddMetadata(MakeShared<FMKHSelfFocusHighlightMetaData>());
	}
	return Widget;
}

void UMKHButton::ApplyButtonStyle()
{
	if (IsValid(ButtonStyle))
	{
		BaseStyle = ButtonStyle->Style;
		bHasBaseStyle = true;
		SetBackgroundColor(ButtonStyle->BackgroundColor);
	}
	ApplyVisualState();
}

void UMKHButton::ApplyVisualState()
{
	if (!bHasBaseStyle)
	{
		return;
	}

	const bool bShowGamepadFocus = bGamepadActive && bHasFocus;
	FButtonStyle Style = BaseStyle;
	if (bGamepadActive)
	{
		// The hidden cursor may still sit over a button: only the focus counts as hover.
		Style.Normal = bShowGamepadFocus ? BaseStyle.Hovered : BaseStyle.Normal;
		Style.Hovered = Style.Normal;
		Style.NormalForeground = bShowGamepadFocus ? BaseStyle.HoveredForeground : BaseStyle.NormalForeground;
		Style.HoveredForeground = Style.NormalForeground;
	}
	SetStyle(Style);

	if (IsValid(ButtonStyle))
	{
		const bool bShowHoverTint = bGamepadActive ? bShowGamepadFocus : bIsHoveredForTint;
		SetColorAndOpacity(bShowHoverTint ? ButtonStyle->HoveredContentColor : ButtonStyle->ContentColor);
	}
}

void UMKHButton::UpdateGamepadHover()
{
	const bool bShouldHover = bGamepadActive && bHasFocus;
	if (bShouldHover == bGamepadHoverBroadcast)
	{
		return;
	}

	bGamepadHoverBroadcast = bShouldHover;
	if (bShouldHover)
	{
		FSlateApplication::Get().PlaySound(BaseStyle.HoveredSlateSound);
		OnHovered.Broadcast();
	}
	else
	{
		OnUnhovered.Broadcast();
	}
}

void UMKHButton::HandleHovered()
{
	bIsHoveredForTint = true;
	ApplyVisualState();

	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(this))
	{
		SoundEffects->PlayUISound(MKHGameplayTags::SFX::UIHover, this);
	}
}

void UMKHButton::HandleClicked()
{
	if (UMKHSoundEffectSubsystem* SoundEffects = UMKHSoundEffectSubsystem::Get(this))
	{
		SoundEffects->PlayUISound(MKHGameplayTags::SFX::UIClick, this);
	}
}

void UMKHButton::HandleUnhovered()
{
	bIsHoveredForTint = false;
	ApplyVisualState();
}

void UMKHButton::HandleReceivedFocus()
{
	bHasFocus = true;
	ApplyVisualState();
	UpdateGamepadHover();
}

void UMKHButton::HandleLostFocus()
{
	bHasFocus = false;
	ApplyVisualState();
	UpdateGamepadHover();
}

void UMKHButton::HandleActiveInputDeviceChanged(EMKHInputDevice NewDevice)
{
	bGamepadActive = NewDevice == EMKHInputDevice::Gamepad;
	ApplyVisualState();
	UpdateGamepadHover();
}

#undef LOCTEXT_NAMESPACE
