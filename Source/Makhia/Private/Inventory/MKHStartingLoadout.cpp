// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/MKHStartingLoadout.h"

#include "MKHLogChannels.h"

// ============================================================
// Public Interface
// ============================================================

TArray<FMKHStartingItem> FMKHStartingLoadoutRow::ReadLoadout(const UDataTable& Table)
{
	TArray<FMKHStartingItem> Loadout;
	if (Table.GetRowStruct() != StaticStruct())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("FMKHStartingLoadoutRow: %s is not a loadout table."), *GetNameSafe(&Table));
		return Loadout;
	}

	Loadout.Reserve(Table.GetRowMap().Num());
	for (const TPair<FName, uint8*>& Row : Table.GetRowMap())
	{
		Loadout.Add(reinterpret_cast<const FMKHStartingLoadoutRow*>(Row.Value)->Item);
	}
	return Loadout;
}
