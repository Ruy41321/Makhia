// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Backend/MKHOnlineServicesSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MKHLogChannels.h"
#include "Misc/CommandLine.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/Null/MKHOnlineBackend_Null.h"
#include "Online/MKHOnlineSettings.h"
#include "UObject/UObjectIterator.h"

// ============================================================
// Lifecycle
// ============================================================

void UMKHOnlineServicesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TSubclassOf<UMKHOnlineBackend> BackendClass = ResolveBackendClass();
	Backend = NewObject<UMKHOnlineBackend>(this, BackendClass);
	UE_LOG(LogMKHOnline, Log, TEXT("Online backend: %s (%s)."), *Backend->GetBackendName().ToString(), *BackendClass->GetName());
}

void UMKHOnlineServicesSubsystem::Deinitialize()
{
	Backend = nullptr;
	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHOnlineServicesSubsystem* UMKHOnlineServicesSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHOnlineServicesSubsystem>() : nullptr;
}

FName UMKHOnlineServicesSubsystem::GetBackendName() const
{
	return IsValid(Backend) ? Backend->GetBackendName() : NAME_None;
}

FMKHOnlineCapabilities UMKHOnlineServicesSubsystem::GetCapabilities() const
{
	return IsValid(Backend) ? Backend->GetCapabilities() : FMKHOnlineCapabilities();
}

FText UMKHOnlineServicesSubsystem::GetPlatformDisplayName() const
{
	return IsValid(Backend) ? Backend->GetPlatformDisplayName() : FText::GetEmpty();
}

bool UMKHOnlineServicesSubsystem::IsRunningOnHandheld() const
{
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("MKHHandheld")))
	{
		return true;
	}
#endif
	return IsValid(Backend) && Backend->IsRunningOnHandheld();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

TSubclassOf<UMKHOnlineBackend> UMKHOnlineServicesSubsystem::ResolveBackendClass() const
{
#if !UE_BUILD_SHIPPING
	FString CommandLineName;
	if (FParse::Value(FCommandLine::Get(), TEXT("MKHOnline="), CommandLineName))
	{
		if (const TSubclassOf<UMKHOnlineBackend> CommandLineClass = FindBackendClassByName(CommandLineName))
		{
			return CommandLineClass;
		}
		UE_LOG(LogMKHOnline, Warning, TEXT("-MKHOnline=%s: no backend with that name, using the project settings."), *CommandLineName);
	}
#endif

	const TSubclassOf<UMKHOnlineBackend> SettingsClass = UMKHOnlineSettings::Get()->BackendClass.LoadSynchronous();
	if (IsValid(SettingsClass) && !SettingsClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return SettingsClass;
	}

	// A backend living in a module that is not compiled (e.g. the dormant EOS one) lands here.
	UE_LOG(LogMKHOnline, Warning, TEXT("No valid online backend in Project Settings -> Makhia Online (%s), using Null."),
		*UMKHOnlineSettings::Get()->BackendClass.ToString());
	return UMKHOnlineBackend_Null::StaticClass();
}

TSubclassOf<UMKHOnlineBackend> UMKHOnlineServicesSubsystem::FindBackendClassByName(const FString& BackendName)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UMKHOnlineBackend::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		const UMKHOnlineBackend* Defaults = Class->GetDefaultObject<UMKHOnlineBackend>();
		if (IsValid(Defaults) && Defaults->GetBackendName().ToString().Equals(BackendName, ESearchCase::IgnoreCase))
		{
			return Class;
		}
	}
	return nullptr;
}
