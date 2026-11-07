// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Cosmetics/MKHSkinCatalog.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// ============================================================
// Public Interface
// ============================================================

const FMKHSkinDefinition* UMKHSkinCatalog::FindSkin(const FName SkinId) const
{
	if (SkinId.IsNone())
	{
		return nullptr;
	}
	return Skins.FindByPredicate([SkinId](const FMKHSkinDefinition& Skin) { return Skin.SkinId == SkinId; });
}

const FMKHSkinDefinition* UMKHSkinCatalog::GetDefaultSkin() const
{
	return Skins.IsEmpty() ? nullptr : &Skins[0];
}

FName UMKHSkinCatalog::ResolveSkinId(const FName SkinId) const
{
	if (FindSkin(SkinId))
	{
		return SkinId;
	}

	const FMKHSkinDefinition* DefaultSkin = GetDefaultSkin();
	return DefaultSkin ? DefaultSkin->SkinId : NAME_None;
}

#if WITH_EDITOR
EDataValidationResult UMKHSkinCatalog::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FName> SeenIds;
	for (const FMKHSkinDefinition& Skin : Skins)
	{
		bool bAlreadySeen = false;
		SeenIds.Add(Skin.SkinId, &bAlreadySeen);
		if (Skin.SkinId.IsNone() || bAlreadySeen)
		{
			Context.AddError(FText::Format(INVTEXT("Skin '{0}': the SkinId is empty or used twice."), FText::FromName(Skin.SkinId)));
			Result = EDataValidationResult::Invalid;
		}
		if (Skin.Mesh.IsNull())
		{
			Context.AddError(FText::Format(INVTEXT("Skin '{0}' has no mesh."), FText::FromName(Skin.SkinId)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

