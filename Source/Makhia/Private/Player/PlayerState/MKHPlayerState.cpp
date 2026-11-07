// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Player/PlayerState/MKHPlayerState.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "Career/MKHCareerSubsystem.h"
#include "Cosmetics/MKHSkinComponent.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AMKHPlayerState::AMKHPlayerState()
{
	SetNetUpdateFrequency(100.f);
	SetMinNetUpdateFrequency(66.f);

	MKHAbilitySystemComponent = CreateDefaultSubobject<UMKHAbilitySystemComponent>("AbilitySystemComp");
	MKHAbilitySystemComponent->SetIsReplicated(true);
	MKHAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	MKHAttributeSet = CreateDefaultSubobject<UMKHAttributeSet>("AttributeSet");
}

void AMKHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHPlayerState, NatType);
	DOREPLIFETIME(AMKHPlayerState, SkinId);
	DOREPLIFETIME(AMKHPlayerState, CareerLevels);
}

void AMKHPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AMKHPlayerState* NewPlayerState = Cast<AMKHPlayerState>(PlayerState); IsValid(NewPlayerState))
	{
		NewPlayerState->NatType = NatType;
		NewPlayerState->SkinId = SkinId;
		NewPlayerState->CareerLevels = CareerLevels;
	}
}

UAbilitySystemComponent* AMKHPlayerState::GetAbilitySystemComponent() const
{
	return MKHAbilitySystemComponent;
}

UMKHAbilitySystemComponent* AMKHPlayerState::GetRPGAbilitySystemComponent() const
{
	return MKHAbilitySystemComponent;
}

UMKHAttributeSet* AMKHPlayerState::GetRPGAttributeSet() const
{
	return MKHAttributeSet;
}

void AMKHPlayerState::SetNatType(EMKHNatType NewNatType)
{
	if (HasAuthority())
	{
		NatType = NewNatType;
	}
}

void AMKHPlayerState::SetSkinId(const FName NewSkinId)
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!HasAuthority() || !IsValid(Skins))
	{
		return;
	}

	// Unknown identifiers (older or modified client) fall back to the default skin.
	SkinId = Skins->ResolveSkinId(NewSkinId);
	ApplySkinToPawn();
}

void AMKHPlayerState::SetCareerLevels(const int32 NewCareerLevels)
{
	if (HasAuthority())
	{
		const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
		CareerLevels = FMath::Clamp(NewCareerLevels, 0, IsValid(Career) ? Career->GetLevelCount() : 0);
	}
}

void AMKHPlayerState::OnRep_SkinId()
{
	ApplySkinToPawn();
}

void AMKHPlayerState::ApplySkinToPawn() const
{
	const APawn* Pawn = GetPawn();
	UMKHSkinComponent* SkinComponent = IsValid(Pawn) ? Pawn->FindComponentByClass<UMKHSkinComponent>() : nullptr;
	if (IsValid(SkinComponent) && !SkinId.IsNone())
	{
		SkinComponent->SetSkin(SkinId);
	}
}
