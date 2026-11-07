// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Spawn/MKHLootTable.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/MKHDataSubsystem.h"
#include "Libraries/EquipmentRollLibrary.h"
#include "MKHLogChannels.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "MKHLootTable"

namespace
{
	/** Whether an item tag belongs to the equipment category (single items with a rolled rarity). */
	bool IsEquipmentTag(const FGameplayTag& ItemTag)
	{
		return ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Equipment);
	}
}

FMKHLootRoll UMKHLootTable::RollEntries(const UObject* WorldContextObject, const TArray<FMKHLootEntry>& InEntries)
{
	FMKHLootRoll Roll;

	TArray<float> Weights;
	Weights.Reserve(InEntries.Num());
	for (const FMKHLootEntry& Entry : InEntries)
	{
		Weights.Add(Entry.Weight);
	}

	const int32 EntryIndex = UEquipmentRollLibrary::WeightedRandomSelect(Weights);
	if (!InEntries.IsValidIndex(EntryIndex))
	{
		return Roll;
	}

	const FMKHLootEntry& Entry = InEntries[EntryIndex];
	const TArray<FGameplayTag> Candidates = ResolveCandidateItems(WorldContextObject, Entry.ItemTag);
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHLootTable: loot tag '%s' matches no item definition."), *Entry.ItemTag.ToString());
		return Roll;
	}

	Roll.ItemTag = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	Roll.Quantity = RollQuantity(Entry, Roll.ItemTag);
	Roll.RarityRule = Entry.RarityRule;
	return Roll;
}

TArray<FGameplayTag> UMKHLootTable::ResolveCandidateItems(const UObject* WorldContextObject, const FGameplayTag& EntryTag)
{
	if (!EntryTag.IsValid())
	{
		return TArray<FGameplayTag>();
	}

	// An exact item definition wins over a category of the same name.
	if (UMKHDataSubsystem::FindItemDefinitionForContext(WorldContextObject, EntryTag))
	{
		return { EntryTag };
	}

	return UMKHDataSubsystem::GetItemTagsMatchingForContext(WorldContextObject, EntryTag);
}

int32 UMKHLootTable::RollQuantity(const FMKHLootEntry& Entry, const FGameplayTag& ItemTag)
{
	if (IsEquipmentTag(ItemTag))
	{
		return 1;
	}

	const int32 MinQuantity = FMath::Max(Entry.QuantityMin, 1);
	return FMath::RandRange(MinQuantity, FMath::Max(Entry.QuantityMax, MinQuantity));
}

#if WITH_EDITOR

EDataValidationResult UMKHLootTable::ValidateEntries(const TArray<FMKHLootEntry>& InEntries, const FString& OwnerName, FDataValidationContext& Context)
{
	if (InEntries.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("NoEntries", "{0}: the loot list is empty, nothing can spawn."), FText::FromString(OwnerName)));
		return EDataValidationResult::Invalid;
	}

	bool bValid = true;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < InEntries.Num(); ++Index)
	{
		const FString Label = FString::Printf(TEXT("%s, loot entry %d"), *OwnerName, Index);
		bValid &= ValidateEntry(InEntries[Index], Label, Context);
		TotalWeight += FMath::Max(InEntries[Index].Weight, 0.f);
	}

	if (TotalWeight <= 0.f)
	{
		Context.AddError(FText::Format(LOCTEXT("ZeroWeights", "{0}: every loot weight is 0, nothing can spawn."), FText::FromString(OwnerName)));
		bValid = false;
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

EDataValidationResult UMKHLootTable::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	const EDataValidationResult EntriesResult = ValidateEntries(Entries, GetName(), Context);
	return CombineDataValidationResults(SuperResult, EntriesResult);
}

bool UMKHLootTable::ValidateEntry(const FMKHLootEntry& Entry, const FString& Label, FDataValidationContext& Context)
{
	const FText LabelText = FText::FromString(Label);
	const TArray<FGameplayTag> Candidates = ResolveCandidateItems(nullptr, Entry.ItemTag);
	if (Candidates.IsEmpty())
	{
		Context.AddError(FText::Format(LOCTEXT("UnknownItem", "{0}: '{1}' matches no item definition."),
			LabelText, FText::FromString(Entry.ItemTag.ToString())));
		return false;
	}

	if (Entry.Weight <= 0.f)
	{
		Context.AddWarning(FText::Format(LOCTEXT("ZeroWeight", "{0}: weight 0, this entry is never rolled."), LabelText));
	}

	if (Entry.QuantityMax < Entry.QuantityMin)
	{
		Context.AddError(FText::Format(LOCTEXT("QuantityRange", "{0}: QuantityMax is lower than QuantityMin."), LabelText));
		return false;
	}

	const bool bOnlyEquipment = !Candidates.ContainsByPredicate([](const FGameplayTag& Tag) { return !IsEquipmentTag(Tag); });
	if (bOnlyEquipment && Entry.QuantityMax > 1)
	{
		Context.AddWarning(FText::Format(LOCTEXT("EquipmentQuantity", "{0}: equipment always spawns a single item, the quantity is ignored."), LabelText));
	}

	return ValidateRarityRule(Entry, Candidates, Label, Context);
}

bool UMKHLootTable::ValidateRarityRule(const FMKHLootEntry& Entry, const TArray<FGameplayTag>& CandidateItems, const FString& Label,
	FDataValidationContext& Context)
{
	const FMKHItemRarityRule& Rule = Entry.RarityRule;
	if (Rule.Mode == EMKHItemRarityMode::Default)
	{
		return true;
	}

	const FText LabelText = FText::FromString(Label);
	if (!CandidateItems.ContainsByPredicate(&IsEquipmentTag))
	{
		Context.AddWarning(FText::Format(LOCTEXT("RarityOnConsumable", "{0}: the rarity rule is ignored, only equipment rolls a rarity (consumables keep the rarity of their definition)."), LabelText));
		return true;
	}

	FRarityDefinition Unused;
	if (Rule.Mode == EMKHItemRarityMode::Fixed && !UMKHDataSubsystem::FindRarityDefinitionForContext(nullptr, Rule.FixedRarityTag, Unused))
	{
		Context.AddError(FText::Format(LOCTEXT("UnknownFixedRarity", "{0}: fixed rarity '{1}' is not in the rarity table."),
			LabelText, FText::FromString(Rule.FixedRarityTag.ToString())));
		return false;
	}

	if (Rule.Mode == EMKHItemRarityMode::Weighted)
	{
		bool bAnyRollable = false;
		for (const TPair<FGameplayTag, float>& Pair : Rule.RarityWeights)
		{
			if (!UMKHDataSubsystem::FindRarityDefinitionForContext(nullptr, Pair.Key, Unused))
			{
				Context.AddWarning(FText::Format(LOCTEXT("UnknownWeightedRarity", "{0}: rarity '{1}' is not in the rarity table and is never rolled."),
					LabelText, FText::FromString(Pair.Key.ToString())));
				continue;
			}
			bAnyRollable |= Pair.Value > 0.f;
		}

		if (!bAnyRollable)
		{
			Context.AddError(FText::Format(LOCTEXT("NoWeightedRarity", "{0}: the weighted rarity rule has no rollable rarity."), LabelText));
			return false;
		}
	}

	return true;
}

#endif

#undef LOCTEXT_NAMESPACE
