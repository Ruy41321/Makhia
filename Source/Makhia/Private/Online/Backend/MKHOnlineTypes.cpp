// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHOnlineTypes.h"

const FName FMKHLobbyKeys::SessionName(TEXT("SessionName"));
const FName FMKHLobbyKeys::IsMatchmaking(TEXT("IsMatchmaking"));
const FName FMKHLobbyKeys::MatchmakingTime(TEXT("MatchmakingTime"));
const FString FMKHLobbyKeys::TrueValue(TEXT("1"));
const FString FMKHLobbyKeys::FalseValue(TEXT("0"));

FString FMKHLobbySearchResult::GetAttribute(FName Key) const
{
	const FString* Value = Attributes.Find(Key);
	return Value ? *Value : FString();
}

int64 FMKHLobbySearchResult::GetMatchmakingTime() const
{
	return FCString::Atoi64(*GetAttribute(FMKHLobbyKeys::MatchmakingTime));
}
