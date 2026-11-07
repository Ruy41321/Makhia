// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MKHSkinSaveGame.generated.h"

/** Skin chosen on this machine, kept by UMKHSkinSubsystem (save slot "Skin") and used again at the next start-up. */
UCLASS()
class MAKHIA_API UMKHSkinSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties
	// ============================================================

	/** Identifier of the chosen skin (FMKHSkinDefinition::SkinId). */
	UPROPERTY()
	FName SkinId;
};
