// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "QuickSlot/MKHQuickSlotPreferenceComponent.h"

#include "MKHLogChannels.h"
#include "TimerManager.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Data/MKHDataSubsystem.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Equipment/Rarity/RarityDefinition.h"
#include "GameFramework/Controller.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "Interfaces/EquipmentInterface.h"
#include "Interfaces/InventoryInterface.h"
#include "Interfaces/QuickSlotInterface.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/MKHSkillLayout.h"
#include "Kismet/GameplayStatics.h"
#include "QuickSlot/MKHDefaultQuickSlots.h"
#include "QuickSlot/QuickSlotManagerComponent.h"

namespace MKHQuickSlotPreferences
{
	static const TCHAR* SaveSlotName = TEXT("QuickSlotPreferences");
	static constexpr int32 SaveUserIndex = 0;
}

UMKHQuickSlotPreferenceComponent::UMKHQuickSlotPreferenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UMKHQuickSlotPreferenceComponent::InitializeComponent()
{
	Super::InitializeComponent();

	const AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = FindInventory();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(Inventory))
	{
		return;
	}
	LoadoutGrantedHandle = Inventory->OnStartingLoadoutGranted.AddUObject(this, &ThisClass::HandleStartingLoadoutGranted);
}

void UMKHQuickSlotPreferenceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocallyControlled())
	{
		return;
	}

	UInventoryComponent* Inventory = FindInventory();
	if (!IsValid(Inventory))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHQuickSlotPreferenceComponent: %s has no inventory, quick slot preferences disabled."), *GetNameSafe(GetOwner()));
		return;
	}

	LoadPreferences();
	QuickSlotRelocatedHandle = Inventory->InventoryList.QuickSlotItemRelocatedDelegate.AddUObject(this, &ThisClass::HandleQuickSlotRelocated);
	if (UEquipmentManagerComponent* Equipment = FindEquipment(); IsValid(Equipment))
	{
		EquipmentEntryHandle = Equipment->EquipmentList.EquipmentEntryDelegate.AddUObject(this, &ThisClass::HandleEquipmentEntryChanged);
	}

	// After the bindings: with authority this applies a waiting loadout at once, and its quick slots must be recorded.
	SendPreferencesToServer();

	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		SettingChangedHandle = UserSettings->OnAutoQuickSlotLoadoutChanged.AddUObject(this, &ThisClass::HandleAutoQuickSlotSettingChanged);
	}
}

void UMKHQuickSlotPreferenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UInventoryComponent* Inventory = FindInventory(); IsValid(Inventory))
	{
		Inventory->InventoryList.QuickSlotItemRelocatedDelegate.Remove(QuickSlotRelocatedHandle);
		Inventory->OnStartingLoadoutGranted.Remove(LoadoutGrantedHandle);
	}
	if (UEquipmentManagerComponent* Equipment = FindEquipment(); IsValid(Equipment))
	{
		Equipment->EquipmentList.EquipmentEntryDelegate.Remove(EquipmentEntryHandle);
	}
	if (UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
	{
		UserSettings->OnAutoQuickSlotLoadoutChanged.Remove(SettingChangedHandle);
	}
	QuickSlotRelocatedHandle.Reset();
	EquipmentEntryHandle.Reset();
	LoadoutGrantedHandle.Reset();
	SettingChangedHandle.Reset();
	bCommitScheduled = false;

	Super::EndPlay(EndPlayReason);
}

FGameplayTag UMKHQuickSlotPreferenceComponent::GetItemKindTag(const FGameplayTag& ItemTag)
{
	const FGameplayTag Roots[] = { MKHGameplayTags::Equip::Category_Weapon, MKHGameplayTags::Equip::Category_Consumable };
	for (const FGameplayTag& Root : Roots)
	{
		if (!ItemTag.MatchesTag(Root) || ItemTag.MatchesTagExact(Root))
		{
			continue;
		}

		// Climb to the ancestor right below the category root.
		FGameplayTag Kind = ItemTag;
		for (FGameplayTag Parent = Kind.RequestDirectParent(); Parent.IsValid() && !Parent.MatchesTagExact(Root); Parent = Parent.RequestDirectParent())
		{
			Kind = Parent;
		}
		return Kind;
	}
	return FGameplayTag();
}

bool UMKHQuickSlotPreferenceComponent::IsValidPreference(const FMKHQuickSlotPreference& Preference)
{
	if (!Preference.IsSet() || GetItemKindTag(Preference.ItemKindTag) != Preference.ItemKindTag)
	{
		return false;
	}

	const FGameplayTag& Slot = Preference.QuickSlotTag;
	const bool bWeaponSlot = Slot.MatchesTag(MKHGameplayTags::Input::WeaponQuickSlotCategory)
		&& !Slot.MatchesTagExact(MKHGameplayTags::Input::WeaponQuickSlotCategory);
	const bool bConsumableSlot = Slot.MatchesTag(MKHGameplayTags::Input::ConsumableQuickSlotCategory)
		&& !Slot.MatchesTagExact(MKHGameplayTags::Input::ConsumableQuickSlotCategory);

	return (bWeaponSlot && Preference.ItemKindTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
		|| (bConsumableSlot && Preference.ItemKindTag.MatchesTag(MKHGameplayTags::Equip::Category_Consumable));
}

bool UMKHQuickSlotPreferenceComponent::IsValidArmorType(const FGameplayTag& ArmorType)
{
	return ArmorType.IsValid() && FMKHDefaultQuickSlots::GetArmorType(ArmorType) == ArmorType;
}

void UMKHQuickSlotPreferenceComponent::NotifyQuickSlotEmptiedByPlayer(const FGameplayTag& QuickSlotTag)
{
	Preferences.RemoveAll([&QuickSlotTag](const FMKHQuickSlotPreference& Existing)
	{
		return Existing.QuickSlotTag.MatchesTagExact(QuickSlotTag);
	});
	ScheduleCommit();
}

void UMKHQuickSlotPreferenceComponent::NotifyArmorUnequippedByPlayer(const FGameplayTag& ItemTag)
{
	const FGameplayTag ArmorType = FMKHDefaultQuickSlots::GetArmorType(ItemTag);
	if (ArmorType.IsValid())
	{
		UnequippedArmorTypes.AddUnique(ArmorType);
		ScheduleCommit();
	}
}

void UMKHQuickSlotPreferenceComponent::NotifySkillSlotChosenByPlayer(const FGameplayTag& AbilityTag, const FGameplayTag& SkillSlotTag)
{
	const FMKHSkillSlotPreference Preference{ AbilityTag, SkillSlotTag };
	if (!IsValidSkillSlotPreference(Preference))
	{
		return;
	}

	SkillSlotPreferences.RemoveAll([&AbilityTag](const FMKHSkillSlotPreference& Existing)
	{
		return Existing.AbilityTag.MatchesTagExact(AbilityTag);
	});
	if (SkillSlotPreferences.Num() >= MaxSkillSlotPreferences)
	{
		SkillSlotPreferences.RemoveAt(0);
	}
	SkillSlotPreferences.Add(Preference);
	ScheduleCommit();
}

bool UMKHQuickSlotPreferenceComponent::IsValidSkillSlotPreference(const FMKHSkillSlotPreference& Preference)
{
	return Preference.AbilityTag.IsValid() && FMKHSkillLayout::IsSkillSlot(Preference.SkillSlotTag);
}

// ============================================================
// Owning machine
// ============================================================

void UMKHQuickSlotPreferenceComponent::LoadPreferences()
{
	Preferences.Reset();
	UnequippedArmorTypes.Reset();
	SkillSlotPreferences.Reset();
	bDefaultsApplied = false;
	if (!UGameplayStatics::DoesSaveGameExist(MKHQuickSlotPreferences::SaveSlotName, MKHQuickSlotPreferences::SaveUserIndex))
	{
		return;
	}

	const UMKHQuickSlotPreferenceSaveGame* SaveData = Cast<UMKHQuickSlotPreferenceSaveGame>(
		UGameplayStatics::LoadGameFromSlot(MKHQuickSlotPreferences::SaveSlotName, MKHQuickSlotPreferences::SaveUserIndex));
	if (!IsValid(SaveData))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHQuickSlotPreferenceComponent: unreadable '%s' save slot, starting without preferences."), MKHQuickSlotPreferences::SaveSlotName);
		return;
	}

	// Tags renamed or removed since the save are dropped.
	for (const FMKHQuickSlotPreference& Preference : SaveData->Preferences)
	{
		if (IsValidPreference(Preference) && Preferences.Num() < MaxPreferences)
		{
			Preferences.Add(Preference);
		}
	}
	for (const FGameplayTag& ArmorType : SaveData->UnequippedArmorTypes)
	{
		if (IsValidArmorType(ArmorType) && UnequippedArmorTypes.Num() < MaxUnequippedArmorTypes)
		{
			UnequippedArmorTypes.AddUnique(ArmorType);
		}
	}
	for (const FMKHSkillSlotPreference& SkillPreference : SaveData->SkillSlotPreferences)
	{
		if (IsValidSkillSlotPreference(SkillPreference) && SkillSlotPreferences.Num() < MaxSkillSlotPreferences)
		{
			SkillSlotPreferences.Add(SkillPreference);
		}
	}
	bDefaultsApplied = SaveData->bDefaultsApplied;
}

void UMKHQuickSlotPreferenceComponent::HandleQuickSlotRelocated(const FRPGInventoryEntry& Entry)
{
	// Removals never change the preferences here: only the player's own go through NotifyQuickSlotEmptiedByPlayer.
	if (!Entry.bIsQuickSlotted)
	{
		return;
	}

	const FMKHQuickSlotPreference Preference{ Entry.QuickSlotTag, GetItemKindTag(Entry.Item.ItemTag) };
	if (!IsValidPreference(Preference))
	{
		return;
	}

	// Without the flag yet, even an unchanged preference commits: this loadout was arranged, the defaults are over.
	if (RecordPreference(Preference.QuickSlotTag, Preference.ItemKindTag) || !bDefaultsApplied)
	{
		ScheduleCommit();
	}
}

void UMKHQuickSlotPreferenceComponent::HandleEquipmentEntryChanged(const FRPGEquipmentEntry& Entry)
{
	if (UnequippedArmorTypes.Remove(FMKHDefaultQuickSlots::GetArmorType(Entry.Item.ItemTag)) > 0)
	{
		ScheduleCommit();
	}
}

bool UMKHQuickSlotPreferenceComponent::RecordPreference(const FGameplayTag& QuickSlotTag, const FGameplayTag& Kind)
{
	const TArray<FMKHQuickSlotPreference> Previous = Preferences;

	// The same kind remembered in a slot that is empty now: the item moved to QuickSlotTag.
	// A weapon kind lives in one slot only (one weapon per type in the quick slots).
	const bool bWeaponKind = Kind.MatchesTag(MKHGameplayTags::Equip::Category_Weapon);
	Preferences.RemoveAll([this, &QuickSlotTag, &Kind, bWeaponKind](const FMKHQuickSlotPreference& Existing)
	{
		return Existing.QuickSlotTag.MatchesTagExact(QuickSlotTag)
			|| (Existing.ItemKindTag.MatchesTagExact(Kind) && (bWeaponKind || !IsQuickSlotFilledLocally(Existing.QuickSlotTag)));
	});
	Preferences.Add({ QuickSlotTag, Kind });
	Preferences.Sort([](const FMKHQuickSlotPreference& A, const FMKHQuickSlotPreference& B)
	{
		return A.QuickSlotTag.GetTagName().LexicalLess(B.QuickSlotTag.GetTagName());
	});

	return Preferences != Previous;
}

bool UMKHQuickSlotPreferenceComponent::IsQuickSlotFilledLocally(const FGameplayTag& QuickSlotTag) const
{
	const UInventoryComponent* Inventory = FindInventory();
	if (!IsValid(Inventory))
	{
		return false;
	}

	return Inventory->GetInventoryEntries().ContainsByPredicate([&QuickSlotTag](const FRPGInventoryEntry& Entry)
	{
		return Entry.bIsQuickSlotted && Entry.QuickSlotTag.MatchesTagExact(QuickSlotTag);
	});
}

void UMKHQuickSlotPreferenceComponent::ScheduleCommit()
{
	UWorld* World = GetWorld();
	if (bCommitScheduled || !IsValid(World))
	{
		return;
	}

	bCommitScheduled = true;
	World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CommitPreferences);
}

void UMKHQuickSlotPreferenceComponent::CommitPreferences()
{
	if (!bCommitScheduled)
	{
		return;
	}
	bCommitScheduled = false;
	bDefaultsApplied = true;

	UMKHQuickSlotPreferenceSaveGame* SaveData = Cast<UMKHQuickSlotPreferenceSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UMKHQuickSlotPreferenceSaveGame::StaticClass()));
	if (IsValid(SaveData))
	{
		// Async: the commit happens mid-match.
		SaveData->Preferences = Preferences;
		SaveData->UnequippedArmorTypes = UnequippedArmorTypes;
		SaveData->bDefaultsApplied = bDefaultsApplied;
		SaveData->SkillSlotPreferences = SkillSlotPreferences;
		UGameplayStatics::AsyncSaveGameToSlot(SaveData, MKHQuickSlotPreferences::SaveSlotName, MKHQuickSlotPreferences::SaveUserIndex);
	}

	SendPreferencesToServer();
}

void UMKHQuickSlotPreferenceComponent::SendPreferencesToServer()
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	const bool bApplyEnabled = IsAutoQuickSlotSettingEnabled();
	if (Owner->HasAuthority())
	{
		ReceivePreferences(Preferences, UnequippedArmorTypes, bDefaultsApplied, SkillSlotPreferences, bApplyEnabled);
		return;
	}
	ServerSetPreferences(Preferences, UnequippedArmorTypes, bDefaultsApplied, SkillSlotPreferences, bApplyEnabled);
}

void UMKHQuickSlotPreferenceComponent::HandleAutoQuickSlotSettingChanged(bool bEnabled)
{
	SendPreferencesToServer();
}

bool UMKHQuickSlotPreferenceComponent::IsAutoQuickSlotSettingEnabled()
{
	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	return !IsValid(UserSettings) || UserSettings->IsAutoQuickSlotLoadoutEnabled();
}

// ============================================================
// Server
// ============================================================

void UMKHQuickSlotPreferenceComponent::ServerSetPreferences_Implementation(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled)
{
	ReceivePreferences(NewPreferences, NewUnequippedArmorTypes, bNewDefaultsApplied, NewSkillSlotPreferences, bApplyEnabled);
}

bool UMKHQuickSlotPreferenceComponent::ServerSetPreferences_Validate(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled)
{
	if (NewPreferences.Num() > MaxPreferences || NewUnequippedArmorTypes.Num() > MaxUnequippedArmorTypes
		|| NewSkillSlotPreferences.Num() > MaxSkillSlotPreferences)
	{
		return false;
	}

	for (const FMKHSkillSlotPreference& SkillPreference : NewSkillSlotPreferences)
	{
		if (!IsValidSkillSlotPreference(SkillPreference))
		{
			return false;
		}
	}

	for (const FGameplayTag& ArmorType : NewUnequippedArmorTypes)
	{
		if (!IsValidArmorType(ArmorType))
		{
			return false;
		}
	}

	TSet<FGameplayTag> Slots;
	for (const FMKHQuickSlotPreference& Preference : NewPreferences)
	{
		bool bDuplicateSlot = false;
		Slots.Add(Preference.QuickSlotTag, &bDuplicateSlot);
		if (bDuplicateSlot || !IsValidPreference(Preference))
		{
			return false;
		}
	}
	return true;
}

void UMKHQuickSlotPreferenceComponent::ReceivePreferences(const TArray<FMKHQuickSlotPreference>& NewPreferences, const TArray<FGameplayTag>& NewUnequippedArmorTypes, bool bNewDefaultsApplied, const TArray<FMKHSkillSlotPreference>& NewSkillSlotPreferences, bool bApplyEnabled)
{
	SkillSlotPreferences = NewSkillSlotPreferences;
	if (UInventoryComponent* Inventory = FindInventory(); IsValid(Inventory))
	{
		// Independent from "Remember Quick Slots": the skill order always applies.
		Inventory->SetSkillLayout(NewSkillSlotPreferences);
	}
	Preferences = NewPreferences;
	UnequippedArmorTypes = NewUnequippedArmorTypes;
	bDefaultsApplied = bNewDefaultsApplied;
	bApplyPreferences = bApplyEnabled;
	bHasReceivedPreferences = true;
	TryApplyPreferences();
}

void UMKHQuickSlotPreferenceComponent::HandleStartingLoadoutGranted(const TArray<int64>& GrantedItemIDs)
{
	bLoadoutAwaitingPreferences = true;
	PendingLoadoutItemIDs = GrantedItemIDs;
	TryApplyPreferences();
}

void UMKHQuickSlotPreferenceComponent::TryApplyPreferences()
{
	if (!bLoadoutAwaitingPreferences || !bHasReceivedPreferences)
	{
		return;
	}
	bLoadoutAwaitingPreferences = false;

	UInventoryComponent* Inventory = FindInventory();
	const UQuickSlotManagerComponent* QuickSlots = FindQuickSlotManager();
	if (!IsValid(Inventory) || !IsValid(QuickSlots))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("UMKHQuickSlotPreferenceComponent: %s lacks inventory or quick slots, preferences not applied."), *GetNameSafe(GetOwner()));
		return;
	}
	ApplyArmor(*Inventory);

	// "Remember Quick Slots" off: the quick slots start empty (no preferences, no defaults).
	if (bApplyPreferences)
	{
		ApplyPreferences(*Inventory, *QuickSlots);
		ApplyDefaultQuickSlots(*Inventory, *QuickSlots);
	}
	else
	{
		UE_LOG(LogMKHInventory, Log, TEXT("UMKHQuickSlotPreferenceComponent: auto quick slots off for %s, loadout left unslotted."), *GetNameSafe(GetOwner()));
	}
	PendingLoadoutItemIDs.Reset();
}

void UMKHQuickSlotPreferenceComponent::ApplyArmor(UInventoryComponent& Inventory) const
{
	// "Remember Quick Slots" off: the armor taken off is not remembered, the rarest is worn all the same.
	const TConstArrayView<FGameplayTag> SkippedArmorTypes = bApplyPreferences ? TConstArrayView<FGameplayTag>(UnequippedArmorTypes) : TConstArrayView<FGameplayTag>();
	for (const int64 ItemID : FMKHDefaultQuickSlots::PickArmorToEquip(Inventory, PendingLoadoutItemIDs, SkippedArmorTypes))
	{
		Inventory.UseItem(ItemID, 1);
	}
}

void UMKHQuickSlotPreferenceComponent::ApplyPreferences(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots) const
{
	TSet<int64> ClaimedIDs;
	TSet<FGameplayTag> AppliedWeaponKinds;
	for (const FMKHQuickSlotPreference& Preference : Preferences)
	{
		if (QuickSlots.GetQuickSlotItemID(Preference.QuickSlotTag) != 0)
		{
			continue;
		}

		// One weapon per type: a save from before the rule may name the same weapon kind in both slots.
		const bool bWeaponKind = Preference.ItemKindTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon);
		if (bWeaponKind && AppliedWeaponKinds.Contains(Preference.ItemKindTag))
		{
			continue;
		}

		const int64 ItemID = FindBestItemOfKind(Inventory, QuickSlots, Preference.ItemKindTag, ClaimedIDs);
		if (ItemID == 0)
		{
			continue;
		}

		ClaimedIDs.Add(ItemID);
		if (bWeaponKind)
		{
			AppliedWeaponKinds.Add(Preference.ItemKindTag);
		}
		// Host: assigned at once; remote owner: ClientAssignQuickSlot, queued until the entry replicates.
		Inventory.AssignQuickSlotOnOwner(ItemID, Preference.QuickSlotTag);
		UE_LOG(LogMKHInventory, Log, TEXT("Quick slot preference: item %lld (%s) -> %s for %s."), ItemID,
			*Preference.ItemKindTag.ToString(), *Preference.QuickSlotTag.ToString(), *GetNameSafe(GetOwner()));
	}
}

void UMKHQuickSlotPreferenceComponent::ApplyDefaultQuickSlots(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots) const
{
	using namespace MKHGameplayTags::Equip;

	// Only the first match ever: from then on an empty slot is the player's choice.
	if (bDefaultsApplied)
	{
		return;
	}

	ApplyDefaultQuickSlotsOfCategory(Inventory, QuickSlots, Category_Weapon, FMKHDefaultQuickSlots::GetWeaponSlots());
	ApplyDefaultQuickSlotsOfCategory(Inventory, QuickSlots, Category_Consumable, FMKHDefaultQuickSlots::GetConsumableSlots());
}

void UMKHQuickSlotPreferenceComponent::ApplyDefaultQuickSlotsOfCategory(UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
	const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags) const
{
	// Saved preferences win: a category the player already arranged keeps only their choice.
	if (HasPreferenceForCategory(CategoryTag))
	{
		return;
	}

	for (const FMKHQuickSlotAssignment& Assignment : FMKHDefaultQuickSlots::PickForCategory(Inventory, QuickSlots, PendingLoadoutItemIDs, CategoryTag, SlotTags))
	{
		Inventory.AssignQuickSlotOnOwner(Assignment.ItemID, Assignment.QuickSlotTag);
		UE_LOG(LogMKHInventory, Log, TEXT("Default quick slot: item %lld -> %s for %s."), Assignment.ItemID,
			*Assignment.QuickSlotTag.ToString(), *GetNameSafe(GetOwner()));
	}
}

bool UMKHQuickSlotPreferenceComponent::HasPreferenceForCategory(const FGameplayTag& CategoryTag) const
{
	return Preferences.ContainsByPredicate([&CategoryTag](const FMKHQuickSlotPreference& Preference)
	{
		return Preference.ItemKindTag.MatchesTag(CategoryTag);
	});
}

int64 UMKHQuickSlotPreferenceComponent::FindBestItemOfKind(const UInventoryComponent& Inventory, const UQuickSlotManagerComponent& QuickSlots,
	const FGameplayTag& Kind, const TSet<int64>& ClaimedIDs) const
{
	const FMKHItemEntry* Best = nullptr;
	const TArray<FRPGInventoryEntry> Entries = Inventory.GetInventoryEntries();
	for (const FRPGInventoryEntry& Entry : Entries)
	{
		const FMKHItemEntry& Item = Entry.Item;
		if (!Item.IsValid() || ClaimedIDs.Contains(Item.ItemID) || QuickSlots.IsItemQuickSlotted(Item.ItemID)
			|| GetItemKindTag(Item.ItemTag) != Kind)
		{
			continue;
		}

		if (!Best || IsBetterCandidate(Item, *Best))
		{
			Best = &Item;
		}
	}
	return Best ? Best->ItemID : 0;
}

bool UMKHQuickSlotPreferenceComponent::IsBetterCandidate(const FMKHItemEntry& Candidate, const FMKHItemEntry& Best) const
{
	if (Candidate.RarityTag != Best.RarityTag)
	{
		const UMKHDataSubsystem* DataSubsystem = UMKHDataSubsystem::Get(this);
		const FRarityDefinition* CandidateRarity = IsValid(DataSubsystem) ? DataSubsystem->FindRarityDefinition(Candidate.RarityTag) : nullptr;
		const FRarityDefinition* BestRarity = IsValid(DataSubsystem) ? DataSubsystem->FindRarityDefinition(Best.RarityTag) : nullptr;
		if (CandidateRarity != BestRarity)
		{
			return IsRarer(CandidateRarity, BestRarity);
		}
	}
	return Candidate.Quantity > Best.Quantity;
}

bool UMKHQuickSlotPreferenceComponent::IsRarer(const FRarityDefinition* Rarity, const FRarityDefinition* Other)
{
	if (!Rarity || !Other)
	{
		return Rarity != nullptr;
	}

	const int32 RolledCount = Rarity->NumPassiveStats + Rarity->NumActiveAbilities;
	const int32 OtherRolledCount = Other->NumPassiveStats + Other->NumActiveAbilities;
	if (RolledCount != OtherRolledCount)
	{
		return RolledCount > OtherRolledCount;
	}
	return Rarity->RollProbability < Other->RollProbability;
}

// ============================================================
// Helpers
// ============================================================

UInventoryComponent* UMKHQuickSlotPreferenceComponent::FindInventory() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UInventoryInterface>())
	{
		return nullptr;
	}
	return IInventoryInterface::Execute_GetInventoryComponent(Owner);
}

UEquipmentManagerComponent* UMKHQuickSlotPreferenceComponent::FindEquipment() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UEquipmentInterface>())
	{
		return nullptr;
	}
	return IEquipmentInterface::Execute_GetEquipmentManagerComponent(Owner);
}

UQuickSlotManagerComponent* UMKHQuickSlotPreferenceComponent::FindQuickSlotManager() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<UQuickSlotInterface>())
	{
		return nullptr;
	}
	return IQuickSlotInterface::Execute_GetQuickSlotManagerComponent(Owner);
}

bool UMKHQuickSlotPreferenceComponent::IsLocallyControlled() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return IsValid(Controller) && Controller->IsLocalController();
}
