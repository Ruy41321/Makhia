// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MKHMatchMode.generated.h"

/**
 * Who the arena duel is played against. Chosen by whoever opens the game level, through the URL option
 * MKHMatchMode::OptionKey, and read by AMKHGameMode::InitGame. A level opened without the option is an OnlineDuel,
 * except the arena started straight from a standalone PIE, which is an OfflineBot in Training.
 */
UENUM(BlueprintType)
enum class EMKHMatchMode : uint8
{
	/** Two human players over the network (listen server). */
	OnlineDuel,
	/** One human player against a bot, standalone and without an online session. */
	OfflineBot
};

/** Which offline mode an OfflineBot match plays. Full model: see Documentation/OfflineModes.md. */
UENUM(BlueprintType)
enum class EMKHOfflineMode : uint8
{
	/** A normal match against a bot with a random skin; its difficulty follows the player round by round. */
	RandomDuel,
	/** No rounds and no timer: a still target in the centre of the arena. */
	Training,
	/** A normal match against one level of the career (UMKHCareerSubsystem). */
	Career
};

/** What UMKHGameInstance opens for an offline match; it travels in the URL of the game level. */
USTRUCT(BlueprintType)
struct FMKHOfflineMatchRequest
{
	GENERATED_BODY()

	/** Offline mode to play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	EMKHOfflineMode Mode = EMKHOfflineMode::RandomDuel;

	/** Career only: level to play; INDEX_NONE means the first one not beaten yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	int32 CareerLevelIndex = INDEX_NONE;
};

namespace MKHMatchMode
{
	/** URL option carrying the match mode, e.g. "ArenaMap?Mode=Bot". */
	inline const TCHAR* OptionKey = TEXT("Mode");

	/** URL option carrying the bot profile, e.g. "ArenaMap?Mode=Bot?BotProfile=Hard"; matched against UMKHDuelBotProfile::GetProfileId. */
	inline const TCHAR* BotProfileOptionKey = TEXT("BotProfile");

	/** Value of OptionKey selecting EMKHMatchMode::OfflineBot. */
	inline const TCHAR* OfflineBotValue = TEXT("Bot");

	/** URL option carrying the offline mode, e.g. "ArenaMap?Mode=Bot?OfflineMode=Training"; no option means a random duel. */
	inline const TCHAR* OfflineModeOptionKey = TEXT("OfflineMode");

	/** Value of OfflineModeOptionKey selecting EMKHOfflineMode::Training. */
	inline const TCHAR* TrainingValue = TEXT("Training");

	/** Value of OfflineModeOptionKey selecting EMKHOfflineMode::Career. */
	inline const TCHAR* CareerValue = TEXT("Career");

	/** URL option carrying the career level, e.g. "ArenaMap?Mode=Bot?OfflineMode=Career?CareerLevel=1". */
	inline const TCHAR* CareerLevelOptionKey = TEXT("CareerLevel");
}
