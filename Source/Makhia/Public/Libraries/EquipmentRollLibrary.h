// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "EquipmentRollLibrary.generated.h"

class UDataTable;
class UGameplayEffect;
class UGameplayAbility;
class UEquipmentDefinition;
class UEquipmentStatEffects;
struct FEquipmentStatEffectDefinition;
struct FEquipmentAbilityDefinition;
struct FRarityDefinition;

/**
 * Static library that handles all equipment roll logic: rarity selection,
 * passive stat rolling, and active ability rolling.
 */
UCLASS()
class MAKHIA_API UEquipmentRollLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Selects a rarity tag from the RarityTable using weighted probability.
	 * Returns the FRarityDefinition pointer for the selected row (nullptr on failure).
	 */
	static const FRarityDefinition* RollRarity(const UDataTable* RarityTable);

	/**
	 * Finds the rarity row matching the given rarity tag.
	 * Used when an item has a pre-defined rarity and the roll must be skipped.
	 *
	 * @param RarityTable The rarity DataTable to search.
	 * @param RarityTag   The rarity tag to match against each row's RarityTag.
	 * @return The matching FRarityDefinition pointer, or nullptr if not found.
	 */
	static const FRarityDefinition* GetRarityByTag(const UDataTable* RarityTable, const FGameplayTag& RarityTag);

	/**
	 * Rolls a rarity among the table rows whose tag is in AllowedRarities, using the table's RollProbability weights.
	 * When every allowed rarity weighs 0 they are picked with equal chance.
	 *
	 * @param RarityTable      The rarity DataTable to roll from.
	 * @param AllowedRarities  Exact rarity tags that may be selected.
	 * @return The selected rarity row, or nullptr when no allowed rarity is in the table.
	 */
	static const FRarityDefinition* RollRarityAmong(const UDataTable* RarityTable, const FGameplayTagContainer& AllowedRarities);

	/**
	 * Selects a rarity with a weighted roll restricted to the given rarities (instead of the table's RollProbability).
	 *
	 * @param RarityTable The rarity DataTable the rarities are looked up in.
	 * @param Weights     Relative weight per rarity tag; tags missing from the table or weighted <= 0 are skipped.
	 * @return The selected rarity row, or nullptr when none of the listed rarities can be rolled.
	 */
	static const FRarityDefinition* RollRarityWeighted(const UDataTable* RarityTable, const TMap<FGameplayTag, float>& Weights);

	/**
	 * Performs a weighted random selection over an array of weights.
	 * @param Weights  Array of positive weights (one per candidate).
	 * @return Index of the selected element, or -1 if the array is empty or every weight is <= 0.
	 */
	static int32 WeightedRandomSelect(const TArray<float>& Weights);

private:

	/**
	 * Copies a stat definition and rolls its CurrentValue in [MinStatLevel, MaxStatLevel].
	 *
	 * @param Definition  The selected stat row.
	 * @return The rolled stat, ready for the item's effect package.
	 */
	static FEquipmentStatEffectDefinition MakeRolledStat(const FEquipmentStatEffectDefinition& Definition);

	/**
	 * Removes from the stat pool every candidate that applies the given effect, keeping weights aligned.
	 *
	 * @param CandidatePool     Remaining stat candidates.
	 * @param CandidateWeights  Selection weight of each candidate (same indices).
	 * @param EffectClass       Effect of the stat just rolled.
	 */
	static void RemoveStatCandidatesWithEffect(TArray<const FEquipmentStatEffectDefinition*>& CandidatePool,
		TArray<float>& CandidateWeights, const TSoftClassPtr<UGameplayEffect>& EffectClass);

	/**
	 * Removes from the ability pool every candidate that grants the given ability class, keeping weights aligned.
	 *
	 * @param CandidatePool     Remaining ability candidates.
	 * @param CandidateWeights  Selection weight of each candidate (same indices).
	 * @param AbilityClass      Ability just rolled.
	 */
	static void RemoveAbilityCandidatesWithClass(TArray<const FEquipmentAbilityDefinition*>& CandidatePool,
		TArray<float>& CandidateWeights, const TSoftClassPtr<UGameplayAbility>& AbilityClass);

public:

	/**
	 * Rolls passive stat effects for an equipment item, never the same perk twice: once a stat is picked,
	 * every candidate applying the same GameplayEffect leaves the pool.
	 * @param EquipmentCDO  The equipment CDO whose PossibleStatRolls define the candidate pool.
	 * @param StatData      The master stat data asset containing DataTables per tag category.
	 * @param NumStats      How many passive stats to attempt rolling (driven by rarity); fewer are returned when
	 *                      the pool has fewer distinct perks.
	 * @return Array of rolled stat effect definitions with their CurrentValue already set.
	 */
	static TArray<FEquipmentStatEffectDefinition> RollPassiveStats(
		const UEquipmentDefinition* EquipmentCDO,
		const UEquipmentStatEffects* StatData,
		int32 NumStats);

	/**
	 * Rolls active abilities for a weapon, never the same ability twice (candidates granting an ability class
	 * already rolled leave the pool).
	 * @param EquipmentCDO  The equipment CDO whose PossibleAbilityRolls define the candidate pool.
	 * @param StatData      The master stat data asset containing DataTables per tag category.
	 * @param NumAbilities  How many active abilities to attempt rolling (driven by rarity); fewer are returned when
	 *                      the pool has fewer distinct abilities.
	 * @return Array of rolled ability definitions.
	 */
	static TArray<FEquipmentAbilityDefinition> RollActiveAbilities(
		const UEquipmentDefinition* EquipmentCDO,
		const UEquipmentStatEffects* StatData,
		int32 NumAbilities);

	/**
	 * Resolves explicit ability tags into ability definitions.
	 * @param AbilityTags Container of ability tags to resolve.
	 * @param StatData    The master stat data asset containing DataTables per tag category.
	 * @return Array of resolved ability definitions (one per matching tag row).
	 */
	static TArray<FEquipmentAbilityDefinition> ResolveAbilitiesByTags(
		const FGameplayTagContainer& AbilityTags,
		const UEquipmentStatEffects* StatData);
};
