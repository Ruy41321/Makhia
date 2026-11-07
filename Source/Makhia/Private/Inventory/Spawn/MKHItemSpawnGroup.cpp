// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Inventory/Spawn/MKHItemSpawnGroup.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

AMKHItemSpawnGroup::AMKHItemSpawnGroup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	// Spawn logic runs on the server for the whole match: never stream it out.
	bIsSpatiallyLoaded = false;

	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (IsValid(SpriteComponent))
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture(TEXT("/Engine/EditorResources/S_Note"));
		SpriteComponent->Sprite = SpriteTexture.Get();
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SetupAttachment(SceneRoot);
	}
#endif
}
