// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHDevLoginPanelWidget.generated.h"

class UMKHButton;
class UMKHButtonStyle;
class UMKHDevLoginPanelWidget;
class UMKHTextStyle;
class UPanelWidget;

/** Routes the click of one generated development login button to its Dev Auth Tool profile. */
UCLASS()
class MAKHIA_API UMKHDevLoginButtonRelay : public UObject
{
	GENERATED_BODY()

public:
	/** Panel that owns the button. */
	TWeakObjectPtr<UMKHDevLoginPanelWidget> Panel;

	/** Index of the profile in UMKHOnlineSettings::DevCredentialNames. */
	int32 ProfileIndex = INDEX_NONE;

	/** Button callback: signs in with the profile. */
	UFUNCTION()
	void HandleClicked();
};

/**
 * Development login buttons of the lobby, one per UMKHOnlineSettings::DevCredentialNames profile (Dev Auth Tool).
 *
 * Shown only when UMKHAuthSubsystem::ShouldShowDevLoginButtons (project flag, never in Shipping); otherwise the
 * panel collapses and builds nothing. Buttons are created at initialization in ButtonBox and are enabled only
 * while a login can start. Labels are culture invariant (developer UI). The Blueprint (W_DevLoginPanel) owns
 * the frame. Full model: see Documentation/OfflineModeArchitecture.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHDevLoginPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface
	// ============================================================

	/**
	 * Signs in with a development profile.
	 *
	 * @param ProfileIndex  Index in UMKHOnlineSettings::DevCredentialNames.
	 */
	void LoginWithProfile(int32 ProfileIndex);

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Creates one styled button per development profile in ButtonBox. */
	void BuildButtons();

	// ============================================================
	// Properties
	// ============================================================

	/** Container the profile buttons are added to. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> ButtonBox;

	/** Look of the generated buttons. */
	UPROPERTY(EditDefaultsOnly, Category = "DevLogin")
	TObjectPtr<UMKHButtonStyle> ButtonStyle;

	/** Text style of the button labels. */
	UPROPERTY(EditDefaultsOnly, Category = "DevLogin")
	TObjectPtr<UMKHTextStyle> LabelStyle;

	/** Space between two buttons. */
	UPROPERTY(EditDefaultsOnly, Category = "DevLogin", meta = (ClampMin = "0.0"))
	float ButtonSpacing = 6.f;

	/** Generated buttons, one per profile. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHButton>> ProfileButtons;

	/** Click relays of ProfileButtons, kept alive by the panel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMKHDevLoginButtonRelay>> ButtonRelays;
};
