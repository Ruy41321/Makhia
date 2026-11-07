// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/MKHAbilitySystemComponent.h"

#include "MKHLogChannels.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/MKHAbilityGrantPayload.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Abilities/MKHGameplayAbility.h"
#include "AbilitySystem/Effects/MKHEquipmentArmorEffect.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Equipment/EquipmentTypes.h"
#include "Interfaces/EquipmentInterface.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Player/PlayerController/MKHPlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UMKHAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	BindToActivateQueuedAbility();
}

void UMKHAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	// The retry MUST be deferred to the next tick: this callback runs mid FastArray delta
	// serialize, where specs removed by the server are still physically inside
	// ActivatableAbilities (they are deleted only after all add/change callbacks).
	// Activating synchronously here can pick the removed weapon's stale spec, whose
	// instance is already destroyed, and lose the buffered input.
	const bool bMatchesQueuedInput = !QueuedAbilityTags.IsEmpty()
		&& AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(QueuedAbilityTags.Top());

	if ((bWeaponSwapInFlight || bMatchesQueuedInput) && !bQueuedFlushScheduled)
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			bQueuedFlushScheduled = true;
			World->GetTimerManager().SetTimerForNextTick(this, &UMKHAbilitySystemComponent::FlushQueuedAbilityAfterGrant);
		}
	}
}

void UMKHAbilitySystemComponent::FlushQueuedAbilityAfterGrant()
{
	bQueuedFlushScheduled = false;

	// The ability list is coherent again: stale specs are gone, granted specs are in place.
	NotifyWeaponSwapCompleted();
	OnActivateQueuedAbility(nullptr);
}

void UMKHAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant)
{
	for (const TSubclassOf<UGameplayAbility>& Ability : AbilitiesToGrant)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Ability, 1.f);

		if (const UMKHGameplayAbility* RPGAbility = Cast<UMKHGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(RPGAbility->InputTag);
			GiveAbility(AbilitySpec);
		}
	}
}

void UMKHAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassivesToGrant)
{
	for (const TSubclassOf<UGameplayAbility>& Ability : PassivesToGrant)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Ability, 1.f);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UMKHAbilitySystemComponent::InitializeDefaultAttributes(const TSubclassOf<UGameplayEffect>& AttributeEffect)
{
	checkf(AttributeEffect, TEXT("No valid default attributes for this character %s"), *GetNameSafe(GetAvatarActor()));

	FGameplayEffectContextHandle ContextHandle = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(AttributeEffect, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	OnAttributesGiven.Broadcast();
}

bool UMKHAbilitySystemComponent::IsOtherWeaponAtk(const FGameplayTag& InputTag) const
{
	using namespace MKHGameplayTags;
	
	return QueuedAbilityTags.Num() > 0 
		&& QueuedAbilityTags[0].MatchesTag(Input::WeaponQuickSlotCategory)
		&& InputTag.MatchesTag(Input::Attacks);
}

bool UMKHAbilitySystemComponent::ShouldQueueAbility(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag) const
{
	using namespace MKHGameplayTags;

	// Don't queue the ability if it's not active the InputBufferWindow, it's not a weapon swap,
	// and it's not an attack that must wait for an in-flight weapon swap round trip.
	if (!HasMatchingGameplayTag(Combat::InputBufferWindow) &&
		!(InputTag.MatchesTag(Input::WeaponQuickSlotCategory)) &&
		!IsAttackWaitingForWeaponSwap(InputTag))
		return false;

	// If the ability is already active don't queue it
	if (Spec.IsActive())
		return false;

	/**
	 * Don't queue the ability if it can be activated instantaneously,
	 * but don't check for this if it's an attack when it's already queued a weapon swap
	 * or a swap is still in flight, because it would be an attack of the other weapon:
	 * the locally matching spec still belongs to the weapon being removed.
	 */
	if (!IsOtherWeaponAtk(InputTag) && !IsAttackWaitingForWeaponSwap(InputTag)
		&& Spec.Ability->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get()))
		return false;

	return true;
}

bool UMKHAbilitySystemComponent::IsAttackWaitingForWeaponSwap(const FGameplayTag& InputTag) const
{
	return bWeaponSwapInFlight && InputTag.MatchesTag(MKHGameplayTags::Input::Attacks);
}

void UMKHAbilitySystemComponent::QueueAbility(const FGameplayTag& InputTag)
{	
	using namespace MKHGameplayTags;
		
	/**
	 * Possible combination after this condition:
	 * 1. Consumable
	 * 2. Consumable -> WeaponSwap/Sheathe
	 */
	if (InputTag.MatchesTag(Input::ConsumableQuickSlotCategory))
	{
		QueuedAbilityTags.RemoveAll([&](const FGameplayTag& Tag) {
			return Tag.MatchesTag(Input::ConsumableQuickSlotCategory) || Tag.MatchesTag(Input::Attacks);
		});
		QueuedAbilityTags.Push(InputTag);
		return;
	}
	
	/**
	* Possible combination after this condition:
	 * 1. Weapon Swap/Sheathe
	 * 2. Consumable -> WeaponSwap/Sheathe
	 */
	if (InputTag.MatchesTag(Input::WeaponQuickSlotCategory) || InputTag.MatchesTag(Input::SheatheWeapon))
	{
		QueuedAbilityTags.RemoveAll([&](const FGameplayTag& Tag) {
			return Tag.MatchesTag(Input::WeaponQuickSlotCategory) || Tag.MatchesTag(Input::SheatheWeapon)
			|| Tag.MatchesTag(Input::Attacks);
		});
		QueuedAbilityTags.Insert(InputTag, 0);
		return;
	}
	
	/**
	 * Possible combination after this condition:
	 * 1. Atk
	 * 2. Weapon Swap -> Atk
	 * 3. Consumable -> Weapon Swap
	 */
	if (InputTag.MatchesTag(Input::Attacks))
	{
		if (QueuedAbilityTags.Num() > 0)
		{
			if (QueuedAbilityTags[0].MatchesTag(Input::WeaponQuickSlotCategory))
			{
				if (!QueuedAbilityTags.Top().MatchesTag(Input::ConsumableQuickSlotCategory))
					QueuedAbilityTags.Insert(InputTag, 0);
				return;
			}
		}
		QueuedAbilityTags.Empty(2);
		QueuedAbilityTags.Push(InputTag);
	}
}

void UMKHAbilitySystemComponent::BindToActivateQueuedAbility()
{
	GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::ActivateQueuedAbility).AddUObject(this, &UMKHAbilitySystemComponent::OnActivateQueuedAbility);
}

void UMKHAbilitySystemComponent::OnActivateQueuedAbility(const FGameplayEventData* Payload)
{
	if (QueuedAbilityTags.IsEmpty())
		return;

	// Keep the buffered attack queued while a weapon swap round trip is in flight:
	// the locally matching spec still belongs to the removed weapon. OnGiveAbility
	// retries this queue once the swapped weapon's abilities replicate down.
	if (IsAttackWaitingForWeaponSwap(QueuedAbilityTags.Top()))
		return;

	AActor* Avatar = GetAvatarActor();
	AController* Controller = Avatar ? Avatar->GetInstigatorController() : nullptr;
    
	if (AMKHPlayerController* MKHPC = Cast<AMKHPlayerController>(Controller))
	{
		MKHPC->AbilityInputPressed(QueuedAbilityTags.Pop());
		return;
	}

	// AI-driven owners (the duel bot) press their inputs straight on the ability system, as their StateTree tasks do.
	if (IsValid(Controller))
	{
		AbilityInputPressed(QueuedAbilityTags.Pop(), false);
	}
}

void UMKHAbilitySystemComponent::Server_RemovePriorityTag_Implementation(FGameplayTag PriorityTag)
{
	using namespace MKHGameplayTags;

	// Only the input-arbitration priority tags may be cleared by the owning client.
	if (PriorityTag != Ability::Priority1 && PriorityTag != Ability::Priority2)
		return;

	// Guarded: the server's own MakeCancellable notify or EndAbility may already have
	// consumed the tag, and the count must never underflow.
	if (HasMatchingGameplayTag(PriorityTag))
	{
		RemoveLooseGameplayTag(PriorityTag);
	}
}

void UMKHAbilitySystemComponent::NotifyWeaponSwapRequested()
{
	// On the authority the swap resolves synchronously inside UseItem: no gating needed.
	if (!AbilityActorInfo.IsValid() || AbilityActorInfo->IsNetAuthority())
		return;

	bWeaponSwapInFlight = true;

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(WeaponSwapTimeoutHandle, this,
			&UMKHAbilitySystemComponent::HandleWeaponSwapTimeout, WeaponSwapTimeoutSeconds, false);
	}
}

void UMKHAbilitySystemComponent::NotifyWeaponSwapCompleted()
{
	bWeaponSwapInFlight = false;

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(WeaponSwapTimeoutHandle);
	}
}

void UMKHAbilitySystemComponent::HandleWeaponSwapTimeout()
{
	UE_LOG(LogMKHAbility, Warning, TEXT("UMKHAbilitySystemComponent::HandleWeaponSwapTimeout - Weapon swap result never replicated on %s, discarding stale queued attacks."),
		*GetNameSafe(GetAvatarActor()));

	bWeaponSwapInFlight = false;

	// The buffered attack targeted the weapon we never received: drop it rather than
	// activating the wrong weapon's ability.
	QueuedAbilityTags.RemoveAll([](const FGameplayTag& Tag)
	{
		return Tag.MatchesTag(MKHGameplayTags::Input::Attacks);
	});
}

void UMKHAbilitySystemComponent::AbilityInputPressed(const FGameplayTag& InputTag, bool bForceQueue)
{
	if (!InputTag.IsValid())
		return;

	if (bForceQueue)
	{
		QueueAbility(InputTag);
		return;
	}
	
	ABILITYLIST_SCOPE_LOCK();

	const bool bIsQuickSlotInput = IsQuickSlotInput(InputTag);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!DoesSpecMatchInput(Spec, InputTag, bIsQuickSlotInput))
			continue;
		
		if (ShouldQueueAbility(Spec, InputTag))
		{
			QueueAbility(InputTag);
		}
		else
		{
			if (bIsQuickSlotInput)
				SendQuickSlotEvent(InputTag);
			else
				HandleAbilityInputPressedForSpec(Spec);
		}
		break;
	}
}

void UMKHAbilitySystemComponent::AbilityInputReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
		return;

	ABILITYLIST_SCOPE_LOCK();

	const bool bIsQuickSlotInput = IsQuickSlotInput(InputTag);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!DoesSpecMatchInput(Spec, InputTag, bIsQuickSlotInput))
			continue;

		if (bIsQuickSlotInput)
			continue;
		
		HandleAbilityInputReleasedForSpec(Spec);
		break;
	}
}

bool UMKHAbilitySystemComponent::IsQuickSlotInput(const FGameplayTag& InputTag) const
{
	return InputTag.MatchesTag(MKHGameplayTags::Input::QuickSlot);
}

bool UMKHAbilitySystemComponent::DoesSpecMatchInput(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag,
	const bool bIsQuickSlotInput) const
{
	FGameplayTag TagToConfront = InputTag;
	if (bIsQuickSlotInput)
		TagToConfront = MKHGameplayTags::Input::QuickSlot;

	// Deliberate over-match: when a weapon swap is queued, the attack targets a weapon whose
	// spec doesn't exist locally yet, so nothing can match it by tag. Returning true here
	// lets AbilityInputPressed fall through to ShouldQueueAbility, which queues it instead.
	if (IsOtherWeaponAtk(InputTag))
		return true;

	return Spec.GetDynamicSpecSourceTags().HasTagExact(TagToConfront);
}

void UMKHAbilitySystemComponent::SendQuickSlotEvent(const FGameplayTag& InputTag) const
{
	FGameplayEventData Payload;
	Payload.TargetTags.AddTag(InputTag);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(), MKHGameplayTags::Event::UseQuickSlot, Payload);
}

void UMKHAbilitySystemComponent::HandleAbilityInputPressedForSpec(const FGameplayAbilitySpec& Spec)
{
	if (!Spec.IsActive())
	{
        TryActivateAbility(Spec.Handle);
		return;
	}

	if (const UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance(); IsValid(PrimaryInstance))
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
			PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}

void UMKHAbilitySystemComponent::HandleAbilityInputReleasedForSpec(const FGameplayAbilitySpec& Spec)
{
	if (!Spec.IsActive())
		return;
	
	if (const UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance(); IsValid(PrimaryInstance))
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
			PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}

void UMKHAbilitySystemComponent::AddEquipmentEffects(FRPGEquipmentEntry* EquipmentEntry)
{
	if (!EquipmentEntry)
		return;

	const FGameplayEffectContextHandle ContextHandle = MakeEffectContext();

	ApplyAndTrackBaseArmor(*EquipmentEntry, ContextHandle);

	for (const FEquipmentStatEffectDefinition& StatEffect : EquipmentEntry->Item.EffectPackage.StatEffects)
	{
		GrantEquipmentStatEffect(*EquipmentEntry, StatEffect, ContextHandle);
	}
}

void UMKHAbilitySystemComponent::RemoveEquipmentEffects(FRPGEquipmentEntry* EquipmentEntry)
{
	if (!EquipmentEntry)
	{
		return;
	}

	for (auto HandleIt = EquipmentEntry->GrantedHandles.ActiveEffects.CreateIterator(); HandleIt; ++HandleIt)
	{
		RemoveActiveGameplayEffect(*HandleIt);
		HandleIt.RemoveCurrent();
	}
}

void UMKHAbilitySystemComponent::AddEquipmentAbility(FRPGEquipmentEntry* EquipmentEntry)
{
	if (!EquipmentEntry)
		return;

	for (const FEquipmentAbilityDefinition& AbilityDef : EquipmentEntry->Item.EffectPackage.Abilities)
	{
		GrantEquipmentAbilityDefinition(*EquipmentEntry, AbilityDef);
	}
}

void UMKHAbilitySystemComponent::RemoveEquipmentAbility(FRPGEquipmentEntry* EquipmentEntry)
{
	if (!EquipmentEntry)
	{
		return;
	}

	for (auto HandleIt = EquipmentEntry->GrantedHandles.GrantedAbilities.CreateConstIterator(); HandleIt; ++HandleIt)
	{
		ClearAbility(*HandleIt);
	}
	EquipmentEntry->GrantedHandles.GrantedAbilities.Empty();
}

void UMKHAbilitySystemComponent::GetCooldownRemainingForTag(const FGameplayTag CooldownTag, float& TimeRemaining,
	float& CooldownDuration) const
{
	TimeRemaining = 0.f;
	CooldownDuration = 0.f;

	if (CooldownTag.IsValid())
	{
		// Creating a query to find the Effect that grants the CooldownTag
		FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
        
		// Gets the Remaining Times of all Effects found with the query
		TArray<float> Durations = GetActiveEffectsTimeRemaining(Query);
        
		if (Durations.Num() > 0)
		{
			TimeRemaining = FMath::Max(Durations[0], 0.f);
            
			// Get the Total Duration as well
			TArray<float> TotalDurations = GetActiveEffectsDuration(Query);
			CooldownDuration = TotalDurations[0];
		}
	}
}

bool UMKHAbilitySystemComponent::HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}
	}
	return false;
}

const FGameplayAbilitySpec* UMKHAbilitySystemComponent::FindAbilitySpecForInputTag(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	// Same mapping as DoesSpecMatchInput, without its deliberate over-match for queued attacks.
	const FGameplayTag SpecInputTag = IsQuickSlotInput(InputTag) ? FGameplayTag(MKHGameplayTags::Input::QuickSlot) : InputTag;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(SpecInputTag))
		{
			return &Spec;
		}
	}
	return nullptr;
}

const FGameplayAbilitySpec* UMKHAbilitySystemComponent::FindAbilitySpecForAbilityTag(const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityTag))
		{
			return &Spec;
		}
	}
	return nullptr;
}

void UMKHAbilitySystemComponent::RefreshEquippedSkillInputs(const FEquipmentEffectPackage& Package)
{
	for (const FEquipmentAbilityDefinition& Ability : Package.Abilities)
	{
		const FGameplayAbilitySpec* FoundSpec = Ability.bIsSkillAbility ? FindAbilitySpecForAbilityTag(Ability.AbilityTag) : nullptr;
		FGameplayAbilitySpec* Spec = FoundSpec ? FindAbilitySpecFromHandle(FoundSpec->Handle) : nullptr;
		if (Spec == nullptr)
		{
			continue;
		}

		FGameplayTagContainer& DynamicTags = Spec->GetDynamicSpecSourceTags();
		DynamicTags.RemoveTags(DynamicTags.Filter(FGameplayTagContainer(MKHGameplayTags::Input::Ability)));
		DynamicTags.AddTag(Ability.SkillInputTag);
		if (UMKHAbilityGrantPayload* Payload = Cast<UMKHAbilityGrantPayload>(Spec->SourceObject.Get()))
		{
			Payload->InputTag = Ability.SkillInputTag;
		}
		MarkAbilitySpecDirty(*Spec);
	}
}

FGameplayTag UMKHAbilitySystemComponent::FindInputTagForAbilityTag(const FGameplayTag& AbilityTag) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecForAbilityTag(AbilityTag);
	if (Spec == nullptr)
	{
		return FGameplayTag();
	}

	const FGameplayTagContainer InputTags = Spec->GetDynamicSpecSourceTags().Filter(FGameplayTagContainer(MKHGameplayTags::Input::Ability));
	return InputTags.IsEmpty() ? FGameplayTag() : InputTags.First();
}

FGameplayTag UMKHAbilitySystemComponent::ResolveEquipmentAbilityInputTag(const FEquipmentAbilityDefinition& AbilityDef,
	const UMKHGameplayAbility* AbilityCDO)
{
	// Skills get their input tag rolled at item creation; other abilities keep the class default.
	return (AbilityDef.bIsSkillAbility || !IsValid(AbilityCDO)) ? AbilityDef.SkillInputTag : AbilityCDO->InputTag;
}

TWeakObjectPtr<UEquipmentManagerComponent> UMKHAbilitySystemComponent::GetWeakEquipmentManager() const
{
	if (!AbilityActorInfo.IsValid())
	{
		return nullptr;
	}

	// The duel bot keeps its equipment on the pawn (it has no PlayerController).
	if (const AActor* Avatar = AbilityActorInfo->AvatarActor.Get(); IsValid(Avatar))
	{
		if (UEquipmentManagerComponent* AvatarEquipment = Avatar->FindComponentByClass<UEquipmentManagerComponent>())
		{
			return AvatarEquipment;
		}
	}

	APlayerController* PlayerController = AbilityActorInfo->PlayerController.Get();
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	if (!PlayerController->GetClass()->ImplementsInterface(UEquipmentInterface::StaticClass()))
	{
		return nullptr;
	}

	return IEquipmentInterface::Execute_GetEquipmentManagerComponent(PlayerController);
}

FRPGEquipmentEntry* UMKHAbilitySystemComponent::FindEquipmentEntry(UEquipmentManagerComponent* EquipmentManager, const int64 ItemId,
	const FGameplayTag& SlotTag) const
{
	if (!IsValid(EquipmentManager))
	{
		return nullptr;
	}

	return EquipmentManager->EquipmentList.FindEntryMutable(ItemId, SlotTag);
}

void UMKHAbilitySystemComponent::ApplyAndTrackBaseArmor(FRPGEquipmentEntry& EquipmentEntry,
	const FGameplayEffectContextHandle& ContextHandle)
{
	const float BaseArmor = EquipmentEntry.GetBaseArmor();
	if (BaseArmor <= 0.f)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(UMKHEquipmentArmorEffect::StaticClass(), 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("UMKHAbilitySystemComponent::ApplyAndTrackBaseArmor - Could not build the armor spec for item '%s'."),
			*EquipmentEntry.Item.ItemTag.ToString());
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MKHGameplayTags::Combat::Data_Armor, BaseArmor);
	EquipmentEntry.GrantedHandles.AddEffectHandle(ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()));
}

void UMKHAbilitySystemComponent::ApplyAndTrackStatEffect(FRPGEquipmentEntry& EquipmentEntry,
	const FEquipmentStatEffectDefinition& StatEffect, const FGameplayEffectContextHandle& ContextHandle)
{
	if (!IsValid(StatEffect.EffectClass.Get()))
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(StatEffect.EffectClass.Get(), StatEffect.CurrentValue, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle ActiveHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	EquipmentEntry.GrantedHandles.AddEffectHandle(ActiveHandle);
}

void UMKHAbilitySystemComponent::GrantEquipmentStatEffect(FRPGEquipmentEntry& EquipmentEntry,
	const FEquipmentStatEffectDefinition& StatEffect, const FGameplayEffectContextHandle& ContextHandle)
{
	if (IsValid(StatEffect.EffectClass.Get()))
	{
		ApplyAndTrackStatEffect(EquipmentEntry, StatEffect, ContextHandle);
		return;
	}

	FStreamableManager& Manager = UAssetManager::GetStreamableManager();
	const TWeakObjectPtr<UMKHAbilitySystemComponent> WeakThis(this);
	const TWeakObjectPtr<UEquipmentManagerComponent> WeakEquipmentManager = GetWeakEquipmentManager();
	const int64 EntryItemID = EquipmentEntry.Item.ItemID;
	const FGameplayTag EntrySlotTag = EquipmentEntry.SlotTag;

	Manager.RequestAsyncLoad(StatEffect.EffectClass.ToSoftObjectPath(),
		[WeakThis, WeakEquipmentManager, StatEffect, ContextHandle, EntryItemID, EntrySlotTag]
		{
			if (!WeakThis.IsValid() || !WeakEquipmentManager.IsValid()) return;

			if (FRPGEquipmentEntry* ResolvedEntry = WeakThis->FindEquipmentEntry(WeakEquipmentManager.Get(), EntryItemID, EntrySlotTag))
			{
				WeakThis->ApplyAndTrackStatEffect(*ResolvedEntry, StatEffect, ContextHandle);
			}
		});
}

void UMKHAbilitySystemComponent::ApplyAndTrackEquipmentAbility(FRPGEquipmentEntry& EquipmentEntry,
	const FEquipmentAbilityDefinition& AbilityDef)
{
	if (!IsValid(AbilityDef.AbilityClass.Get()))
	{
		return;
	}

	EquipmentEntry.GrantedHandles.AddAbilityHandle(GrantEquipmentAbility(AbilityDef));
}

void UMKHAbilitySystemComponent::GrantEquipmentAbilityDefinition(FRPGEquipmentEntry& EquipmentEntry,
	const FEquipmentAbilityDefinition& AbilityDef)
{
	if (IsValid(AbilityDef.AbilityClass.Get()))
	{
		ApplyAndTrackEquipmentAbility(EquipmentEntry, AbilityDef);
		return;
	}

	FStreamableManager& Manager = UAssetManager::GetStreamableManager();
	const TWeakObjectPtr<UMKHAbilitySystemComponent> WeakThis(this);
	const TWeakObjectPtr<UEquipmentManagerComponent> WeakEquipmentManager = GetWeakEquipmentManager();
	const int64 EntryItemID = EquipmentEntry.Item.ItemID;
	const FGameplayTag EntrySlotTag = EquipmentEntry.SlotTag;

	Manager.RequestAsyncLoad(AbilityDef.AbilityClass.ToSoftObjectPath(),
		[WeakThis, WeakEquipmentManager, AbilityDef, EntryItemID, EntrySlotTag]
		{
			if (!WeakThis.IsValid() || !WeakEquipmentManager.IsValid()) return;

			if (FRPGEquipmentEntry* ResolvedEntry = WeakThis->FindEquipmentEntry(WeakEquipmentManager.Get(), EntryItemID, EntrySlotTag))
			{
				WeakThis->ApplyAndTrackEquipmentAbility(*ResolvedEntry, AbilityDef);
			}
		});
}

FGameplayAbilitySpecHandle UMKHAbilitySystemComponent::GrantEquipmentAbility(const FEquipmentAbilityDefinition& AbilityDef)
{
	FGameplayAbilitySpec Spec = FGameplayAbilitySpec(AbilityDef.AbilityClass.Get(), 1.f);

	// Per-item data goes on a payload (Spec.SourceObject), never on Spec.Ability — that's
	// the class CDO, shared by every owner of this class. See UMKHAbilityGrantPayload.
	const UMKHGameplayAbility* AbilityCDO = Cast<UMKHGameplayAbility>(Spec.Ability);

	const FGameplayTag InputTag = ResolveEquipmentAbilityInputTag(AbilityDef, AbilityCDO);

	if (InputTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}

	// Dynamic source tags replicate with the spec, the payload does not: the AbilityTag lets every machine
	// find the ability's tuning row (Documentation/AbilityTuning.md).
	if (AbilityDef.AbilityTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(AbilityDef.AbilityTag);
	}

	UMKHAbilityGrantPayload* Payload = NewObject<UMKHAbilityGrantPayload>(this);
	Payload->InputTag = InputTag;
	Payload->ProjectileToSpawnTag = AbilityDef.ContextTag;
	Payload->bIsSkillAbility = AbilityDef.bIsSkillAbility;
	if (AbilityDef.bIsSkillAbility)
	{
		Payload->CooldownTags = AbilityDef.CooldownTag.GetSingleTagContainer();
	}
	Spec.SourceObject = Payload;

	return GiveAbility(Spec);
}
