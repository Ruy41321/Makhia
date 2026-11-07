// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MKHCareerSaveGame.generated.h"

/** Career progress of this machine (save slot "Career"). */
UCLASS()
class MAKHIA_API UMKHCareerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Levels beaten so far, counted from the first one. */
	UPROPERTY()
	int32 CompletedLevels = 0;
};
