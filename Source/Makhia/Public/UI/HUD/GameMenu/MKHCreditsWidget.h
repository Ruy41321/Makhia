// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Text/TextLayout.h"
#include "MKHCreditsWidget.generated.h"

class UButton;
class UMKHCreditsData;
class UMKHTextBlock;
class UMKHTextStyle;
class UScrollBox;
struct FMKHCreditsLine;
struct FMKHCreditsSection;

/**
 * Modal credits window shown over the game menu.
 *
 * The rows are built once from a UMKHCreditsData asset, so the Blueprint only owns the frame:
 * it must provide a UScrollBox named ContentScrollBox, and may add a CloseButton and a full screen
 * BackdropButton (a click outside the window closes it). While open the content scrolls slowly
 * on its own; the first scroll made by the player hands control back to them.
 * The game menu input action closes it too (see AMainHUD::ToggleGameMenu).
 */
UCLASS(Abstract)
class MAKHIA_API UMKHCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/** Shows the window from the top of the credits and restarts the automatic scroll. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Credits")
	void OpenCredits();

	/** Hides the window. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Credits")
	void CloseCredits();

	/** Returns true while the window is shown. */
	UFUNCTION(BlueprintPure, Category = "GameMenu|Credits")
	bool IsOpen() const;

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Fills ContentScrollBox with every section and the legal notices of CreditsData. */
	void BuildContent();

	/** Adds one section: its title followed by its lines. */
	void AddSection(const FMKHCreditsSection& Section, bool bIsFirstSection);

	/** Adds one line: role on the left and name on the right, or the name alone centred. */
	void AddLine(const FMKHCreditsLine& Line);

	/** Adds the legal notices as small centred paragraphs. */
	void AddLegalNotices();

	/**
	 * Creates a styled text ready to be added to a panel.
	 *
	 * @param Text     Text to display.
	 * @param Style    Typography role; null keeps the default font.
	 * @param Color    Text colour.
	 * @param Justify  Horizontal justification inside the text's own box.
	 * @return The new text widget, or null if it could not be created.
	 */
	UMKHTextBlock* CreateText(const FText& Text, UMKHTextStyle* Style, const FLinearColor& Color, ETextJustify::Type Justify);

	/** Advances the automatic scroll after its start delay, stopping at the end of the content. */
	void TickAutoScroll(float DeltaTime);

	/** Close and backdrop button callback. */
	UFUNCTION()
	void HandleCloseClicked();

	/** Scroll box callback: the player scrolled, so the automatic scroll stops. */
	UFUNCTION()
	void HandleUserScrolled(float CurrentOffset);

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Credits content to display. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Content")
	TObjectPtr<UMKHCreditsData> CreditsData;

	/** Typography of section titles. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	TObjectPtr<UMKHTextStyle> SectionTitleStyle;

	/** Typography of the role column. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	TObjectPtr<UMKHTextStyle> RoleStyle;

	/** Typography of the name column. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	TObjectPtr<UMKHTextStyle> NameStyle;

	/** Typography of the legal notices. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	TObjectPtr<UMKHTextStyle> LegalStyle;

	/** Colour of section titles. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	FLinearColor SectionTitleColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Colour of the role column. Linear value of #97897A. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	FLinearColor RoleColor = FLinearColor(0.30947f, 0.25016f, 0.19462f, 1.0f);

	/** Colour of the name column. Linear value of the #EDE3D1 ivory. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	FLinearColor NameColor = FLinearColor(0.84687f, 0.76815f, 0.63760f, 1.0f);

	/** Colour of the legal notices. Linear value of #7A6F63. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style")
	FLinearColor LegalColor = FLinearColor(0.19462f, 0.15896f, 0.12477f, 1.0f);

	/** Empty space above every section title except the first one. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style", meta = (ClampMin = "0"))
	float SectionSpacing = 36.0f;

	/** Empty space between the section title and its first line. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style", meta = (ClampMin = "0"))
	float SectionTitleSpacing = 12.0f;

	/** Empty space above every line. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style", meta = (ClampMin = "0"))
	float LineSpacing = 6.0f;

	/** Empty space above the legal notices block. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Style", meta = (ClampMin = "0"))
	float LegalSpacing = 48.0f;

	/** Automatic scroll speed in Slate units per second. 0 disables the automatic scroll. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Scroll", meta = (ClampMin = "0"))
	float AutoScrollSpeed = 22.0f;

	/** Seconds the window waits after opening before it starts scrolling on its own. */
	UPROPERTY(EditDefaultsOnly, Category = "Credits|Scroll", meta = (ClampMin = "0"))
	float AutoScrollDelay = 2.5f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Scrollable column the credits rows are added to. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ContentScrollBox;

	/** Optional close button inside the window. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Optional full screen button behind the window: a click outside the window closes it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackdropButton;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Seconds elapsed since the window opened, used for the automatic scroll delay. */
	float SecondsSinceOpened = 0.0f;

	/** True while the content scrolls on its own. */
	bool bAutoScrollActive = false;
};
