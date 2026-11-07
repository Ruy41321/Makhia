// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Loading/MKHLoadingScreenSettings.h"

#include "Materials/MaterialInterface.h"
#include "UI/Text/MKHTextStyle.h"

UMKHLoadingScreenSettings::UMKHLoadingScreenSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Makhia Loading Screen");

	BackgroundMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Makhia/UI/GameMenu/Material/MI_UI_Background.MI_UI_Background")));
	TitleStyle = TSoftObjectPtr<UMKHTextStyle>(FSoftObjectPath(TEXT("/Game/Makhia/UI/Fonts/Styles/TS_Display.TS_Display")));
	StatusStyle = TSoftObjectPtr<UMKHTextStyle>(FSoftObjectPath(TEXT("/Game/Makhia/UI/Fonts/Styles/TS_Status.TS_Status")));
}

const UMKHLoadingScreenSettings* UMKHLoadingScreenSettings::Get()
{
	return GetDefault<UMKHLoadingScreenSettings>();
}
