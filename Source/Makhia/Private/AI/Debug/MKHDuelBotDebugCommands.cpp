// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Console commands to test the duel bot's decisions in a match against it (e.g. open Makhion?Mode=Bot): pause it,
// restrict its states, put a duelist in a state, override a tuning value, then read the bot's tables. Development
// builds only. See Documentation/DuelBotAI.md (Debugging).

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MKHGameplayTags.h"
#include "AbilitySystem/Attributes/MKHAttributeSet.h"
#include "AI/Controller/MKHDuelBotController.h"
#include "Character/CharacterBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Libraries/MKHAbilitySystemLibrary.h"
#include "MKHLogChannels.h"

#if !UE_BUILD_SHIPPING
namespace MKHDuelBotDebugCommands
{
	/**
	 * Finds the duel bot's controller (server world: offline match or listen server).
	 *
	 * @param World  World the command runs in.
	 * @return The first duel bot controller, or null.
	 */
	static AMKHDuelBotController* FindBotController(UWorld* World)
	{
		TActorIterator<AMKHDuelBotController> It(World);
		return It ? *It : nullptr;
	}

	/**
	 * Resolves the bot or its opponent from a command argument.
	 *
	 * @param BotController  The bot's controller.
	 * @param Args           Command arguments; "bot" at ArgIndex picks the bot, anything else the player.
	 * @param ArgIndex       Index of the target argument.
	 * @return The target character, or null.
	 */
	static ACharacterBase* FindTarget(const AMKHDuelBotController& BotController, const TArray<FString>& Args, const int32 ArgIndex)
	{
		const bool bTargetBot = Args.IsValidIndex(ArgIndex) && Args[ArgIndex].Equals(TEXT("bot"), ESearchCase::IgnoreCase);
		return bTargetBot ? Cast<ACharacterBase>(BotController.GetPawn()) : BotController.GetOpponent();
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpAttackOptionsCommand(TEXT("MKH.Bot.DumpAttackOptions"),
		TEXT("Prints the duel bot's scored attack candidates (considerations, score, choice probability) on its next tick, without it acting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->RequestAttackOptionsDump();
				BotController->RequestStateScoresDump();
			}
		}));

	/**
	 * Resolves state names (e.g. "Attack", "React.Dodge" or the full "AI.State.Attack") into AI.State.* tags.
	 *
	 * @param Args       Command arguments, one name each.
	 * @param OutStates  Tags found.
	 * @return False when a name matches no selectable state (logged with the valid names).
	 */
	static bool ParseStateNames(const TArray<FString>& Args, FGameplayTagContainer& OutStates)
	{
		const FGameplayTagContainer& Selectable = MKHGameplayTags::AIState::GetSelectableStates();
		const FString Prefix = FString(MKHGameplayTags::AIState::Root.GetTag().ToString()) + TEXT(".");
		for (const FString& Arg : Args)
		{
			const FString TagName = Arg.StartsWith(Prefix, ESearchCase::IgnoreCase) ? Arg : Prefix + Arg;
			const FGameplayTag* Found = Selectable.GetGameplayTagArray().FindByPredicate([&TagName](const FGameplayTag& Tag)
			{
				return Tag.ToString().Equals(TagName, ESearchCase::IgnoreCase);
			});
			if (Found == nullptr)
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Bot.State: unknown state '%s'. Valid: %s"), *Arg, *Selectable.ToStringSimple());
				return false;
			}
			OutStates.AddTag(*Found);
		}
		return !OutStates.IsEmpty();
	}

	/** Registers MKH.Bot.State.<Name> for a state change (Disable, Enable, Only). */
	static void RunStateCommand(const TArray<FString>& Args, UWorld* World, const TCHAR* Command)
	{
		AMKHDuelBotController* BotController = FindBotController(World);
		FGameplayTagContainer States;
		if (BotController == nullptr || !ParseStateNames(Args, States))
		{
			UE_LOG(LogMKHAI, Warning, TEXT("MKH.Bot.State.%s <Name> [Name...]: no duel bot match or no valid state."), Command);
			return;
		}
		if (FCString::Stricmp(Command, TEXT("Only")) == 0)
		{
			BotController->SetOnlyStatesEnabled(States);
		}
		else
		{
			BotController->SetStatesEnabled(States, FCString::Stricmp(Command, TEXT("Enable")) == 0);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs PauseCommand(TEXT("MKH.Bot.Pause"),
		TEXT("Pauses the duel bot: StateTree paused, movement stopped, held inputs released (MKH.Bot.Resume to continue)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->SetBotPaused(true);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ResumeCommand(TEXT("MKH.Bot.Resume"),
		TEXT("Resumes the duel bot paused with MKH.Bot.Pause."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->SetBotPaused(false);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs StateDisableCommand(TEXT("MKH.Bot.State.Disable"),
		TEXT("MKH.Bot.State.Disable <Name> [Name...]: the bot never selects these states (e.g. Bait, React.Dodge)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) { RunStateCommand(Args, World, TEXT("Disable")); }));

	static FAutoConsoleCommandWithWorldAndArgs StateEnableCommand(TEXT("MKH.Bot.State.Enable"),
		TEXT("MKH.Bot.State.Enable <Name> [Name...]: enables states disabled before."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) { RunStateCommand(Args, World, TEXT("Enable")); }));

	static FAutoConsoleCommandWithWorldAndArgs StateOnlyCommand(TEXT("MKH.Bot.State.Only"),
		TEXT("MKH.Bot.State.Only <Name> [Name...]: disables every other selectable state (e.g. Only Attack, Only Bait React)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) { RunStateCommand(Args, World, TEXT("Only")); }));

	static FAutoConsoleCommandWithWorldAndArgs StateResetCommand(TEXT("MKH.Bot.State.Reset"),
		TEXT("Enables every state again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->SetStatesEnabled(MKHGameplayTags::AIState::GetSelectableStates(), true);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs StateListCommand(TEXT("MKH.Bot.State.List"),
		TEXT("Logs every selectable state and whether it is enabled."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			const AMKHDuelBotController* BotController = FindBotController(World);
			for (const FGameplayTag& State : MKHGameplayTags::AIState::GetSelectableStates())
			{
				UE_LOG(LogMKHAI, Display, TEXT("MKH.Bot.State: %s %s"), *State.ToString(),
					BotController == nullptr || BotController->IsStateEnabled(State) ? TEXT("enabled") : TEXT("DISABLED"));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs TuningSetCommand(TEXT("MKH.Bot.Tuning.Set"),
		TEXT("MKH.Bot.Tuning.Set <Field> <Value>: session override of a profile tuning or scoring row field (e.g. Aggression 0.9, States.Bait.ChaseMemory 5)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			AMKHDuelBotController* BotController = FindBotController(World);
			FString Error = TEXT("no duel bot in this world");
			if (BotController == nullptr || Args.Num() < 2
				|| !BotController->SetParameterOverride(Args[0], FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")), Error))
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Bot.Tuning.Set <Field> <Value>: %s."), Args.Num() < 2 ? TEXT("missing arguments") : *Error);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs TuningDumpCommand(TEXT("MKH.Bot.Tuning.Dump"),
		TEXT("Logs the bot's effective tuning and scoring row and the session overrides."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (const AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->DumpParameters();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs TuningResetCommand(TEXT("MKH.Bot.Tuning.Reset"),
		TEXT("Drops every MKH.Bot.Tuning.Set override."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			if (AMKHDuelBotController* BotController = FindBotController(World))
			{
				BotController->ClearParameterOverrides();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ApplyStatusCommand(TEXT("MKH.Debug.ApplyStatus"),
		TEXT("MKH.Debug.ApplyStatus <PerkTag> [player|bot]: applies a status effect as a hit would (e.g. StatEffect.Perk.Stun.ThunderShock); default target the player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const AMKHDuelBotController* BotController = FindBotController(World);
			if (BotController == nullptr)
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Debug.ApplyStatus: no duel bot in this world."));
				return;
			}

			// The other duelist is the source, as if its hit applied the effect.
			const FGameplayTag PerkTag = Args.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), false) : FGameplayTag();
			ACharacterBase* Target = FindTarget(*BotController, Args, 1);
			ACharacterBase* Source = Target == BotController->GetOpponent() ? Cast<ACharacterBase>(BotController->GetPawn()) : BotController->GetOpponent();
			UAbilitySystemComponent* TargetAbilitySystem = IsValid(Target) ? Target->GetAbilitySystemComponent() : nullptr;
			UAbilitySystemComponent* SourceAbilitySystem = IsValid(Source) ? Source->GetAbilitySystemComponent() : nullptr;
			if (!PerkTag.IsValid() || !IsValid(TargetAbilitySystem) || !IsValid(SourceAbilitySystem))
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Debug.ApplyStatus: unknown perk tag or no duel bot match."));
				return;
			}
			UMKHAbilitySystemLibrary::ApplyStatusEffects(SourceAbilitySystem, TargetAbilitySystem, PerkTag.GetSingleTagContainer(),
				SourceAbilitySystem->MakeEffectContext());
			UE_LOG(LogMKHAI, Display, TEXT("MKH.Debug.ApplyStatus: %s applied to %s."), *PerkTag.ToString(), *GetNameSafe(Target));
		}));

	static FAutoConsoleCommandWithWorldAndArgs SetStaminaCommand(TEXT("MKH.Debug.SetStamina"),
		TEXT("MKH.Debug.SetStamina <Value> [player|bot]: sets the current stamina (it regenerates as usual); default target the player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const AMKHDuelBotController* BotController = FindBotController(World);
			ACharacterBase* Target = BotController != nullptr ? FindTarget(*BotController, Args, 1) : nullptr;
			UAbilitySystemComponent* AbilitySystem = IsValid(Target) ? Target->GetAbilitySystemComponent() : nullptr;
			if (Args.IsEmpty() || !IsValid(AbilitySystem))
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Debug.SetStamina: missing value or no duel bot match."));
				return;
			}
			AbilitySystem->SetNumericAttributeBase(UMKHAttributeSet::GetStaminaAttribute(), FCString::Atof(*Args[0]));
			UE_LOG(LogMKHAI, Display, TEXT("MKH.Debug.SetStamina: %s stamina %s."), *GetNameSafe(Target), *Args[0]);
		}));

	static FAutoConsoleCommandWithWorldAndArgs SetHealthCommand(TEXT("MKH.Debug.SetHealth"),
		TEXT("MKH.Debug.SetHealth <Value> [player|bot]: sets the current health (above 0: it never kills); default target the player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const AMKHDuelBotController* BotController = FindBotController(World);
			ACharacterBase* Target = BotController != nullptr ? FindTarget(*BotController, Args, 1) : nullptr;
			UAbilitySystemComponent* AbilitySystem = IsValid(Target) ? Target->GetAbilitySystemComponent() : nullptr;
			if (Args.IsEmpty() || !IsValid(AbilitySystem))
			{
				UE_LOG(LogMKHAI, Warning, TEXT("MKH.Debug.SetHealth: missing value or no duel bot match."));
				return;
			}
			// A test value never runs the death flow: that only starts from a damage execution.
			const float MaxHealth = AbilitySystem->GetNumericAttribute(UMKHAttributeSet::GetMaxHealthAttribute());
			const float Health = FMath::Clamp(FCString::Atof(*Args[0]), 1.f, FMath::Max(MaxHealth, 1.f));
			AbilitySystem->SetNumericAttributeBase(UMKHAttributeSet::GetHealthAttribute(), Health);
			UE_LOG(LogMKHAI, Display, TEXT("MKH.Debug.SetHealth: %s health %.0f / %.0f."), *GetNameSafe(Target), Health, MaxHealth);
		}));
}
#endif
