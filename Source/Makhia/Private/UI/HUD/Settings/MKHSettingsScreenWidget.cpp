// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Settings/MKHSettingsScreenWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "MKHLogChannels.h"
#include "UI/HUD/Settings/MKHSettingsNavButtonWidget.h"
#include "UI/HUD/Settings/MKHSettingsPageWidget.h"
#include "UI/Navigation/MKHMenuNavigationLibrary.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHSettingsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildPages();
	BuildNavigationRail();
	BindActionBar();

	ShowPage(SettingsPages.IsValidIndex(DefaultPageIndex) ? DefaultPageIndex : 0);

	// Focusable so clicks inside the panel keep the keyboard focus here rather than handing it
	// back to the viewport; the screen itself never acts on focus changes.
	SetIsFocusable(true);
	SetKeyboardFocus();
}

void UMKHSettingsScreenWidget::NativeDestruct()
{
	// Only Apply persists: anything still pending when the screen goes away is rolled back.
	DiscardPendingChanges();

	UnbindAll();

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

FReply UMKHSettingsScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Only reached when no child handled the click, i.e. the player hit empty space in the panel.
	// Keeping the focus here stops stray clicks from moving it out to the viewport.
	if (const TSharedPtr<SWidget> Self = GetCachedWidget(); Self.IsValid())
	{
		return FReply::Handled().SetUserFocus(Self.ToSharedRef(), EFocusCause::Mouse);
	}

	return FReply::Handled();
}

FReply UMKHSettingsScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UMKHMenuNavigationLibrary::IsGamepadBackKey(InKeyEvent))
	{
		RequestClose();
		return FReply::Handled();
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
	{
		ShowAdjacentPage(Key == EKeys::Gamepad_RightShoulder ? 1 : -1);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UMKHSettingsScreenWidget::RequestClose()
{
	const UMKHSettingsPageWidget* ActivePage = GetActivePage();
	if (IsValid(ActivePage) && ActivePage->IsBlockingClose())
	{
		return false;
	}

	CloseSettings();
	return true;
}

void UMKHSettingsScreenWidget::ShowPage(int32 PageIndex)
{
	if (!PageInstances.IsValidIndex(PageIndex) || PageIndex == ActivePageIndex)
	{
		return;
	}

	if (UMKHSettingsPageWidget* PreviousPage = GetActivePage(); IsValid(PreviousPage))
	{
		PreviousPage->DeactivatePage();
	}

	ActivePageIndex = PageIndex;

	if (IsValid(PageSwitcher))
	{
		PageSwitcher->SetActiveWidgetIndex(PageIndex);
	}

	if (UMKHSettingsPageWidget* NewPage = GetActivePage(); IsValid(NewPage))
	{
		NewPage->ActivatePage();
	}

	for (int32 Index = 0; Index < NavButtons.Num(); ++Index)
	{
		if (IsValid(NavButtons[Index]))
		{
			NavButtons[Index]->SetSelected(Index == PageIndex);
		}
	}

	RefreshHeader();
	RefreshActionBar();
}

void UMKHSettingsScreenWidget::ApplyActivePage()
{
	if (UMKHSettingsPageWidget* ActivePage = GetActivePage(); IsValid(ActivePage))
	{
		ActivePage->ApplyChanges();
		RefreshActionBar();
	}
}

void UMKHSettingsScreenWidget::RevertActivePage()
{
	if (UMKHSettingsPageWidget* ActivePage = GetActivePage(); IsValid(ActivePage))
	{
		ActivePage->RevertChanges();
		RefreshActionBar();
	}
}

void UMKHSettingsScreenWidget::ResetActivePageToDefaults()
{
	if (UMKHSettingsPageWidget* ActivePage = GetActivePage(); IsValid(ActivePage))
	{
		ActivePage->ResetToDefaults();
		RefreshActionBar();
	}
}

void UMKHSettingsScreenWidget::CloseSettings()
{
	DiscardPendingChanges();

	OnSettingsClosed.Broadcast();
}

void UMKHSettingsScreenWidget::DiscardPendingChanges()
{
	// Rebinds are written to the live profile as they happen so the player can feel them, so a
	// page left dirty has to be rolled back to its saved state rather than simply not saved.
	if (UMKHSettingsPageWidget* ActivePage = GetActivePage(); IsValid(ActivePage) && ActivePage->HasPendingChanges())
	{
		ActivePage->RevertChanges();
	}
}

UMKHSettingsPageWidget* UMKHSettingsScreenWidget::GetActivePage() const
{
	return PageInstances.IsValidIndex(ActivePageIndex) ? PageInstances[ActivePageIndex].Get() : nullptr;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSettingsScreenWidget::BuildPages()
{
	if (!IsValid(PageSwitcher))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("SettingsScreen: PageSwitcher is not bound, no page can be shown."));
		return;
	}

	for (const TSubclassOf<UMKHSettingsPageWidget>& PageClass : SettingsPages)
	{
		if (!IsValid(PageClass))
		{
			UE_LOG(LogMKHInput, Warning, TEXT("SettingsScreen: a null entry in SettingsPages was skipped."));
			continue;
		}

		UMKHSettingsPageWidget* Page = CreateWidget<UMKHSettingsPageWidget>(GetOwningPlayer(), PageClass);
		if (!IsValid(Page))
		{
			continue;
		}

		Page->OnPageDirtyStateChanged.AddUObject(this, &UMKHSettingsScreenWidget::HandlePageDirtyStateChanged);

		// Runtime slots start centred on their content; the page must span the whole switcher.
		if (UWidgetSwitcherSlot* PageSlot = Cast<UWidgetSwitcherSlot>(PageSwitcher->AddChild(Page)); PageSlot != nullptr)
		{
			PageSlot->SetHorizontalAlignment(HAlign_Fill);
			PageSlot->SetVerticalAlignment(VAlign_Fill);
		}

		PageInstances.Add(Page);
	}
}

void UMKHSettingsScreenWidget::BuildNavigationRail()
{
	if (!IsValid(NavButtonContainer) || !IsValid(NavButtonClass))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("SettingsScreen: navigation rail not built (missing container or NavButtonClass)."));
		return;
	}

	for (int32 Index = 0; Index < PageInstances.Num(); ++Index)
	{
		UMKHSettingsNavButtonWidget* NavButton = CreateWidget<UMKHSettingsNavButtonWidget>(GetOwningPlayer(), NavButtonClass);
		if (!IsValid(NavButton))
		{
			continue;
		}

		NavButton->SetupNavButton(Index, PageInstances[Index]->GetPageDisplayName());
		NavButton->OnNavButtonClicked.AddUObject(this, &UMKHSettingsScreenWidget::HandleNavButtonClicked);

		if (UVerticalBoxSlot* NavSlot = Cast<UVerticalBoxSlot>(NavButtonContainer->AddChild(NavButton)); NavSlot != nullptr)
		{
			NavSlot->SetHorizontalAlignment(HAlign_Fill);
			NavSlot->SetPadding(NavButtonSpacing);
		}

		NavButtons.Add(NavButton);
	}
}

void UMKHSettingsScreenWidget::RefreshActionBar()
{
	const UMKHSettingsPageWidget* ActivePage = GetActivePage();
	const bool bUsesActionBar = IsValid(ActivePage) && ActivePage->UsesActionBar();
	const bool bHasChanges = bUsesActionBar && ActivePage->HasPendingChanges();

	// Collapsed rather than hidden, so Back does not sit next to a gap on pages without them.
	const ESlateVisibility ButtonVisibility = bUsesActionBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (IsValid(DefaultsButton))
	{
		DefaultsButton->SetVisibility(ButtonVisibility);
	}
	if (IsValid(ApplyButton))
	{
		ApplyButton->SetVisibility(ButtonVisibility);
		ApplyButton->SetIsEnabled(bHasChanges);
	}
	if (IsValid(RevertButton))
	{
		RevertButton->SetVisibility(ButtonVisibility);
		RevertButton->SetIsEnabled(bHasChanges);
	}

	if (IsValid(DirtyStateText))
	{
		DirtyStateText->SetVisibility(ButtonVisibility);
		DirtyStateText->SetText(bHasChanges ? UnsavedChangesText : NoChangesText);
		DirtyStateText->SetColorAndOpacity(bHasChanges ? DirtyFooterColor : IdleFooterColor);
	}
}

void UMKHSettingsScreenWidget::RefreshHeader() const
{
	if (!IsValid(PageTitleText))
	{
		return;
	}

	const UMKHSettingsPageWidget* ActivePage = GetActivePage();
	PageTitleText->SetText(IsValid(ActivePage) ? ActivePage->GetPageDisplayName() : FText::GetEmpty());
}

void UMKHSettingsScreenWidget::ShowAdjacentPage(int32 Step)
{
	const int32 PageCount = PageInstances.Num();
	if (PageCount == 0)
	{
		return;
	}

	ShowPage((ActivePageIndex + Step + PageCount) % PageCount);
	if (UMKHMenuNavigationSubsystem* MenuNavigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(MenuNavigation))
	{
		MenuNavigation->RequestFocusInside(GetActivePage());
	}
}

void UMKHSettingsScreenWidget::HandleNavButtonClicked(int32 PageIndex)
{
	ShowPage(PageIndex);
}

void UMKHSettingsScreenWidget::HandleApplyClicked()
{
	ApplyActivePage();
}

void UMKHSettingsScreenWidget::HandleRevertClicked()
{
	RevertActivePage();
}

void UMKHSettingsScreenWidget::HandleDefaultsClicked()
{
	ResetActivePageToDefaults();
}

void UMKHSettingsScreenWidget::HandleBackClicked()
{
	CloseSettings();
}

void UMKHSettingsScreenWidget::HandlePageDirtyStateChanged()
{
	RefreshActionBar();
}

void UMKHSettingsScreenWidget::BindActionBar()
{
	if (IsValid(ApplyButton))
	{
		ApplyButton->OnClicked.AddDynamic(this, &UMKHSettingsScreenWidget::HandleApplyClicked);
	}
	if (IsValid(RevertButton))
	{
		RevertButton->OnClicked.AddDynamic(this, &UMKHSettingsScreenWidget::HandleRevertClicked);
	}
	if (IsValid(DefaultsButton))
	{
		DefaultsButton->OnClicked.AddDynamic(this, &UMKHSettingsScreenWidget::HandleDefaultsClicked);
	}
	if (IsValid(BackButton))
	{
		BackButton->OnClicked.AddDynamic(this, &UMKHSettingsScreenWidget::HandleBackClicked);
	}
}

void UMKHSettingsScreenWidget::UnbindAll()
{
	if (IsValid(ApplyButton))
	{
		ApplyButton->OnClicked.RemoveDynamic(this, &UMKHSettingsScreenWidget::HandleApplyClicked);
	}
	if (IsValid(RevertButton))
	{
		RevertButton->OnClicked.RemoveDynamic(this, &UMKHSettingsScreenWidget::HandleRevertClicked);
	}
	if (IsValid(DefaultsButton))
	{
		DefaultsButton->OnClicked.RemoveDynamic(this, &UMKHSettingsScreenWidget::HandleDefaultsClicked);
	}
	if (IsValid(BackButton))
	{
		BackButton->OnClicked.RemoveDynamic(this, &UMKHSettingsScreenWidget::HandleBackClicked);
	}

	for (const TObjectPtr<UMKHSettingsPageWidget>& Page : PageInstances)
	{
		if (IsValid(Page))
		{
			Page->OnPageDirtyStateChanged.RemoveAll(this);
		}
	}

	for (const TObjectPtr<UMKHSettingsNavButtonWidget>& NavButton : NavButtons)
	{
		if (IsValid(NavButton))
		{
			NavButton->OnNavButtonClicked.RemoveAll(this);
		}
	}
}
