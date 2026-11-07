// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Text/MKHTextBlock.h"

#include "UI/Text/MKHTextStyle.h"

#define LOCTEXT_NAMESPACE "MKHTextBlock"

void UMKHTextBlock::SynchronizeProperties()
{
	ApplyTextStyle();
	Super::SynchronizeProperties();
}

#if WITH_EDITOR
void UMKHTextBlock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Mirrors the style into the saved Font too, so the designer shows the real values.
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UMKHTextBlock, TextStyle))
	{
		ApplyTextStyle();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

const FText UMKHTextBlock::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Makhia");
}
#endif

void UMKHTextBlock::SetTextStyle(UMKHTextStyle* InTextStyle)
{
	TextStyle = InTextStyle;
	ApplyTextStyle();
}

void UMKHTextBlock::ApplyTextStyle()
{
	if (!IsValid(TextStyle))
	{
		return;
	}

	SetFont(TextStyle->Font);
	SetTextTransformPolicy(TextStyle->TextTransform);
}

#undef LOCTEXT_NAMESPACE
