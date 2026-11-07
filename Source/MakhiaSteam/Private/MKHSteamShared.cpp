// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "MKHSteamShared.h"

#include "OnlineSubsystem.h"

const FName MKHSteam::SubsystemName(TEXT("STEAM"));

IOnlineSubsystem* MKHSteam::GetSubsystem()
{
	// Steam has one client per process: no per-world instance to pick. A failed start (Steam closed) is not retried:
	// every IOnlineSubsystem::Get would try SteamAPI_Init again and stall the frame. Steam only starts with the process.
	static bool bStartFailed = false;
	if (bStartFailed || !IOnlineSubsystem::IsEnabled(SubsystemName))
	{
		return nullptr;
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(SubsystemName);
	bStartFailed = Subsystem == nullptr;
	return Subsystem;
}

uint64 MKHSteam::ParseSteamId64(const FString& UserId)
{
	const FString Trimmed = UserId.TrimStartAndEnd();
	return !Trimmed.IsEmpty() && Trimmed.IsNumeric() ? FCString::Strtoui64(*Trimmed, nullptr, 10) : 0;
}
