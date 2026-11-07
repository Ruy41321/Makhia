// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHLegalDocument.generated.h"

/** Legal documents the game can show (Game menu > Privacy & EULA). */
UENUM(BlueprintType)
enum class EMKHLegalDocument : uint8
{
	PrivacyPolicy	UMETA(DisplayName = "Privacy Policy"),
	Eula			UMETA(DisplayName = "EULA")
};

/** A numbered section of a legal document: heading and paragraphs (list items start with a bullet). */
USTRUCT(BlueprintType)
struct FMKHLegalSection
{
	GENERATED_BODY()

	/** Section heading, e.g. "2. What data the game uses". */
	UPROPERTY(BlueprintReadOnly, Category = "Legal")
	FText Heading;

	/** Paragraphs in display order; a paragraph may contain line breaks (rows of a table). */
	UPROPERTY(BlueprintReadOnly, Category = "Legal")
	TArray<FText> Paragraphs;
};

/** Whole text of one legal document, localized like every other game text. */
USTRUCT(BlueprintType)
struct FMKHLegalDocumentContent
{
	GENERATED_BODY()

	/** Document title. */
	UPROPERTY(BlueprintReadOnly, Category = "Legal")
	FText Title;

	/** Paragraphs before the first section (version, date, introduction). */
	UPROPERTY(BlueprintReadOnly, Category = "Legal")
	TArray<FText> Introduction;

	/** Sections in display order. */
	UPROPERTY(BlueprintReadOnly, Category = "Legal")
	TArray<FMKHLegalSection> Sections;
};

namespace MKHLegalDocuments
{
	/**
	 * Returns the text of a legal document in the current language.
	 * Generated from Documentation/Legal/*.md by Tools/Legal/legal_docs.py (MKHLegalDocumentsContent.cpp).
	 *
	 * @param Document  Document to read.
	 * @return Its title, introduction and sections; empty for an unknown document.
	 */
	MAKHIA_API FMKHLegalDocumentContent GetContent(EMKHLegalDocument Document);
}
