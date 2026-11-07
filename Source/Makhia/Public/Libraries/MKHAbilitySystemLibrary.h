// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "MKHAbilitySystemLibrary.generated.h"

struct FMKHItemEntry;
class UCharacterClassInfo;
class UProjectileInfo;
struct FDamageEffectInfo;
// struct FStatusEffectData;
struct FGameplayEffectContextHandle;
/**
 *
 */
UCLASS()
class MAKHIA_API UMKHAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure)
	static UCharacterClassInfo* GetCharacterClassDefaultInfo(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure)
	static UProjectileInfo* GetProjectileInfo(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable)
	static void ApplyDamageEffect(const FDamageEffectInfo& DamageEffectInfo);

	/**
	 * Shared application pipeline for status effects: builds one outgoing spec per entry,
	 * assigns duration and value as set-by-caller magnitudes, and applies it to the target.
	 * Used for target on-hit effects, self on-hit effects and self activation effects,
	 * so every status effect follows the same path from definition to application.
	 *
	 * @param SourceASC      ASC that creates the outgoing specs (the effect instigator).
	 * @param TargetASC      ASC receiving the effects (pass SourceASC for self-applied effects).
	 * @param StatusEffectsContainer  Status effect entries to apply.
	 * @param ContextHandle  Effect context forwarded to the specs (crit flag, source object, ...).
	 */
	static void ApplyStatusEffects(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FGameplayTagContainer& StatusEffectsContainer, const FGameplayEffectContextHandle& ContextHandle);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Loose Tag Count Static"))
	static void K2_SetLooseTagCountStatic(UAbilitySystemComponent* ASC, FGameplayTag Tag, int32 NewCount);

	template<typename T>
	static T* GetDataTableRowByTag(const UDataTable* DataTable, const FGameplayTag& Tag);

	/**
	 * Assigns the skill input tags (SkillSlot1..3), in order, to the skill abilities rolled on a weapon item.
	 *
	 * @param NewItem Item whose EffectPackage abilities receive the dynamic input tags.
	 */
	static void AssignDynamicSkillInputTag(FMKHItemEntry& NewItem);
	
	/** Returns a container with all the gameplay tag children of the every given tag in the container. 
	 * @param ParentTagContainer The container of parent tags to retrieve children for.
	 * @param ExcludingDepth The depth of children to not retrieve. E.g. a depth of 0 will return everything, a depth of 1 will return only the children of the parent tags.
	 */
	UFUNCTION(BlueprintPure, Category = "GameplayTags")
	static FGameplayTagContainer GetGameplayTagChildren(const FGameplayTagContainer ParentTagContainer, int ExcludingDepth = 0);
	
	UFUNCTION(BlueprintPure, Category = "GameplayTags")
	static FName GetGameplayTagLeafName(const FGameplayTag Tag);
};

template<typename T>
T* UMKHAbilitySystemLibrary::GetDataTableRowByTag(const UDataTable* DataTable, const FGameplayTag& Tag)
{
	if (!DataTable)
	{
		return nullptr;
	}
	return DataTable->FindRow<T>(Tag.GetTagName(), FString());
}
