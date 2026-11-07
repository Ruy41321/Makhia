// Fill out your copyright notice in the Description page of Project Settings.

#include "Libraries/EquipmentRollLibrary.h"

#include "Equipment/Rarity/RarityDefinition.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentTypes.h"
#include "Data/EquipmentStatEffects.h"
#include "Libraries/MKHAbilitySystemLibrary.h"
#include "Engine/DataTable.h"

int32 UEquipmentRollLibrary::WeightedRandomSelect(const TArray<float>& Weights)
{
	if (Weights.Num() == 0)
	{
		return -1;
	}

	float TotalWeight = 0.f;
	for (const float W : Weights)
	{
		TotalWeight += W;
	}

	if (TotalWeight <= 0.f)
	{
		return -1;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	float Accumulator = 0.f;

	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		Accumulator += Weights[i];
		if (Roll <= Accumulator)
		{
			return i;
		}
	}

	return Weights.Num() - 1;
}

const FRarityDefinition* UEquipmentRollLibrary::RollRarity(const UDataTable* RarityTable)
{
	if (!RarityTable)
	{
		return nullptr;
	}

	TArray<FRarityDefinition*> Rows;
	RarityTable->GetAllRows<FRarityDefinition>(TEXT("RollRarity"), Rows);

	float TotalWeight = 0.f;
	for (const FRarityDefinition* Row : Rows)
	{
		TotalWeight += Row->RollProbability;
	}

	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	float Accumulator = 0.f;

	for (const FRarityDefinition* Row : Rows)
	{
		Accumulator += Row->RollProbability;
		if (Roll <= Accumulator)
		{
			return Row;
		}
	}

	// Fallback: return the last row
	return Rows.Last();
}

const FRarityDefinition* UEquipmentRollLibrary::GetRarityByTag(const UDataTable* RarityTable, const FGameplayTag& RarityTag)
{
	if (!RarityTable || !RarityTag.IsValid())
	{
		return nullptr;
	}

	TArray<FRarityDefinition*> Rows;
	RarityTable->GetAllRows<FRarityDefinition>(TEXT("GetRarityByTag"), Rows);

	for (const FRarityDefinition* Row : Rows)
	{
		if (Row && Row->RarityTag.MatchesTagExact(RarityTag))
		{
			return Row;
		}
	}

	return nullptr;
}

const FRarityDefinition* UEquipmentRollLibrary::RollRarityAmong(const UDataTable* RarityTable, const FGameplayTagContainer& AllowedRarities)
{
	if (!RarityTable || AllowedRarities.IsEmpty())
	{
		return nullptr;
	}

	TArray<FRarityDefinition*> Rows;
	RarityTable->GetAllRows<FRarityDefinition>(TEXT("RollRarityAmong"), Rows);

	TArray<const FRarityDefinition*> Candidates;
	TArray<float> CandidateWeights;
	for (const FRarityDefinition* Row : Rows)
	{
		if (Row && AllowedRarities.HasTagExact(Row->RarityTag))
		{
			Candidates.Add(Row);
			CandidateWeights.Add(Row->RollProbability);
		}
	}

	int32 SelectedIndex = WeightedRandomSelect(CandidateWeights);
	if (SelectedIndex == INDEX_NONE && !Candidates.IsEmpty())
	{
		// Every allowed rarity weighs 0 (e.g. a rarity never rolled naturally): pick one uniformly.
		SelectedIndex = FMath::RandRange(0, Candidates.Num() - 1);
	}

	return Candidates.IsValidIndex(SelectedIndex) ? Candidates[SelectedIndex] : nullptr;
}

const FRarityDefinition* UEquipmentRollLibrary::RollRarityWeighted(const UDataTable* RarityTable, const TMap<FGameplayTag, float>& Weights)
{
	TArray<const FRarityDefinition*> Candidates;
	TArray<float> CandidateWeights;

	for (const TPair<FGameplayTag, float>& Pair : Weights)
	{
		const FRarityDefinition* Rarity = Pair.Value > 0.f ? GetRarityByTag(RarityTable, Pair.Key) : nullptr;
		if (Rarity)
		{
			Candidates.Add(Rarity);
			CandidateWeights.Add(Pair.Value);
		}
	}

	const int32 SelectedIndex = WeightedRandomSelect(CandidateWeights);
	return Candidates.IsValidIndex(SelectedIndex) ? Candidates[SelectedIndex] : nullptr;
}

TArray<FEquipmentStatEffectDefinition> UEquipmentRollLibrary::RollPassiveStats(
	const UEquipmentDefinition* EquipmentCDO,
	const UEquipmentStatEffects* StatData,
	int32 NumStats)
{
	TArray<FEquipmentStatEffectDefinition> Result;

	if (!EquipmentCDO || !StatData || NumStats <= 0)
	{
		return Result;
	}

	// Build the weighted candidate pool from PossibleStatRolls
	TArray<const FEquipmentStatEffectDefinition*> CandidatePool;
	TArray<float> CandidateWeights;

	const FGameplayTagContainer& PossibleTags = EquipmentCDO->PossibleStatRolls;

	for (int32 i = 0; i < PossibleTags.Num(); ++i)
	{
		const FGameplayTag& Tag = PossibleTags.GetByIndex(i);

		for (const auto& Pair : StatData->MasterStatMap)
		{
			if (!Tag.MatchesTag(Pair.Key))
			{
				continue;
			}

			const FEquipmentStatEffectDefinition* PossibleStat =
				UMKHAbilitySystemLibrary::GetDataTableRowByTag<FEquipmentStatEffectDefinition>(Pair.Value, Tag);

			if (PossibleStat && PossibleStat->ProbabilityToSelect > 0.f)
			{
				CandidatePool.Add(PossibleStat);
				CandidateWeights.Add(PossibleStat->ProbabilityToSelect);
			}
			break;
		}
	}

	if (CandidatePool.Num() == 0)
	{
		return Result;
	}

	// Roll up to NumStats distinct stats: each pick leaves the pool, so an item never gets the same perk twice.
	for (int32 i = 0; i < NumStats; ++i)
	{
		const int32 SelectedIndex = WeightedRandomSelect(CandidateWeights);
		if (SelectedIndex < 0)
		{
			break;
		}

		const FEquipmentStatEffectDefinition* Selected = CandidatePool[SelectedIndex];
		Result.Add(MakeRolledStat(*Selected));
		RemoveStatCandidatesWithEffect(CandidatePool, CandidateWeights, Selected->EffectClass);
	}

	return Result;
}

FEquipmentStatEffectDefinition UEquipmentRollLibrary::MakeRolledStat(const FEquipmentStatEffectDefinition& Definition)
{
	FEquipmentStatEffectDefinition NewStat = Definition;
	NewStat.CurrentValue = Definition.bFractionalStat
		? FMath::FRandRange(Definition.MinStatLevel, Definition.MaxStatLevel)
		: static_cast<float>(FMath::TruncToInt(FMath::RandRange(Definition.MinStatLevel, Definition.MaxStatLevel)));
	return NewStat;
}

void UEquipmentRollLibrary::RemoveStatCandidatesWithEffect(TArray<const FEquipmentStatEffectDefinition*>& CandidatePool,
	TArray<float>& CandidateWeights, const TSoftClassPtr<UGameplayEffect>& EffectClass)
{
	// Same effect = same perk, even when it comes from another row/tier (e.g. LowTier and HighTier ArmorUp).
	for (int32 Index = CandidatePool.Num() - 1; Index >= 0; --Index)
	{
		if (CandidatePool[Index]->EffectClass == EffectClass)
		{
			CandidatePool.RemoveAtSwap(Index);
			CandidateWeights.RemoveAtSwap(Index);
		}
	}
}

TArray<FEquipmentAbilityDefinition> UEquipmentRollLibrary::RollActiveAbilities(
	const UEquipmentDefinition* EquipmentCDO,
	const UEquipmentStatEffects* StatData,
	int32 NumAbilities)
{
	TArray<FEquipmentAbilityDefinition> Result;

	if (!EquipmentCDO || !StatData || NumAbilities <= 0)
	{
		return Result;
	}

	// Build the weighted candidate pool from PossibleAbilityRolls
	TArray<const FEquipmentAbilityDefinition*> CandidatePool;
	TArray<float> CandidateWeights;

	const FGameplayTagContainer& PossibleTags = EquipmentCDO->PossibleAbilityRolls;

	for (int32 i = 0; i < PossibleTags.Num(); ++i)
	{
		const FGameplayTag& Tag = PossibleTags.GetByIndex(i);

		for (const auto& Pair : StatData->MasterStatMap)
		{
			if (!Tag.MatchesTag(Pair.Key))
			{
				continue;
			}

			const FEquipmentAbilityDefinition* PossibleAbility =
				UMKHAbilitySystemLibrary::GetDataTableRowByTag<FEquipmentAbilityDefinition>(Pair.Value, Tag);

			if (PossibleAbility && PossibleAbility->ProbabilityToSelect > 0.f)
			{
				CandidatePool.Add(PossibleAbility);
				CandidateWeights.Add(PossibleAbility->ProbabilityToSelect);
			}
			break;
		}
	}

	if (CandidatePool.Num() == 0)
	{
		return Result;
	}

	// Roll up to NumAbilities distinct abilities: each pick leaves the pool, so a weapon never gets one twice.
	for (int32 i = 0; i < NumAbilities; ++i)
	{
		const int32 SelectedIndex = WeightedRandomSelect(CandidateWeights);
		if (SelectedIndex < 0)
		{
			break;
		}

		const FEquipmentAbilityDefinition* Selected = CandidatePool[SelectedIndex];
		Result.Add(*Selected);
		RemoveAbilityCandidatesWithClass(CandidatePool, CandidateWeights, Selected->AbilityClass);
	}

	return Result;
}

void UEquipmentRollLibrary::RemoveAbilityCandidatesWithClass(TArray<const FEquipmentAbilityDefinition*>& CandidatePool,
	TArray<float>& CandidateWeights, const TSoftClassPtr<UGameplayAbility>& AbilityClass)
{
	// Same ability class = same ability, even when it comes from another row.
	for (int32 Index = CandidatePool.Num() - 1; Index >= 0; --Index)
	{
		if (CandidatePool[Index]->AbilityClass == AbilityClass)
		{
			CandidatePool.RemoveAtSwap(Index);
			CandidateWeights.RemoveAtSwap(Index);
		}
	}
}

TArray<FEquipmentAbilityDefinition> UEquipmentRollLibrary::ResolveAbilitiesByTags(
	const FGameplayTagContainer& AbilityTags,
	const UEquipmentStatEffects* StatData)
{
	TArray<FEquipmentAbilityDefinition> Result;

	if (!StatData || AbilityTags.IsEmpty())
	{
		return Result;
	}

	for (int32 i = 0; i < AbilityTags.Num(); ++i)
	{
		const FGameplayTag& Tag = AbilityTags.GetByIndex(i);

		for (const auto& Pair : StatData->MasterStatMap)
		{
			if (!Tag.MatchesTag(Pair.Key))
			{
				continue;
			}

			if (const FEquipmentAbilityDefinition* AbilityDef =
				UMKHAbilitySystemLibrary::GetDataTableRowByTag<FEquipmentAbilityDefinition>(Pair.Value, Tag))
			{
				Result.Add(*AbilityDef);
			}

			break;
		}
	}

	return Result;
}

