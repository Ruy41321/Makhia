// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Data/MKHDataSettings.h"

UMKHDataSettings::UMKHDataSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Makhia Data");
}

const UMKHDataSettings* UMKHDataSettings::Get()
{
	return GetDefault<UMKHDataSettings>();
}
