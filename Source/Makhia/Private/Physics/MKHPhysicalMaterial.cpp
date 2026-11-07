// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Physics/MKHPhysicalMaterial.h"

#include "Materials/MaterialInterface.h"

// ============================================================
// Public Interface
// ============================================================

bool UMKHPhysicalMaterial::HasFootprints() const
{
	return IsValid(FootprintDecalLeft) && IsValid(FootprintDecalRight);
}

UMaterialInterface* UMKHPhysicalMaterial::GetFootprintDecal(const bool bLeftFoot) const
{
	if (!HasFootprints())
	{
		return nullptr;
	}
	return bLeftFoot ? FootprintDecalLeft.Get() : FootprintDecalRight.Get();
}
