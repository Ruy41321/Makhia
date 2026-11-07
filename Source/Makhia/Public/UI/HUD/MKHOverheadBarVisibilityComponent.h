#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MKHOverheadBarVisibilityComponent.generated.h"

class ACharacterBase;
class APawn;
class APlayerController;
class UPrimitiveComponent;
class UUserWidget;

/** Per-opponent reveal state of an overhead bar. */
USTRUCT()
struct FMKHOverheadBarRevealState
{
	GENERATED_BODY()

	/** Current render opacity of the bar, faded towards 0 or 1. */
	float Opacity = 0.f;

	/** World time of the last tick that revealed the bar (proximity or aim); negative when never revealed. */
	double LastRevealTime = -1.0;
};

/**
 * Lives on AMainHUD and decides, for the local viewer, when the opponents' overhead health bars are shown: only when
 * the opponent is close (ProximityRevealDistance) or the camera centre is aimed at its body within AimToleranceRadius.
 * Drives the bar widget's render opacity, so the HUD's HiddenInGame toggle and the Blueprint stay independent.
 * Details: Documentation/HUDQuickSlots.md, "Opponent overhead bar".
 */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class MAKHIA_API UMKHOverheadBarVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

	// ============================================================
	// Lifecycle
	// ============================================================

public:
	/** Ticks after the camera update, so the aim test uses this frame's view. */
	UMKHOverheadBarVisibilityComponent();

	/** Updates the reveal state and opacity of every opponent's overhead bar. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * @param Target The opponent to test.
	 * @return True when the local viewer currently reveals Target's overhead bar (proximity or aim), ignoring the linger time.
	 */
	UFUNCTION(BlueprintPure, Category = "HUD|OverheadBar")
	bool IsOverheadBarRevealed(const ACharacterBase* Target) const;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

private:
	/** @return The player controller owning the HUD this component lives on, or nullptr. */
	APlayerController* GetViewingController() const;

	/**
	 * @param Target The character whose bar is looked up.
	 * @return The user widget of Target's overhead bar component, or nullptr when it has none (the local pawn).
	 */
	UUserWidget* FindOverheadBarWidget(const ACharacterBase& Target) const;

	/**
	 * @param Target     The opponent to test.
	 * @param Viewer     The local pawn, may be null (spectating, between rounds).
	 * @param ViewOrigin Camera location.
	 * @param ViewDir    Camera forward vector.
	 * @return True when Target is within ProximityRevealDistance of the viewer or aimed at.
	 */
	bool IsRevealedFromView(const ACharacterBase& Target, const APawn* Viewer, const FVector& ViewOrigin, const FVector& ViewDir) const;

	/**
	 * @param Target     The opponent to test.
	 * @param Viewer     The local pawn, ignored by the line of sight trace.
	 * @param ViewOrigin Camera location.
	 * @param ViewDir    Camera forward vector.
	 * @return True when the camera ray, thickened to AimToleranceRadius, touches Target's body and nothing hides it.
	 */
	bool IsAimedAt(const ACharacterBase& Target, const APawn* Viewer, const FVector& ViewOrigin, const FVector& ViewDir) const;

	/**
	 * @param Target The opponent whose body is wanted.
	 * @return The mesh when its physics asset bodies accept queries, otherwise the capsule.
	 */
	static UPrimitiveComponent* GetAimBody(const ACharacterBase& Target);

	/**
	 * @param Target The opponent hit by the aim ray.
	 * @param Viewer The local pawn, ignored.
	 * @param From   Camera location.
	 * @param To     Point of Target's body touched by the aim ray.
	 * @return True when world geometry on OcclusionChannel stands between From and To.
	 */
	bool IsLineOfSightBlocked(const ACharacterBase& Target, const APawn* Viewer, const FVector& From, const FVector& To) const;

	/**
	 * @param CurrentOpacity Opacity of the bar this frame.
	 * @param bShow          True to fade in, false to fade out.
	 * @param DeltaTime      Frame time.
	 * @return The opacity after one fade step (FadeInTime / FadeOutTime; instant when the time is 0).
	 */
	float StepOpacity(float CurrentOpacity, bool bShow, float DeltaTime) const;

	/** Drops the states of opponents that no longer exist. */
	void PruneStaleStates();

	// ============================================================
	// Properties
	// ============================================================

	// --- Reveal rules ---

	/** Distance (cm) from the local pawn under which the opponent's bar is always shown. Default 5 m. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Reveal", meta = (ClampMin = "0", Units = "cm"))
	float ProximityRevealDistance = 1500.f;

	/** Radius (cm) of the aim sweep from the camera centre: how far the crosshair may miss the body. Default 5 mm. 0 = exact line trace. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Reveal", meta = (ClampMin = "0", Units = "cm"))
	float AimToleranceRadius = 2.f;

	/** Maximum distance (cm) from the camera at which aiming reveals the bar. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Reveal", meta = (ClampMin = "0", Units = "cm"))
	float MaxAimDistance = 5000.f;

	/** When true, aiming at the opponent through a wall does not reveal the bar. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Reveal")
	bool bRequireLineOfSight = true;

	/** Trace channel used for the line of sight test (pawns ignore Visibility, so only world geometry blocks it). */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Reveal", meta = (EditCondition = "bRequireLineOfSight"))
	TEnumAsByte<ECollisionChannel> OcclusionChannel = ECC_Visibility;

	// --- Presentation ---

	/** Seconds the bar stays visible after the last reveal: a millimetre tolerance would otherwise flicker as the aim passes over the body. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Presentation", meta = (ClampMin = "0", Units = "s"))
	float RevealLingerTime = 0.5f;

	/** Seconds to fade the bar in. 0 = instant. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Presentation", meta = (ClampMin = "0", Units = "s"))
	float FadeInTime = 0.1f;

	/** Seconds to fade the bar out. 0 = instant. */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Presentation", meta = (ClampMin = "0", Units = "s"))
	float FadeOutTime = 0.1f;

	/** Name of the widget component holding the bar on the character Blueprint (BP_MKHPlayerCharacter). */
	UPROPERTY(EditAnywhere, Category = "Overhead Bar|Presentation")
	FName OverheadBarComponentName = TEXT("OverheadBar");

	// --- State ---

	/** Reveal state per opponent. */
	TMap<TWeakObjectPtr<const ACharacterBase>, FMKHOverheadBarRevealState> RevealStates;
};
