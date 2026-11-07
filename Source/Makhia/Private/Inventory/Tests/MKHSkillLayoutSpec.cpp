// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

// Automation Spec of the skill slots (FMKHSkillLayout). Run from Session Frontend (Automation, filter "Makhia.Inventory").
// See Documentation/InventoryDashboard.md.

#include "AbilitySystem/MKHGameplayTags.h"
#include "Equipment/EquipmentTypes.h"
#include "Inventory/MKHSkillLayout.h"
#include "Misc/AutomationTest.h"
#include "QuickSlot/MKHQuickSlotPreferenceSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FMKHSkillLayoutSpec, "Makhia.Inventory.SkillLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

	/** Slot of the skill at a position of the package. */
	static int32 SlotOf(const FEquipmentEffectPackage& Package, int32 AbilityIndex)
	{
		return FMKHSkillLayout::GetSkillSlots().Find(Package.Abilities[AbilityIndex].SkillInputTag);
	}

	/** A weapon with a basic ability and NumSkills skills on the slots in order; skills use existing tags as identity. */
	static FEquipmentEffectPackage MakeWeapon(int32 NumSkills)
	{
		FEquipmentEffectPackage Package;
		Package.Abilities.AddDefaulted();
		for (int32 Index = 0; Index < NumSkills; ++Index)
		{
			FEquipmentAbilityDefinition& Skill = Package.Abilities.AddDefaulted_GetRef();
			Skill.bIsSkillAbility = true;
			Skill.AbilityTag = FMKHSkillLayout::GetSlotAt(Index);
			Skill.SkillInputTag = FMKHSkillLayout::GetSlotAt(Index);
		}
		return Package;
	}
END_DEFINE_SPEC(FMKHSkillLayoutSpec)

void FMKHSkillLayoutSpec::Define()
{
	const FGameplayTag First = FMKHSkillLayout::GetSlotAt(0);
	const FGameplayTag Second = FMKHSkillLayout::GetSlotAt(1);
	const FGameplayTag Third = FMKHSkillLayout::GetSlotAt(2);

	Describe("SwapSlots", [this, First, Second, Third]()
	{
		It("swaps two skills", [this, First, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(3);
			TestTrue("Moved", FMKHSkillLayout::SwapSlots(Package, First, Third));
			TestEqual("First skill", SlotOf(Package, 1), 2);
			TestEqual("Third skill", SlotOf(Package, 3), 0);
			TestEqual("Second skill untouched", SlotOf(Package, 2), 1);
		});

		It("moves a skill to an empty slot and leaves its own empty", [this, First, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(2);
			TestTrue("Moved", FMKHSkillLayout::SwapSlots(Package, First, Third));
			TestEqual("Moved skill", SlotOf(Package, 1), 2);
			TestNull("Slot left empty", FMKHSkillLayout::FindSkillInSlot(Package, First));
		});

		It("refuses an empty source, the same slot and other tags", [this, First, Second, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(1);
			TestFalse("Empty source", FMKHSkillLayout::SwapSlots(Package, Third, First));
			TestFalse("Same slot", FMKHSkillLayout::SwapSlots(Package, First, First));
			TestFalse("Not a skill slot", FMKHSkillLayout::SwapSlots(Package, First, MKHGameplayTags::Input::Skill.GetTag()));
			TestEqual("Nothing moved", SlotOf(Package, 1), 0);
		});
	});

	Describe("ApplyPreferences", [this, First, Second, Third]()
	{
		It("puts a remembered skill on its slot, on a weapon with other skills", [this, First, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(3);
			const TArray<FMKHSkillSlotPreference> Preferences = { { First, Third } };
			TestTrue("Moved", FMKHSkillLayout::ApplyPreferences(Package, Preferences));
			TestEqual("Remembered skill", SlotOf(Package, 1), 2);
			TestEqual("Pushed skill", SlotOf(Package, 3), 0);
		});

		It("gives the slot to the first skill when two claim it", [this, First, Second, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(2);
			const TArray<FMKHSkillSlotPreference> Preferences = { { First, Third }, { Second, Third } };
			FMKHSkillLayout::ApplyPreferences(Package, Preferences);
			TestEqual("First skill", SlotOf(Package, 1), 2);
			TestEqual("Second skill", SlotOf(Package, 2), 1);
		});

		It("sends the loser of a claimed slot to the slot left free", [this, First, Second, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(3);
			const TArray<FMKHSkillSlotPreference> Preferences = { { Second, First }, { Third, First } };
			FMKHSkillLayout::ApplyPreferences(Package, Preferences);
			TestEqual("Winner on the slot", SlotOf(Package, 2), 0);
			TestEqual("Pushed skill on the slot left", SlotOf(Package, 1), 1);
			TestEqual("Loser keeps a slot of its own", SlotOf(Package, 3), 2);
		});

		It("keeps a layout whose skills are already placed", [this, First, Second, Third]()
		{
			FEquipmentEffectPackage Package = MakeWeapon(3);
			const TArray<FMKHSkillSlotPreference> Preferences = { { First, Second }, { Second, First } };
			TestTrue("First pass moves", FMKHSkillLayout::ApplyPreferences(Package, Preferences));
			TestFalse("Second pass changes nothing", FMKHSkillLayout::ApplyPreferences(Package, Preferences));
			TestEqual("Third skill untouched", SlotOf(Package, 3), 2);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
