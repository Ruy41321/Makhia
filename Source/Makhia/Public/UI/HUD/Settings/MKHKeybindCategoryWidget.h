// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/MKHSystemWidget.h"
#include "MKHKeybindCategoryWidget.generated.h"

class UPanelWidget;
class UTextBlock;

/**
 * Section header separating groups of keybind rows.
 *
 * The category text comes from the Display Category set on each action's Player Mappable Key
 * Settings, so the grouping shown in game always matches what is authored on the Input Actions.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHKeybindCategoryWidget : public UMKHSystemWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Sets the label of the section and how many actions it holds.
	 *
	 * @param InCategoryName  Display category shared by the rows underneath.
	 * @param InActionCount   Number of rows in the section, shown next to the label.
	 */
	void SetCategoryName(const FText& InCategoryName, int32 InActionCount);

	/**
	 * Shows or collapses the "Action / Primary / Secondary" column labels. They only describe keybind
	 * rows, so pages reusing this header for option sections (General, Video) hide them.
	 *
	 * @param bVisible  True to show the column labels, false to collapse them.
	 */
	void SetColumnHeadersVisible(bool bVisible);

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Section label. Must exist in the Blueprint subclass with this exact name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> CategoryNameText;

	/** Optional count of the actions listed under this section. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UTextBlock> CategoryCountText;

	/** Optional row holding the keybind column labels (Action / Primary / Secondary). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> ColumnHeaderBox;
};
