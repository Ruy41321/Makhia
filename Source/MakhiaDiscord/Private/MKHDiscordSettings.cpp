// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHDiscordSettings.h"

// ============================================================
// Lifecycle
// ============================================================

UMKHDiscordSettings::UMKHDiscordSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Makhia Discord");
}

// ============================================================
// Public Interface
// ============================================================

const UMKHDiscordSettings* UMKHDiscordSettings::Get()
{
	return GetDefault<UMKHDiscordSettings>();
}
