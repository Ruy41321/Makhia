// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Lobby/MKHTravelErrorWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/Lobby/MKHLobbyWidgetUtils.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"

#define LOCTEXT_NAMESPACE "MKHTravelError"

// ============================================================
// Public Interface
// ============================================================

void UMKHTravelErrorWidget::SetError(const FMKHTravelErrorInfo& ErrorInfo)
{
	MKHLobbyUI::SetText(TitleText, UMKHTravelErrorLibrary::GetErrorTitle(ErrorInfo.Error));
	MKHLobbyUI::SetText(MessageText, UMKHTravelErrorLibrary::GetErrorMessage(ErrorInfo.Error));
	MKHLobbyUI::SetText(CodeText, FText::Format(LOCTEXT("ErrorCode", "Error code: {Code}"),
		FFormatNamedArguments{ { TEXT("Code"), FText::AsCultureInvariant(UMKHTravelErrorLibrary::GetErrorCode(ErrorInfo.Error)) } }));
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHTravelErrorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(OkButton))
	{
		OkButton->OnClicked.AddUniqueDynamic(this, &UMKHTravelErrorWidget::HandleOkClicked);
	}
}

void UMKHTravelErrorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UMKHTravelErrorWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		FMKHTravelErrorInfo Preview;
		Preview.Error = PreviewError;
		SetError(Preview);
	}
}

FReply UMKHTravelErrorWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Enter
		|| UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent) || UMKHMenuNavigationLibrary::IsGamepadAcceptKey(InKeyEvent))
	{
		HandleOkClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHTravelErrorWidget::HandleOkClicked()
{
	OnCloseRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
