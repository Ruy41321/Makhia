// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MKHSkinCatalog.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/** Static mesh worn on top of a skin (e.g. the horns of Ember), attached to a socket or bone of the character mesh. */
USTRUCT(BlueprintType)
struct FMKHSkinAccessory
{
	GENERATED_BODY()

	/** Mesh of the accessory; loaded together with the skin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Socket or bone of the character mesh the accessory follows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FName AttachSocket = TEXT("head");

	/** Placement relative to the socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FTransform RelativeTransform;
};

/** One look of the player character: identity, name and portrait for the UI, meshes to show. */
USTRUCT(BlueprintType)
struct FMKHSkinDefinition
{
	GENERATED_BODY()

	/** Stable identifier, saved on disk and replicated: never rename it once shipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FName SkinId;

	/** Name shown in the appearance panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FText DisplayName;

	/** Portrait shown in the appearance panel (small texture, always loaded with the catalog). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	TObjectPtr<UTexture2D> Portrait;

	/** Character mesh; must use the skeleton of the player character. Loaded only when the skin is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Extra meshes worn with this skin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	TArray<FMKHSkinAccessory> Accessories;
};

/**
 * Every skin of the player character (Project Settings -> Game -> Makhia Data -> SkinCatalog), read through
 * UMKHSkinSubsystem. The first skin is the default one: shown when nothing was chosen or a saved/received
 * identifier is unknown. Full model: see Documentation/CharacterSkins.md.
 */
UCLASS(BlueprintType)
class MAKHIA_API UMKHSkinCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Finds a skin by identifier.
	 *
	 * @param SkinId  Identifier of the skin.
	 * @return The skin, or null when the identifier is unknown. Do not store it.
	 */
	const FMKHSkinDefinition* FindSkin(FName SkinId) const;

	/** Returns the default skin (the first one), or null when the catalog is empty. Do not store it. */
	const FMKHSkinDefinition* GetDefaultSkin() const;

	/**
	 * Returns the identifier itself when it is known, otherwise the default skin's identifier.
	 *
	 * @param SkinId  Identifier to check (saved on disk or received from the network).
	 */
	FName ResolveSkinId(FName SkinId) const;

	/** Returns every skin, default first. */
	const TArray<FMKHSkinDefinition>& GetSkins() const { return Skins; }

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif

protected:
	// ============================================================
	// Properties
	// ============================================================

	/** Skins in panel order; the first one is the default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skins", meta = (TitleProperty = "SkinId"))
	TArray<FMKHSkinDefinition> Skins;
};
