// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

/** Helpers shared by the lobby widgets, which refresh every tick and must touch a widget only when it changes. */
namespace MKHLobbyUI
{
	/** Shows or collapses a widget, touching it only when its visibility changes. */
	inline void SetShown(UWidget* Widget, const bool bShown, const ESlateVisibility ShownVisibility = ESlateVisibility::Visible)
	{
		const ESlateVisibility Wanted = bShown ? ShownVisibility : ESlateVisibility::Collapsed;
		if (IsValid(Widget) && Widget->GetVisibility() != Wanted)
		{
			Widget->SetVisibility(Wanted);
		}
	}

	/** Enables or disables a widget, touching it only when its state changes. */
	inline void SetEnabled(UWidget* Widget, const bool bEnabled)
	{
		if (IsValid(Widget) && Widget->GetIsEnabled() != bEnabled)
		{
			Widget->SetIsEnabled(bEnabled);
		}
	}

	/** Sets a text, touching the widget only when the displayed string changes. */
	inline void SetText(UTextBlock* TextBlock, const FText& Text)
	{
		if (IsValid(TextBlock) && !TextBlock->GetText().EqualTo(Text))
		{
			TextBlock->SetText(Text);
		}
	}
}
