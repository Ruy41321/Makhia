// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Automation Spec of the duel bot's pure state scoring (FMKHStateScoring): React choice (block, dodge, counter), Bait
// from chase and from danger, PlayPassive and Attack. Run from Session Frontend (Automation, filter "Makhia.AI") or
// the automation commands. See Documentation/DuelBotAI.md (Tests).

#include "AI/MKHStateScoring.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MKHStateScoringSpecHelpers
{
	/** An even duel at close range, both at full health and stamina. */
	static FMKHStateScoringContext MakeContext()
	{
		FMKHStateScoringContext Context;
		Context.Distance = 200.f;
		Context.BotStamina = 100.f;
		Context.Aggression = 1.f;
		Context.BaitWilling = 1.f;
		return Context;
	}

	/** An attack in reach with the given crowd control and damage / status scores. */
	static FMKHAttackCandidate MakeCandidate(const EMKHCrowdControl CrowdControl, const float DamageScore = 1.f, const float Reach = 300.f)
	{
		FMKHAttackCandidate Candidate;
		Candidate.Reach = Reach;
		Candidate.CrowdControl = CrowdControl;
		Candidate.Breakdown.Damage = DamageScore;
		Candidate.Breakdown.StatusEffect = 0.5f;
		return Candidate;
	}
}

BEGIN_DEFINE_SPEC(FMKHStateScoringSpec, "Makhia.AI.StateScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FMKHStateScoringParams Params;
END_DEFINE_SPEC(FMKHStateScoringSpec)

void FMKHStateScoringSpec::Define()
{
	using namespace MKHStateScoringSpecHelpers;

	Describe("Utility curve", [this]()
	{
		It("normalizes over the range, clamps and follows the keys", [this]()
		{
			const FMKHUtilityCurve Curve(0.f, 2.f, { { 0.f, 0.6f }, { 1.f, 1.f } });
			TestEqual("Below the range", Curve.Evaluate(-1.f), 0.6f, 1.e-4f);
			TestEqual("Middle", Curve.Evaluate(1.f), 0.8f, 1.e-4f);
			TestEqual("Above the range", Curve.Evaluate(5.f), 1.f, 1.e-4f);
			TestEqual("Empty curve is linear", FMKHUtilityCurve().Evaluate(0.25f), 0.25f, 1.e-4f);
		});
	});

	Describe("React", [this]()
	{
		It("blocks a light hit in an even duel", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.bKnowsIncomingAttack = true;
			Context.IncomingStaminaDamage = 10.f;
			const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(Context, {}, Params);
			TestTrue("Block beats dodge", Scores.Block > Scores.Dodge);
		});

		It("never blocks a hit that breaks the guard (veto)", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.bKnowsIncomingAttack = true;
			Context.IncomingStaminaDamage = 60.f;
			Context.BotStamina = 40.f;
			TestEqual("Block vetoed", FMKHStateScoring::ScoreBlock(Context, Params.React), 0.f);
			Context.BotBlockStability = 0.5f;
			TestTrue("Block stability makes it safe", FMKHStateScoring::ScoreBlock(Context, Params.React) > 0.f);
		});

		It("uses the average stamina damage without a recognised attack", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.IncomingStaminaDamage = 90.f;
			TestEqual("Average, not the hidden value", FMKHStateScoring::ComputeBlockStaminaDamage(Context, Params.React),
				Params.React.AverageIncomingStaminaDamage, 1.e-4f);
		});

		It("dodges more when behind on health, never without stamina", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.BotHealthRatio = 0.4f;
			const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(Context, {}, Params);
			TestTrue("Dodge beats block when behind", Scores.Dodge > Scores.Block);
			TestTrue("Higher than when even", Scores.Dodge > FMKHStateScoring::ScoreDodge(MakeContext(), Params.React));
			Context.bCanAffordDodge = false;
			TestEqual("Dodge vetoed", FMKHStateScoring::ScoreDodge(Context, Params.React), 0.f);
		});

		It("counters with a stun in reach, preferring it to a stagger", [this]()
		{
			const TArray<FMKHAttackCandidate> Candidates = { MakeCandidate(EMKHCrowdControl::Stagger), MakeCandidate(EMKHCrowdControl::Stun) };
			const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(MakeContext(), Candidates, Params);
			TestEqual("The stun is the counter", Scores.CounterIndex, 1);
			TestTrue("Counter beats block and dodge", Scores.Counter > Scores.Block && Scores.Counter > Scores.Dodge);
		});

		It("counters only in reach and with plain attacks only when it can trade", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			int32 Index = INDEX_NONE;
			TestEqual("Out of reach", FMKHStateScoring::ScoreCounter(Context, { MakeCandidate(EMKHCrowdControl::Stun, 1.f, 100.f) }, Params.React, Index), 0.f);
			TestEqual("No counter index", Index, INDEX_NONE);

			const TArray<FMKHAttackCandidate> Plain = { MakeCandidate(EMKHCrowdControl::None) };
			TestTrue("Healthy bot trades", FMKHStateScoring::ScoreCounter(Context, Plain, Params.React, Index) > 0.f);
			Context.BotHealthRatio = 0.3f;
			TestEqual("Low bot does not trade a plain hit", FMKHStateScoring::ScoreCounter(Context, Plain, Params.React, Index), 0.f);
		});

		It("answers a bait with an attack when the roll asks for it, a plain one included", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.BotHealthRatio = 0.3f;
			Context.bBaitWantsCounter = true;
			const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(Context, { MakeCandidate(EMKHCrowdControl::None) }, Params);
			TestEqual("The attack is the counter", Scores.CounterIndex, 0);
			TestTrue("Counter wins", Scores.Counter > 0.f && Scores.Block == 0.f && Scores.Dodge == 0.f);

			const FMKHStateScores OutOfReach = FMKHStateScoring::ScoreStates(Context, { MakeCandidate(EMKHCrowdControl::None, 1.f, 100.f) }, Params);
			TestTrue("Without an attack it still defends", OutOfReach.Block > 0.f && OutOfReach.Dodge > 0.f);
		});

		It("never counters a stun or stagger strike with an attack that does not land first", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.IncomingCrowdControl = EMKHCrowdControl::Stagger;
			Context.IncomingSecondsToHit = 0.4f;
			FMKHAttackCandidate Slow = MakeCandidate(EMKHCrowdControl::Stun);
			Slow.TimeToHit = 0.4f;
			FMKHAttackCandidate Fast = MakeCandidate(EMKHCrowdControl::Stagger);
			Fast.TimeToHit = 0.3f;

			int32 Index = INDEX_NONE;
			FMKHStateScoring::ScoreCounter(Context, { Slow, Fast }, Params.React, Index);
			TestEqual("The slower stun loses to the faster stagger", Index, 1);
			TestEqual("Same time to hit is too slow", FMKHStateScoring::ScoreCounter(Context, { Slow }, Params.React, Index), 0.f);
			TestEqual("No counter left", Index, INDEX_NONE);

			Context.IncomingCrowdControl = EMKHCrowdControl::None;
			TestTrue("Without crowd control a slower attack is allowed", FMKHStateScoring::ScoreCounter(Context, { Slow }, Params.React, Index) > 0.f);
		});
	});

	Describe("Bait", [this]()
	{
		It("rises with a chase the opponent keeps running from", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.UnansweredAttackSeconds = 3.f;
			Context.RetreatShare = 1.f;
			float Chase = 0.f;
			float Danger = 0.f;
			TestEqual("Full chase", FMKHStateScoring::ScoreBait(Context, Params.Bait, Chase, Danger), 1.f, 1.e-4f);
			Context.RetreatShare = 0.f;
			TestEqual("Standing opponent: no chase", FMKHStateScoring::ScoreBait(Context, Params.Bait, Chase, Danger), 0.f, 1.e-4f);
		});

		It("rises with the opponent's ready danger, scaled by the will to bait", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.OpponentReadyDanger = 1.f;
			float Chase = 0.f;
			float Danger = 0.f;
			const float Willing = FMKHStateScoring::ScoreBait(Context, Params.Bait, Chase, Danger);
			TestTrue("A stun ready invites a bait", Willing > 0.5f);
			Context.BaitWilling = 0.5f;
			TestEqual("Half as willing", FMKHStateScoring::ScoreBait(Context, Params.Bait, Chase, Danger), Willing * 0.5f, 1.e-4f);
			Context.OpponentReadyDanger = 0.3f;
			TestEqual("Nothing dangerous ready", FMKHStateScoring::ScoreBait(Context, Params.Bait, Chase, Danger), 0.f, 1.e-4f);
		});
	});

	Describe("PlayPassive and Attack", [this]()
	{
		It("plays passive when low on health or stamina and behind", [this]()
		{
			const float Fresh = FMKHStateScoring::ScorePlayPassive(MakeContext(), Params.PlayPassive);
			FMKHStateScoringContext Context = MakeContext();
			Context.BotHealthRatio = 0.2f;
			Context.DamageAdvantage = 0.6f;
			TestTrue("Low health and losing", FMKHStateScoring::ScorePlayPassive(Context, Params.PlayPassive) > Fresh);
			Context = MakeContext();
			Context.BotStaminaRatio = 0.1f;
			TestTrue("Low stamina", FMKHStateScoring::ScorePlayPassive(Context, Params.PlayPassive) > Fresh);
		});

		It("prefers PlayPassive to Attack and Bait at 20% health against a full opponent", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.BotHealthRatio = 0.2f;
			Context.AttackUtility = 0.4f;
			Context.Aggression = 0.7f;
			Context.OpponentReadyDanger = 1.f;
			const FMKHStateScores Scores = FMKHStateScoring::ScoreStates(Context, {}, Params);
			TestTrue("Passive beats Attack", Scores.PlayPassive > Scores.Attack);
			TestTrue("Passive beats Bait", Scores.PlayPassive > Scores.Bait);

			Context.BotHealthRatio = 1.f;
			const FMKHStateScores Healthy = FMKHStateScoring::ScoreStates(Context, {}, Params);
			TestTrue("A healthy bot attacks", Healthy.Attack > Healthy.PlayPassive);
		});

		It("attacks more with a better choice, a window and an advantage, never down to 0", [this]()
		{
			FMKHStateScoringContext Context = MakeContext();
			Context.Aggression = 0.f;
			Context.DamageAdvantage = 0.5f;
			const float Floor = FMKHStateScoring::ScoreAttack(Context, Params.Attack);
			TestTrue("Floors keep it above 0", Floor > 0.f);
			Context.AttackUtility = 0.3f;
			Context.PunishWindowScore = 1.f;
			Context.DamageAdvantage = 2.f;
			Context.Aggression = 1.f;
			TestEqual("Best case", FMKHStateScoring::ScoreAttack(Context, Params.Attack), 1.f, 1.e-4f);
		});
	});
}

#endif
