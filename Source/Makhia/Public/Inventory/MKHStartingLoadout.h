// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/MKHStartingItem.h"
#include "MKHStartingLoadout.generated.h"

/**
 * One item of a starting loadout: a table with this row is one whole loadout, for any participant (players:
 * AMKHGameMode::StartingLoadoutTable; duel bot: UMKHDuelBotProfile::LoadoutTable). The row order is the loadout order,
 * which decides the default quick slots. See Documentation/MatchFlowArchitecture.md.
 */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHStartingLoadoutRow : public FTableRowBase
{
	GENERATED_BODY()

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Reads a whole loadout table.
	 *
	 * @param Table  Table with FMKHStartingLoadoutRow rows.
	 * @return The items in row order; empty when the table has another row struct (logged).
	 */
	static TArray<FMKHStartingItem> ReadLoadout(const UDataTable& Table);

	// ============================================================
	// Properties
	// ============================================================

	/** The item to grant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FMKHStartingItem Item;
};
