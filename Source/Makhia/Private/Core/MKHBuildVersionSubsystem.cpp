// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Core/MKHBuildVersionSubsystem.h"

#include "Dom/JsonObject.h"
#include "GeneralProjectSettings.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "MKHLogChannels.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace MKHBuildVersion
{
	/** Crash context game data key read by Sentry, and the release name prefix (Sentry "package@version"). */
	static const TCHAR* SentryGameDataKey = TEXT("__sentry");
	static const TCHAR* ReleasePrefix = TEXT("makhia@");

	/** Sentry environment of this build: crashes of test builds stay apart from the players' ones. */
	static const TCHAR* Environment = UE_BUILD_SHIPPING ? TEXT("production") : TEXT("development");
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHBuildVersionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMKHNet, Log, TEXT("Makhia version %s"), *GetGameVersion());
	ApplyReleaseToCrashReporter();
}

// ============================================================
// Public Interface
// ============================================================

FString UMKHBuildVersionSubsystem::GetGameVersion()
{
	return GetDefault<UGeneralProjectSettings>()->ProjectVersion;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHBuildVersionSubsystem::ApplyReleaseToCrashReporter() const
{
	// Sentry's UE crash reporter attributes (Documentation/CrashReporting.md); no user data is ever added here.
	const TSharedRef<FJsonObject> SentryAttributes = MakeShared<FJsonObject>();
	SentryAttributes->SetStringField(TEXT("release"), FString(MKHBuildVersion::ReleasePrefix) + GetGameVersion());
	SentryAttributes->SetStringField(TEXT("environment"), MKHBuildVersion::Environment);

	FString SentryJson;
	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = FCondensedWriterFactory::Create(&SentryJson);
	if (!FJsonSerializer::Serialize(SentryAttributes, JsonWriter))
	{
		UE_LOG(LogMKHNet, Warning, TEXT("UMKHBuildVersionSubsystem: could not serialize the crash reporter release."));
		return;
	}

	FGenericCrashContext::SetGameData(MKHBuildVersion::SentryGameDataKey, SentryJson);
}
