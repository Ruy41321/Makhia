// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/PlayerController/MKHPlayerController.h"
#include "Career/MKHCareerSubsystem.h"

#include <MKHLogChannels.h>

#include "Input/MKHSystemInputComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/MKHItemComponentLinks.h"
#include "Inventory/Pickup/MKHPickupAutoEquip.h"
#include "Inventory/Pickup/MKHPickupInteractionComponent.h"
#include "Inventory/Pickup/MKHPickupItem.h"
#include "Libraries/MKHInputSettingsLibrary.h"
#include "Online/MKHNetDiagnosticsSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameFramework/GameMode.h"
#include "GameMode/MKHGameMode.h"
#include "GameMode/MKHGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "Player/PlayerState/MKHPlayerState.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Player/MKHPlayerCharacter.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "QuickSlot/MKHQuickSlotPreferenceComponent.h"
#include "QuickSlot/QuickSlotManagerComponent.h"
#include "UI/HUD/MainHUD.h"
#include "UI/Navigation/MKHMenuNavigationSubsystem.h"
#include "Voice/MKHVoicePlayerComponent.h"

AMKHPlayerController::AMKHPlayerController()
{
	bReplicates = true;

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->SetIsReplicated(true);

	EquipmentComponent = CreateDefaultSubobject<UEquipmentManagerComponent>(TEXT("EquipmentComponent"));
	EquipmentComponent->SetIsReplicated(true);
	
	QuickSlotManagerComponent = CreateDefaultSubobject<UQuickSlotManagerComponent>(TEXT("QuickSlotManagerComponent"));
	QuickSlotManagerComponent->SetIsReplicated(true);

	QuickSlotPreferenceComponent = CreateDefaultSubobject<UMKHQuickSlotPreferenceComponent>(TEXT("QuickSlotPreferenceComponent"));

	PickupInteractionComponent = CreateDefaultSubobject<UMKHPickupInteractionComponent>(TEXT("PickupInteractionComponent"));

	VoiceComponent = CreateDefaultSubobject<UMKHVoicePlayerComponent>(TEXT("VoiceComponent"));
}

UMKHAbilitySystemComponent* AMKHPlayerController::GetMKHAbilitySystemComponent()
{
	if (!IsValid(MKHAbilitySystemComponent))
	{
		if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>())
		{
			if (UMKHAbilitySystemComponent* ASC = MKHPlayerState->GetRPGAbilitySystemComponent())
			{
				MKHAbilitySystemComponent = ASC;
			}
		}
	}

	return MKHAbilitySystemComponent;
}

void AMKHPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();
	if (UMKHSystemInputComponent* RPGInputComponent = Cast<UMKHSystemInputComponent>(InputComponent))
	{
		if (IsValid(VoiceComponent))
		{
			VoiceComponent->SetupInput(RPGInputComponent);
		}

		RPGInputComponent->BindAbilityActions(MKHInputConfig, MKHGameplayTags::Input::Ability,this, &AMKHPlayerController::AbilityInputPressed, &AMKHPlayerController::AbilityInputReleased);
		
		if (IsValid(ToggleInventoryAction))
		{
			RPGInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AMKHPlayerController::ToggleInventory);
		}
		
		if (IsValid(ToggleGameMenuAction))
		{
			RPGInputComponent->BindAction(ToggleGameMenuAction, ETriggerEvent::Started, this, &AMKHPlayerController::ToggleGameMenu);
		}

		if (IsValid(ToggleHUDAction))
		{
			RPGInputComponent->BindAction(ToggleHUDAction, ETriggerEvent::Started, this, &AMKHPlayerController::ToggleHUD);
		}

		if (IsValid(InteractAction))
		{
			// IA_Interact has no trigger (implicit Down): Triggered every frame while held, Completed on release.
			RPGInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AMKHPlayerController::HandleInteractStarted);
			RPGInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &AMKHPlayerController::HandleInteractHeld);
			RPGInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AMKHPlayerController::HandleInteractReleased);
			RPGInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AMKHPlayerController::HandleInteractReleased);
		}

		if (IsValid(SkipPreparationAction))
		{
			RPGInputComponent->BindAction(SkipPreparationAction, ETriggerEvent::Started, this, &AMKHPlayerController::HandleSkipPreparation);
		}

		if (IsValid(SwitchWeaponAction))
		{
			RPGInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AMKHPlayerController::HandleSwitchWeaponPressed);
			RPGInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Completed, this, &AMKHPlayerController::HandleSwitchWeaponReleased);
		}

		if (IsValid(SkipIntroAction))
		{
			RPGInputComponent->BindAction(SkipIntroAction, ETriggerEvent::Ongoing, this, &AMKHPlayerController::HandleSkipIntroHeld);
			RPGInputComponent->BindAction(SkipIntroAction, ETriggerEvent::Canceled, this, &AMKHPlayerController::HandleSkipIntroReleased);
			RPGInputComponent->BindAction(SkipIntroAction, ETriggerEvent::Completed, this, &AMKHPlayerController::HandleSkipIntroReleased);
			RPGInputComponent->BindAction(SkipIntroAction, ETriggerEvent::Triggered, this, &AMKHPlayerController::HandleSkipIntroTriggered);
		}
	}
}

void AMKHPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMKHPlayerController, InventoryComponent);
}

void AMKHPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	BindCallbacksToDependencies();

	if (HasAuthority())
	{
		SubscribeToEquipEvents();
	}
}

void AMKHPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	RegisterMappingContextsWithUserSettings();
	ChangeMappingContext(GetGameplayMappingContext());

	if (IsLocalController())
	{
		StartNatTypeReporting();
		ReportSkin();
		ReportCareerLevels();

		if (const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings(); IsValid(UserSettings))
		{
			UserSettings->ApplyControllerVibration(this);
		}
	}
}

void AMKHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMKHNetDiagnosticsSubsystem* Diagnostics = UMKHNetDiagnosticsSubsystem::Get(this); IsValid(Diagnostics))
	{
		Diagnostics->OnLocalNatTypeChanged.Remove(NatTypeChangedHandle);
	}
	NatTypeChangedHandle.Reset();

	if (UMKHMenuNavigationSubsystem* Navigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(Navigation))
	{
		Navigation->SetMenuInputActive(false);
	}

	Super::EndPlay(EndPlayReason);
}

void AMKHPlayerController::AcknowledgePossession(APawn* NewPawn)
{
	Super::AcknowledgePossession(NewPawn);

	// A freshly possessed pawn is alive by definition, so a block left over from a previous life
	// must not survive into it. Matters because this controller outlives its pawn across a seamless
	// travel, and a brand new character never exits the Dead state that would otherwise clear it.
	// No-op in the common case: the flag is already false and SetGameplayInputEnabled early-outs.
	SetGameplayInputEnabled(true);
}

void AMKHPlayerController::ChangeMappingContext(const UInputMappingContext* NewMappingContext) const
{
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();

		// A null context is a valid request: it means "no mappings at all", used to fully block input.
		if (IsValid(NewMappingContext))
		{
			Subsystem->AddMappingContext(NewMappingContext, 0);
		}

		// The intro context is an overlay: it must outlive focus and blocked-input swaps.
		if (bMatchIntroInputActive && IsValid(MatchIntroMappingContext))
		{
			Subsystem->AddMappingContext(MatchIntroMappingContext, MatchIntroMappingPriority);
		}

		MappingContextChangedDelegate.Broadcast(NewMappingContext);
	}
}

void AMKHPlayerController::RegisterMappingContextsWithUserSettings() const
{
	UEnhancedInputUserSettings* UserSettings = UMKHInputSettingsLibrary::GetInputUserSettings(this);
	if (!IsValid(UserSettings))
	{
		// Expected on remote controllers; on the local one it means user settings are disabled.
		if (IsLocalController())
		{
			UE_LOG(LogMKHInput, Warning,
				TEXT("PlayerController: no input user settings. Enable bEnableUserSettings in DefaultInput.ini."));
		}
		return;
	}

	// Every context the player can ever be switched to, so no action is missing from the profile.
	TSet<UInputMappingContext*> Contexts;
	Contexts.Add(DefaultMappingContext);
	Contexts.Add(UIOpenMappingContext);
	Contexts.Add(DeadMappingContext);
	Contexts.Add(MatchIntroMappingContext);
	Contexts.Remove(nullptr);

	UserSettings->RegisterInputMappingContexts(Contexts);
}

const UInputMappingContext* AMKHPlayerController::GetGameplayMappingContext() const
{
	return bGameplayInputBlocked ? DeadMappingContext.Get() : DefaultMappingContext.Get();
}

void AMKHPlayerController::SetGameplayInputEnabled(const bool bEnabled)
{
	// Only the machine that owns the local player has an Enhanced Input subsystem to talk to;
	// on every other machine this is a no-op and the flag must not be left out of sync.
	if (!IsLocalController())
	{
		return;
	}

	if (bGameplayInputBlocked == !bEnabled)
	{
		return;
	}

	bGameplayInputBlocked = !bEnabled;
	ChangeMappingContext(GetGameplayMappingContext());
}

void AMKHPlayerController::SetMatchIntroInputActive(const bool bActive)
{
	if (!IsLocalController() || bMatchIntroInputActive == bActive)
	{
		return;
	}
	if (!IsValid(MatchIntroMappingContext) || !IsValid(SkipIntroAction))
	{
		UE_LOG(LogMKHInput, Warning, TEXT("PlayerController: MatchIntroMappingContext / SkipIntroAction not set, the intro cannot be skipped."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}

	bMatchIntroInputActive = bActive;
	if (bActive)
	{
		Subsystem->AddMappingContext(MatchIntroMappingContext, MatchIntroMappingPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(MatchIntroMappingContext);
		OnSkipIntroHoldChanged.Broadcast(0.f);
	}
}

float AMKHPlayerController::GetSkipIntroHoldSeconds() const
{
	if (IsValid(SkipIntroAction))
	{
		for (const UInputTrigger* Trigger : SkipIntroAction->Triggers)
		{
			if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
			{
				return Hold->HoldTimeThreshold;
			}
		}
	}
	return 0.f;
}

void AMKHPlayerController::Server_RequestRematch_Implementation()
{
	AMKHGameMode* GameMode = Cast<AMKHGameMode>(UGameplayStatics::GetGameMode(this));
	if (!IsValid(GameMode))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("Server_RequestRematch: no authoritative MKH game mode, vote ignored."));
		return;
	}

	GameMode->RequestRematch(this);
}

void AMKHPlayerController::Server_RestartMatch_Implementation()
{
	AGameMode* GameMode = Cast<AGameMode>(UGameplayStatics::GetGameMode(this));
	if (!IsValid(GameMode))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("Server_RestartMatch: no authoritative game mode, restart ignored."));
		return;
	}

	GameMode->RestartGame();
}

void AMKHPlayerController::Server_ToggleSkipPreparationVote_Implementation()
{
	AMKHGameMode* GameMode = Cast<AMKHGameMode>(UGameplayStatics::GetGameMode(this));
	if (!IsValid(GameMode))
	{
		UE_LOG(LogMKHAbility, Warning, TEXT("Server_ToggleSkipPreparationVote: no authoritative MKH game mode, vote ignored."));
		return;
	}

	GameMode->ToggleSkipPreparationVote(this);
}

bool AMKHPlayerController::Server_PickupItem_Validate(AMKHPickupItem* Pickup)
{
	// Nothing here can prove a cheat: a null pickup is a normal race, handled softly by the implementation.
	return true;
}

void AMKHPlayerController::Server_PickupItem_Implementation(AMKHPickupItem* Pickup)
{
	// A refused request must not leave an older pickup to be equipped by the hold that follows.
	LastCollectedItemID = 0;


	// Null when the pickup was destroyed before the request arrived (collected by someone else, double press).
	if (!IsValid(Pickup))
	{
		UE_LOG(LogMKHInventory, Verbose, TEXT("Server_PickupItem: %s asked for a pickup that no longer exists."), *GetNameSafe(this));
		return;
	}

	// Every check comes before the claim: a claimed pickup can never be collected by anyone else.
	if (!CanCollectPickup(*Pickup) || !Pickup->TryClaimForPickup())
	{
		return;
	}

	// The item is ours right away; the pickup only lives on for its collection animation.
	LastCollectedItemID = InventoryComponent->AddItemEntry(Pickup->GetItem());
	LastCollectedQuantity = FMath::Max(Pickup->GetItem().Quantity, 1);
	LastCollectedTime = GetWorld()->GetTimeSeconds();
	Pickup->StartCollect(GetPawn());
}

bool AMKHPlayerController::Server_EquipLastPickup_Validate()
{
	return true;
}

void AMKHPlayerController::Server_EquipLastPickup_Implementation()
{
	// Used once: a second request (or one after a refused pickup) finds nothing.
	const int64 ItemID = LastCollectedItemID;
	LastCollectedItemID = 0;
	if (ItemID == 0 || GetWorld()->GetTimeSeconds() - LastCollectedTime > HoldToEquipServerWindow)
	{
		UE_LOG(LogMKHInventory, Verbose, TEXT("Server_EquipLastPickup: %s has no recent pickup to equip."), *GetNameSafe(this));
		return;
	}

	if (!IsValid(InventoryComponent) || !IsValid(QuickSlotManagerComponent))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Server_EquipLastPickup: %s has no inventory or quick slots, item %lld only picked up."), *GetNameSafe(this), ItemID);
		return;
	}
	FMKHPickupAutoEquip::ApplyOnServer(*InventoryComponent, *QuickSlotManagerComponent, ItemID, LastCollectedQuantity);
}

bool AMKHPlayerController::Server_ReportNatType_Validate(EMKHNatType NatType)
{
	return NatType <= EMKHNatType::Strict;
}

void AMKHPlayerController::Server_ReportNatType_Implementation(EMKHNatType NatType)
{
	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetNatType(NatType);
	}
}

void AMKHPlayerController::StartNatTypeReporting()
{
	UMKHNetDiagnosticsSubsystem* Diagnostics = UMKHNetDiagnosticsSubsystem::Get(this);
	if (!IsValid(Diagnostics) || NatTypeChangedHandle.IsValid())
	{
		return;
	}

	NatTypeChangedHandle = Diagnostics->OnLocalNatTypeChanged.AddUObject(this, &AMKHPlayerController::ReportNatType);
	ReportNatType(Diagnostics->GetLocalNatType());
}

void AMKHPlayerController::ReportNatType(EMKHNatType NatType)
{
	if (!HasAuthority())
	{
		Server_ReportNatType(NatType);
		return;
	}

	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetNatType(NatType);
	}
}

void AMKHPlayerController::Server_ReportSkin_Implementation(FName SkinId)
{
	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetSkinId(SkinId);
	}
}

void AMKHPlayerController::ReportSkin()
{
	const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
	if (!IsValid(Skins))
	{
		return;
	}

	if (!HasAuthority())
	{
		Server_ReportSkin(Skins->GetSelectedSkinId());
		return;
	}

	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetSkinId(Skins->GetSelectedSkinId());
	}
}

void AMKHPlayerController::Server_ReportCareerLevels_Implementation(const int32 CareerLevels)
{
	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetCareerLevels(CareerLevels);
	}
}

void AMKHPlayerController::ReportCareerLevels()
{
	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	if (!IsValid(Career))
	{
		return;
	}

	if (!HasAuthority())
	{
		Server_ReportCareerLevels(Career->GetCompletedLevels());
		return;
	}

	if (AMKHPlayerState* MKHPlayerState = GetPlayerState<AMKHPlayerState>(); IsValid(MKHPlayerState))
	{
		MKHPlayerState->SetCareerLevels(Career->GetCompletedLevels());
	}
}

bool AMKHPlayerController::CanCollectPickup(const AMKHPickupItem& Pickup) const
{
	const APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn) || !IsValid(InventoryComponent))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Server_PickupItem: %s has no pawn or no inventory."), *GetNameSafe(this));
		return false;
	}

	// Not a warning: the request may have left the client just before the death replicated.
	if (IsPawnDead())
	{
		UE_LOG(LogMKHInventory, Log, TEXT("Server_PickupItem: %s is dead, %s not collected."), *GetNameSafe(MyPawn), *Pickup.GetName());
		return false;
	}

	if (!Pickup.GetItem().IsValid())
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Server_PickupItem: %s holds no item."), *Pickup.GetName());
		return false;
	}

	// Not a warning: the client only offers a pickup once its appearance ended there, which is after the server.
	if (!Pickup.HasFinishedAppearing())
	{
		UE_LOG(LogMKHInventory, Log, TEXT("Server_PickupItem: %s is still appearing, not collected."), *Pickup.GetName());
		return false;
	}

	if (!Pickup.CanBeCollectedBy(PlayerState))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Server_PickupItem: %s is reserved to another player, %s cannot collect it."),
			*Pickup.GetName(), *GetNameSafe(this));
		return false;
	}

	if (!Pickup.IsPawnInInteractionRange(*MyPawn, PickupServerReachTolerance))
	{
		UE_LOG(LogMKHInventory, Warning, TEXT("Server_PickupItem: %s is out of range of %s."), *GetNameSafe(MyPawn), *Pickup.GetName());
		return false;
	}

	return true;
}

bool AMKHPlayerController::IsPawnDead() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(MKHGameplayTags::State::Movement::Dead);
}

UAbilitySystemComponent* AMKHPlayerController::GetAbilitySystemComponent() const
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
}

UEquipmentManagerComponent* AMKHPlayerController::GetEquipmentManagerComponent_Implementation() const
{
	return EquipmentComponent;
}

UInventoryComponent* AMKHPlayerController::GetInventoryComponent_Implementation() const
{
	return InventoryComponent;
}

UQuickSlotManagerComponent* AMKHPlayerController::GetQuickSlotManagerComponent_Implementation() const
{
	return QuickSlotManagerComponent;
}

bool AMKHPlayerController::EnsureWeaponEquipped(const FGameplayTag& InputTag, bool& bForceQueue)
{
	// While a weapon swap is in flight the equipment list may be momentarily stale
	// (PlayerController and PlayerState replicate independently): queue the attack
	// instead of re-triggering the quick-equip flow on stale data.
	UMKHAbilitySystemComponent* MKHAsc = GetMKHAbilitySystemComponent();
	if (IsValid(MKHAsc) && MKHAsc->IsWeaponSwapInFlight())
	{
		bForceQueue = true;
		return true;
	}

	// The ASC already granting this input tag counts as armed too, even if the equipment
	// list (a separate actor) hasn't caught up yet.
	const bool bHasWeapon = EquipmentComponent->GetEquipmentEntryBySlot(MKHGameplayTags::Equip::WeaponSlot) != nullptr
		|| (IsValid(MKHAsc) && MKHAsc->HasGrantedAbilityForInputTag(InputTag));

	// Handle weapon equipping logic if unarmed
	if (!bHasWeapon)
	{       
		const FGameplayTag QuickEquipTag = QuickSlotManagerComponent->GetQuickEquipWeaponSlotTag();
		if (!QuickEquipTag.IsValid())
		{
			// Doesn't have a weapon in quick slots
			return false;
		}

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			// States when the ability should be queued
			if (ASC->HasMatchingGameplayTag(MKHGameplayTags::Ability::Priority1))
			{
				// Queue both the ability to equip the weapon and the attack (toggleing bForceQueue)
				bForceQueue = true;
				if (IsValid(GetMKHAbilitySystemComponent()))
				{
					MKHAbilitySystemComponent->AbilityInputPressed(QuickEquipTag, bForceQueue);
				}
			}
			else if (QuickSlotManagerComponent->TryEquipWeapon())
			{
				// On a listen server the equip runs synchronously in authority and the ability may
				// already be granted; only queue the attack if it truly isn't available yet, which
				// happens on remote clients while the equip RPC round trip is still in flight.
				if (!IsValid(MKHAsc) || !MKHAsc->HasGrantedAbilityForInputTag(InputTag))
				{
					bForceQueue = true;
				}
			}
			else
			{
				// Abort input processing if we are not queuing and equip attempt fails
				return false;
			}
		}
	}

	return true;
}

void AMKHPlayerController::AbilityInputPressed(const FGameplayTag& InputTag)
{
    bool bForceQueue = false;

    // Process specific logic for attacks that are not dodges and so need an equipped weapon
    if (InputTag.MatchesTag(MKHGameplayTags::Input::Attacks) && !InputTag.MatchesTag(MKHGameplayTags::Input::Dodge))
    {
        if (IsLocalController() && InputTag.MatchesTag(MKHGameplayTags::Input::Skill))
        {
            SkillActivatedDelegate.ExecuteIfBound(InputTag);
        }
       
    	// Return true if has equipped the weapon or queued it.
        if (!EnsureWeaponEquipped(InputTag, bForceQueue))
        {
            return;
        }
    }
       
    // On clients the ASC lives on the PlayerState and may not have replicated yet
    // (e.g. input pressed right after joining): drop the input instead of crashing.
    if (!IsValid(GetMKHAbilitySystemComponent()))
    {
        UE_LOG(LogMKHAbility, Warning, TEXT("AMKHPlayerController::AbilityInputPressed - ASC not available yet, ignoring input %s"), *InputTag.ToString());
        return;
    }

    MKHAbilitySystemComponent->AbilityInputPressed(InputTag, bForceQueue);
}

void AMKHPlayerController::AbilityInputReleased(const FGameplayTag& InputTag)
{
	if (IsValid(GetMKHAbilitySystemComponent()))
	{
		MKHAbilitySystemComponent->AbilityInputReleased(InputTag);
	}
}

/**
 * Callbacks between components (stays in the model)
 */
void AMKHPlayerController::BindCallbacksToDependencies() const
{
	if (IsValid(InventoryComponent) && IsValid(EquipmentComponent) && IsValid(QuickSlotManagerComponent))
	{
		FMKHItemComponentLinks::Bind(*InventoryComponent, *EquipmentComponent, *QuickSlotManagerComponent);
	}
}

void AMKHPlayerController::ChangeFocus(const EFocusOption NewFocus)
{
	CurrentFocus = NewFocus;

	switch (NewFocus)
	{
		case EFocusOption::UI:
			SetShowMouseCursor(true);
			SetInputMode(FInputModeGameAndUI());
			ChangeMappingContext(UIOpenMappingContext);
			break;
		case EFocusOption::Game:
		default:
			SetShowMouseCursor(false);
			SetInputMode(FInputModeGameOnly());
			// Returning to game focus must not resurrect the full gameplay bindings while input is blocked
			// (e.g. closing the menu after dying).
			ChangeMappingContext(GetGameplayMappingContext());
	}

	// The gamepad drives the menus only while a UI screen has the input.
	if (UMKHMenuNavigationSubsystem* Navigation = UMKHMenuNavigationSubsystem::Get(this); IsValid(Navigation))
	{
		Navigation->SetMenuInputActive(NewFocus == EFocusOption::UI);
	}
}

void AMKHPlayerController::ToggleInventory()
{
	if (AMainHUD* HUD = GetHUD<AMainHUD>())
	{
		HUD->ToggleInventory();
	}
}

void AMKHPlayerController::ToggleGameMenu()
{
	if (AMainHUD* HUD = GetHUD<AMainHUD>())
	{
		HUD->ToggleGameMenu();
	}
}

void AMKHPlayerController::ToggleHUD()
{
	if (AMainHUD* HUD = GetHUD<AMainHUD>())
	{
		HUD->ToggleHUDHiddenByPlayer();
	}
}

void AMKHPlayerController::HandleInteractStarted()
{
	bInteractHoldPending = false;
	AMKHPickupItem* FocusedPickup = IsValid(PickupInteractionComponent) ? PickupInteractionComponent->GetFocusedPickup() : nullptr;
	if (!IsValid(FocusedPickup))
	{
		return;
	}

	// Collected at once (collection animation, item in the bag); keeping the button held then equips it.
	Server_PickupItem(FocusedPickup);

	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	bInteractHoldPending = IsValid(UserSettings) && UserSettings->IsHoldToEquipPickupEnabled();
}

void AMKHPlayerController::HandleInteractHeld(const FInputActionInstance& Instance)
{
	if (!bInteractHoldPending)
	{
		return;
	}

	if (IsUIFocused() || IsPawnDead())
	{
		bInteractHoldPending = false;
		return;
	}

	if (Instance.GetElapsedTime() >= HoldToEquipDuration)
	{
		bInteractHoldPending = false;
		Server_EquipLastPickup();
	}
}

void AMKHPlayerController::HandleInteractReleased()
{
	bInteractHoldPending = false;
}

void AMKHPlayerController::HandleSwitchWeaponPressed()
{
	// Same path as the two weapon slot keys, so queueing and weapon swap rules stay in one place.
	PressedSwitchWeaponTag = ResolveSwitchWeaponTag();
	if (PressedSwitchWeaponTag.IsValid())
	{
		AbilityInputPressed(PressedSwitchWeaponTag);
	}
}

void AMKHPlayerController::HandleSwitchWeaponReleased()
{
	if (PressedSwitchWeaponTag.IsValid())
	{
		AbilityInputReleased(PressedSwitchWeaponTag);
		PressedSwitchWeaponTag = FGameplayTag();
	}
}

FGameplayTag AMKHPlayerController::ResolveSwitchWeaponTag() const
{
	using namespace MKHGameplayTags::Equip;

	if (!IsValid(QuickSlotManagerComponent))
	{
		return FGameplayTag();
	}

	const FGameplayTag Current = QuickSlotManagerComponent->GetQuickEquipWeaponSlotTag();
	const FGameplayTag Other = Current.MatchesTagExact(WeaponQuickSlot1) ? WeaponQuickSlot2 : WeaponQuickSlot1;

	if (QuickSlotManagerComponent->GetQuickSlotItemID(Other) != 0)
	{
		return Other;
	}
	return QuickSlotManagerComponent->GetQuickSlotItemID(Current) != 0 ? Current : FGameplayTag();
}

void AMKHPlayerController::HandleSkipPreparation()
{
	// Local pre-check only, to avoid pointless RPCs: the server re-validates every vote.
	const UWorld* World = GetWorld();
	const AMKHGameState* GameState = IsValid(World) ? World->GetGameState<AMKHGameState>() : nullptr;
	if (!IsValid(GameState) || !GameState->IsPreparationSkipBallotOpen())
	{
		return;
	}

	Server_ToggleSkipPreparationVote();
}

void AMKHPlayerController::HandleSkipIntroHeld(const FInputActionInstance& Instance)
{
	const float HoldSeconds = GetSkipIntroHoldSeconds();
	OnSkipIntroHoldChanged.Broadcast(HoldSeconds > 0.f ? FMath::Clamp(Instance.GetElapsedTime() / HoldSeconds, 0.f, 1.f) : 0.f);
}

void AMKHPlayerController::HandleSkipIntroReleased()
{
	OnSkipIntroHoldChanged.Broadcast(0.f);
}

void AMKHPlayerController::HandleSkipIntroTriggered()
{
	OnSkipIntroHoldChanged.Broadcast(1.f);
	OnSkipIntroRequested.Broadcast();
}

void AMKHPlayerController::SubscribeToEquipEvents() const
{
	if (!IsValid(EquipmentComponent))
	{
		return;
	}

	// Binding to Equipped
	EquipmentComponent->EquipmentList.EquipmentEntryDelegate.AddLambda(
		[this](const FRPGEquipmentEntry& Entry)
		{
			if (Entry.SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
			{
				AMKHPlayerCharacter* PlayerCharacter = Cast<AMKHPlayerCharacter>(GetPawn());
				if (PlayerCharacter)
				{
					PlayerCharacter->bIsHoldingWeapon = true;
				}
			}
		});

	//Binding to Unequipped
	EquipmentComponent->EquipmentList.UnEquippedEntryDelegate.AddLambda(
		[this](const FRPGEquipmentEntry& Entry)
		{
			if (Entry.SlotTag.MatchesTag(MKHGameplayTags::Equip::WeaponSlot))
			{
				AMKHPlayerCharacter* PlayerCharacter = Cast<AMKHPlayerCharacter>(GetPawn());
				if (PlayerCharacter)
				{
					PlayerCharacter->bIsHoldingWeapon = false;
				}
			}
		});
}
