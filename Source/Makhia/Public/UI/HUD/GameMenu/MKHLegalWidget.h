// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Text/TextLayout.h"
#include "UI/HUD/GameMenu/MKHLegalDocument.h"
#include "MKHLegalWidget.generated.h"

class UButton;
class UMKHButton;
class UMKHButtonStyle;
class UMKHTextBlock;
class UMKHTextStyle;
class UScrollBox;

/**
 * Modal window over the game menu that shows the privacy policy or the EULA, with one tab per document.
 *
 * Same frame as the credits window (W_Credits); the text comes from MKHLegalDocuments::GetContent, generated
 * from Documentation/Legal/*.md. The Blueprint (W_Legal) must provide ContentScrollBox and may add the two tab
 * buttons, CloseButton and a full screen BackdropButton. See Documentation/LegalDocuments.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHLegalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Shows the window on a document, scrolled to the top.
	 *
	 * @param Document  Document to show first.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Legal")
	void OpenLegal(EMKHLegalDocument Document);

	/** Hides the window. */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Legal")
	void CloseLegal();

	/** Returns true while the window is shown. */
	UFUNCTION(BlueprintPure, Category = "GameMenu|Legal")
	bool IsOpen() const;

	/**
	 * Replaces the content with another document and marks its tab as selected.
	 *
	 * @param Document  Document to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameMenu|Legal")
	void ShowDocument(EMKHLegalDocument Document);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Fills ContentScrollBox with the title, introduction and sections of a document. */
	void BuildContent(const FMKHLegalDocumentContent& Content);

	/**
	 * Adds a wrapped, styled text to the content column.
	 *
	 * @param Text        Text to display.
	 * @param Style       Typography role; null keeps the default font.
	 * @param Color       Text colour.
	 * @param TopPadding  Empty space above the text.
	 * @param Justify     Horizontal justification.
	 */
	void AddText(const FText& Text, UMKHTextStyle* Style, const FLinearColor& Color, float TopPadding, ETextJustify::Type Justify = ETextJustify::Left);

	/** Gives the selected tab the selected look and the other one the normal look. */
	void RefreshTabs();

	/** Privacy tab callback. */
	UFUNCTION()
	void HandlePrivacyTabClicked();

	/** EULA tab callback. */
	UFUNCTION()
	void HandleEulaTabClicked();

	/** Close and backdrop button callback. */
	UFUNCTION()
	void HandleCloseClicked();

	// ============================================================
	// Properties - Style
	// ============================================================

	/** Typography of the document title. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	TObjectPtr<UMKHTextStyle> TitleStyle;

	/** Typography of the section headings. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	TObjectPtr<UMKHTextStyle> HeadingStyle;

	/** Typography of the paragraphs. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	TObjectPtr<UMKHTextStyle> BodyStyle;

	/** Look of the tab of the document on screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	TObjectPtr<UMKHButtonStyle> SelectedTabStyle;

	/** Look of the other tab. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	TObjectPtr<UMKHButtonStyle> TabStyle;

	/** Colour of the title and headings. Linear value of the #C9973E brass accent. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	FLinearColor HeadingColor = FLinearColor(0.58408f, 0.30947f, 0.04817f, 1.0f);

	/** Colour of the paragraphs. Linear value of the #EDE3D1 ivory. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style")
	FLinearColor BodyColor = FLinearColor(0.84687f, 0.76815f, 0.63760f, 1.0f);

	/** Empty space above every section heading. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style", meta = (ClampMin = "0"))
	float SectionSpacing = 28.0f;

	/** Empty space above every paragraph. */
	UPROPERTY(EditDefaultsOnly, Category = "Legal|Style", meta = (ClampMin = "0"))
	float ParagraphSpacing = 10.0f;

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Scrollable column the document is written into. Must exist in the Blueprint subclass. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ContentScrollBox;

	/** Tab showing the privacy policy. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHButton> PrivacyTabButton;

	/** Tab showing the EULA. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMKHButton> EulaTabButton;

	/** Optional close button inside the window. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Optional full screen button behind the window: a click outside the window closes it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackdropButton;

private:
	/** Document currently on screen. */
	EMKHLegalDocument CurrentDocument = EMKHLegalDocument::PrivacyPolicy;
};
