// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MKHCreditsData.generated.h"

/** One credited contribution: what was done and by whom. */
USTRUCT(BlueprintType)
struct FMKHCreditsLine
{
	GENERATED_BODY()

	/** What the person did or which work is credited (e.g. "Game design", "Fantastic Dungeon Pack"). Localizable. Leave empty to show Name alone, centred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText Role;

	/** Who is credited (person, studio, licence). Proper names are usually culture invariant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText Name;
};

/** A titled group of credits lines (e.g. "Development", "Fonts"). */
USTRUCT(BlueprintType)
struct FMKHCreditsSection
{
	GENERATED_BODY()

	/** Section header, shown upper case by its text style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits")
	FText Title;

	/** Lines of the section, in display order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (TitleProperty = "Name"))
	TArray<FMKHCreditsLine> Lines;
};

/**
 * Content of the credits window: sections of lines plus the legal notices printed at the bottom.
 * Edited as data (DA_Credits), so updating the credits never needs a code or widget change.
 * See Documentation/GameMenuArchitecture.md.
 */
UCLASS(BlueprintType, Const)
class MAKHIA_API UMKHCreditsData : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Sections in display order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Credits", meta = (TitleProperty = "Title"))
	TArray<FMKHCreditsSection> Sections;

	/** Small print shown after the sections (trademark and licence notices), one paragraph per entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Credits", meta = (MultiLine = true))
	TArray<FText> LegalNotices;
};
