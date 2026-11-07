// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/Data/MKHDuelBotProfile.h"

#include "AI/Data/MKHDuelBotScoring.h"
#include "Engine/DataTable.h"
#include "Inventory/MKHStartingLoadout.h"
#include "MKHLogChannels.h"

// ============================================================
// Public Interface
// ============================================================

const FMKHDuelBotScoringRow* UMKHDuelBotProfile::FindScoringRow() const
{
	if (!IsValid(ScoringTable))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotProfile: %s has no ScoringTable, default scoring."), *GetNameSafe(this));
		return nullptr;
	}

	// None: the first row, so a table with one row needs no name.
	const TMap<FName, uint8*>::TConstIterator FirstRow = ScoringTable->GetRowMap().CreateConstIterator();
	const FName RowName = ScoringRowName.IsNone() && FirstRow ? FirstRow.Key() : ScoringRowName;
	return ScoringTable->FindRow<FMKHDuelBotScoringRow>(RowName, TEXT("MKHDuelBotProfile::FindScoringRow"));
}

TArray<FMKHStartingItem> UMKHDuelBotProfile::BuildLoadout() const
{
	if (!IsValid(LoadoutTable))
	{
		UE_LOG(LogMKHAI, Error, TEXT("MKHDuelBotProfile: %s has no LoadoutTable, the bot gets no items."), *GetNameSafe(this));
		return {};
	}

	return FMKHStartingLoadoutRow::ReadLoadout(*LoadoutTable);
}
