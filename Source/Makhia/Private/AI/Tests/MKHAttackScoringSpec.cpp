// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Automation Spec of the duel bot's pure attack scoring (FMKHAttackScoring): fixed duel situations and the
// distribution of the choices per Expertise. Run from Session Frontend (Automation, filter "Makhia.AI") or the
// automation commands. See Documentation/DuelBotAI.md (Tests).

#include "AI/MKHAttackScoring.h"
#include "AI/Data/MKHDuelBotProfile.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MKHAttackScoringSpecHelpers
{
	/** A strike with the given damage, stamina damage and hit time. */
	static FMKHAttackStrikeInput MakeStrike(const float DamagePercent, const float StaminaDamage, const float TimeToHit)
	{
		FMKHAttackStrikeInput Strike;
		Strike.DamagePercent = DamagePercent;
		Strike.StaminaDamage = StaminaDamage;
		Strike.TimeToHit = TimeToHit;
		return Strike;
	}

	/** Light: the one quick strike a press performs (the bot decides one strike at a time). */
	static FMKHAttackCandidateInput MakeLight()
	{
		FMKHAttackCandidateInput Candidate;
		Candidate.AbilityTag = FGameplayTag();
		Candidate.Reach = 300.f;
		Candidate.WeaponBaseDamage = 10.f;
		Candidate.Strikes = { MakeStrike(0.6f, 10.f, 0.3f) };
		return Candidate;
	}

	/** Heavy: one slow strong strike. */
	static FMKHAttackCandidateInput MakeHeavy()
	{
		FMKHAttackCandidateInput Candidate;
		Candidate.Reach = 300.f;
		Candidate.WeaponBaseDamage = 10.f;
		Candidate.Strikes = { MakeStrike(2.2f, 40.f, 1.2f) };
		return Candidate;
	}

	/** An even duel at close range. */
	static FMKHAttackScoringContext MakeContext()
	{
		FMKHAttackScoringContext Context;
		Context.Distance = 200.f;
		Context.OpponentHealth = 100.f;
		Context.OpponentStamina = 100.f;
		return Context;
	}
}

BEGIN_DEFINE_SPEC(FMKHAttackScoringSpec, "Makhia.AI.AttackScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FMKHAttackScoringParams Params;
END_DEFINE_SPEC(FMKHAttackScoringSpec)

void FMKHAttackScoringSpec::Define()
{
	using namespace MKHAttackScoringSpecHelpers;

	Describe("DamageAdvantage", [this]()
	{
		It("is 1 in an even duel and above 1 when the bot hits harder", [this]()
		{
			TestEqual("Even", FMKHAttackScoring::ComputeDamageAdvantage(1.f, 1.f, 1.f, 1.f), 1.f);
			TestTrue("Empowered bot", FMKHAttackScoring::ComputeDamageAdvantage(1.2f, 1.f, 1.f, 1.f) > 1.f);
			TestTrue("Exposed bot", FMKHAttackScoring::ComputeDamageAdvantage(1.f, 1.3f, 1.f, 1.f) < 1.f);
		});
	});

	Describe("Crowd control", [this]()
	{
		It("prefers the slow heavy that fits a long stun", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.OpponentCrowdControl = EMKHCrowdControl::Stun;
			Context.OpponentCrowdControlTimeLeft = 2.f;
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ MakeLight(), MakeHeavy() }, Context, Params);
			TestTrue("Heavy beats light", Scores[1].Total > Scores[0].Total);
		});

		It("prefers the fastest strike in a short stagger", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.OpponentCrowdControl = EMKHCrowdControl::Stagger;
			Context.OpponentCrowdControlTimeLeft = 0.6f;
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ MakeLight(), MakeHeavy() }, Context, Params);
			TestEqual("Heavy misses the window", Scores[1].CrowdControlFit, Params.CrowdControlMissScore);
			TestTrue("Light beats heavy", Scores[0].Total > Scores[1].Total);
		});
	});

	Describe("Single strike", [this]()
	{
		It("prices a light by the one strike a press performs", [this]()
		{
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ MakeLight(), MakeHeavy() }, MakeContext(), Params);
			TestEqual("Damage of one strike", Scores[0].ExpectedDamage, 6.f);
			TestEqual("Heavy is the reference", Scores[1].Damage, 1.f);
			TestTrue("Light scores less damage", Scores[0].Damage < Scores[1].Damage);
		});
	});

	Describe("Speed", [this]()
	{
		It("favours the faster attack in a neutral situation", [this]()
		{
			TestEqual("Fast", FMKHAttackScoring::ScoreSpeed(MakeLight(), MakeContext(), Params), 1.f);
			FMKHAttackCandidateInput Slow = MakeHeavy();
			Slow.Strikes[0].TimeToHit = Params.SlowHitTime;
			TestEqual("Slow", FMKHAttackScoring::ScoreSpeed(Slow, MakeContext(), Params), Params.MinSpeedScore);

			FMKHAttackCandidateInput SlowTwin = MakeLight();
			SlowTwin.Strikes[0].TimeToHit = Params.SlowHitTime;
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ MakeLight(), SlowTwin }, MakeContext(), Params);
			TestTrue("Same attack, faster wins", Scores[0].Total > Scores[1].Total);
		});

		It("leaves crowd control to its own consideration", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.OpponentCrowdControl = EMKHCrowdControl::Stun;
			Context.OpponentCrowdControlTimeLeft = 2.f;
			TestEqual("No speed score on a stun", FMKHAttackScoring::ScoreSpeed(MakeHeavy(), Context, Params), 1.f);
		});
	});

	Describe("Guard break", [this]()
	{
		It("rates highest the attack that empties a blocking opponent's stamina", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.bOpponentBlocking = true;
			Context.OpponentStamina = 35.f;
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ MakeLight(), MakeHeavy() }, Context, Params);
			TestTrue("Heavy breaks the guard", Scores[1].bBreaksGuard);
			TestFalse("Light does not", Scores[0].bBreaksGuard);
			TestEqual("Guard score of the break", Scores[1].GuardBreak, 1.f);
			TestTrue("Heavy beats light", Scores[1].Total > Scores[0].Total);
		});

		It("counts the block stability of the opponent's weapon", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.bOpponentBlocking = true;
			Context.OpponentStamina = 35.f;
			Context.OpponentBlockStability = 0.5f;
			TestFalse("Half of 40 stamina damage does not break 35", FMKHAttackScoring::BreaksGuard(MakeHeavy(), Context));
		});
	});

	Describe("Lethal hit", [this]()
	{
		It("spends a skill cooldown freely when the hit kills", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.OpponentHealth = 10.f;
			FMKHAttackCandidateInput Skill = MakeHeavy();
			Skill.CooldownTime = 20.f;
			const TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ Skill }, Context, Params);
			TestTrue("Lethal", Scores[0].bLethal);
			TestEqual("No cooldown cost", Scores[0].CooldownEconomy, 1.f);
			TestEqual("Damage score", Scores[0].Damage, 1.f);
		});

		It("charges the cooldown outside any punish window", [this]()
		{
			FMKHAttackCandidateInput Skill = MakeHeavy();
			Skill.CooldownTime = 20.f;
			const float Closed = FMKHAttackScoring::ScoreCooldownEconomy(Skill, MakeContext(), false, Params);
			FMKHAttackScoringContext Window = MakeContext();
			Window.PunishWindowScore = 1.f;
			TestTrue("Costs outside a window", Closed < 1.f);
			TestEqual("Free in a full window", FMKHAttackScoring::ScoreCooldownEconomy(Skill, Window, false, Params), 1.f);
		});
	});

	Describe("Status effects", [this]()
	{
		It("values a status effect by the advantage it gains, within the caps", [this]()
		{
			FMKHAttackScoringContext Context = MakeContext();
			Context.ModifierRoom.BotOutput = 0.5f;
			FMKHAttackCandidateInput Empowering = MakeLight();
			Empowering.Strikes[0].ModifierShift.BotOutput = 0.2f;
			const float Plain = FMKHAttackScoring::ScoreStatusEffects(MakeLight(), Context, Params);
			const float WithEmpower = FMKHAttackScoring::ScoreStatusEffects(Empowering, Context, Params);
			TestEqual("Plain attack", Plain, Params.BaseStatusScore);
			TestTrue("Empower adds", WithEmpower > Plain);

			Context.ModifierRoom.BotOutput = 0.f;
			TestEqual("Capped empower adds nothing", FMKHAttackScoring::ScoreStatusEffects(Empowering, Context, Params), Params.BaseStatusScore);
		});
	});

	Describe("Light loyalty", [this]()
	{
		It("keeps the light on the weapon in hand, repeats it and swaps only for a clearly better one", [this]()
		{
			FMKHAttackCandidateInput HeldLight = MakeLight();
			HeldLight.bIsLight = true;
			FMKHAttackCandidateInput SheathedLight = MakeLight();
			SheathedLight.bIsLight = true;
			SheathedLight.bNeedsSwap = true;

			// Two equal lights: the one in hand wins.
			TArray<FMKHAttackScoreBreakdown> Scores = FMKHAttackScoring::ScoreCandidates({ HeldLight, SheathedLight }, MakeContext(), Params);
			TestTrue("Held light beats the sheathed one", Scores[0].Total > Scores[1].Total);

			// Used last: no repeat penalty for a light, it stays on the same weapon.
			HeldLight.bIsLastUsed = true;
			Scores = FMKHAttackScoring::ScoreCandidates({ HeldLight, SheathedLight }, MakeContext(), Params);
			TestEqual("No repeat penalty", Scores[0].Variety, 1.f);
			TestEqual("Sheathed light pays the swap", Scores[1].Variety, Params.LightSwapScore);
			TestTrue("Still on the held weapon", Scores[0].Total > Scores[1].Total);

			// A much stronger sheathed light (damage 1 vs 0.3) wins despite the swap.
			SheathedLight.WeaponBaseDamage = 40.f;
			Scores = FMKHAttackScoring::ScoreCandidates({ HeldLight, SheathedLight }, MakeContext(), Params);
			TestTrue("Clearly better sheathed light wins", Scores[1].Total > Scores[0].Total);
		});

		It("keeps the repeat penalty for the other attacks", [this]()
		{
			FMKHAttackCandidateInput Heavy = MakeHeavy();
			Heavy.bIsLastUsed = true;
			TestEqual("Heavy used last", FMKHAttackScoring::ScoreVariety(Heavy, Params), Params.RepeatScore);
		});
	});

	Describe("Danger", [this]()
	{
		It("rates stun 1, stagger 0.7 and a strong hit by the health it takes", [this]()
		{
			const FMKHDangerParams Danger;
			FMKHAttackCandidateInput Stun = MakeLight();
			Stun.Strikes[0].CrowdControl = EMKHCrowdControl::Stun;
			FMKHAttackCandidateInput Stagger = MakeLight();
			Stagger.Strikes[0].CrowdControl = EMKHCrowdControl::Stagger;
			TestEqual("Stun", FMKHAttackScoring::ComputeAbilityDanger(Stun, 1.f, 1.f, 0.f, 100.f, Danger), 1.f);
			TestEqual("Stagger", FMKHAttackScoring::ComputeAbilityDanger(Stagger, 1.f, 1.f, 0.f, 100.f, Danger), 0.7f);

			const float HealthyBot = FMKHAttackScoring::ComputeAbilityDanger(MakeHeavy(), 1.f, 1.f, 0.f, 100.f, Danger);
			const float DyingBot = FMKHAttackScoring::ComputeAbilityDanger(MakeHeavy(), 1.f, 1.f, 0.f, 20.f, Danger);
			TestTrue("Heavy above the threshold is dangerous", HealthyBot >= Danger.HighDamageDanger);
			TestEqual("Lethal heavy is maximal", DyingBot, 1.f);
		});
	});

	Describe("Choice distribution", [this]()
	{
		const TArray<float> Scores = { 0.9f, 0.6f, 0.3f };

		It("is uniform at Expertise 0", [this, Scores]()
		{
			const TArray<float> Probabilities = FMKHAttackScoring::ComputeChoiceProbabilities(Scores, FMKHDuelBotTuning::ComputeDecisionSharpness(0.f), false);
			for (const float Probability : Probabilities)
			{
				TestEqual("Uniform", Probability, 1.f / 3.f, 1.e-4f);
			}
		});

		It("draws following s^k at Expertise 0.5 (fixed seed)", [this, Scores]()
		{
			const TArray<float> Probabilities = FMKHAttackScoring::ComputeChoiceProbabilities(Scores, FMKHDuelBotTuning::ComputeDecisionSharpness(0.5f), false);
			TestTrue("Best is likeliest", Probabilities[0] > Probabilities[1] && Probabilities[1] > Probabilities[2]);
			TestTrue("Mistakes stay possible", Probabilities[2] > 0.f);

			FRandomStream RandomStream(1234);
			TArray<int32> Counts = { 0, 0, 0 };
			constexpr int32 Draws = 20000;
			for (int32 Draw = 0; Draw < Draws; ++Draw)
			{
				++Counts[FMKHAttackScoring::PickIndex(Probabilities, RandomStream)];
			}
			for (int32 Index = 0; Index < Scores.Num(); ++Index)
			{
				TestEqual(FString::Printf(TEXT("Frequency %d"), Index), static_cast<float>(Counts[Index]) / Draws, Probabilities[Index], 0.02f);
			}
		});

		It("always takes the best at Expertise 1", [this, Scores]()
		{
			FMKHDuelBotTuning Tuning;
			Tuning.Expertise = 1.f;
			const TArray<float> Probabilities = FMKHAttackScoring::ComputeChoiceProbabilities(Scores, Tuning.GetDecisionSharpness(), Tuning.IsExactChoice());
			TestEqual("Best", Probabilities[0], 1.f);
			FRandomStream RandomStream(42);
			for (int32 Draw = 0; Draw < 100; ++Draw)
			{
				TestEqual("Always the best", FMKHAttackScoring::PickIndex(Probabilities, RandomStream), 0);
			}
		});

		It("gives the attack utility as the expected score", [this, Scores]()
		{
			const TArray<float> Uniform = FMKHAttackScoring::ComputeChoiceProbabilities(Scores, 0.f, false);
			TestEqual("Mean of the scores", FMKHAttackScoring::ComputeExpectedUtility(Scores, Uniform), 0.6f, 1.e-4f);
			TestEqual("0^0 counts as 1", FMKHAttackScoring::ApplySharpness(0.f, 0.f), 1.f);
		});
	});
}

#endif
