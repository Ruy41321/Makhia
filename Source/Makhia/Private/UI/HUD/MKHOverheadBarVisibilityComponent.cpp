#include "UI/HUD/MKHOverheadBarVisibilityComponent.h"

#include "Blueprint/UserWidget.h"
#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

UMKHOverheadBarVisibilityComponent::UMKHOverheadBarVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UMKHOverheadBarVisibilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APlayerController* Viewer = GetViewingController();
	UWorld* World = GetWorld();
	if (!IsValid(Viewer) || !IsValid(World))
	{
		return;
	}

	FVector ViewOrigin;
	FRotator ViewRotation;
	Viewer->GetPlayerViewPoint(ViewOrigin, ViewRotation);
	const APawn* ViewerPawn = Viewer->GetPawn();
	const double Now = World->GetTimeSeconds();

	for (TActorIterator<ACharacterBase> It(World); It; ++It)
	{
		const ACharacterBase* Target = *It;
		UUserWidget* Bar = (Target != ViewerPawn) ? FindOverheadBarWidget(*Target) : nullptr;
		if (!IsValid(Bar))
		{
			continue;
		}

		FMKHOverheadBarRevealState& State = RevealStates.FindOrAdd(Target);
		if (IsRevealedFromView(*Target, ViewerPawn, ViewOrigin, ViewRotation.Vector()))
		{
			State.LastRevealTime = Now;
		}

		const bool bShow = State.LastRevealTime >= 0.0 && Now - State.LastRevealTime <= RevealLingerTime;
		State.Opacity = StepOpacity(State.Opacity, bShow, DeltaTime);
		Bar->SetRenderOpacity(State.Opacity);
	}

	PruneStaleStates();
}

bool UMKHOverheadBarVisibilityComponent::IsOverheadBarRevealed(const ACharacterBase* Target) const
{
	const APlayerController* Viewer = GetViewingController();
	if (!IsValid(Target) || !IsValid(Viewer))
	{
		return false;
	}

	FVector ViewOrigin;
	FRotator ViewRotation;
	Viewer->GetPlayerViewPoint(ViewOrigin, ViewRotation);
	return IsRevealedFromView(*Target, Viewer->GetPawn(), ViewOrigin, ViewRotation.Vector());
}

APlayerController* UMKHOverheadBarVisibilityComponent::GetViewingController() const
{
	const AHUD* HUD = Cast<AHUD>(GetOwner());
	return IsValid(HUD) ? HUD->GetOwningPlayerController() : nullptr;
}

UUserWidget* UMKHOverheadBarVisibilityComponent::FindOverheadBarWidget(const ACharacterBase& Target) const
{
	TInlineComponentArray<UWidgetComponent*> WidgetComponents(&Target);
	for (const UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (IsValid(WidgetComponent) && WidgetComponent->GetFName() == OverheadBarComponentName)
		{
			return WidgetComponent->GetWidget();
		}
	}
	return nullptr;
}

bool UMKHOverheadBarVisibilityComponent::IsRevealedFromView(const ACharacterBase& Target, const APawn* Viewer, const FVector& ViewOrigin, const FVector& ViewDir) const
{
	const FVector ViewerLocation = IsValid(Viewer) ? Viewer->GetActorLocation() : ViewOrigin;
	if (FVector::DistSquared(ViewerLocation, Target.GetActorLocation()) <= FMath::Square(ProximityRevealDistance))
	{
		return true;
	}
	return IsAimedAt(Target, Viewer, ViewOrigin, ViewDir);
}

bool UMKHOverheadBarVisibilityComponent::IsAimedAt(const ACharacterBase& Target, const APawn* Viewer, const FVector& ViewOrigin, const FVector& ViewDir) const
{
	UPrimitiveComponent* Body = GetAimBody(Target);
	if (!IsValid(Body))
	{
		return false;
	}

	// Component-level sweep: only the opponent's body is tested, whatever its channel responses.
	const FVector AimEnd = ViewOrigin + ViewDir * MaxAimDistance;
	FHitResult BodyHit;
	const bool bHitBody = AimToleranceRadius > 0.f
		? Body->SweepComponent(BodyHit, ViewOrigin, AimEnd, FQuat::Identity, FCollisionShape::MakeSphere(AimToleranceRadius))
		: Body->LineTraceComponent(BodyHit, ViewOrigin, AimEnd, FCollisionQueryParams(SCENE_QUERY_STAT(MKHOverheadBarAim)));
	if (!bHitBody)
	{
		return false;
	}

	return !bRequireLineOfSight || !IsLineOfSightBlocked(Target, Viewer, ViewOrigin, BodyHit.ImpactPoint);
}

UPrimitiveComponent* UMKHOverheadBarVisibilityComponent::GetAimBody(const ACharacterBase& Target)
{
	USkeletalMeshComponent* Mesh = Target.GetMesh();
	const bool bMeshHasQueryBodies = IsValid(Mesh)
		&& Mesh->Bodies.Num() > 0
		&& CollisionEnabledHasQuery(Mesh->GetCollisionEnabled());
	return bMeshHasQueryBodies ? static_cast<UPrimitiveComponent*>(Mesh) : Target.GetCapsuleComponent();
}

bool UMKHOverheadBarVisibilityComponent::IsLineOfSightBlocked(const ACharacterBase& Target, const APawn* Viewer, const FVector& From, const FVector& To) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MKHOverheadBarLineOfSight));
	Params.AddIgnoredActor(&Target);
	Params.AddIgnoredActor(Viewer);
	return GetWorld()->LineTraceTestByChannel(From, To, OcclusionChannel, Params);
}

float UMKHOverheadBarVisibilityComponent::StepOpacity(const float CurrentOpacity, const bool bShow, const float DeltaTime) const
{
	const float FadeTime = bShow ? FadeInTime : FadeOutTime;
	const float TargetOpacity = bShow ? 1.f : 0.f;
	if (FadeTime <= 0.f)
	{
		return TargetOpacity;
	}
	return FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, DeltaTime, 1.f / FadeTime);
}

void UMKHOverheadBarVisibilityComponent::PruneStaleStates()
{
	for (auto It = RevealStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
