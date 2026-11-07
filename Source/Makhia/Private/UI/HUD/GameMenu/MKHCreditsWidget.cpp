// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/MKHCreditsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "MKHLogChannels.h"
#include "UI/HUD/GameMenu/MKHCreditsData.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"
#include "UI/Text/MKHTextBlock.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHCreditsWidget::OpenCredits()
{
	SetVisibility(ESlateVisibility::Visible);

	if (IsValid(ContentScrollBox))
	{
		ContentScrollBox->ScrollToStart();
	}

	SecondsSinceOpened = 0.0f;
	bAutoScrollActive = AutoScrollSpeed > 0.0f;

	// Modal over the menu: the gamepad focus stays inside the window until it closes.
	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->PushModalScreen(this);
	}
}

void UMKHCreditsWidget::CloseCredits()
{
	bAutoScrollActive = false;
	SetVisibility(ESlateVisibility::Collapsed);

	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->PopModalScreen(this);
	}
}

bool UMKHCreditsWidget::IsOpen() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility != ESlateVisibility::Collapsed && CurrentVisibility != ESlateVisibility::Hidden;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHCreditsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UMKHCreditsWidget::HandleCloseClicked);
	}

	if (IsValid(BackdropButton))
	{
		BackdropButton->OnClicked.AddUniqueDynamic(this, &UMKHCreditsWidget::HandleCloseClicked);
	}

	if (IsValid(ContentScrollBox))
	{
		ContentScrollBox->OnUserScrolled.AddUniqueDynamic(this, &UMKHCreditsWidget::HandleUserScrolled);
	}

	BuildContent();
}

void UMKHCreditsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TickAutoScroll(InDeltaTime);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHCreditsWidget::BuildContent()
{
	if (!IsValid(ContentScrollBox))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHCreditsWidget::BuildContent: ContentScrollBox is missing."));
		return;
	}

	ContentScrollBox->ClearChildren();

	if (!IsValid(CreditsData))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHCreditsWidget::BuildContent: no CreditsData assigned on %s."), *GetClass()->GetName());
		return;
	}

	for (int32 SectionIndex = 0; SectionIndex < CreditsData->Sections.Num(); ++SectionIndex)
	{
		AddSection(CreditsData->Sections[SectionIndex], SectionIndex == 0);
	}

	AddLegalNotices();
}

void UMKHCreditsWidget::AddSection(const FMKHCreditsSection& Section, const bool bIsFirstSection)
{
	UMKHTextBlock* TitleText = CreateText(Section.Title, SectionTitleStyle, SectionTitleColor, ETextJustify::Center);
	if (!IsValid(TitleText))
	{
		return;
	}

	if (UScrollBoxSlot* TitleSlot = Cast<UScrollBoxSlot>(ContentScrollBox->AddChild(TitleText)))
	{
		TitleSlot->SetPadding(FMargin(0.0f, bIsFirstSection ? 0.0f : SectionSpacing, 0.0f, SectionTitleSpacing));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	for (const FMKHCreditsLine& Line : Section.Lines)
	{
		AddLine(Line);
	}
}

void UMKHCreditsWidget::AddLine(const FMKHCreditsLine& Line)
{
	// A line without a role is a name on its own (e.g. the developer), shown centred.
	if (Line.Role.IsEmpty())
	{
		UMKHTextBlock* NameText = CreateText(Line.Name, NameStyle, NameColor, ETextJustify::Center);
		if (UScrollBoxSlot* NameSlot = IsValid(NameText) ? Cast<UScrollBoxSlot>(ContentScrollBox->AddChild(NameText)) : nullptr)
		{
			NameSlot->SetPadding(FMargin(0.0f, LineSpacing, 0.0f, 0.0f));
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UMKHTextBlock* RoleText = CreateText(Line.Role, RoleStyle, RoleColor, ETextJustify::Left);
	UMKHTextBlock* NameText = CreateText(Line.Name, NameStyle, NameColor, ETextJustify::Right);
	if (!IsValid(Row) || !IsValid(RoleText) || !IsValid(NameText))
	{
		return;
	}

	if (UHorizontalBoxSlot* RoleSlot = Row->AddChildToHorizontalBox(RoleText))
	{
		RoleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RoleSlot->SetVerticalAlignment(VAlign_Center);
		RoleSlot->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
	}

	if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(ContentScrollBox->AddChild(Row)))
	{
		RowSlot->SetPadding(FMargin(0.0f, LineSpacing, 0.0f, 0.0f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UMKHCreditsWidget::AddLegalNotices()
{
	for (int32 NoticeIndex = 0; NoticeIndex < CreditsData->LegalNotices.Num(); ++NoticeIndex)
	{
		UMKHTextBlock* NoticeText = CreateText(CreditsData->LegalNotices[NoticeIndex], LegalStyle, LegalColor, ETextJustify::Center);
		if (!IsValid(NoticeText))
		{
			continue;
		}

		NoticeText->SetAutoWrapText(true);

		if (UScrollBoxSlot* NoticeSlot = Cast<UScrollBoxSlot>(ContentScrollBox->AddChild(NoticeText)))
		{
			NoticeSlot->SetPadding(FMargin(0.0f, NoticeIndex == 0 ? LegalSpacing : LineSpacing * 2.0f, 0.0f, 0.0f));
			NoticeSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

UMKHTextBlock* UMKHCreditsWidget::CreateText(const FText& Text, UMKHTextStyle* Style, const FLinearColor& Color, const ETextJustify::Type Justify)
{
	if (!IsValid(WidgetTree))
	{
		return nullptr;
	}

	UMKHTextBlock* TextBlock = WidgetTree->ConstructWidget<UMKHTextBlock>(UMKHTextBlock::StaticClass());
	if (!IsValid(TextBlock))
	{
		return nullptr;
	}

	TextBlock->SetTextStyle(Style);
	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetJustification(Justify);
	return TextBlock;
}

void UMKHCreditsWidget::TickAutoScroll(const float DeltaTime)
{
	if (!bAutoScrollActive || !IsValid(ContentScrollBox))
	{
		return;
	}

	SecondsSinceOpened += DeltaTime;
	if (SecondsSinceOpened < AutoScrollDelay)
	{
		return;
	}

	const float EndOffset = ContentScrollBox->GetScrollOffsetOfEnd();
	const float NextOffset = FMath::Min(ContentScrollBox->GetScrollOffset() + AutoScrollSpeed * DeltaTime, EndOffset);
	ContentScrollBox->SetScrollOffset(NextOffset);

	if (NextOffset >= EndOffset)
	{
		bAutoScrollActive = false;
	}
}

void UMKHCreditsWidget::HandleCloseClicked()
{
	CloseCredits();
}

void UMKHCreditsWidget::HandleUserScrolled(float CurrentOffset)
{
	bAutoScrollActive = false;
}
