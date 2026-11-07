// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MKHBuildVersionSubsystem.generated.h"

/**
 * Single source of the game version: Project Settings > Description > Project Version (DefaultGame.ini,
 * [/Script/EngineSettings.GeneralProjectSettings] ProjectVersion).
 *
 * The engine derives the network version from it (players on different versions never meet), and this subsystem
 * gives it to the crash reporter as the Sentry release ("makhia@<version>"), so crash reports are grouped by the same
 * version the symbols are uploaded for. See Documentation/BuildVersion.md and Documentation/CrashReporting.md.
 */
UCLASS()
class MAKHIA_API UMKHBuildVersionSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem Interface

	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Returns the game version set in the project settings (e.g. "0.3.0").
	 *
	 * @return The Project Version; "1.0.0.0" (engine default) if it was never set.
	 */
	UFUNCTION(BlueprintPure, Category = "Build")
	static FString GetGameVersion();

protected:
	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/**
	 * Sets the "__sentry" crash context game data (release "makhia@<version>", environment "production" in Shipping,
	 * "development" otherwise), which Sentry reads from every report of the engine crash reporter.
	 */
	void ApplyReleaseToCrashReporter() const;
};
