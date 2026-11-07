// Fill out your copyright notice in the Description page of Project Settings.


#include "Libraries/MKHAbilitySystemLibrary.h"
#include "AbilitySystem/MKHAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameMode/MKHGameMode.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/MKHItemEntry.h"
#include "Data/EquipmentStatEffects.h"
#include "Data/MKHDataSubsystem.h"
#include "MKHLogChannels.h"
#include "Kismet/GameplayStatics.h"

UCharacterClassInfo* UMKHAbilitySystemLibrary::GetCharacterClassDefaultInfo(const UObject* WorldContextObject)
{
	if (const AMKHGameMode* MKHGameMode = Cast<AMKHGameMode>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		return MKHGameMode->GetCharacterClassDefaultInfo();
	}

	return nullptr;
}

UProjectileInfo* UMKHAbilitySystemLibrary::GetProjectileInfo(const UObject* WorldContextObject)
{
	if (const AMKHGameMode* MKHGameMode = Cast<AMKHGameMode>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		return MKHGameMode->GetProjectileInfo();
	}

	return nullptr;
}

void UMKHAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectInfo& DamageEffectInfo)
{
	if (!IsValid(DamageEffectInfo.SourceASC) or !IsValid(DamageEffectInfo.TargetASC))
		return;
	
	FGameplayEffectContextHandle ContextHandle = DamageEffectInfo.SourceASC->MakeEffectContext();
	ContextHandle.AddSourceObject(DamageEffectInfo.AvatarActor);

	if (FMKHGameplayEffectContext* MKHContext = FMKHGameplayEffectContext::GetEffectContext(ContextHandle))
	{
		MKHContext->SetTargetStatusEffectsContainer(DamageEffectInfo.TargetStatusEffectsContainer);
		MKHContext->SetSelfOnHitStatusEffectsContainer(DamageEffectInfo.SelfOnHitStatusEffectsContainer);
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = DamageEffectInfo.SourceASC->MakeOutgoingSpec(DamageEffectInfo.DamageEffect,
		1.f, ContextHandle);

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, MKHGameplayTags::Combat::Data_Damage, DamageEffectInfo.DamageMultiplier);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, MKHGameplayTags::Combat::Data_StaminaDamage, DamageEffectInfo.StaminaDamage);

	DamageEffectInfo.TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
}

void UMKHAbilitySystemLibrary::ApplyStatusEffects(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
    const FGameplayTagContainer& StatusEffectsContainer, const FGameplayEffectContextHandle& ContextHandle)
{
    if (!IsValid(SourceASC) || !IsValid(TargetASC) || StatusEffectsContainer.IsEmpty())
    {
        return;
    }
 
    const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(SourceASC);
    const UEquipmentStatEffects* DataTables = IsValid(DataSubsystem) ? DataSubsystem->GetEquipmentStatEffects() : nullptr;
    if (!IsValid(DataTables))
    {
        UE_LOG(LogMKHAbility, Warning, TEXT("ApplyStatusEffects: equipment stat effects data unavailable, status effects skipped."));
        return;
    }

	const UDataTable* PerkTable = nullptr;
	
	for (const auto& Pair : DataTables->MasterStatMap)
	{
		if (MKHGameplayTags::Misc::PerkDataTable.GetTag().MatchesTag(Pair.Key))
		{
			PerkTable = Pair.Value;
			break;
		}
	}
	
	if (!PerkTable)
	{
		return;
	}
	
    for (const FGameplayTag& StatusEffectTag : StatusEffectsContainer)
    {
        if (!StatusEffectTag.IsValid())
        {
            continue;
        }

        const FPerkDefinition* StatusEffectData = UMKHAbilitySystemLibrary::GetDataTableRowByTag<FPerkDefinition>(PerkTable, StatusEffectTag);
        if (!StatusEffectData)
        {
            UE_LOG(LogMKHAbility, Warning, TEXT("ApplyStatusEffects: no perk row named '%s' in %s."), *StatusEffectTag.ToString(), *GetNameSafe(PerkTable));
            continue;
        }

        // Soft class: IsValid() is false until loaded, and nothing else keeps perk effects in memory.
        const TSubclassOf<UGameplayEffect> EffectClass = StatusEffectData->EffectClass.LoadSynchronous();
        if (!IsValid(EffectClass))
        {
            UE_LOG(LogMKHAbility, Warning, TEXT("ApplyStatusEffects: perk '%s' has no loadable EffectClass."), *StatusEffectTag.ToString());
            continue;
        }

        const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, 1.0f, ContextHandle);
        if (!SpecHandle.IsValid())
        {
            continue;
        }
 
        FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
 
        // Setting duration and intensity of the status effect
        UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, MKHGameplayTags::Combat::Data_StatusEffectDuration, StatusEffectData->EffectDuration);
        UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, MKHGameplayTags::Combat::Data_StatusEffectValue, StatusEffectData->EffectValue);
 
        // Dynamically granting the specific perk tag to retrieve the cooldown of this specific status effect in the HUD
        Spec->DynamicGrantedTags.AddTag(StatusEffectData->PerkTag);
 
        TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);
    }
}

void UMKHAbilitySystemLibrary::K2_SetLooseTagCountStatic(UAbilitySystemComponent* ASC, FGameplayTag Tag, int32 NewCount)
{
	if (IsValid(ASC))
	{
		ASC->SetLooseGameplayTagCount(Tag, NewCount);
	}
}

void UMKHAbilitySystemLibrary::AssignDynamicSkillInputTag(FMKHItemEntry& NewItem)
{
	TArray<FGameplayTag> SkillInputTag = {
		MKHGameplayTags::Input::SkillSlot1, 
		MKHGameplayTags::Input::SkillSlot2, 
		MKHGameplayTags::Input::SkillSlot3
	};
	
	uint8 i = 0;
	for (FEquipmentAbilityDefinition& Ability : NewItem.EffectPackage.Abilities)
	{
		// Assign a dynamic Input Tag to Skill Abilities in order of as
		if (Ability.bIsSkillAbility)
			Ability.SkillInputTag = SkillInputTag[i++];
			
		// Bind Maximum the num of SkillInputTag available
		if (i == SkillInputTag.Num())
			break;
	}
}

FGameplayTagContainer UMKHAbilitySystemLibrary::GetGameplayTagChildren(const FGameplayTagContainer ParentTagContainer, int ExcludingDepth)
{
	FGameplayTagContainer ChildrenTags;
	for (const FGameplayTag& ParentTag : ParentTagContainer)
	{
		if (ExcludingDepth > 0)
		{
			for (const FGameplayTag& ChildTag : UGameplayTagsManager::Get().RequestGameplayTagChildren(ParentTag))
			{
				ChildrenTags.AppendTags(GetGameplayTagChildren(FGameplayTagContainer(ChildTag), ExcludingDepth - 1));
			}
		}
		else
		{
			ChildrenTags.AppendTags(UGameplayTagsManager::Get().RequestGameplayTagChildren(ParentTag));
		}
	}
	return ChildrenTags;
}

FName UMKHAbilitySystemLibrary::GetGameplayTagLeafName(const FGameplayTag Tag)
{
	return Tag.GetTagLeafName();
}
