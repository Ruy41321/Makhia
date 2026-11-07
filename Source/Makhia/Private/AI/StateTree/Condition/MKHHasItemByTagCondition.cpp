// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Condition/MKHHasItemByTagCondition.h"

#include "AIController.h"
#include "Inventory/InventoryComponent.h"
#include "MKHLogChannels.h"
#include "StateTreeExecutionContext.h"

// ============================================================
// Public Interface
// ============================================================

bool FMKHHasItemByTagCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const APawn* Pawn = IsValid(Data.AIController) ? Data.AIController->GetPawn() : nullptr;
	const UInventoryComponent* Inventory = IsValid(Pawn) ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Data.ItemTag.IsValid() || !IsValid(Inventory))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHHasItemByTagCondition: missing item tag or inventory on %s."), *GetNameSafe(Pawn));
		return false;
	}

	// The bot keeps its inventory on the pawn (AMKHDuelBot), so any entry there is an item it owns.
	for (const FRPGInventoryEntry& Entry : Inventory->GetInventoryEntries())
	{
		if (Entry.Item.ItemTag.MatchesTag(Data.ItemTag))
		{
			return true;
		}
	}
	return false;
}
