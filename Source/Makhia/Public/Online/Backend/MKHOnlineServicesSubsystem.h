// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/Backend/MKHOnlineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MKHOnlineServicesSubsystem.generated.h"

class UMKHOnlineBackend;

/**
 * Picks the online backend once per game instance and gives it to the online subsystems of the game
 * (UMKHAuthSubsystem, UMKHSessionSubsystem, ...), which create their providers from it.
 *
 * Choice, in order: the -MKHOnline=<BackendName> switch (not in Shipping), UMKHOnlineSettings::BackendClass, the
 * Null backend. Full model: see Documentation/OnlineBackends.md.
 */
UCLASS()
class MAKHIA_API UMKHOnlineServicesSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle (Constructor, BeginPlay, Tick, EndPlay, etc.)
	// ============================================================

	/** Creates the backend. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Releases the backend. */
	virtual void Deinitialize() override;

	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/**
	 * Returns the online services subsystem of the game instance of a world context object.
	 *
	 * @param WorldContextObject  Any object living in a world.
	 * @return The subsystem, or null without a game instance.
	 */
	static UMKHOnlineServicesSubsystem* Get(const UObject* WorldContextObject);

	/** Returns the active backend (never null after Initialize). */
	const UMKHOnlineBackend* GetBackend() const { return Backend; }

	/** Returns the short name of the active backend (e.g. "Steam"). */
	UFUNCTION(BlueprintPure, Category = "Online")
	FName GetBackendName() const;

	/** Returns the features of the active backend, for the UI. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FMKHOnlineCapabilities GetCapabilities() const;

	/** Returns the platform name of the active backend (e.g. "Steam"), empty without a platform. */
	UFUNCTION(BlueprintPure, Category = "Online")
	FText GetPlatformDisplayName() const;

	/**
	 * Returns true on a handheld PC (Steam Deck), as reported by the backend. Outside Shipping, -MKHHandheld forces
	 * true so the handheld profile can be tried on a desktop PC (Documentation/SteamDeck.md).
	 */
	UFUNCTION(BlueprintPure, Category = "Online")
	bool IsRunningOnHandheld() const;

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Returns the backend class to create (see the class comment). */
	TSubclassOf<UMKHOnlineBackend> ResolveBackendClass() const;

	/**
	 * Returns the concrete backend class whose GetBackendName matches a name, ignoring case.
	 *
	 * @param BackendName  Name given on the command line.
	 * @return The class, or null when no loaded backend has that name.
	 */
	static TSubclassOf<UMKHOnlineBackend> FindBackendClassByName(const FString& BackendName);

	// ============================================================
	// Properties
	// ============================================================

	/** Active backend. */
	UPROPERTY(Transient)
	TObjectPtr<UMKHOnlineBackend> Backend;
};
