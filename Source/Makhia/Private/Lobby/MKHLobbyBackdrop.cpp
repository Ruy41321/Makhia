// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyBackdrop.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/MKHLobbyBackdropDefinition.h"
#include "Lobby/MKHLobbySettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MKHLogChannels.h"
#include "SceneView.h"
#include "UObject/ConstructorHelpers.h"

namespace MKHLobbyBackdrop
{
	/** Material parameter names (see BackdropMaterial). */
	const FName ImageParam(TEXT("Image"));
	const FName TintParam(TEXT("Tint"));
	const FName BrightnessParam(TEXT("Brightness"));
	const FName UVTransformParam(TEXT("UVTransform"));

	/** Size of the engine plane mesh, in cm. */
	constexpr float PlaneSize = 100.f;
}

// ============================================================
// Lifecycle
// ============================================================

AMKHLobbyBackdrop::AMKHLobbyBackdrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BackdropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropMesh"));
	BackdropMesh->SetupAttachment(SceneRoot);
	BackdropMesh->SetUsingAbsoluteLocation(true);
	BackdropMesh->SetUsingAbsoluteRotation(true);
	BackdropMesh->SetUsingAbsoluteScale(true);
	BackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropMesh->SetCastShadow(false);
	BackdropMesh->SetMobility(EComponentMobility::Movable);
	BackdropMesh->bReceivesDecals = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		BackdropMesh->SetStaticMesh(PlaneMesh.Object);
	}

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetCastShadows(false);
	KeyLight->SetAtmosphereSunLight(false);
	KeyLight->SetRelativeRotation(FRotator(-35.f, 25.f, 0.f));

	RimLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("RimLight"));
	RimLight->SetupAttachment(SceneRoot);
	RimLight->SetMobility(EComponentMobility::Movable);
	RimLight->SetCastShadows(false);
	RimLight->SetAtmosphereSunLight(false);
	RimLight->SetRelativeRotation(FRotator(-25.f, 160.f, 0.f));
}

void AMKHLobbyBackdrop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ActiveBackdrop = ResolveBackdrop();
	ApplyBackdrop();

	FVector Location;
	FRotator Rotation;
	FVector2D HalfTangents;
	ReadPreviewView(Location, Rotation, HalfTangents);
	FitToView(Location, Rotation, HalfTangents);
}

void AMKHLobbyBackdrop::BeginPlay()
{
	Super::BeginPlay();

	// Nothing to show without a local viewport (dedicated server).
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Construction scripts do not run again in cooked levels and the dynamic material is transient.
	SetBackdrop(nullptr);
	if (!IsValid(ActiveBackdrop))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("AMKHLobbyBackdrop: no backdrop configured (Project Settings -> Makhia Lobby)."));
	}

	SetActorTickEnabled(true);
}

void AMKHLobbyBackdrop::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Location;
	FRotator Rotation;
	FVector2D HalfTangents;
	if (!ReadLocalView(Location, Rotation, HalfTangents))
	{
		return;
	}

	const bool bViewChanged = !Location.Equals(FittedLocation, 0.1)
		|| !Rotation.Equals(FittedRotation, 0.01)
		|| !HalfTangents.Equals(FittedHalfTangents, 1e-4);
	if (bViewChanged)
	{
		FitToView(Location, Rotation, HalfTangents);
	}
}

// ============================================================
// Public Interface
// ============================================================

void AMKHLobbyBackdrop::SetBackdrop(UMKHLobbyBackdropDefinition* NewBackdrop)
{
	ActiveBackdrop = IsValid(NewBackdrop) ? NewBackdrop : ResolveBackdrop();
	ApplyBackdrop();

	// The crop depends on the picture's aspect ratio: force a new fit.
	FittedHalfTangents = FVector2D::ZeroVector;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

UMKHLobbyBackdropDefinition* AMKHLobbyBackdrop::ResolveBackdrop() const
{
	if (IsValid(BackdropOverride))
	{
		return BackdropOverride;
	}

	const UMKHLobbySettings* Settings = UMKHLobbySettings::Get();
	return IsValid(Settings) ? Settings->Backdrop.LoadSynchronous() : nullptr;
}

void AMKHLobbyBackdrop::ApplyBackdrop()
{
	const bool bHasBackdrop = IsValid(ActiveBackdrop);
	UMaterialInstanceDynamic* Material = GetOrCreateMaterialInstance();

	if (IsValid(Material) && bHasBackdrop)
	{
		if (IsValid(ActiveBackdrop->Image))
		{
			Material->SetTextureParameterValue(MKHLobbyBackdrop::ImageParam, ActiveBackdrop->Image);
		}
		Material->SetVectorParameterValue(MKHLobbyBackdrop::TintParam, ActiveBackdrop->Tint);
		Material->SetScalarParameterValue(MKHLobbyBackdrop::BrightnessParam, ActiveBackdrop->Brightness);
	}

	if (IsValid(BackdropMesh))
	{
		BackdropMesh->SetVisibility(bHasBackdrop && IsValid(ActiveBackdrop->Image));
	}

	if (IsValid(KeyLight))
	{
		KeyLight->SetVisibility(bHasBackdrop);
		if (bHasBackdrop)
		{
			KeyLight->SetLightColor(ActiveBackdrop->KeyLightColor);
			KeyLight->SetIntensity(ActiveBackdrop->KeyLightIntensity);
		}
	}

	if (IsValid(RimLight))
	{
		RimLight->SetVisibility(bHasBackdrop);
		if (bHasBackdrop)
		{
			RimLight->SetLightColor(ActiveBackdrop->RimLightColor);
			RimLight->SetIntensity(ActiveBackdrop->RimLightIntensity);
		}
	}
}

UMaterialInstanceDynamic* AMKHLobbyBackdrop::GetOrCreateMaterialInstance()
{
	if (!IsValid(BackdropMesh) || !IsValid(BackdropMaterial))
	{
		return nullptr;
	}

	if (!IsValid(MaterialInstance) || MaterialInstance->Parent != BackdropMaterial)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(BackdropMaterial, this);
		BackdropMesh->SetMaterial(0, MaterialInstance);
	}

	return MaterialInstance;
}

bool AMKHLobbyBackdrop::ReadLocalView(FVector& OutLocation, FRotator& OutRotation, FVector2D& OutHalfTangents) const
{
	const ULocalPlayer* LocalPlayer = IsValid(GEngine) ? GEngine->GetFirstGamePlayer(GetWorld()) : nullptr;
	if (!IsValid(LocalPlayer) || !IsValid(LocalPlayer->ViewportClient) || !LocalPlayer->ViewportClient->Viewport)
	{
		return false;
	}

	const APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
	const APlayerCameraManager* CameraManager = IsValid(PlayerController) ? PlayerController->PlayerCameraManager : nullptr;
	if (!IsValid(CameraManager))
	{
		return false;
	}

	FSceneViewProjectionData Projection;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
	{
		return false;
	}

	// Perspective projection: the diagonal holds 1 / tan(half FOV) for each axis.
	const FMatrix& Matrix = Projection.ProjectionMatrix;
	if (FMath::IsNearlyZero(Matrix.M[0][0]) || FMath::IsNearlyZero(Matrix.M[1][1]))
	{
		return false;
	}

	OutLocation = CameraManager->GetCameraLocation();
	OutRotation = CameraManager->GetCameraRotation();
	OutHalfTangents = FVector2D(1.0 / Matrix.M[0][0], 1.0 / Matrix.M[1][1]);
	return true;
}

void AMKHLobbyBackdrop::ReadPreviewView(FVector& OutLocation, FRotator& OutRotation, FVector2D& OutHalfTangents) const
{
	const float HalfTangentX = FMath::Tan(FMath::DegreesToRadians(EditorPreviewFOV * 0.5f));
	OutLocation = GetActorLocation();
	OutRotation = GetActorRotation();
	OutHalfTangents = FVector2D(HalfTangentX, HalfTangentX / FMath::Max(EditorPreviewAspectRatio, 0.1f));
}

void AMKHLobbyBackdrop::FitToView(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector2D& HalfTangents)
{
	FittedLocation = ViewLocation;
	FittedRotation = ViewRotation;
	FittedHalfTangents = HalfTangents;

	if (!IsValid(BackdropMesh))
	{
		return;
	}

	const FRotationMatrix ViewAxes(ViewRotation);
	const FVector Forward = ViewAxes.GetUnitAxis(EAxis::X);
	const FVector Right = ViewAxes.GetUnitAxis(EAxis::Y);
	const FVector Up = ViewAxes.GetUnitAxis(EAxis::Z);

	// The engine plane maps U to +X and V to +Y: +X runs to screen right, +Y down, so +Z faces the camera.
	const FRotator PlaneRotation = FRotationMatrix::MakeFromXY(Right, -Up).Rotator();
	const float Width = 2.f * Distance * HalfTangents.X * Overscan;
	const float Height = 2.f * Distance * HalfTangents.Y * Overscan;

	BackdropMesh->SetWorldLocationAndRotation(ViewLocation + Forward * Distance, PlaneRotation);
	BackdropMesh->SetWorldScale3D(FVector(Width / MKHLobbyBackdrop::PlaneSize, Height / MKHLobbyBackdrop::PlaneSize, 1.f));

	if (IsValid(MaterialInstance))
	{
		MaterialInstance->SetVectorParameterValue(MKHLobbyBackdrop::UVTransformParam, ComputeImageCrop(HalfTangents, ViewRotation.Pitch));
	}
}

FLinearColor AMKHLobbyBackdrop::ComputeImageCrop(const FVector2D& HalfTangents, const float ViewPitch) const
{
	const UTexture2D* Image = IsValid(ActiveBackdrop) ? ActiveBackdrop->Image.Get() : nullptr;
	if (!IsValid(Image) || Image->GetSurfaceHeight() <= 0.f || HalfTangents.Y <= 0.0)
	{
		return FLinearColor(1.f, 1.f, 0.f, 0.f);
	}

	const float ImageAspect = Image->GetSurfaceWidth() / Image->GetSurfaceHeight();
	const float ViewAspect = HalfTangents.X / HalfTangents.Y;
	const FVector2D Focus = ActiveBackdrop->Focus;

	// "Cover" fit: the picture fills the view and the extra side is cropped around Focus.
	float ScaleU = ViewAspect >= ImageAspect ? 1.f : ViewAspect / ImageAspect;
	float ScaleV = ViewAspect >= ImageAspect ? ImageAspect / ViewAspect : 1.f;

	if (!ActiveBackdrop->bAlignHorizon)
	{
		return FLinearColor(ScaleU, ScaleV, (1.f - ScaleU) * Focus.X, (1.f - ScaleV) * Focus.Y);
	}

	// Where the camera's real horizon falls on the plane (0 = top), then zoom just enough for the picture's
	// horizon to land there with the plane still covered. Full model: see Documentation/LobbyPresentation.md.
	const float HorizonOnScreen = 0.5f + 0.5f * FMath::Tan(FMath::DegreesToRadians(ViewPitch)) / HalfTangents.Y;
	const float HorizonOnPlane = FMath::Clamp(0.5f + (HorizonOnScreen - 0.5f) / Overscan, 0.05f, 0.95f);
	const float PictureHorizon = ActiveBackdrop->HorizonHeight;

	const float FittedScaleV = FMath::Min3(ScaleV, PictureHorizon / HorizonOnPlane, (1.f - PictureHorizon) / (1.f - HorizonOnPlane));
	ScaleU *= FittedScaleV / ScaleV;
	ScaleV = FittedScaleV;

	return FLinearColor(ScaleU, ScaleV, (1.f - ScaleU) * Focus.X, PictureHorizon - ScaleV * HorizonOnPlane);
}
