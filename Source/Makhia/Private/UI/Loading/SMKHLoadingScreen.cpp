// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Loading/SMKHLoadingScreen.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace MKHLoadingScreenLayout
{
	/** Same values as W_BootOverlay. */
	constexpr float DividerWidth = 220.f;
	constexpr float DividerTopPadding = 18.f;
	constexpr float DividerBottomPadding = 22.f;
	constexpr int32 ThrobberPieces = 8;
	constexpr float ThrobberPeriod = 0.75f;
	constexpr float ThrobberRadius = 12.f;
	constexpr float StatusLeftPadding = 14.f;
}

void SMKHLoadingScreen::Construct(const FArguments& InArgs, const FStyle& InStyle)
{
	using namespace MKHLoadingScreenLayout;

	// The game name is the same in every language.
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(&SolidBrush)
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(InStyle.BackgroundBrush)
			.Visibility(InStyle.BackgroundBrush ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::AsCultureInvariant(TEXT("Makhia")))
				.Font(InStyle.TitleFont)
				.ColorAndOpacity(InStyle.TitleColor)
				.TransformPolicy(InStyle.TitleTransform)
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, DividerTopPadding, 0.f, DividerBottomPadding)
			[
				SNew(SBox).WidthOverride(DividerWidth).HeightOverride(1.f)
				[
					SNew(SImage).Image(&DividerBrush).ColorAndOpacity(InStyle.DividerColor)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCircularThrobber)
					.NumPieces(ThrobberPieces)
					.Period(ThrobberPeriod)
					.Radius(ThrobberRadius)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(StatusLeftPadding, 0.f, 0.f, 0.f)
				[
					SAssignNew(StatusTextBlock, STextBlock)
					.Text(InArgs._StatusText)
					.Font(InStyle.StatusFont)
					.ColorAndOpacity(InStyle.StatusColor)
					.TransformPolicy(InStyle.StatusTransform)
				]
			]
		]
	];
}

void SMKHLoadingScreen::SetStatusText(const FText& InText)
{
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(InText);
	}
}
