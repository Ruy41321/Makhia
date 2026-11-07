// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Career/MKHCareerSubsystem.h"

#include "Career/MKHCareerLadder.h"
#include "Career/MKHCareerSaveGame.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"

namespace MKHCareer
{
	/** Save slot of the career progress. */
	static const TCHAR* SaveSlotName = TEXT("Career");
	static constexpr int32 SaveUserIndex = 0;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHCareerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMKHDataSubsystem>();
	Super::Initialize(Collection);

	LoadProgress();
}

// ============================================================
// Public Interface
// ============================================================

UMKHCareerSubsystem* UMKHCareerSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHCareerSubsystem>() : nullptr;
}

const UMKHCareerLadder* UMKHCareerSubsystem::GetLadder() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UMKHDataSubsystem* Data = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHDataSubsystem>() : nullptr;
	return IsValid(Data) ? Data->GetCareerLadder() : nullptr;
}

const FMKHCareerLevel* UMKHCareerSubsystem::FindLevel(const int32 LevelIndex) const
{
	const UMKHCareerLadder* Ladder = GetLadder();
	return IsValid(Ladder) && Ladder->GetLevels().IsValidIndex(LevelIndex) ? &Ladder->GetLevels()[LevelIndex] : nullptr;
}

int32 UMKHCareerSubsystem::GetLevelCount() const
{
	const UMKHCareerLadder* Ladder = GetLadder();
	return IsValid(Ladder) ? Ladder->GetLevels().Num() : 0;
}

FText UMKHCareerSubsystem::GetTitleForCompletedLevels(const int32 InCompletedLevels) const
{
	const UMKHCareerLadder* Ladder = GetLadder();
	if (!IsValid(Ladder))
	{
		return FText::GetEmpty();
	}

	const int32 LastBeaten = FMath::Min(InCompletedLevels, Ladder->GetLevels().Num()) - 1;
	return Ladder->GetLevels().IsValidIndex(LastBeaten) ? Ladder->GetLevels()[LastBeaten].TitleAwarded : Ladder->GetStartingTitle();
}

bool UMKHCareerSubsystem::IsLevelUnlocked(const int32 LevelIndex) const
{
	return FindLevel(LevelIndex) != nullptr && LevelIndex <= CompletedLevels;
}

int32 UMKHCareerSubsystem::GetNextLevelIndex() const
{
	const int32 LevelCount = GetLevelCount();
	return LevelCount > 0 ? FMath::Min(CompletedLevels, LevelCount - 1) : INDEX_NONE;
}

void UMKHCareerSubsystem::RecordLevelWon(const int32 LevelIndex)
{
	if (!IsLevelUnlocked(LevelIndex))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Career: level %d cannot be won, it is locked or unknown (%d beaten)."), LevelIndex, CompletedLevels);
		return;
	}

	// Replaying a beaten level changes nothing.
	if (LevelIndex < CompletedLevels)
	{
		return;
	}

	CompletedLevels = LevelIndex + 1;
	SaveProgress();
	UE_LOG(LogMKHSession, Log, TEXT("Career: level %d beaten for the first time (%d/%d)."), LevelIndex, CompletedLevels, GetLevelCount());
	OnLevelCompleted.Broadcast(LevelIndex);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHCareerSubsystem::LoadProgress()
{
	if (!UGameplayStatics::DoesSaveGameExist(MKHCareer::SaveSlotName, MKHCareer::SaveUserIndex))
	{
		return;
	}

	const UMKHCareerSaveGame* Save = Cast<UMKHCareerSaveGame>(UGameplayStatics::LoadGameFromSlot(MKHCareer::SaveSlotName, MKHCareer::SaveUserIndex));
	if (!IsValid(Save))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Career: the save slot could not be read, the career starts over."));
		return;
	}

	CompletedLevels = FMath::Max(Save->CompletedLevels, 0);
}

void UMKHCareerSubsystem::SaveProgress() const
{
	UMKHCareerSaveGame* Save = Cast<UMKHCareerSaveGame>(UGameplayStatics::CreateSaveGameObject(UMKHCareerSaveGame::StaticClass()));
	if (!IsValid(Save))
	{
		return;
	}

	Save->CompletedLevels = CompletedLevels;
	if (!UGameplayStatics::SaveGameToSlot(Save, MKHCareer::SaveSlotName, MKHCareer::SaveUserIndex))
	{
		UE_LOG(LogMKHSession, Warning, TEXT("Career: the progress could not be saved."));
	}
}
