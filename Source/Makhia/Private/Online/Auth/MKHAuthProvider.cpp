// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Auth/MKHAuthProvider.h"

#include "Online/Auth/MKHAuthSubsystem.h"

#define LOCTEXT_NAMESPACE "MKHAuthProvider"

FText UMKHAuthProvider::GetMethodDisplayName(EMKHAuthMethod Method) const
{
	switch (Method)
	{
	case EMKHAuthMethod::PlatformAccount:
		return LOCTEXT("PlatformAccount", "Online account");
	case EMKHAuthMethod::Guest:
		return LOCTEXT("Guest", "Guest");
	case EMKHAuthMethod::Developer:
		return INVTEXT("Developer");
	default:
		return FText::GetEmpty();
	}
}

FText UMKHAuthProvider::GetUnavailableText() const
{
	return UMKHAuthSubsystem::GetAuthErrorText(EMKHAuthError::BackendUnavailable);
}

#undef LOCTEXT_NAMESPACE
