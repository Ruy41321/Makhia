// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/MKHNetDiagnosticsTypes.h"
#include "MKHNetStatsWidget.generated.h"

class UBorder;
class UImage;
class UMKHTextBlock;

/**
 * Network stats overlay of the match HUD (W_NetStat, top left of W_HUDOverlay).
 *
 * Compact: signal bars, ping, a HOST / CLIENT chip and a RELAY chip when the traffic goes through an
 * platform relay. Detailed adds jitter, packet loss, route and both NAT types. The mode comes from the
 * user setting (UMKHGameUserSettings::GetNetStatsMode), overridable with the console variable
 * mkh.NetStats. Hidden whenever there is no connection to another player (vs bot, offline).
 * On the host every value describes the connection to the client.
 * C++ owns the values and colours, the Blueprint owns the layout. See Documentation/NetworkDiagnostics.md.
 */
UCLASS(Abstract)
class MAKHIA_API UMKHNetStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================
	// Public Interface  (BlueprintCallable / externally-facing API)
	// ============================================================

	/** Reads the current measurements and updates every bound widget (also run by the refresh timer). */
	UFUNCTION(BlueprintCallable, Category = "HUD|Network Stats")
	void RefreshStats();

	/** Mode in effect: the console variable mkh.NetStats when set (0-2), the user setting otherwise. */
	UFUNCTION(BlueprintPure, Category = "HUD|Network Stats")
	static EMKHNetStatsMode GetEffectiveMode();

protected:
	// ============================================================
	// Lifecycle
	// ============================================================

	/** Starts the refresh timer and draws the first values. */
	virtual void NativeConstruct() override;

	/** Stops the refresh timer. */
	virtual void NativeDestruct() override;

	// ============================================================
	// Protected / Internal Logic
	// ============================================================

	/** Ping text, signal bars, host/client chip and relay chip. */
	void ApplyCompactRow(const FMKHNetStatsSnapshot& Stats);

	/** Jitter, packet loss, route and NAT rows. */
	void ApplyDetails(const FMKHNetStatsSnapshot& Stats);

	/** Lights as many bars as the ping deserves, in the ping colour. */
	void ApplySignalBars(int32 PingMs);

	/** Good / warning / bad colour of a ping. */
	FLinearColor GetPingColor(int32 PingMs) const;

	/** Colour of a NAT type: warning for Strict, neutral otherwise. */
	FLinearColor GetNatColor(EMKHNatType NatType) const;

	/** Player-facing name of a NAT type. */
	static FText GetNatText(EMKHNatType NatType);

	/** Player-facing name of a connection route. */
	static FText GetPathText(EMKHConnectionPath Path);

	/**
	 * Returns the route text with the relay location when known (e.g. "Relay (fra)").
	 *
	 * @param Path           Route of the connection.
	 * @param RelayLocation  Relay location, empty when unknown or direct.
	 */
	static FText GetRouteText(EMKHConnectionPath Path, const FString& RelayLocation);

	/**
	 * Shows or hides an optional value with its row (the parent panel, unless it is the details panel itself).
	 *
	 * @param ValueText  LocalNatText, RemoteNatText or QualityText.
	 * @param bShown     False when the backend does not report the value.
	 */
	void SetRowShown(UMKHTextBlock* ValueText, bool bShown) const;

	/** Returns the colour of a delivery quality in [0, 1]. */
	FLinearColor GetQualityColor(float Quality) const;

	/** Sets text and colour of an optional text block. */
	static void SetTextAndColor(UMKHTextBlock* TextBlock, const FText& Text, const FLinearColor& Color);

	// ============================================================
	// Properties - Configuration
	// ============================================================

	/** Seconds between two refreshes. The engine averages the ping over one second anyway. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats", meta = (ClampMin = "0.1", Units = "s"))
	float RefreshInterval = 0.5f;

	/** Pings up to this value light all four bars. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Thresholds", meta = (ClampMin = "0", Units = "ms"))
	int32 ExcellentPingMs = 50;

	/** Pings above this value turn yellow (three bars). */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Thresholds", meta = (ClampMin = "0", Units = "ms"))
	int32 WarningPingMs = 80;

	/** Pings above this value turn red (one bar). */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Thresholds", meta = (ClampMin = "0", Units = "ms"))
	int32 BadPingMs = 150;

	/** Good ping, direct route. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor GoodColor = FLinearColor(0.212f, 0.527f, 0.141f);

	/** Medium ping, strict NAT. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor WarningColor = FLinearColor(0.745f, 0.434f, 0.068f);

	/** High ping, relayed route. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor BadColor = FLinearColor(0.631f, 0.093f, 0.045f);

	/** Values with nothing to signal. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor NeutralColor = FLinearColor(0.815f, 0.745f, 0.597f);

	/** Signal bars that are off. */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor InactiveBarColor = FLinearColor(0.068f, 0.054f, 0.040f);

	/** Frame of the role chip on the host (gold). */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor HostChipColor = FLinearColor(0.584f, 0.397f, 0.145f);

	/** Frame of the role chip on the client (bronze grey). */
	UPROPERTY(EditDefaultsOnly, Category = "Network Stats|Colors")
	FLinearColor ClientChipColor = FLinearColor(0.147f, 0.130f, 0.099f);

	// ============================================================
	// Properties - Bound Widgets
	// ============================================================

	/** Ping in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidget))
	TObjectPtr<UMKHTextBlock> PingText;

	/** Signal bars, shortest first. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SignalBar0;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SignalBar1;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SignalBar2;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SignalBar3;

	/** Frame of the HOST / CLIENT chip; its brush colour follows the role. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RoleChip;

	/** "HOST" or "CLIENT". */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> RoleText;

	/** Shown only while the connection goes through a platform relay. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RelayChip;

	/** Rows of the detailed mode; collapsed in compact mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DetailsPanel;

	/** Detailed values. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> JitterText;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> LossText;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> RouteText;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> LocalNatText;

	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> RemoteNatText;

	/** Delivery quality measured by the transport, "local / remote"; its row is hidden when not reported. */
	UPROPERTY(BlueprintReadOnly, Category = "Network Stats", meta = (BindWidgetOptional))
	TObjectPtr<UMKHTextBlock> QualityText;

private:
	// ============================================================
	// Properties - Runtime State
	// ============================================================

	/** Timer driving RefreshStats (a collapsed widget does not tick, so NativeTick cannot be used). */
	FTimerHandle RefreshTimerHandle;
};
