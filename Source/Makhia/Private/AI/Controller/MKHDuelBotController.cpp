// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "AI/Controller/MKHDuelBotController.h"

#include "AbilitySystem/MKHAbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "AI/MKHDuelBot.h"
#include "Character/CharacterBase.h"
#include "AI/Controller/MKHDuelBotStateTreeComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Interfaces/MKHDuelParticipantInterface.h"
#include "MKHLogChannels.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "AI/Data/MKHDuelBotScoring.h"

namespace MKHDuelBotControllerPrivate
{
	/**
	 * Imports a value into a (nested) field of a struct, found by a dot separated, case insensitive path.
	 *
	 * @param Struct    Type of Data.
	 * @param Data      Struct memory.
	 * @param Path      Field path (e.g. "States.Bait.ChaseMemory").
	 * @param Value     Value in the property's text format.
	 * @param OutError  Why it failed.
	 * @return True when imported.
	 */
	static bool ImportFieldValue(const UScriptStruct* Struct, void* Data, const FString& Path, const FString& Value, FString& OutError)
	{
		TArray<FString> Names;
		Path.ParseIntoArray(Names, TEXT("."));
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			FProperty* Property = nullptr;
			for (TFieldIterator<FProperty> It(Struct); It && Property == nullptr; ++It)
			{
				Property = It->GetName().Equals(Names[Index], ESearchCase::IgnoreCase) ? *It : nullptr;
			}
			if (Property == nullptr)
			{
				OutError = FString::Printf(TEXT("no field '%s' in %s"), *Names[Index], *Struct->GetName());
				return false;
			}

			void* FieldData = Property->ContainerPtrToValuePtr<void>(Data);
			if (Index == Names.Num() - 1)
			{
				const TCHAR* Result = Property->ImportText_Direct(*Value, FieldData, nullptr, PPF_None);
				OutError = Result == nullptr ? FString::Printf(TEXT("'%s' is not a valid %s"), *Value, *Property->GetCPPType()) : FString();
				return Result != nullptr;
			}

			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty == nullptr)
			{
				OutError = FString::Printf(TEXT("'%s' has no fields"), *Names[Index]);
				return false;
			}
			Struct = StructProperty->Struct;
			Data = FieldData;
		}
		OutError = TEXT("empty path");
		return false;
	}

	/** Applies overrides in order; a failing one is logged and skipped. */
	static void ApplyOverrides(const UScriptStruct* Struct, void* Data, TConstArrayView<TPair<FString, FString>> Overrides)
	{
		for (const TPair<FString, FString>& Override : Overrides)
		{
			FString Error;
			if (!ImportFieldValue(Struct, Data, Override.Key, Override.Value, Error))
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotController: override %s = %s skipped: %s."), *Override.Key, *Override.Value, *Error);
			}
		}
	}

	/** Logs every top level field of a struct as text. */
	static void LogStructFields(const TCHAR* Title, const UScriptStruct* Struct, const void* Data)
	{
		UE_LOG(LogMKHAI, Display, TEXT("%s:"), Title);
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FString Text;
			It->ExportText_InContainer(0, Text, Data, nullptr, nullptr, PPF_None);
			UE_LOG(LogMKHAI, Display, TEXT("  %s = %s"), *It->GetName(), *Text);
		}
	}
}

// ============================================================
// Lifecycle
// ============================================================

AMKHDuelBotController::AMKHDuelBotController()
{
	StateTreeComponent = CreateDefaultSubobject<UMKHDuelBotStateTreeComponent>(TEXT("StateTreeComponent"));
	StateTreeComponent->SetStartLogicAutomatically(false);
	PrimaryActorTick.bCanEverTick = true;

	// The Idle fallback is off while every state is enabled.
	DisabledStates.AddTag(MKHGameplayTags::AIState::Idle);
}

void AMKHDuelBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AcquireOpponent();
	if (!IsValid(StateTreeComponent))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotController: no StateTree component, %s will not act."), *GetNameSafe(InPawn));
		return;
	}
	StateTreeComponent->StartLogic();
}

void AMKHDuelBotController::OnUnPossess()
{
	if (IsValid(StateTreeComponent))
	{
		StateTreeComponent->StopLogic(TEXT("Unpossessed"));
	}

	Super::OnUnPossess();
}

void AMKHDuelBotController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshEffectiveParameters();
	if (bBotPaused && IsValid(StateTreeComponent))
	{
		// Paused: the evaluators keep reading the duel (scores, MKH.Bot.DumpAttackOptions), no task acts.
		StateTreeComponent->TickEvaluatorsOnly(DeltaSeconds);
	}
	RestartStoppedTree();
	if (bPrintStateTreeChanges)
	{
		PrintStateTreeChanges();
	}
}

// ============================================================
// Public Interface
// ============================================================

ACharacterBase* AMKHDuelBotController::GetOpponent() const
{
	return Opponent.Get();
}

bool AMKHDuelBotController::IsCharacterDead(const ACharacterBase* Character)
{
	if (!IsValid(Character))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(MKHGameplayTags::State::Movement::Dead);
}

UEquipmentManagerComponent* AMKHDuelBotController::GetEquipmentManagerComponent_Implementation() const
{
	const AMKHDuelBot* Bot = Cast<AMKHDuelBot>(GetPawn());
	return IsValid(Bot) ? Bot->GetEquipmentComponent() : nullptr;
}

UInventoryComponent* AMKHDuelBotController::GetInventoryComponent_Implementation() const
{
	const AMKHDuelBot* Bot = Cast<AMKHDuelBot>(GetPawn());
	return IsValid(Bot) ? Bot->GetInventory() : nullptr;
}

UQuickSlotManagerComponent* AMKHDuelBotController::GetQuickSlotManagerComponent_Implementation() const
{
	const AMKHDuelBot* Bot = Cast<AMKHDuelBot>(GetPawn());
	return IsValid(Bot) ? Bot->GetQuickSlots() : nullptr;
}

bool AMKHDuelBotController::SetParameterOverride(const FString& Path, const FString& Value, FString& OutError)
{
	// Tried on a copy first: a wrong path or value never touches the effective parameters.
	FMKHDuelBotTuning TuningCopy = EffectiveTuning;
	if (MKHDuelBotControllerPrivate::ImportFieldValue(FMKHDuelBotTuning::StaticStruct(), &TuningCopy, Path, Value, OutError))
	{
		TuningOverrides.Emplace(Path, Value);
	}
	else
	{
		const FString TuningError = OutError;
		FMKHDuelBotScoringRow ScoringCopy = EffectiveScoring;
		if (!MKHDuelBotControllerPrivate::ImportFieldValue(FMKHDuelBotScoringRow::StaticStruct(), &ScoringCopy, Path, Value, OutError))
		{
			OutError = FString::Printf(TEXT("tuning: %s; scoring: %s"), *TuningError, *OutError);
			return false;
		}
		ScoringOverrides.Emplace(Path, Value);
	}

	RefreshEffectiveParameters();
	UE_LOG(LogMKHAI, Display, TEXT("MKHDuelBotController: override %s = %s (session only)."), *Path, *Value);
	return true;
}

void AMKHDuelBotController::ClearParameterOverrides()
{
	TuningOverrides.Reset();
	ScoringOverrides.Reset();
	RefreshEffectiveParameters();
	UE_LOG(LogMKHAI, Display, TEXT("MKHDuelBotController: session overrides cleared."));
}

void AMKHDuelBotController::DumpParameters() const
{
	UE_LOG(LogMKHAI, Display, TEXT("MKHDuelBotController: profile %s, scoring table %s."), *GetNameSafe(ActiveProfile),
		IsValid(ActiveProfile) ? *GetNameSafe(ActiveProfile->GetScoringTable()) : TEXT("none"));
	MKHDuelBotControllerPrivate::LogStructFields(TEXT("Tuning"), FMKHDuelBotTuning::StaticStruct(), &EffectiveTuning);
	MKHDuelBotControllerPrivate::LogStructFields(TEXT("Scoring"), FMKHDuelBotScoringRow::StaticStruct(), &EffectiveScoring);
	for (const TPair<FString, FString>& Override : TuningOverrides)
	{
		UE_LOG(LogMKHAI, Display, TEXT("  tuning override %s = %s"), *Override.Key, *Override.Value);
	}
	for (const TPair<FString, FString>& Override : ScoringOverrides)
	{
		UE_LOG(LogMKHAI, Display, TEXT("  scoring override %s = %s"), *Override.Key, *Override.Value);
	}
}

void AMKHDuelBotController::SetStatesEnabled(const FGameplayTagContainer& StateTags, const bool bEnabled)
{
	if (bEnabled)
	{
		// Enabling a state enables its children too (React enables React.Dodge, React.Block, React.ActivateAbility).
		for (const FGameplayTag& State : MKHGameplayTags::AIState::GetSelectableStates())
		{
			if (State.MatchesAny(StateTags))
			{
				DisabledStates.RemoveTag(State);
			}
		}
	}
	else
	{
		DisabledStates.AppendTags(StateTags);
	}

	// Combat > Idle is the fallback while some state is disabled: Combat then always has a child with a score.
	DisabledStates.RemoveTag(MKHGameplayTags::AIState::Idle);
	const bool bAllEnabled = DisabledStates.IsEmpty();
	if (bAllEnabled)
	{
		DisabledStates.AddTag(MKHGameplayTags::AIState::Idle);
	}
	UE_LOG(LogMKHAI, Display, TEXT("MKHDuelBotController: disabled states: %s"),
		bAllEnabled ? TEXT("none") : *DisabledStates.ToStringSimple());
}

void AMKHDuelBotController::SetOnlyStatesEnabled(const FGameplayTagContainer& StateTags)
{
	DisabledStates = MKHGameplayTags::AIState::GetSelectableStates();
	SetStatesEnabled(StateTags, true);
}

void AMKHDuelBotController::SetBotPaused(const bool bPause)
{
	if (bPause == bBotPaused || !IsValid(StateTreeComponent))
	{
		return;
	}

	bBotPaused = bPause;
	if (bPause)
	{
		StateTreeComponent->PauseLogic(TEXT("MKH.Bot.Pause"));
		StopMovement();
		ReleaseHeldInputs();
	}
	else
	{
		StateTreeComponent->ResumeLogic(TEXT("MKH.Bot.Resume"));
	}
	UE_LOG(LogMKHAI, Display, TEXT("MKHDuelBotController: %s %s."), *GetNameSafe(GetPawn()), bPause ? TEXT("paused") : TEXT("resumed"));
}

void AMKHDuelBotController::NotifyAttackCommitted()
{
	LastAttackCommitTime = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AMKHDuelBotController::SetProfile(UMKHDuelBotProfile* NewProfile)
{
	ActiveProfile = IsValid(NewProfile) ? NewProfile : DefaultProfile.Get();
	RefreshEffectiveParameters();
	if (!IsValid(ActiveProfile))
	{
		UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotController: no profile and no DefaultProfile for %s."), *GetNameSafe(GetPawn()));
		return;
	}

	UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotController: %s plays with profile '%s' (%s)."), *GetNameSafe(GetPawn()),
		*ActiveProfile->GetProfileId().ToString(), *GetNameSafe(ActiveProfile));
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHDuelBotController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotController: move of %s ended: %s"), *GetNameSafe(GetPawn()), *Result.ToString());
}

void AMKHDuelBotController::RefreshEffectiveParameters()
{
	const FMKHDuelBotScoringRow* ScoringRow = IsValid(ActiveProfile) ? ActiveProfile->FindScoringRow() : nullptr;
	EffectiveTuning = IsValid(ActiveProfile) ? ActiveProfile->GetTuning() : FMKHDuelBotTuning();
	EffectiveScoring = ScoringRow != nullptr ? *ScoringRow : FMKHDuelBotScoringRow();
	MKHDuelBotControllerPrivate::ApplyOverrides(FMKHDuelBotTuning::StaticStruct(), &EffectiveTuning, TuningOverrides);
	MKHDuelBotControllerPrivate::ApplyOverrides(FMKHDuelBotScoringRow::StaticStruct(), &EffectiveScoring, ScoringOverrides);

	// Edits in the DataTable editor are read on the next tick anyway; the binding only logs them.
	UDataTable* ScoringTable = IsValid(ActiveProfile) ? ActiveProfile->GetScoringTable() : nullptr;
	if (ScoringTable == BoundScoringTable.Get())
	{
		return;
	}
	if (UDataTable* OldTable = BoundScoringTable.Get())
	{
		OldTable->OnDataTableChanged().RemoveAll(this);
	}
	if (IsValid(ScoringTable))
	{
		ScoringTable->OnDataTableChanged().AddUObject(this, &AMKHDuelBotController::HandleScoringTableChanged);
	}
	BoundScoringTable = ScoringTable;
}

void AMKHDuelBotController::RestartStoppedTree()
{
	// The tree stops for good when Root selects nothing (e.g. every Combat child scores 0): never leave the bot frozen.
	const UWorld* World = GetWorld();
	if (bBotPaused || !IsValid(GetPawn()) || !IsValid(StateTreeComponent) || StateTreeComponent->IsRunning() || !IsValid(World)
		|| World->GetTimeSeconds() - LastTreeRestartTime < TreeRestartInterval)
	{
		return;
	}
	LastTreeRestartTime = World->GetTimeSeconds();
	UE_LOG(LogMKHAI, Warning, TEXT("MKHDuelBotController: StateTree of %s stopped (no state could be selected), restarting it."), *GetNameSafe(GetPawn()));
	StateTreeComponent->StartLogic();
}

void AMKHDuelBotController::HandleScoringTableChanged()
{
	UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotController: scoring table %s changed, new values from the next tick."),
		*GetNameSafe(BoundScoringTable.Get()));
}

void AMKHDuelBotController::ReleaseHeldInputs() const
{
	const ACharacterBase* Bot = Cast<ACharacterBase>(GetPawn());
	UMKHAbilitySystemComponent* AbilitySystem = IsValid(Bot) ? Cast<UMKHAbilitySystemComponent>(Bot->GetAbilitySystemComponent()) : nullptr;
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	// The held input (e.g. Block) is the spec's Input.* dynamic tag, as the task pressed it.
	TArray<FGameplayTag> HeldInputs;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		{
			if (Spec.InputPressed && Tag.MatchesTag(MKHGameplayTags::Input::Ability))
			{
				HeldInputs.AddUnique(Tag);
			}
		}
	}
	for (const FGameplayTag& InputTag : HeldInputs)
	{
		AbilitySystem->AbilityInputReleased(InputTag);
	}
}

void AMKHDuelBotController::AcquireOpponent()
{
	for (TActorIterator<ACharacterBase> It(GetWorld()); It; ++It)
	{
		ACharacterBase* Candidate = *It;
		if (IsValid(Candidate) && Candidate != GetPawn() && UMKHDuelParticipantLibrary::IsHumanParticipant(Candidate))
		{
			SetFocus(Candidate);
			Opponent = Candidate;
			UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotController: %s chases %s."), *GetNameSafe(GetPawn()), *Candidate->GetName());
			return;
		}
	}
}

void AMKHDuelBotController::PrintStateTreeChanges()
{
#if !UE_BUILD_SHIPPING
	if (!IsValid(StateTreeComponent))
	{
		return;
	}

	// The component exposes no state change delegate: compare the active path with the last one printed.
	TArray<FString> StateNames;
	for (const FName& ActiveStateName : StateTreeComponent->GetActiveStateNames())
	{
		StateNames.Add(ActiveStateName.ToString());
	}
	const FString StatePath = StateNames.IsEmpty() ? TEXT("(not running)") : FString::Join(StateNames, TEXT(" > "));
	if (StatePath == LastPrintedStatePath)
	{
		return;
	}

	LastPrintedStatePath = StatePath;
	UE_LOG(LogMKHAI, Log, TEXT("MKHDuelBotController: %s enters %s"), *GetNameSafe(GetPawn()), *StatePath);
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, StateChangePrintDuration, FColor::Cyan,
			FString::Printf(TEXT("[%s] %s"), *GetNameSafe(GetPawn()), *StatePath));
	}
#endif
}
