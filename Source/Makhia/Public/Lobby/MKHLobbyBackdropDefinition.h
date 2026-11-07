// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MKHLobbyBackdropDefinition.generated.h"

class UTexture2D;

/**
 * One lobby background: the picture shown behind the players and the light colours that match it.
 * The active one is picked in Project Settings -> Game -> Makhia Lobby (UMKHLobbySettings) and shown by
 * AMKHLobbyBackdrop. Full model and how to make a new one: see Documentation/LobbyPresentation.md.
 */
UCLASS(BlueprintType)
class MAKHIA_API UMKHLobbyBackdropDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Properties - Picture
	// ============================================================

	/** Picture shown behind the players; cropped to fill the screen whatever its aspect ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture")
	TObjectPtr<UTexture2D> Image;

	/**
	 * Point of the picture (0..1, from the top left) kept in view when the screen crops it: (0.5, 0.5) crops
	 * evenly, (0.5, 1) keeps the bottom.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D Focus = FVector2D(0.5, 0.5);

	/**
	 * When true, the picture is shifted (and zoomed just enough to keep the screen covered) so that its eye-level
	 * line (HorizonHeight) lies on the camera's real horizon; Focus.Y is then ignored. Keeps the players standing
	 * on the painted ground instead of floating over it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture")
	bool bAlignHorizon = true;

	/** Height of the picture's eye-level line (horizon / vanishing line of the ground), 0 = top, 1 = bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture", meta = (ClampMin = "0.05", ClampMax = "0.95", EditCondition = "bAlignHorizon"))
	float HorizonHeight = 0.5f;

	/** Colour multiplied over the picture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture")
	FLinearColor Tint = FLinearColor::White;

	/** Brightness of the picture; raise it if the tonemapper makes it look dull. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Picture", meta = (ClampMin = "0.0"))
	float Brightness = 1.f;

	// ============================================================
	// Properties - Character Lighting
	// ============================================================

	/** Colour of the main light on the players; match the dominant light of the picture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Lighting")
	FLinearColor KeyLightColor = FLinearColor(1.f, 0.85f, 0.65f);

	/** Intensity of the main light, in lux. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Lighting", meta = (ClampMin = "0.0"))
	float KeyLightIntensity = 3.f;

	/** Colour of the back light outlining the players against the picture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Lighting")
	FLinearColor RimLightColor = FLinearColor(1.f, 0.6f, 0.3f);

	/** Intensity of the back light, in lux. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Lighting", meta = (ClampMin = "0.0"))
	float RimLightIntensity = 5.f;
};
