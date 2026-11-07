// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Input/MKHInputConfig.h"
#include "MKHSystemInputComponent.generated.h"

/**
 * 
 */
UCLASS()
class MAKHIA_API UMKHSystemInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(UMKHInputConfig* InputConfig, const FGameplayTag& TagFilter, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc);

};

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UMKHSystemInputComponent::BindAbilityActions(UMKHInputConfig* InputConfig, const FGameplayTag& TagFilter,
	UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc)
{
	check(InputConfig);

	for (const FMKHInputAction& Action : InputConfig->MKHInputActions)
	{
		if (IsValid(Action.InputAction) && Action.InputTag.IsValid() && Action.InputTag.MatchesTag(TagFilter))
		{
			if constexpr (!std::is_same_v<PressedFuncType, std::nullptr_t>)
			{
				if (PressedFunc)
				{
					FGameplayTag Tag = Action.InputTag;
					BindActionValueLambda(Action.InputAction, ETriggerEvent::Triggered,
						[Object, PressedFunc, Tag](const FInputActionValue&)
						{
							(Object->*PressedFunc)(Tag);
						});
				}
			}
			
			if constexpr (!std::is_same_v<ReleasedFuncType, std::nullptr_t>)
			{
				if (ReleasedFunc)
				{
					FGameplayTag Tag = Action.InputTag;
					BindActionValueLambda(Action.InputAction, ETriggerEvent::Completed,
						[Object, ReleasedFunc, Tag](const FInputActionValue&)
						{
							(Object->*ReleasedFunc)(Tag);
						});
				}
			}
		}
	}
}
