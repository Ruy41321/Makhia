// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/GameMenu/MKHLegalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "MKHLogChannels.h"
#include "UI/Button/MKHButton.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"
#include "UI/Text/MKHTextBlock.h"

// ============================================================
// Public Interface
// ============================================================

void UMKHLegalWidget::OpenLegal(const EMKHLegalDocument Document)
{
	SetVisibility(ESlateVisibility::Visible);
	ShowDocument(Document);

	// Modal over the menu: the gamepad focus stays inside the window until it closes.
	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->PushModalScreen(this);
	}
}

void UMKHLegalWidget::CloseLegal()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->PopModalScreen(this);
	}
}

bool UMKHLegalWidget::IsOpen() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility != ESlateVisibility::Collapsed && CurrentVisibility != ESlateVisibility::Hidden;
}

void UMKHLegalWidget::ShowDocument(const EMKHLegalDocument Document)
{
	CurrentDocument = Document;
	BuildContent(MKHLegalDocuments::GetContent(Document));
	RefreshTabs();

	if (IsValid(ContentScrollBox))
	{
		ContentScrollBox->ScrollToStart();
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHLegalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(PrivacyTabButton))
	{
		PrivacyTabButton->OnClicked.AddUniqueDynamic(this, &UMKHLegalWidget::HandlePrivacyTabClicked);
	}

	if (IsValid(EulaTabButton))
	{
		EulaTabButton->OnClicked.AddUniqueDynamic(this, &UMKHLegalWidget::HandleEulaTabClicked);
	}

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UMKHLegalWidget::HandleCloseClicked);
	}

	if (IsValid(BackdropButton))
	{
		BackdropButton->OnClicked.AddUniqueDynamic(this, &UMKHLegalWidget::HandleCloseClicked);
	}
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHLegalWidget::BuildContent(const FMKHLegalDocumentContent& Content)
{
	if (!IsValid(ContentScrollBox))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHLegalWidget::BuildContent: ContentScrollBox is missing on %s."), *GetClass()->GetName());
		return;
	}

	ContentScrollBox->ClearChildren();
	AddText(Content.Title, TitleStyle, HeadingColor, 0.0f, ETextJustify::Center);

	for (const FText& Paragraph : Content.Introduction)
	{
		AddText(Paragraph, BodyStyle, BodyColor, ParagraphSpacing);
	}

	for (const FMKHLegalSection& Section : Content.Sections)
	{
		AddText(Section.Heading, HeadingStyle, HeadingColor, SectionSpacing);
		for (const FText& Paragraph : Section.Paragraphs)
		{
			AddText(Paragraph, BodyStyle, BodyColor, ParagraphSpacing);
		}
	}
}

void UMKHLegalWidget::AddText(const FText& Text, UMKHTextStyle* Style, const FLinearColor& Color, const float TopPadding, const ETextJustify::Type Justify)
{
	if (!IsValid(WidgetTree))
	{
		return;
	}

	UMKHTextBlock* TextBlock = WidgetTree->ConstructWidget<UMKHTextBlock>(UMKHTextBlock::StaticClass());
	if (!IsValid(TextBlock))
	{
		return;
	}

	TextBlock->SetTextStyle(Style);
	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetJustification(Justify);
	TextBlock->SetAutoWrapText(true);

	if (UScrollBoxSlot* TextSlot = Cast<UScrollBoxSlot>(ContentScrollBox->AddChild(TextBlock)))
	{
		TextSlot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UMKHLegalWidget::RefreshTabs()
{
	const bool bPrivacySelected = CurrentDocument == EMKHLegalDocument::PrivacyPolicy;

	if (IsValid(PrivacyTabButton))
	{
		PrivacyTabButton->SetButtonStyle(bPrivacySelected ? SelectedTabStyle : TabStyle);
	}

	if (IsValid(EulaTabButton))
	{
		EulaTabButton->SetButtonStyle(bPrivacySelected ? TabStyle : SelectedTabStyle);
	}
}

void UMKHLegalWidget::HandlePrivacyTabClicked()
{
	ShowDocument(EMKHLegalDocument::PrivacyPolicy);
}

void UMKHLegalWidget::HandleEulaTabClicked()
{
	ShowDocument(EMKHLegalDocument::Eula);
}

void UMKHLegalWidget::HandleCloseClicked()
{
	CloseLegal();
}
