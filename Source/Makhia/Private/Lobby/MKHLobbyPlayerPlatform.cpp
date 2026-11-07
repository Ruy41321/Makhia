// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Lobby/MKHLobbyPlayerPlatform.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Cosmetics/MKHSkinSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Career/MKHCareerSubsystem.h"
#include "Lobby/MKHLobbyCharacterPreview.h"
#include "Lobby/MKHLobbyPlayerState.h"
#include "MKHLogChannels.h"
#include "TimerManager.h"
#include "UI/Lobby/MKHLobbyNameplateWidget.h"

namespace MKHLobbyPlatform
{
	/** Names of the Blueprint components (BP_LobbyPlayerPlatform). */
	const FName PedestalName(TEXT("StaticMesh"));
	const FName ContactShadowName(TEXT("ContactShadow"));
	const FName NameplateName(TEXT("PlayerStatusWidget"));
	const FName AnchorName(TEXT("PlayerSpawnTransform"));

	/** Returns the component of the given class and name, or null. */
	template <typename T>
	T* FindNamedComponent(const AActor& Actor, const FName Name)
	{
		TArray<T*> Components;
		Actor.GetComponents<T>(Components);
		for (T* Component : Components)
		{
			if (IsValid(Component) && Component->GetFName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
	}
}

// ============================================================
// Lifecycle
// ============================================================

AMKHLobbyPlayerPlatform::AMKHLobbyPlayerPlatform()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AMKHLobbyPlayerPlatform::BeginPlay()
{
	Super::BeginPlay();

	ResolveComponents();
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins))
	{
		DisplayedSkinChangedHandle = Skins->OnDisplayedSkinChanged.AddUObject(this, &AMKHLobbyPlayerPlatform::HandleDisplayedSkinChanged);
	}
	RefreshPresentation();
}

void AMKHLobbyPlayerPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindShownPlayer(nullptr);
	DestroyPreview();
	GetWorldTimerManager().ClearTimer(NameplateRetryHandle);
	if (UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this); IsValid(Skins))
	{
		Skins->OnDisplayedSkinChanged.Remove(DisplayedSkinChangedHandle);
	}
	DisplayedSkinChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Public Interface
// ============================================================

void AMKHLobbyPlayerPlatform::ShowPlayer(APlayerState* PlayerState, const bool bLocal)
{
	if (!IsValid(PlayerState))
	{
		ShowEmpty();
		return;
	}

	if (ShownPlayer.Get() == PlayerState && bShownLocal == bLocal)
	{
		return;
	}

	ShownPlayer = PlayerState;
	bShownLocal = bLocal;
	BindShownPlayer(PlayerState);
	SpawnPreview();
	RefreshPresentation();
}

void AMKHLobbyPlayerPlatform::ShowEmpty()
{
	if (!ShownPlayer.IsValid() && !IsValid(Preview))
	{
		RefreshPresentation();
		return;
	}

	ShownPlayer.Reset();
	bShownLocal = false;
	BindShownPlayer(nullptr);
	DestroyPreview();
	RefreshPresentation();
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void AMKHLobbyPlayerPlatform::ResolveComponents()
{
	PedestalMesh = MKHLobbyPlatform::FindNamedComponent<UStaticMeshComponent>(*this, MKHLobbyPlatform::PedestalName);
	ContactShadowComponent = MKHLobbyPlatform::FindNamedComponent<UPrimitiveComponent>(*this, MKHLobbyPlatform::ContactShadowName);
	NameplateComponent = MKHLobbyPlatform::FindNamedComponent<UWidgetComponent>(*this, MKHLobbyPlatform::NameplateName);
	PreviewAnchor = MKHLobbyPlatform::FindNamedComponent<USceneComponent>(*this, MKHLobbyPlatform::AnchorName);

	if (!IsValid(PedestalMesh) || !IsValid(ContactShadowComponent) || !IsValid(NameplateComponent) || !IsValid(PreviewAnchor))
	{
		UE_LOG(LogMKHUI, Warning, TEXT("%s: missing Blueprint components (pedestal %d, contact shadow %d, nameplate %d, anchor %d)."),
			*GetName(), IsValid(PedestalMesh), IsValid(ContactShadowComponent), IsValid(NameplateComponent), IsValid(PreviewAnchor));
	}
}

void AMKHLobbyPlayerPlatform::SpawnPreview()
{
	UWorld* World = GetWorld();
	if (IsValid(Preview) || !IsValid(World))
	{
		return;
	}

	if (!PreviewClass)
	{
		UE_LOG(LogMKHUI, Warning, TEXT("%s: no PreviewClass, the player is shown without a character."), *GetName());
		return;
	}

	const FTransform SpawnTransform = IsValid(PreviewAnchor) ? PreviewAnchor->GetComponentTransform() : GetActorTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	Preview = World->SpawnActor<AMKHLobbyCharacterPreview>(PreviewClass, SpawnTransform, SpawnParams);
}

void AMKHLobbyPlayerPlatform::DestroyPreview()
{
	if (IsValid(Preview))
	{
		Preview->Destroy();
	}
	Preview = nullptr;
}

void AMKHLobbyPlayerPlatform::BindShownPlayer(APlayerState* PlayerState)
{
	AMKHLobbyPlayerState* LobbyPlayerState = Cast<AMKHLobbyPlayerState>(PlayerState);
	if (BoundPlayerState.Get() == LobbyPlayerState && (LobbyInfoChangedHandle.IsValid() || !IsValid(LobbyPlayerState)))
	{
		return;
	}

	if (AMKHLobbyPlayerState* OldPlayerState = BoundPlayerState.Get(); IsValid(OldPlayerState))
	{
		OldPlayerState->OnLobbyInfoChanged.Remove(LobbyInfoChangedHandle);
	}
	LobbyInfoChangedHandle.Reset();
	BoundPlayerState = LobbyPlayerState;

	if (IsValid(LobbyPlayerState))
	{
		LobbyInfoChangedHandle = LobbyPlayerState->OnLobbyInfoChanged.AddUObject(this, &AMKHLobbyPlayerPlatform::HandleLobbyInfoChanged);
	}
}

void AMKHLobbyPlayerPlatform::HandleLobbyInfoChanged(AMKHLobbyPlayerState* PlayerState)
{
	RefreshPresentation();
}

void AMKHLobbyPlayerPlatform::RefreshPresentation()
{
	if (IsValid(PedestalMesh))
	{
		PedestalMesh->SetScalarParameterValueOnMaterials(GlowParameter, ComputeGlow());
	}
	if (IsValid(ContactShadowComponent))
	{
		ContactShadowComponent->SetVisibility(ShownPlayer.IsValid());
	}

	RefreshPreviewSkin();
	RefreshNameplate();
}

void AMKHLobbyPlayerPlatform::RefreshPreviewSkin()
{
	if (!IsValid(Preview))
	{
		return;
	}

	FName SkinId;
	if (bShownLocal)
	{
		const UMKHSkinSubsystem* Skins = UMKHSkinSubsystem::Get(this);
		SkinId = IsValid(Skins) ? Skins->GetDisplayedSkinId() : NAME_None;
	}
	else if (const AMKHLobbyPlayerState* LobbyPlayerState = Cast<AMKHLobbyPlayerState>(ShownPlayer.Get()); IsValid(LobbyPlayerState))
	{
		SkinId = LobbyPlayerState->GetLobbySkinId();
	}

	// None (another player's skin not replicated yet) shows the default skin.
	Preview->SetSkin(SkinId);
}

void AMKHLobbyPlayerPlatform::HandleDisplayedSkinChanged(FName SkinId)
{
	if (bShownLocal)
	{
		RefreshPreviewSkin();
	}
}

void AMKHLobbyPlayerPlatform::RefreshNameplate()
{
	if (!IsValid(NameplateComponent))
	{
		return;
	}

	const AMKHLobbyPlayerState* LobbyPlayerState = Cast<AMKHLobbyPlayerState>(ShownPlayer.Get());
	const FString PlayerName = IsValid(LobbyPlayerState) ? LobbyPlayerState->GetLobbyDisplayName() : FString();

	// A player who is not signed in has no name: the plate stays hidden, like an empty platform.
	NameplateComponent->SetVisibility(!PlayerName.IsEmpty());
	if (PlayerName.IsEmpty())
	{
		return;
	}

	UMKHLobbyNameplateWidget* Nameplate = Cast<UMKHLobbyNameplateWidget>(NameplateComponent->GetUserWidgetObject());
	if (!IsValid(Nameplate))
	{
		GetWorldTimerManager().SetTimer(NameplateRetryHandle, this, &AMKHLobbyPlayerPlatform::RefreshNameplate, NameplateRetryInterval, false);
		return;
	}

	Nameplate->SetPlayer(PlayerName, LobbyPlayerState->IsLobbyReady(), LobbyPlayerState);

	const UMKHCareerSubsystem* Career = UMKHCareerSubsystem::Get(this);
	const int32 CareerLevels = LobbyPlayerState->GetLobbyCareerLevels();
	const bool bHighestTitle = IsValid(Career) && Career->GetLevelCount() > 0 && CareerLevels >= Career->GetLevelCount();
	Nameplate->SetTitle(IsValid(Career) ? Career->GetTitleForCompletedLevels(CareerLevels) : FText::GetEmpty(), bHighestTitle);
}

float AMKHLobbyPlayerPlatform::ComputeGlow() const
{
	if (!ShownPlayer.IsValid())
	{
		return EmptyGlow;
	}
	return bShownLocal ? LocalGlow : RemoteGlow;
}
