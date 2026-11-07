// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Data/MKHDataSubsystem.h"

#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Abilities/MKHDamageAbility.h"
#include "Animation/AnimMontage.h"
#include "Data/EquipmentStatEffects.h"
#include "Career/MKHCareerLadder.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Data/MKHDataSettings.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/ItemTypesToTables.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Libraries/EquipmentRollLibrary.h"
#include "MKHLogChannels.h"

namespace
{
	/**
	 * Synchronously loads one asset or class referenced by UMKHDataSettings, warning when it is missing.
	 *
	 * @param SoftAsset   Soft reference (TSoftObjectPtr / TSoftClassPtr) read from the settings.
	 * @param SettingName Name of the settings field, used in log messages.
	 * @return The loaded asset or class, or nullptr if unset or not loadable.
	 */
	template <typename TSoftReference>
	auto LoadConfiguredAsset(const TSoftReference& SoftAsset, const TCHAR* SettingName) -> decltype(SoftAsset.LoadSynchronous())
	{
		if (SoftAsset.IsNull())
		{
			UE_LOG(LogMKHData, Warning, TEXT("UMKHDataSubsystem: '%s' is not set in Project Settings -> Game -> Makhia Data."), SettingName);
			return nullptr;
		}

		auto* Asset = SoftAsset.LoadSynchronous();
		if (!IsValid(Asset))
		{
			UE_LOG(LogMKHData, Warning, TEXT("UMKHDataSubsystem: failed to load '%s' (%s)."), SettingName, *SoftAsset.ToString());
		}
		return Asset;
	}
}

void UMKHDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMKHDataSettings* Settings = UMKHDataSettings::Get();
	if (!IsValid(Settings))
	{
		UE_LOG(LogMKHData, Warning, TEXT("UMKHDataSubsystem::Initialize: UMKHDataSettings unavailable, no data loaded."));
		return;
	}

	ItemDefinitions = LoadConfiguredAsset(Settings->ItemDefinitions, TEXT("ItemDefinitions"));
	PickupItemClass = LoadConfiguredAsset(Settings->PickupItemClass, TEXT("PickupItemClass"));
	RarityTable = LoadConfiguredAsset(Settings->RarityTable, TEXT("RarityTable"));
	EquipmentStatEffects = LoadConfiguredAsset(Settings->EquipmentStatEffects, TEXT("EquipmentStatEffects"));
	AbilityTuningTable = LoadConfiguredAsset(Settings->AbilityTuningTable, TEXT("AbilityTuningTable"));
	SkinCatalog = LoadConfiguredAsset(Settings->SkinCatalog, TEXT("SkinCatalog"));
	CareerLadder = LoadConfiguredAsset(Settings->CareerLadder, TEXT("CareerLadder"));

#if WITH_EDITOR
	ValidateAbilityTuning();
#endif
}

UMKHDataSubsystem* UMKHDataSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(GEngine)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHDataSubsystem>() : nullptr;
}

const FMasterItemDefinition* UMKHDataSubsystem::FindItemDefinition(const FGameplayTag& ItemTag) const
{
	return FindItemDefinitionIn(ItemDefinitions, ItemTag);
}

TArray<FGameplayTag> UMKHDataSubsystem::GetItemTagsMatching(const FGameplayTag& ParentTag) const
{
	return CollectItemTagsIn(ItemDefinitions, ParentTag);
}

const FMasterItemDefinition* UMKHDataSubsystem::FindItemDefinitionForContext(const UObject* WorldContextObject, const FGameplayTag& ItemTag)
{
	return FindItemDefinitionIn(ResolveItemDefinitionsForContext(WorldContextObject), ItemTag);
}

TArray<FGameplayTag> UMKHDataSubsystem::GetItemTagsMatchingForContext(const UObject* WorldContextObject, const FGameplayTag& ParentTag)
{
	return CollectItemTagsIn(ResolveItemDefinitionsForContext(WorldContextObject), ParentTag);
}

FText UMKHDataSubsystem::GetItemCategoryName(const FGameplayTag& ItemTag) const
{
	return FindCategoryNameIn(ItemDefinitions, ItemTag);
}

FText UMKHDataSubsystem::GetItemCategoryNameForContext(const UObject* WorldContextObject, const FGameplayTag& ItemTag)
{
	return FindCategoryNameIn(ResolveItemDefinitionsForContext(WorldContextObject), ItemTag);
}

bool UMKHDataSubsystem::GetItemDefinition(const FGameplayTag& ItemTag, FMasterItemDefinition& OutDefinition) const
{
	const FMasterItemDefinition* ItemDef = FindItemDefinition(ItemTag);
	OutDefinition = ItemDef ? *ItemDef : FMasterItemDefinition();
	return ItemDef != nullptr;
}

const FRarityDefinition* UMKHDataSubsystem::FindRarityDefinition(const FGameplayTag& RarityTag) const
{
	return UEquipmentRollLibrary::GetRarityByTag(RarityTable, RarityTag);
}

int32 UMKHDataSubsystem::GetRarityRank(const FGameplayTag& RarityTag) const
{
	if (!RarityTag.IsValid() || !IsValid(RarityTable))
	{
		return INDEX_NONE;
	}

	int32 Rank = 0;
	for (const TPair<FName, uint8*>& Row : RarityTable->GetRowMap())
	{
		if (reinterpret_cast<const FRarityDefinition*>(Row.Value)->RarityTag == RarityTag)
		{
			return Rank;
		}
		++Rank;
	}
	return INDEX_NONE;
}

bool UMKHDataSubsystem::GetRarityDefinition(const FGameplayTag& RarityTag, FRarityDefinition& OutDefinition) const
{
	const FRarityDefinition* Rarity = FindRarityDefinition(RarityTag);
	OutDefinition = Rarity ? *Rarity : FRarityDefinition();
	return Rarity != nullptr;
}

bool UMKHDataSubsystem::FindRarityDefinitionForContext(const UObject* WorldContextObject, const FGameplayTag& RarityTag, FRarityDefinition& OutDefinition)
{
	const FRarityDefinition* Rarity = nullptr;
	const UMKHDataSubsystem* Data = Get(WorldContextObject);
	if (IsValid(Data))
	{
		Rarity = Data->FindRarityDefinition(RarityTag);
	}
#if WITH_EDITOR
	else
	{
		// No GameInstance (widget designer preview): read the same table straight from the project settings.
		const UMKHDataSettings* Settings = UMKHDataSettings::Get();
		const UDataTable* Table = IsValid(Settings) ? Settings->RarityTable.LoadSynchronous() : nullptr;
		Rarity = UEquipmentRollLibrary::GetRarityByTag(Table, RarityTag);
	}
#endif

	OutDefinition = Rarity ? *Rarity : FRarityDefinition();
	return Rarity != nullptr;
}

const FMKHAbilityTuningRow* UMKHDataSubsystem::FindAbilityTuning(const FGameplayTag& AbilityTag) const
{
	if (!IsValid(AbilityTuningTable) || !AbilityTag.IsValid())
	{
		return nullptr;
	}

	// No DataTable warning on a miss: callers decide whether a missing row is an error.
	return AbilityTuningTable->FindRow<FMKHAbilityTuningRow>(AbilityTag.GetTagName(), TEXT("FindAbilityTuning"), false);
}

bool UMKHDataSubsystem::GetAbilityTuning(const FGameplayTag& AbilityTag, FMKHAbilityTuningRow& OutTuning) const
{
	const FMKHAbilityTuningRow* Tuning = FindAbilityTuning(AbilityTag);
	OutTuning = Tuning ? *Tuning : FMKHAbilityTuningRow();
	return Tuning != nullptr;
}

const FMKHAbilityTiming* UMKHDataSubsystem::FindAbilityTiming(const FGameplayTag& AbilityTag) const
{
	if (const FMKHAbilityTiming* CachedTiming = AbilityTimingCache.Find(AbilityTag))
	{
		return CachedTiming;
	}

	// Get(), never LoadSynchronous: this runs in game (AI evaluators). An unloaded class is retried on the next call.
	const FEquipmentAbilityDefinition* Definition = FindAbilityDefinition(AbilityTag);
	const UClass* AbilityClass = Definition != nullptr ? Definition->AbilityClass.Get() : nullptr;
	if (!IsValid(AbilityClass))
	{
		return nullptr;
	}

	const UMKHGameplayAbility* AbilityCDO = Cast<UMKHGameplayAbility>(AbilityClass->GetDefaultObject());
	const UAnimMontage* Montage = IsValid(AbilityCDO) ? AbilityCDO->GetMontageToPlay() : nullptr;
	FMKHAbilityTiming Timing = IsValid(Montage) ? FMKHAbilityTiming::BuildFromMontage(*Montage) : FMKHAbilityTiming();
	if (!Timing.HasStrikes())
	{
		UE_LOG(LogMKHData, Warning, TEXT("UMKHDataSubsystem: %s (%s) has no HitScan.Start / SpawnProjectile notify in its montage %s: no strike timing."),
			*AbilityTag.ToString(), *GetNameSafe(AbilityClass), *GetNameSafe(Montage));
	}
	return &AbilityTimingCache.Add(AbilityTag, MoveTemp(Timing));
}

bool UMKHDataSubsystem::GetAbilityTiming(const FGameplayTag& AbilityTag, FMKHAbilityTiming& OutTiming) const
{
	const FMKHAbilityTiming* Timing = FindAbilityTiming(AbilityTag);
	OutTiming = Timing ? *Timing : FMKHAbilityTiming();
	return Timing != nullptr;
}

const FPerkDefinition* UMKHDataSubsystem::FindPerkDefinition(const FGameplayTag& PerkTag) const
{
	if (!IsValid(EquipmentStatEffects) || !PerkTag.IsValid())
	{
		return nullptr;
	}

	// Same table lookup as UMKHAbilitySystemLibrary::ApplyStatusEffects.
	for (const TPair<FGameplayTag, TObjectPtr<UDataTable>>& Pair : EquipmentStatEffects->MasterStatMap)
	{
		if (MKHGameplayTags::Misc::PerkDataTable.GetTag().MatchesTag(Pair.Key) && IsValid(Pair.Value))
		{
			return Pair.Value->FindRow<FPerkDefinition>(PerkTag.GetTagName(), TEXT("FindPerkDefinition"), false);
		}
	}
	return nullptr;
}

const UItemTypesToTables* UMKHDataSubsystem::ResolveItemDefinitionsForContext(const UObject* WorldContextObject)
{
	const UMKHDataSubsystem* Data = Get(WorldContextObject);
	if (IsValid(Data))
	{
		return Data->ItemDefinitions;
	}

#if WITH_EDITOR
	// No GameInstance (editor preview or validation): read the same tables straight from the project settings.
	const UMKHDataSettings* Settings = UMKHDataSettings::Get();
	return IsValid(Settings) ? Settings->ItemDefinitions.LoadSynchronous() : nullptr;
#else
	return nullptr;
#endif
}

const FMasterItemDefinition* UMKHDataSubsystem::FindItemDefinitionIn(const UItemTypesToTables* Definitions, const FGameplayTag& ItemTag)
{
	if (!IsValid(Definitions) || !ItemTag.IsValid())
	{
		return nullptr;
	}

	for (const TPair<FGameplayTag, TObjectPtr<UDataTable>>& Pair : Definitions->TagsToTables)
	{
		if (!ItemTag.MatchesTag(Pair.Key) || !IsValid(Pair.Value))
		{
			continue;
		}

		// No DataTable warning on a miss: category tags (loot entries) are looked up here first, and callers log real misses.
		if (const FMasterItemDefinition* ItemDef = Pair.Value->FindRow<FMasterItemDefinition>(ItemTag.GetTagName(), TEXT("FindItemDefinitionIn"), false))
		{
			return ItemDef;
		}
	}

	return nullptr;
}

TArray<FGameplayTag> UMKHDataSubsystem::CollectItemTagsIn(const UItemTypesToTables* Definitions, const FGameplayTag& ParentTag)
{
	TArray<FGameplayTag> ItemTags;
	if (!IsValid(Definitions) || !ParentTag.IsValid())
	{
		return ItemTags;
	}

	for (const TPair<FGameplayTag, TObjectPtr<UDataTable>>& Pair : Definitions->TagsToTables)
	{
		// A table can hold matching items only if its category contains the tag or lies under it.
		const bool bRelatedCategory = ParentTag.MatchesTag(Pair.Key) || Pair.Key.MatchesTag(ParentTag);
		if (!bRelatedCategory || !IsValid(Pair.Value))
		{
			continue;
		}

		Pair.Value->ForeachRow<FMasterItemDefinition>(TEXT("CollectItemTagsIn"),
			[&ItemTags, &ParentTag](const FName& RowName, const FMasterItemDefinition& Row)
			{
				if (Row.ItemTag.MatchesTag(ParentTag))
				{
					ItemTags.AddUnique(Row.ItemTag);
				}
			});
	}

	return ItemTags;
}

FText UMKHDataSubsystem::FindCategoryNameIn(const UItemTypesToTables* Definitions, const FGameplayTag& ItemTag)
{
	if (!IsValid(Definitions))
	{
		return FText::GetEmpty();
	}

	for (FGameplayTag Tag = ItemTag; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const FText* CategoryName = Definitions->CategoryNames.Find(Tag))
		{
			return *CategoryName;
		}
	}

	return FText::GetEmpty();
}

const FEquipmentAbilityDefinition* UMKHDataSubsystem::FindAbilityDefinition(const FGameplayTag& AbilityTag) const
{
	if (!IsValid(EquipmentStatEffects) || !AbilityTag.IsValid())
	{
		return nullptr;
	}

	// The map also holds the passive stat tables: only ability definition tables are searched.
	for (const TPair<FGameplayTag, TObjectPtr<UDataTable>>& Pair : EquipmentStatEffects->MasterStatMap)
	{
		const UDataTable* Table = Pair.Value;
		if (!IsValid(Table) || Table->GetRowStruct() != FEquipmentAbilityDefinition::StaticStruct())
		{
			continue;
		}
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			const FEquipmentAbilityDefinition* Definition = reinterpret_cast<const FEquipmentAbilityDefinition*>(Row.Value);
			if (Definition != nullptr && Definition->AbilityTag == AbilityTag)
			{
				return Definition;
			}
		}
	}
	return nullptr;
}

#if WITH_EDITOR
void UMKHDataSubsystem::ValidateAbilityTuning() const
{
	if (!IsValid(AbilityTuningTable) || !IsValid(EquipmentStatEffects))
	{
		return;
	}

	for (const TPair<FGameplayTag, TObjectPtr<UDataTable>>& Pair : EquipmentStatEffects->MasterStatMap)
	{
		// The map also holds the passive stat tables: only ability definition tables are checked.
		const UDataTable* Table = Pair.Value;
		if (!IsValid(Table) || Table->GetRowStruct() != FEquipmentAbilityDefinition::StaticStruct())
		{
			continue;
		}

		Table->ForeachRow<FEquipmentAbilityDefinition>(TEXT("ValidateAbilityTuning"),
			[this, Table](const FName& RowName, const FEquipmentAbilityDefinition& Definition)
			{
				// Editor only: the classes are loaded anyway when the abilities are granted.
				const UClass* AbilityClass = Definition.AbilityClass.LoadSynchronous();
				const bool bIsDamageAbility = IsValid(AbilityClass) && AbilityClass->IsChildOf<UMKHDamageAbility>();
				if (bIsDamageAbility && FindAbilityTuning(Definition.AbilityTag) == nullptr)
				{
					UE_LOG(LogMKHData, Warning, TEXT("UMKHDataSubsystem: %s (%s) has no row in %s: its attacks use neutral damage and no reach."),
						*Definition.AbilityTag.ToString(), *GetNameSafe(Table), *GetNameSafe(AbilityTuningTable));
				}
			});
	}
}
#endif
