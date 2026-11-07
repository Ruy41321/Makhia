// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/MKHDuelBot.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/MKHSkinCatalog.h"
#include "Cosmetics/MKHSkinComponent.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/MKHItemComponentLinks.h"
#include "Inventory/MKHStartingItem.h"
#include "MKHLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "QuickSlot/MKHDefaultQuickSlots.h"
#include "QuickSlot/QuickSlotManagerComponent.h"

#define LOCTEXT_NAMESPACE "MKHDuelBot"

#if !UE_BUILD_SHIPPING
namespace MKHDuelBotDebug
{
	/** Logs the bot's quick slot state as the StateTree getters see it. */
	static void LogQuickSlots(const AMKHDuelBot& Bot, const TCHAR* When)
	{
		FString Slots;
		for (const TConstArrayView<FGameplayTag> Category : { FMKHDefaultQuickSlots::GetWeaponSlots(), FMKHDefaultQuickSlots::GetConsumableSlots() })
		{
			for (const FGameplayTag& SlotTag : Category)
			{
				Slots += FString::Printf(TEXT(" [%s x%d%s]"), *SlotTag.GetTagLeafName().ToString(), Bot.GetQuickSlotQuantity(SlotTag),
					Bot.CanUseQuickSlot(SlotTag) ? TEXT(" usable") : TEXT(""));
			}
		}
		UE_LOG(LogMKHAI, Display, TEXT("MKH.Bot %s: weapon %s, swap to %s (%s), using quick slot %s,%s"), When,
			*Bot.GetActiveWeaponQuickSlot().ToString(), *Bot.GetOtherWeaponQuickSlot().ToString(), Bot.CanSwapWeapon() ? TEXT("can swap") : TEXT("cannot swap"),
			Bot.IsUsingQuickSlot() ? TEXT("yes") : TEXT("no"), *Slots);
	}

	static FAutoConsoleCommandWithWorldAndArgs PressInputCommand(TEXT("MKH.Bot.PressInput"),
		TEXT("MKH.Bot.PressInput <InputTag>: taps an input on the duel bot's ability system, as its StateTree tasks do (e.g. Input.Ability.QuickSlot.Weapon.Secondary)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const FGameplayTag InputTag = Args.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), false) : FGameplayTag();
			for (TActorIterator<AMKHDuelBot> It(World); It && InputTag.IsValid(); ++It)
			{
				UMKHAbilitySystemComponent* AbilitySystem = Cast<UMKHAbilitySystemComponent>(It->GetAbilitySystemComponent());
				if (It->HasAuthority() && IsValid(AbilitySystem))
				{
					LogQuickSlots(**It, TEXT("before"));
					AbilitySystem->AbilityInputPressed(InputTag, false);
					AbilitySystem->AbilityInputReleased(InputTag);
					LogQuickSlots(**It, TEXT("after"));
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LogItemsCommand(TEXT("MKH.Bot.Items"), TEXT("Logs the duel bot's weapon and quick slots."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			for (TActorIterator<AMKHDuelBot> It(World); It; ++It)
			{
				LogQuickSlots(**It, TEXT("now"));
			}
		}));
}

#endif

namespace MKHDuelBotLoadout
{
	/** Turns the weapon sheathe / unsheathe events of the bot's equipment on or off (loadout rebuilds make no sound). */
	void SetWeaponEventsEnabled(UEquipmentManagerComponent* Equipment, const bool bEnabled)
	{
		if (IsValid(Equipment))
		{
			Equipment->SetWeaponEventsEnabled(bEnabled);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

AMKHDuelBot::AMKHDuelBot()
{
	DisplayName = LOCTEXT("TheAbyssDisplayName", "The Abyss");
	SkinComponent = CreateDefaultSubobject<UMKHSkinComponent>(TEXT("SkinComponent"));
	EquipmentComponent = CreateDefaultSubobject<UEquipmentManagerComponent>(TEXT("EquipmentComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	QuickSlotManagerComponent = CreateDefaultSubobject<UQuickSlotManagerComponent>(TEXT("QuickSlotManagerComponent"));

	// Driven by AMKHDuelBotController; it faces where it walks instead of following the controller rotation.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AMKHDuelBotController::StaticClass();
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bRunPhysicsWithNoController = true;
		Movement->bOrientRotationToMovement = true;
	}
}

void AMKHDuelBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMKHDuelBot, bIsDead);
}

void AMKHDuelBot::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!IsValid(InventoryComponent) || !IsValid(EquipmentComponent) || !IsValid(QuickSlotManagerComponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: %s lacks an item component, it cannot use quick slots."), *GetName());
		return;
	}

	FMKHItemComponentLinks::Bind(*InventoryComponent, *EquipmentComponent, *QuickSlotManagerComponent);
	InventoryComponent->OnStartingLoadoutGranted.AddUObject(this, &AMKHDuelBot::HandleLoadoutGranted);
}

void AMKHDuelBot::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(SkinComponent) && !SkinId.IsNone())
	{
		SkinComponent->SetSkin(SkinId);
	}

	if (const USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		DefaultMeshProfile = MeshComponent->GetCollisionProfileName();
		DefaultMeshRelativeTransform = MeshComponent->GetRelativeTransform();
	}

	if (!HasAuthority())
	{
		return;
	}

	SubscribeToEquipEvents();

	// Super::BeginPlay gave the default attributes: the equipment stats now scale a full Health.
	if (!PendingLoadout.IsEmpty())
	{
		GrantLoadoutItems(PendingLoadout);
		PendingLoadout.Reset();
	}

	if (!IsValid(MKHAbilitySystemComponent))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("MKHDuelBot: %s has no ability system component, it can never die."), *GetName());
		return;
	}

	MKHAbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(MKHGameplayTags::Event::Death)
		.AddUObject(this, &AMKHDuelBot::HandleDeathEvent);
}

void AMKHDuelBot::SetIsBaiting(bool bNewIsBaiting)
{
	bIsBaiting = bNewIsBaiting;
	SetBaitElapsedTime(0.f);
}

// ============================================================
// Public Interface (IMKHDuelParticipant)
// ============================================================

void AMKHDuelBot::ResetForNewRound(const FTransform& SpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	// Revived first: the ragdoll is put back on the capsule before the capsule moves.
	SetDead(false);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (AController* BotController = GetController())
	{
		BotController->SetControlRotation(FRotator(0.f, SpawnTransform.Rotator().Yaw, 0.f));
	}
}

void AMKHDuelBot::SetOpponentSkin(const FName NewSkinId)
{
	SkinId = NewSkinId;

	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	const FMKHSkinDefinition* Skin = IsValid(Skins) ? Skins->FindSkin(NewSkinId) : nullptr;
	if (Skin != nullptr)
	{
		DisplayName = Skin->DisplayName;
	}
	else
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: unknown skin '%s', the name does not change."), *NewSkinId.ToString());
	}

	// Before BeginPlay the skin is applied there.
	if (HasActorBegunPlay() && IsValid(SkinComponent))
	{
		SkinComponent->SetSkin(SkinId);
	}
}

FText AMKHDuelBot::GetParticipantDisplayName() const
{
	return DisplayName;
}

// ============================================================
// Public Interface (Loadout)
// ============================================================

void AMKHDuelBot::EquipLoadout(const TArray<FMKHStartingItem>& Loadout, const TArray<FGameplayTag>& Weapons)
{
	if (!HasAuthority())
	{
		return;
	}

	StartingWeapons = Weapons;

	// AGameMode can spawn the bot before the world begins play: equipping now would let the default attributes,
	// given at BeginPlay, overwrite the Health raised by the equipment stats.
	if (!HasActorBegunPlay())
	{
		PendingLoadout = Loadout;
		return;
	}

	GrantLoadoutItems(Loadout);
}

void AMKHDuelBot::ResetLoadout(const TArray<FMKHStartingItem>& Loadout, const TArray<FGameplayTag>& Weapons)
{
	if (!HasAuthority() || !IsValid(InventoryComponent))
	{
		return;
	}

	// Rebuilding the round's loadout is not a gesture of the bot: no sheathe / unsheathe sound.
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, false);
	InventoryComponent->ResetInventory();
	EquipLoadout(Loadout, Weapons);
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, true);
}

void AMKHDuelBot::AssignWeaponQuickSlots(const TArray<FGameplayTag>& Weapons)
{
	if (!HasAuthority() || !IsValid(InventoryComponent) || !IsValid(EquipmentComponent) || !IsValid(QuickSlotManagerComponent))
	{
		return;
	}

	// Resolved first: the ids must be read while the current slots still exist.
	TArray<int64> WeaponIDs;
	for (const FGameplayTag& WeaponTag : Weapons)
	{
		if (const int64 ItemID = FindOwnedWeaponID(WeaponTag))
		{
			WeaponIDs.Add(ItemID);
		}
	}

	const TConstArrayView<FGameplayTag> Slots = FMKHDefaultQuickSlots::GetWeaponSlots();
	if (WeaponIDs.IsEmpty() || Slots.IsEmpty())
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: %s owns none of the requested weapons, its quick slots do not change."), *GetName());
		return;
	}

	// New weapons for the round (loadout or difficulty director): not a gesture of the bot, no sheathe sound.
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, false);
	for (const FGameplayTag& SlotTag : Slots)
	{
		if (const int64 ItemID = QuickSlotManagerComponent->GetQuickSlotItemID(SlotTag))
		{
			InventoryComponent->RemoveEntryFromQuickSlot(ItemID);
		}
	}
	for (int32 Index = 0; Index < WeaponIDs.Num() && Slots.IsValidIndex(Index); ++Index)
	{
		InventoryComponent->AddEntryToQuickSlot(WeaponIDs[Index], Slots[Index]);
	}

	// Using the weapon already in hand would change nothing, so it is skipped.
	const FRPGEquipmentEntry* InHand = EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot);
	if (InHand == nullptr || InHand->Item.ItemID != WeaponIDs[0])
	{
		QuickSlotManagerComponent->UseQuickSlot(Slots[0]);
	}
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, true);
}

TArray<FGameplayTag> AMKHDuelBot::GetOwnedWeaponTags() const
{
	TArray<FGameplayTag> WeaponTags;
	if (!IsValid(InventoryComponent) || !IsValid(EquipmentComponent))
	{
		return WeaponTags;
	}

	for (const FRPGInventoryEntry& Entry : InventoryComponent->GetInventoryEntries())
	{
		if (Entry.Item.ItemTag.MatchesTag(MKHGameplayTags::Equip::Category_Weapon))
		{
			WeaponTags.AddUnique(Entry.Item.ItemTag);
		}
	}
	if (const FRPGEquipmentEntry* InHand = EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot))
	{
		WeaponTags.AddUnique(InHand->Item.ItemTag);
	}
	return WeaponTags;
}

// ============================================================
// Public Interface (Quick slot queries, for the StateTree)
// ============================================================

FGameplayTag AMKHDuelBot::GetActiveWeaponQuickSlot() const
{
	if (!IsValid(EquipmentComponent) || !IsValid(QuickSlotManagerComponent))
	{
		return FGameplayTag();
	}

	const FRPGEquipmentEntry* Weapon = EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot);
	return Weapon != nullptr ? QuickSlotManagerComponent->FindQuickSlotTagByItemID(Weapon->Item.ItemID) : FGameplayTag();
}

FGameplayTag AMKHDuelBot::GetOtherWeaponQuickSlot() const
{
	if (!IsValid(QuickSlotManagerComponent))
	{
		return FGameplayTag();
	}

	const FGameplayTag ActiveSlot = GetActiveWeaponQuickSlot();
	for (const FGameplayTag& SlotTag : FMKHDefaultQuickSlots::GetWeaponSlots())
	{
		if (SlotTag != ActiveSlot && QuickSlotManagerComponent->GetQuickSlotItemID(SlotTag) != 0)
		{
			return SlotTag;
		}
	}
	return FGameplayTag();
}

bool AMKHDuelBot::CanSwapWeapon() const
{
	const FGameplayTag OtherSlot = GetOtherWeaponQuickSlot();
	return OtherSlot.IsValid() && CanUseQuickSlot(OtherSlot);
}

bool AMKHDuelBot::IsUsingQuickSlot() const
{
	return IsValid(MKHAbilitySystemComponent) && MKHAbilitySystemComponent->HasMatchingGameplayTag(MKHGameplayTags::State::QuickSlotUse);
}

bool AMKHDuelBot::CanUseQuickSlot(const FGameplayTag& QuickSlotTag) const
{
	FRPGInventoryEntry Entry;
	if (!IsValid(MKHAbilitySystemComponent) || !FindQuickSlotEntry(QuickSlotTag, Entry) || Entry.bIsUsed)
	{
		return false;
	}

	// GA_QuickSlot checks the consumable cooldown only after activating (and then ends at once).
	const FGameplayTag CooldownTag = InventoryComponent->GetConsumableCooldownTag(Entry.Item.ItemID);
	if (CooldownTag.IsValid() && MKHAbilitySystemComponent->HasMatchingGameplayTag(CooldownTag))
	{
		return false;
	}

	// A running GA_QuickSlot would queue the press instead of using the slot now.
	const FGameplayAbilitySpec* Spec = MKHAbilitySystemComponent->FindAbilitySpecForInputTag(QuickSlotTag);
	return Spec != nullptr && !Spec->IsActive() && IsValid(Spec->Ability)
		&& Spec->Ability->CanActivateAbility(Spec->Handle, MKHAbilitySystemComponent->AbilityActorInfo.Get());
}

int32 AMKHDuelBot::GetQuickSlotQuantity(const FGameplayTag& QuickSlotTag) const
{
	FRPGInventoryEntry Entry;
	return FindQuickSlotEntry(QuickSlotTag, Entry) ? FMath::Max(Entry.Item.Quantity, 1) : 0;
}

FGameplayTag AMKHDuelBot::FindConsumableQuickSlot(const FGameplayTag& ItemTag) const
{
	for (const FGameplayTag& SlotTag : FMKHDefaultQuickSlots::GetConsumableSlots())
	{
		FRPGInventoryEntry Entry;
		if (FindQuickSlotEntry(SlotTag, Entry) && Entry.Item.ItemTag.MatchesTag(ItemTag))
		{
			return SlotTag;
		}
	}
	return FGameplayTag();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

int64 AMKHDuelBot::FindOwnedWeaponID(const FGameplayTag& WeaponTag) const
{
	for (const FRPGInventoryEntry& Entry : InventoryComponent->GetInventoryEntries())
	{
		if (Entry.Item.ItemTag.MatchesTag(WeaponTag))
		{
			return Entry.Item.ItemID;
		}
	}

	const FRPGEquipmentEntry* InHand = EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot);
	return InHand != nullptr && InHand->Item.ItemTag.MatchesTag(WeaponTag) ? InHand->Item.ItemID : 0;
}

void AMKHDuelBot::GrantLoadoutItems(const TArray<FMKHStartingItem>& Loadout)
{
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: %s has no inventory, loadout not granted."), *GetName());
		return;
	}

	// Same entry point as the player's loadout; HandleLoadoutGranted runs from its broadcast (in this call, so the
	// weapon it takes in hand sends no unsheathe sound).
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, false);
	InventoryComponent->GrantStartingLoadout(Loadout);
	MKHDuelBotLoadout::SetWeaponEventsEnabled(EquipmentComponent, true);
}


void AMKHDuelBot::HandleLoadoutGranted(const TArray<int64>& GrantedItemIDs)
{
	if (IsValid(InventoryComponent))
	{
		for (const int64 ArmorItemID : FMKHDefaultQuickSlots::PickArmorToEquip(*InventoryComponent, GrantedItemIDs, {}))
		{
			InventoryComponent->UseItem(ArmorItemID, 1);
		}
	}

	if (StartingWeapons.IsEmpty())
	{
		AssignDefaultQuickSlots(GrantedItemIDs, MKHGameplayTags::Equip::Category_Weapon, FMKHDefaultQuickSlots::GetWeaponSlots());
	}
	else
	{
		AssignWeaponQuickSlots(StartingWeapons);
	}
	AssignDefaultQuickSlots(GrantedItemIDs, MKHGameplayTags::Equip::Category_Consumable, FMKHDefaultQuickSlots::GetConsumableSlots());
	EquipFirstWeaponIfUnarmed();
}

void AMKHDuelBot::AssignDefaultQuickSlots(TConstArrayView<int64> ItemIDs, const FGameplayTag& CategoryTag, TConstArrayView<FGameplayTag> SlotTags)
{
	if (!IsValid(InventoryComponent) || !IsValid(QuickSlotManagerComponent))
	{
		return;
	}

	// The bot's flags live on the server (no owning client): assigned directly, never through ClientAssignQuickSlot.
	for (const FMKHQuickSlotAssignment& Assignment : FMKHDefaultQuickSlots::PickForCategory(*InventoryComponent, *QuickSlotManagerComponent, ItemIDs, CategoryTag, SlotTags))
	{
		InventoryComponent->AddEntryToQuickSlot(Assignment.ItemID, Assignment.QuickSlotTag);
		UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBot: item %lld -> %s for %s."), Assignment.ItemID, *Assignment.QuickSlotTag.ToString(), *GetName());
	}
}

void AMKHDuelBot::EquipFirstWeaponIfUnarmed()
{
	if (!IsValid(EquipmentComponent) || !IsValid(QuickSlotManagerComponent)
		|| EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot) != nullptr)
	{
		return;
	}

	// The first weapon quick slotted (Primary) is the quick equip slot; using it on the server equips at once.
	const FGameplayTag FirstWeaponSlot = QuickSlotManagerComponent->GetQuickEquipWeaponSlotTag();
	if (!FirstWeaponSlot.IsValid())
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: %s has no weapon quick slotted, it starts unarmed."), *GetName());
		return;
	}
	QuickSlotManagerComponent->UseQuickSlot(FirstWeaponSlot);
}

bool AMKHDuelBot::FindQuickSlotEntry(const FGameplayTag& QuickSlotTag, FRPGInventoryEntry& OutEntry) const
{
	if (!IsValid(InventoryComponent) || !IsValid(QuickSlotManagerComponent))
	{
		return false;
	}

	const int64 ItemID = QuickSlotManagerComponent->GetQuickSlotItemID(QuickSlotTag);
	if (ItemID == 0)
	{
		return false;
	}

	OutEntry = InventoryComponent->FindInventoryEntryByID(ItemID);
	return OutEntry.IsValid();
}

void AMKHDuelBot::HandleDeathEvent(const FGameplayEventData* Payload)
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogMKHAbility, Log, TEXT("MKHDuelBot: %s died."), *GetName());
	SetDead(true);
}

void AMKHDuelBot::SubscribeToEquipEvents()
{
	if (!IsValid(EquipmentComponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBot: %s has no equipment component, it cannot hold a weapon."), *GetName());
		return;
	}

	EquipmentComponent->EquipmentList.EquipmentEntryDelegate.AddWeakLambda(this, [this](const FRPGEquipmentEntry& Entry)
	{
		if (Entry.SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
		{
			bIsHoldingWeapon = true;
		}
	});

	EquipmentComponent->EquipmentList.UnEquippedEntryDelegate.AddWeakLambda(this, [this](const FRPGEquipmentEntry& Entry)
	{
		if (Entry.SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
		{
			bIsHoldingWeapon = false;
		}
	});
}

void AMKHDuelBot::SetDead(bool bNewDead)
{
	if (!HasAuthority() || bIsDead == bNewDead)
	{
		return;
	}

	bIsDead = bNewDead;

	// Same path as the player: the Dead state owns State.Movement.Dead (blocks abilities, damage and a second death
	// event) and the dead capsule collision on every machine. Forced both ways: death must win over any state (e.g.
	// Attacking), and forcing is the only way out of Dead.
	Multicast_TransitionToMovementState(bIsDead ? EMovementStateValue::Dead : EMovementStateValue::Idle, true);

	// OnRep does not run on the authority.
	ApplyDeathPresentation();
	ForceNetUpdate();
}

void AMKHDuelBot::OnRep_IsDead()
{
	ApplyDeathPresentation();
}

void AMKHDuelBot::ApplyDeathPresentation()
{
	if (bIsDead)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (bRagdollOnDeath)
	{
		SetMeshRagdoll(bIsDead);
	}
}

void AMKHDuelBot::SetMeshRagdoll(bool bRagdoll)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!IsValid(MeshComponent))
	{
		return;
	}

	if (bRagdoll)
	{
		MeshComponent->SetCollisionProfileName(RagdollCollisionProfile);
		IgnoreCameraCollision();
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
		return;
	}

	// Simulation moves the mesh away from the capsule: snap it back to its original layout.
	MeshComponent->SetSimulatePhysics(false);
	if (!DefaultMeshProfile.IsNone())
	{
		MeshComponent->SetCollisionProfileName(DefaultMeshProfile);
	}
	IgnoreCameraCollision();
	MeshComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComponent->SetRelativeTransform(DefaultMeshRelativeTransform);
}

#undef LOCTEXT_NAMESPACE
