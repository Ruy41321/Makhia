// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Libraries/MKHLocalizationLibrary.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "MKHLogChannels.h"

// ============================================================
// Public Interface
// ============================================================

TArray<FString> UMKHLocalizationLibrary::GetAvailableLanguages()
{
	const FTextLocalizationManager& LocManager = FTextLocalizationManager::Get();

	FString NativeLanguage = LocManager.GetNativeCultureName(ELocalizedTextSourceCategory::Game);
	if (NativeLanguage.IsEmpty())
	{
		NativeLanguage = TEXT("en");
	}

	TArray<FString> Languages = LocManager.GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
	Languages.Remove(NativeLanguage);
	Languages.Insert(NativeLanguage, 0);
	return Languages;
}

FString UMKHLocalizationLibrary::GetCurrentLanguage()
{
	FString ActiveCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();

#if WITH_EDITOR
	// In the editor the game text follows the localization preview, not the editor's own culture.
	if (GIsEditor)
	{
		const FTextLocalizationManager& LocManager = FTextLocalizationManager::Get();
		ActiveCulture = LocManager.IsGameLocalizationPreviewEnabled()
			? LocManager.GetConfiguredGameLocalizationPreviewLanguage()
			: LocManager.GetNativeCultureName(ELocalizedTextSourceCategory::Game);
	}
#endif

	const FString Language = FindAvailableLanguage(ActiveCulture);
	return Language.IsEmpty() ? GetAvailableLanguages()[0] : Language;
}

FString UMKHLocalizationLibrary::GetDefaultLanguage()
{
	const FString Language = FindAvailableLanguage(FInternationalization::Get().GetDefaultLanguage()->GetName());
	return Language.IsEmpty() ? GetAvailableLanguages()[0] : Language;
}

bool UMKHLocalizationLibrary::SetLanguage(const FString& CultureName)
{
	if (!GetAvailableLanguages().Contains(CultureName))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("SetLanguage: '%s' is not a shipped language."), *CultureName);
		return false;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Switching the real culture here would translate the whole editor: preview the game text only.
		FTextLocalizationManager& LocManager = FTextLocalizationManager::Get();
		LocManager.ConfigureGameLocalizationPreviewLanguage(CultureName);
		LocManager.EnableGameLocalizationPreview(CultureName);
		return true;
	}
#endif

	return UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(CultureName, /*SaveToConfig*/ true);
}

FText UMKHLocalizationLibrary::GetLanguageDisplayName(const FString& CultureName)
{
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	FString NativeName = Culture.IsValid() ? Culture->GetNativeLanguage() : CultureName;

	// ICU writes most language names in lower case ("italiano", "français").
	if (!NativeName.IsEmpty())
	{
		NativeName[0] = FChar::ToUpper(NativeName[0]);
	}
	return FText::AsCultureInvariant(NativeName);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

FString UMKHLocalizationLibrary::FindAvailableLanguage(const FString& CultureName)
{
	const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	if (!Culture.IsValid())
	{
		return FString();
	}

	const TArray<FString> Languages = GetAvailableLanguages();
	for (const FString& Candidate : Culture->GetPrioritizedParentCultureNames())
	{
		if (Languages.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}
