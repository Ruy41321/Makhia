// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MKHLocalizationLibrary.generated.h"

/**
 * Interface language of the game: which languages ship, which one is active, and switching between them.
 *
 * The engine's current language is the only source of truth. In a packaged game SetLanguage saves it to
 * GameUserSettings.ini ([Internationalization] Language), which the engine reads back at startup; on first
 * launch the engine picks the system language when it ships, English otherwise. In the editor the switch
 * drives the game localization preview instead, so the editor's own language never changes.
 *
 * Full model: see Documentation/Localization.md.
 */
UCLASS()
class MAKHIA_API UMKHLocalizationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Languages the game ships, native language (English) first.
	 *
	 * @return Culture codes, e.g. { "en", "it", "fr" }.
	 */
	UFUNCTION(BlueprintPure, Category = "Localization")
	static TArray<FString> GetAvailableLanguages();

	/**
	 * Language the game text is currently shown in.
	 *
	 * @return One of GetAvailableLanguages(), or the native language when the active culture is not shipped.
	 */
	UFUNCTION(BlueprintPure, Category = "Localization")
	static FString GetCurrentLanguage();

	/**
	 * Language used when the player never picked one: the system language when shipped, the native one otherwise.
	 *
	 * @return One of GetAvailableLanguages().
	 */
	UFUNCTION(BlueprintPure, Category = "Localization")
	static FString GetDefaultLanguage();

	/**
	 * Switches the game text (and number formats) to a language and remembers the choice.
	 * Text already on screen updates on its own; see Documentation/Localization.md for the exceptions.
	 *
	 * @param CultureName  One of GetAvailableLanguages().
	 * @return True if the language was applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Localization")
	static bool SetLanguage(const FString& CultureName);

	/**
	 * Name of a language written in that language ("English", "Italiano", "Français"), never translated.
	 *
	 * @param CultureName  Culture code.
	 * @return Culture-invariant display name, or the code itself when the culture is unknown.
	 */
	UFUNCTION(BlueprintPure, Category = "Localization")
	static FText GetLanguageDisplayName(const FString& CultureName);

private:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Maps any culture (e.g. "fr-CA") to the closest shipped language ("fr").
	 *
	 * @param CultureName  Culture code to match.
	 * @return The matching shipped language, or an empty string when none matches.
	 */
	static FString FindAvailableLanguage(const FString& CultureName);
};
