// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHOnlineBackend.h"

#include "Online/Backend/MKHNetTransportProvider.h"
#include "Online/Backend/MKHVoiceProvider.h"

TSubclassOf<UMKHNetTransportProvider> UMKHOnlineBackend::GetTransportProviderClass() const
{
	return UMKHNetTransportProvider::StaticClass();
}

TSubclassOf<UMKHVoiceProvider> UMKHOnlineBackend::GetVoiceProviderClass() const
{
	return UMKHVoiceProvider::StaticClass();
}
