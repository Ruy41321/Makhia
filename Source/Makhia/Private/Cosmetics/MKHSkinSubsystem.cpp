// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Cosmetics/MKHSkinSubsystem.h"

#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinSaveGame.h"
#include "Data/MKHDataSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"

namespace MKHSkin
{
	/** Save slot of the chosen skin. */
	static const TCHAR* SaveSlotName = TEXT("Skin");
	static constexpr int32 SaveUserIndex = 0;
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHSkinSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMKHDataSubsystem>();
	Super::Initialize(Collection);

	LoadSelection();
}

// ============================================================
// Public Interface
// ============================================================

UMKHSkinSubsystem* UMKHSkinSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHSkinSubsystem>() : nullptr;
}

const UMKHSkinCatalog* UMKHSkinSubsystem::GetCatalog() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UMKHDataSubsystem* Data = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHDataSubsystem>() : nullptr;
	return IsValid(Data) ? Data->GetSkinCatalog() : nullptr;
}

const FMKHSkinDefinition* UMKHSkinSubsystem::FindSkin(const FName SkinId) const
{
	const UMKHSkinCatalog* Catalog = GetCatalog();
	return IsValid(Catalog) ? Catalog->FindSkin(SkinId) : nullptr;
}

FName UMKHSkinSubsystem::ResolveSkinId(const FName SkinId) const
{
	const UMKHSkinCatalog* Catalog = GetCatalog();
	return IsValid(Catalog) ? Catalog->ResolveSkinId(SkinId) : NAME_None;
}

void UMKHSkinSubsystem::SelectSkin(const FName SkinId)
{
	if (!FindSkin(SkinId))
	{
		UE_LOG(LogMKHCosmetics, Warning, TEXT("SelectSkin: unknown skin '%s'."), *SkinId.ToString());
		return;
	}

	const FName PreviouslyDisplayed = GetDisplayedSkinId();
	const bool bChanged = SelectedSkinId != SkinId;
	SelectedSkinId = SkinId;
	PreviewSkinId = NAME_None;

	if (bChanged)
	{
		SaveSelection();
		OnSelectedSkinChanged.Broadcast(SelectedSkinId);
	}
	BroadcastDisplayedChange(PreviouslyDisplayed);
}

void UMKHSkinSubsystem::SetPreviewSkin(const FName SkinId)
{
	if (!FindSkin(SkinId))
	{
		return;
	}

	const FName PreviouslyDisplayed = GetDisplayedSkinId();
	PreviewSkinId = SkinId;
	BroadcastDisplayedChange(PreviouslyDisplayed);
}

void UMKHSkinSubsystem::ClearPreviewSkin()
{
	const FName PreviouslyDisplayed = GetDisplayedSkinId();
	PreviewSkinId = NAME_None;
	BroadcastDisplayedChange(PreviouslyDisplayed);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHSkinSubsystem::LoadSelection()
{
	FName SavedSkinId;
	if (UGameplayStatics::DoesSaveGameExist(MKHSkin::SaveSlotName, MKHSkin::SaveUserIndex))
	{
		if (const UMKHSkinSaveGame* SaveData = Cast<UMKHSkinSaveGame>(UGameplayStatics::LoadGameFromSlot(MKHSkin::SaveSlotName, MKHSkin::SaveUserIndex)); IsValid(SaveData))
		{
			SavedSkinId = SaveData->SkinId;
		}
	}

	SelectedSkinId = ResolveSkinId(SavedSkinId);
	if (!SavedSkinId.IsNone() && SelectedSkinId != SavedSkinId)
	{
		UE_LOG(LogMKHCosmetics, Warning, TEXT("Saved skin '%s' no longer exists, using '%s'."), *SavedSkinId.ToString(), *SelectedSkinId.ToString());
	}
}

void UMKHSkinSubsystem::SaveSelection() const
{
	UMKHSkinSaveGame* SaveData = Cast<UMKHSkinSaveGame>(UGameplayStatics::CreateSaveGameObject(UMKHSkinSaveGame::StaticClass()));
	if (!IsValid(SaveData))
	{
		return;
	}

	SaveData->SkinId = SelectedSkinId;
	if (!UGameplayStatics::SaveGameToSlot(SaveData, MKHSkin::SaveSlotName, MKHSkin::SaveUserIndex))
	{
		UE_LOG(LogMKHCosmetics, Warning, TEXT("Could not write the chosen skin to the '%s' save slot."), MKHSkin::SaveSlotName);
	}
}

void UMKHSkinSubsystem::BroadcastDisplayedChange(const FName PreviouslyDisplayed)
{
	if (GetDisplayedSkinId() != PreviouslyDisplayed)
	{
		OnDisplayedSkinChanged.Broadcast(GetDisplayedSkinId());
	}
}
