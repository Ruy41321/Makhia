// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/Inventory/MKHInventoryStatsWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "Components/TextBlock.h"
#include "MKHLogChannels.h"

#define LOCTEXT_NAMESPACE "MKHInventoryStats"

void UMKHInventoryStatsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UnbindFromAbilitySystem();
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwningPlayerPawn());
	if (!IsValid(ASC))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("UMKHInventoryStatsWidget: the owning pawn has no ability system yet, stats stay empty."));
		return;
	}

	AbilitySystem = ASC;
	ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetArmorAttribute()).AddUObject(this, &ThisClass::HandleArmorChanged);
	RefreshHealth();
	RefreshArmor();
}

void UMKHInventoryStatsWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();
	Super::NativeDestruct();
}

void UMKHInventoryStatsWidget::HandleHealthChanged(const FOnAttributeChangeData& ChangeData) const
{
	RefreshHealth();
}

void UMKHInventoryStatsWidget::HandleArmorChanged(const FOnAttributeChangeData& ChangeData) const
{
	RefreshArmor();
}

void UMKHInventoryStatsWidget::RefreshHealth() const
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!IsValid(ASC) || !IsValid(HpValueText))
	{
		return;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Current"), FText::AsNumber(FMath::CeilToInt(ASC->GetNumericAttribute(UMKHAttributeSet::GetHealthAttribute()))));
	Arguments.Add(TEXT("Max"), FText::AsNumber(FMath::CeilToInt(ASC->GetNumericAttribute(UMKHAttributeSet::GetMaxHealthAttribute()))));
	HpValueText->SetText(FText::Format(LOCTEXT("HealthValue", "{Current}/{Max}"), Arguments));
}

void UMKHInventoryStatsWidget::RefreshArmor() const
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!IsValid(ASC) || !IsValid(ArmorValueText))
	{
		return;
	}

	const float Armor = ASC->GetNumericAttribute(UMKHAttributeSet::GetArmorAttribute());
	FNumberFormattingOptions PercentOptions;
	PercentOptions.SetMaximumFractionalDigits(0);

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Armor"), FText::AsNumber(FMath::CeilToInt(Armor)));
	Arguments.Add(TEXT("Reduction"), FText::AsPercent(UMKHAttributeSet::CalculateArmorDamageReduction(Armor), &PercentOptions));
	ArmorValueText->SetText(FText::Format(LOCTEXT("ArmorValue", "{Armor} (-{Reduction})"), Arguments));
}

void UMKHInventoryStatsWidget::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UMKHAttributeSet::GetArmorAttribute()).RemoveAll(this);
	}
	AbilitySystem.Reset();
}

#undef LOCTEXT_NAMESPACE
