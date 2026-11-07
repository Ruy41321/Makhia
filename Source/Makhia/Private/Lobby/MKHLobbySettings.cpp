// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbySettings.h"

UMKHLobbySettings::UMKHLobbySettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Makhia Lobby");
}

const UMKHLobbySettings* UMKHLobbySettings::Get()
{
	return GetDefault<UMKHLobbySettings>();
}
