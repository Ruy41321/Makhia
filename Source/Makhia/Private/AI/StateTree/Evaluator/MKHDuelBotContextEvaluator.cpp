// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/StateTree/Evaluator/MKHDuelBotContextEvaluator.h"

#include "MKHLogChannels.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "StateTreeExecutionContext.h"
#include "GameMode/MKHGameState.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/InventoryComponent.h"
#include "QuickSlot/MKHDefaultQuickSlots.h"
#include "QuickSlot/QuickSlotManagerComponent.h"
#include "VisualLogger/VisualLogger.h"

// ============================================================
// Public Interface
// ============================================================

void FMKHDuelBotContextEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	const EMKHDuelDistanceBand PreviousBand = Data.DistanceBand;
	const bool bWonLastTick = Data.bWonRound;
	const bool bTauntedLastTick = Data.bShouldTaunt;
	ResetOutputs(Data);
	
	const AMKHDuelBotController* BotController = Cast<AMKHDuelBotController>(Data.AIController);
	if (!IsValid(BotController))
	{
		CopyEntryIfChanged(nullptr, Data.CurrentWeaponEntry);
		CopyEntryIfChanged(nullptr, Data.SheathedWeaponEntry);
		return;
	}

	Data.Tuning = BotController->GetTuning();
	Data.BotCharacter = Cast<ACharacterBase>(BotController->GetPawn());
	Data.Opponent = BotController->GetOpponent();

	const bool bHasBot = IsValid(Data.BotCharacter);
	CopyEntryIfChanged(bHasBot ? FindCurrentWeapon(*Data.BotCharacter) : nullptr, Data.CurrentWeaponEntry);
	if (bHasBot)
	{
		UpdateSheathedWeapon(*Data.BotCharacter, Data.SheathedWeaponEntry);
	}
	else
	{
		CopyEntryIfChanged(nullptr, Data.SheathedWeaponEntry);
	}

	if (!bHasBot || !IsValid(Data.Opponent))
	{
		// Normal until the controller has found an opponent: Verbose, not to flood the log every tick.
		UE_LOG(LogMKHAI, Verbose, TEXT("MKHDuelBotContextEvaluator: no bot (%s) or opponent (%s) yet."),
			*GetNameSafe(Data.BotCharacter), *GetNameSafe(Data.Opponent));
		return;
	}

	UpdateCombatMetrics(Context, *Data.BotCharacter, *Data.Opponent, PreviousBand, Data);
	SendDistanceBandEventOnChange(Context, PreviousBand, Data);
	UpdateTaunt(bWonLastTick, bTauntedLastTick, Data);
}

void FMKHDuelBotContextEvaluator::SendDistanceBandEventOnChange(FStateTreeExecutionContext& Context,
	const EMKHDuelDistanceBand PreviousBand, const FMKHDuelBotContextEvaluatorInstanceData& Data)
{
	if (Data.DistanceBand == PreviousBand)
	{
		return;
	}

	Context.SendEvent(GetDistanceBandEventTag(Data.DistanceBand));
	UE_VLOG_UELOG(Data.AIController.Get(), LogMKHAI, Log, TEXT("Distance band %s -> %s (%.0f cm)."),
		*UEnum::GetDisplayValueAsText(PreviousBand).ToString(), *UEnum::GetDisplayValueAsText(Data.DistanceBand).ToString(), Data.Distance);
}

FGameplayTag FMKHDuelBotContextEvaluator::GetDistanceBandEventTag(const EMKHDuelDistanceBand Band)
{
	switch (Band)
	{
	case EMKHDuelDistanceBand::Combat: return MKHGameplayTags::Event::AIDistanceBandCombat;
	case EMKHDuelDistanceBand::Safe:   return MKHGameplayTags::Event::AIDistanceBandSafe;
	default:                           return MKHGameplayTags::Event::AIDistanceBandFar;
	}
}

const FMKHItemEntry* FMKHDuelBotContextEvaluator::FindCurrentWeapon(const ACharacterBase& Character)
{
	// The project reaches equipment through the controller (Documentation/MatchFlowArchitecture.md, Equipment lookup).
	const AController* Controller = Character.GetController();
	if (!IsValid(Controller) || !Controller->Implements<UEquipmentInterface>())
	{
		return nullptr;
	}

	const UEquipmentManagerComponent* Equipment = IEquipmentInterface::Execute_GetEquipmentManagerComponent(Controller);
	const FRPGEquipmentEntry* Weapon = IsValid(Equipment) ? Equipment->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot) : nullptr;
	return Weapon != nullptr && Weapon->Item.IsValid() ? &Weapon->Item : nullptr;
}

void FMKHDuelBotContextEvaluator::UpdateSheathedWeapon(const ACharacterBase& Character, FMKHItemEntry& InOutWeapon)
{
	const AController* Controller = Character.GetController();
	const bool bHasInterfaces = IsValid(Controller) && Controller->Implements<UQuickSlotInterface>() && Controller->Implements<UInventoryInterface>();
	const UQuickSlotManagerComponent* QuickSlots = bHasInterfaces ? IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(Controller) : nullptr;
	const UInventoryComponent* Inventory = bHasInterfaces ? IInventoryInterface::Execute_GetInventoryComponent(Controller) : nullptr;
	if (!IsValid(QuickSlots) || !IsValid(Inventory))
	{
		CopyEntryIfChanged(nullptr, InOutWeapon);
		return;
	}

	const FMKHItemEntry* CurrentWeapon = FindCurrentWeapon(Character);
	const int64 CurrentWeaponID = CurrentWeapon != nullptr ? CurrentWeapon->ItemID : 0;
	for (const FGameplayTag& SlotTag : FMKHDefaultQuickSlots::GetWeaponSlots())
	{
		const int64 ItemID = QuickSlots->GetQuickSlotItemID(SlotTag);
		if (ItemID == 0 || ItemID == CurrentWeaponID)
		{
			continue;
		}
		if (ItemID != InOutWeapon.ItemID)
		{
			// The inventory hands out a copy, empty if the slot points to an item no longer in the inventory.
			InOutWeapon = Inventory->FindInventoryEntryByID(ItemID).Item;
		}
		return;
	}
	CopyEntryIfChanged(nullptr, InOutWeapon);
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void FMKHDuelBotContextEvaluator::CopyEntryIfChanged(const FMKHItemEntry* Source, FMKHItemEntry& Target)
{
	if (Source == nullptr)
	{
		if (Target.IsValid())
		{
			Target = FMKHItemEntry();
		}
		return;
	}
	if (Target.ItemID != Source->ItemID)
	{
		Target = *Source;
	}
}

void FMKHDuelBotContextEvaluator::ResetOutputs(FMKHDuelBotContextEvaluatorInstanceData& Data)
{
	Data.Tuning = FMKHDuelBotTuning();
	Data.BotCharacter = nullptr;
	Data.Opponent = nullptr;
	Data.Distance = 0.f;
	Data.DistanceBand = EMKHDuelDistanceBand::Far;
	Data.ApproachStopDistance = FMath::Max(Data.CombatBandMaxDistance - Data.BandHysteresis, 0.f);
	Data.bCanFight = false;
	Data.bWonRound = false;
	Data.bShouldTaunt = false;
}

void FMKHDuelBotContextEvaluator::UpdateCombatMetrics(const FStateTreeExecutionContext& Context,
	const ACharacterBase& BotCharacter, const ACharacterBase& Opponent, const EMKHDuelDistanceBand PreviousBand,
	FMKHDuelBotContextEvaluatorInstanceData& Data)
{
	Data.Distance = BotCharacter.GetDistanceTo(&Opponent);
	Data.DistanceBand = ComputeDistanceBand(Data.Distance, PreviousBand, Data);

	const UWorld* World = Context.GetWorld();
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	Data.bCanFight = IsValid(GameState) && GameState->IsDuelRunning()
		&& !AMKHDuelBotController::IsCharacterDead(&BotCharacter)
		&& !AMKHDuelBotController::IsCharacterDead(&Opponent);

	// The opponent stays dead from the decisive death to the round reset, and on the field after the match is over.
	Data.bWonRound = !AMKHDuelBotController::IsCharacterDead(&BotCharacter)
		&& AMKHDuelBotController::IsCharacterDead(&Opponent);
}

void FMKHDuelBotContextEvaluator::UpdateTaunt(const bool bWonLastTick, const bool bTauntedLastTick,
	FMKHDuelBotContextEvaluatorInstanceData& Data)
{
	if (!Data.bWonRound)
	{
		return;
	}

	// One roll per won round, as the reaction roll of the Perception evaluator.
	Data.bShouldTaunt = bWonLastTick ? bTauntedLastTick : FMath::FRand() < Data.Tuning.TauntChance;
	if (!bWonLastTick)
	{
		UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotContextEvaluator: round won, taunt %s (chance %.2f)."),
			Data.bShouldTaunt ? TEXT("yes") : TEXT("no"), Data.Tuning.TauntChance);
	}
}

EMKHDuelDistanceBand FMKHDuelBotContextEvaluator::ComputeDistanceBand(const float Distance,
	const EMKHDuelDistanceBand CurrentBand, const FMKHDuelBotContextEvaluatorInstanceData& Data)
{
	const float CombatLimit = Data.CombatBandMaxDistance + (CurrentBand == EMKHDuelDistanceBand::Combat ? Data.BandHysteresis : 0.f);
	const float SafeLimit = Data.SafeBandMaxDistance + (CurrentBand != EMKHDuelDistanceBand::Far ? Data.BandHysteresis : 0.f);

	if (Distance <= CombatLimit)
	{
		return EMKHDuelDistanceBand::Combat;
	}
	return Distance <= SafeLimit ? EMKHDuelDistanceBand::Safe : EMKHDuelDistanceBand::Far;
}
