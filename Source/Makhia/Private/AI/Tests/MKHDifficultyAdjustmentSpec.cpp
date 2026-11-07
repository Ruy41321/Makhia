// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Automation Spec of the Random Duel's difficulty adjustment (FMKHDifficultyAdjustment). Run from Session Frontend
// (Automation, filter "Makhia.AI"). See Documentation/OfflineModes.md.

#include "AI/MKHDifficultyAdjustment.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FMKHDifficultyAdjustmentSpec, "Makhia.AI.DifficultyAdjustment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FMKHDifficultyAdjustmentSettings Settings;
END_DEFINE_SPEC(FMKHDifficultyAdjustmentSpec)

void FMKHDifficultyAdjustmentSpec::Define()
{
	Describe("ComputeRoundScore", [this]()
	{
		It("is positive when the player dominates and negative when overwhelmed", [this]()
		{
			TestEqual("Flawless win", FMKHDifficultyAdjustment::ComputeRoundScore(120.f, 0.f), 1.f);
			TestEqual("Flawless loss", FMKHDifficultyAdjustment::ComputeRoundScore(0.f, 120.f), -1.f);
		});

		It("compares the absolute damage, whatever the potions drunk made it reach", [this]()
		{
			TestEqual("Three times the damage", FMKHDifficultyAdjustment::ComputeRoundScore(300.f, 100.f), 0.5f);
			TestEqual("Even", FMKHDifficultyAdjustment::ComputeRoundScore(250.f, 250.f), 0.f);
		});

		It("is even when nobody was hit", [this]()
		{
			TestEqual("No hits", FMKHDifficultyAdjustment::ComputeRoundScore(0.f, 0.f), 0.f);
		});
	});

	Describe("ComputeNextLevel", [this]()
	{
		It("stays on the same level inside the thresholds", [this]()
		{
			TestEqual("Close win", FMKHDifficultyAdjustment::ComputeNextLevel(1, 0.35f, Settings), 1);
			TestEqual("Close loss", FMKHDifficultyAdjustment::ComputeNextLevel(1, -0.35f, Settings), 1);
		});

		It("moves one level at most", [this]()
		{
			TestEqual("Dominated", FMKHDifficultyAdjustment::ComputeNextLevel(1, 1.f, Settings), 2);
			TestEqual("Overwhelmed", FMKHDifficultyAdjustment::ComputeNextLevel(1, -1.f, Settings), 0);
		});

		It("never lowers from the hardest level", [this]()
		{
			TestEqual("Thrown round", FMKHDifficultyAdjustment::ComputeNextLevel(2, -1.f, Settings), 2);
		});

		It("never leaves the profile order", [this]()
		{
			TestEqual("Above the hardest", FMKHDifficultyAdjustment::ComputeNextLevel(2, 1.f, Settings), 2);
			TestEqual("Below the easiest", FMKHDifficultyAdjustment::ComputeNextLevel(0, -1.f, Settings), 0);
		});
	});
}

#endif
