// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UEquipmentManagerComponent;
class UInventoryComponent;
class UQuickSlotManagerComponent;

/**
 * Wires an owner's inventory, equipment and quick slot components to each other, the same way for the player
 * (AMKHPlayerController) and the duel bot (AMKHDuelBot), so using a quick slot behaves identically for both:
 * - inventory use of an equipment item → equip it;
 * - inventory release of an equipped item → unequip it;
 * - unequipped entry → back into the inventory (weapons: flagged unused again);
 * - inventory quick slot relocation → quick slot manager binding.
 *
 * See Documentation/InventoryDashboard.md and Documentation/MatchFlowArchitecture.md (Bot AI, Items).
 */
struct MAKHIA_API FMKHItemComponentLinks
{
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Binds the callbacks between the three components; call once, on every machine that runs them. The lambdas hold
	 * weak references only, so destroying a component never leaves a dangling callback.
	 *
	 * @param Inventory   Owner's inventory.
	 * @param Equipment   Owner's equipment.
	 * @param QuickSlots  Owner's quick slot manager.
	 */
	static void Bind(UInventoryComponent& Inventory, UEquipmentManagerComponent& Equipment, UQuickSlotManagerComponent& QuickSlots);
};
