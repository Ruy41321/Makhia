// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ISlateMetaData.h"

/**
 * Tag on a Slate widget that shows its own gamepad focus state (UMKHButton swaps to its hovered look), so the
 * menu navigation does not draw its focus frame around it. See Documentation/GamepadMenuNavigation.md.
 */
class FMKHSelfFocusHighlightMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FMKHSelfFocusHighlightMetaData, ISlateMetaData)
};
