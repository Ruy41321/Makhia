// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/MKHAbilityTypes.h"

FMKHGameplayEffectContext* FMKHGameplayEffectContext::GetEffectContext(FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* EffectContext = Handle.Get();
	if (EffectContext && EffectContext->GetScriptStruct()->IsChildOf(StaticStruct()))
	{
		return static_cast<FMKHGameplayEffectContext*>(EffectContext);
	}

	return nullptr;
}

/** Serializes a status effect array with an explicit element count, shared by both context lists. */
// static void NetSerializeStatusEffectArray(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess, TArray<FStatusEffectData>& StatusEffects)
// {
// 	// Safe and clean manual array serialization for custom contexts
// 	uint8 NumEffects = StatusEffects.Num();
// 	Ar << NumEffects;
//
// 	if (Ar.IsLoading())
// 	{
// 		StatusEffects.SetNumZeroed(NumEffects);
// 	}
//
// 	for (uint8 i = 0; i < NumEffects; ++i)
// 	{
// 		StatusEffects[i].NetSerialize(Ar, Map, bOutSuccess);
// 	}
// }

bool FMKHGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint16 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (bReplicateInstigator && Instigator.IsValid())
		{
			RepBits |= 1 << 0;
		}
		if (bReplicateEffectCauser && EffectCauser.IsValid())
		{
			RepBits |= 1 << 1;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2;
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
		if (bCriticalHit)
		{
			RepBits |= 1 << 7;
		}
		if (TargetStatusEffectsContainer.Num() > 0)
		{
			RepBits |= 1 << 8;
		}
		if (SelfOnHitStatusEffectsContainer.Num() > 0)
		{
			RepBits |= 1 << 9;
		}
	}

	Ar.SerializeBits(&RepBits, 10);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}
	if (RepBits & 1 << 7)
	{
		Ar << bCriticalHit;
	}
	if (RepBits & 1 << 8)
	{
		TargetStatusEffectsContainer.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & 1 << 9)
	{
		SelfOnHitStatusEffectsContainer.NetSerialize(Ar, Map, bOutSuccess);
	}

	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}

	bOutSuccess = true;
	return true;
}

bool FMKHGameplayAbilityTargetData_MeleeHit::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);
	Ar << ComboIndex;

	bOutSuccess = true;
	return true;
}