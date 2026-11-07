// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/MKHOnlineSettings.h"

UMKHOnlineSettings::UMKHOnlineSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Makhia Online");

	DevCredentialNames = { TEXT("MainRuy"), TEXT("DevRuy") };
}

const UMKHOnlineSettings* UMKHOnlineSettings::Get()
{
	return GetDefault<UMKHOnlineSettings>();
}

bool UMKHOnlineSettings::ShouldShowDevLoginButtons() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bShowDevLoginButtons;
#endif
}
