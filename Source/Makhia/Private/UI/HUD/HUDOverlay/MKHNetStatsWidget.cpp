// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/HUD/HUDOverlay/MKHNetStatsWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameUserSettings/MKHGameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Online/MKHNetDiagnosticsSubsystem.h"
#include "TimerManager.h"
#include "UI/Text/MKHTextBlock.h"

#define LOCTEXT_NAMESPACE "MKHNetStats"

namespace MKHNetStats
{
	static TAutoConsoleVariable<int32> CVarNetStatsMode(
		TEXT("mkh.NetStats"), -1,
		TEXT("Network stats overlay: -1 = user setting, 0 = off, 1 = compact, 2 = detailed."));

	/** Placeholder for a value not known yet. */
	static const FText& Unknown()
	{
		static const FText Text = FText::AsCultureInvariant(TEXT("—"));
		return Text;
	}

	/** "±" in front of a localized value. */
	static const FTextFormat& PlusMinus()
	{
		static const FTextFormat Format = FTextFormat(FText::AsCultureInvariant(TEXT("±{0}")));
		return Format;
	}
}

// ============================================================
// Lifecycle
// ============================================================

void UMKHNetStatsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UMKHNetStatsWidget::RefreshStats, RefreshInterval, true);
	}
	RefreshStats();
}

void UMKHNetStatsWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	Super::NativeDestruct();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHNetStatsWidget::RefreshStats()
{
	const EMKHNetStatsMode Mode = GetEffectiveMode();
	const UMKHNetDiagnosticsSubsystem* Diagnostics = UMKHNetDiagnosticsSubsystem::Get(this);
	const FMKHNetStatsSnapshot Stats = IsValid(Diagnostics) ? Diagnostics->GetNetStats() : FMKHNetStatsSnapshot();

	if (Mode == EMKHNetStatsMode::Off || !Stats.bConnected)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyCompactRow(Stats);

	if (IsValid(DetailsPanel))
	{
		DetailsPanel->SetVisibility(Mode == EMKHNetStatsMode::Detailed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Mode == EMKHNetStatsMode::Detailed)
	{
		ApplyDetails(Stats);
	}
}

EMKHNetStatsMode UMKHNetStatsWidget::GetEffectiveMode()
{
	const int32 Override = MKHNetStats::CVarNetStatsMode.GetValueOnGameThread();
	if (Override >= 0)
	{
		return static_cast<EMKHNetStatsMode>(FMath::Min(Override, static_cast<int32>(EMKHNetStatsMode::Detailed)));
	}

	const UMKHGameUserSettings* UserSettings = UMKHGameUserSettings::GetMKHGameUserSettings();
	return IsValid(UserSettings) ? UserSettings->GetNetStatsMode() : EMKHNetStatsMode::Off;
}

// ============================================================
// Protected / Internal Logic
// ============================================================

void UMKHNetStatsWidget::ApplyCompactRow(const FMKHNetStatsSnapshot& Stats)
{
	// AvgLag stays at zero during the first second of a connection.
	const bool bHasPing = Stats.PingMs > 0;
	SetTextAndColor(PingText,
		bHasPing ? FText::Format(LOCTEXT("PingMs", "{0} ms"), FText::AsNumber(Stats.PingMs)) : MKHNetStats::Unknown(),
		bHasPing ? GetPingColor(Stats.PingMs) : NeutralColor);
	ApplySignalBars(bHasPing ? Stats.PingMs : -1);

	SetTextAndColor(RoleText, Stats.bIsHost ? LOCTEXT("RoleHost", "Host") : LOCTEXT("RoleClient", "Client"), NeutralColor);
	if (IsValid(RoleChip))
	{
		RoleChip->SetBrushColor(Stats.bIsHost ? HostChipColor : ClientChipColor);
	}

	if (IsValid(RelayChip))
	{
		RelayChip->SetVisibility(Stats.Path == EMKHConnectionPath::Relayed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMKHNetStatsWidget::ApplyDetails(const FMKHNetStatsSnapshot& Stats)
{
	// The sign stays out of the localized pattern: the gatherer mangles escaped characters in LOCTEXT.
	SetTextAndColor(JitterText,
		FText::Format(MKHNetStats::PlusMinus(), FText::Format(LOCTEXT("PingMs", "{0} ms"), FText::AsNumber(Stats.JitterMs))),
		NeutralColor);

	FNumberFormattingOptions PercentFormat;
	PercentFormat.SetMaximumFractionalDigits(0);
	const bool bLosing = Stats.PacketLossIn > 0.01f || Stats.PacketLossOut > 0.01f;
	SetTextAndColor(LossText,
		FText::Format(LOCTEXT("PacketLoss", "{In} / {Out}"),
			FFormatNamedArguments{
				{ TEXT("In"), FText::AsPercent(Stats.PacketLossIn, &PercentFormat) },
				{ TEXT("Out"), FText::AsPercent(Stats.PacketLossOut, &PercentFormat) } }),
		bLosing ? WarningColor : NeutralColor);

	const FLinearColor RouteColor = Stats.Path == EMKHConnectionPath::Relayed ? BadColor
		: Stats.Path == EMKHConnectionPath::Direct ? GoodColor : NeutralColor;
	SetTextAndColor(RouteText, GetRouteText(Stats.Path, Stats.RelayLocation), RouteColor);

	SetRowShown(QualityText, Stats.bQualityReported);
	if (Stats.bQualityReported)
	{
		SetTextAndColor(QualityText,
			FText::Format(LOCTEXT("Quality", "{Local} / {Remote}"),
				FFormatNamedArguments{
					{ TEXT("Local"), FText::AsPercent(Stats.QualityLocal, &PercentFormat) },
					{ TEXT("Remote"), FText::AsPercent(Stats.QualityRemote, &PercentFormat) } }),
			GetQualityColor(FMath::Min(Stats.QualityLocal, Stats.QualityRemote)));
	}

	SetRowShown(LocalNatText, Stats.bNatTypeReported);
	SetRowShown(RemoteNatText, Stats.bNatTypeReported);
	SetTextAndColor(LocalNatText, GetNatText(Stats.LocalNatType), GetNatColor(Stats.LocalNatType));
	SetTextAndColor(RemoteNatText, GetNatText(Stats.RemoteNatType), GetNatColor(Stats.RemoteNatType));
}

void UMKHNetStatsWidget::ApplySignalBars(int32 PingMs)
{
	int32 LitBars = 0;
	if (PingMs >= 0)
	{
		LitBars = PingMs <= ExcellentPingMs ? 4 : PingMs <= WarningPingMs ? 3 : PingMs <= BadPingMs ? 2 : 1;
	}

	const FLinearColor LitColor = PingMs >= 0 ? GetPingColor(PingMs) : InactiveBarColor;
	const TObjectPtr<UImage> Bars[] = { SignalBar0, SignalBar1, SignalBar2, SignalBar3 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bars); ++Index)
	{
		if (IsValid(Bars[Index]))
		{
			Bars[Index]->SetColorAndOpacity(Index < LitBars ? LitColor : InactiveBarColor);
		}
	}
}

FLinearColor UMKHNetStatsWidget::GetPingColor(int32 PingMs) const
{
	return PingMs <= WarningPingMs ? GoodColor : PingMs <= BadPingMs ? WarningColor : BadColor;
}

FLinearColor UMKHNetStatsWidget::GetNatColor(EMKHNatType NatType) const
{
	return NatType == EMKHNatType::Strict ? WarningColor : NeutralColor;
}

FText UMKHNetStatsWidget::GetNatText(EMKHNatType NatType)
{
	switch (NatType)
	{
	case EMKHNatType::Open:     return LOCTEXT("NatOpen", "Open");
	case EMKHNatType::Moderate: return LOCTEXT("NatModerate", "Moderate");
	case EMKHNatType::Strict:   return LOCTEXT("NatStrict", "Strict");
	default:                    return MKHNetStats::Unknown();
	}
}

FText UMKHNetStatsWidget::GetRouteText(EMKHConnectionPath Path, const FString& RelayLocation)
{
	if (Path != EMKHConnectionPath::Relayed || RelayLocation.IsEmpty())
	{
		return GetPathText(Path);
	}
	return FText::Format(LOCTEXT("RouteRelayedAt", "{Route} ({Location})"),
		FFormatNamedArguments{ { TEXT("Route"), GetPathText(Path) }, { TEXT("Location"), FText::AsCultureInvariant(RelayLocation) } });
}

void UMKHNetStatsWidget::SetRowShown(UMKHTextBlock* ValueText, bool bShown) const
{
	if (!IsValid(ValueText))
	{
		return;
	}

	// The row is the label + value panel holding the text; never collapse the whole details panel.
	UPanelWidget* Row = ValueText->GetParent();
	UWidget* Target = IsValid(Row) && Row != DetailsPanel && Row->GetChildrenCount() <= 2 ? static_cast<UWidget*>(Row) : ValueText;
	Target->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FLinearColor UMKHNetStatsWidget::GetQualityColor(float Quality) const
{
	return Quality >= 0.99f ? GoodColor : Quality >= 0.95f ? WarningColor : BadColor;
}

FText UMKHNetStatsWidget::GetPathText(EMKHConnectionPath Path)
{
	switch (Path)
	{
	case EMKHConnectionPath::Direct:  return LOCTEXT("RouteDirect", "Direct");
	case EMKHConnectionPath::Relayed: return LOCTEXT("RouteRelayed", "Relay");
	default:                          return MKHNetStats::Unknown();
	}
}

void UMKHNetStatsWidget::SetTextAndColor(UMKHTextBlock* TextBlock, const FText& Text, const FLinearColor& Color)
{
	if (IsValid(TextBlock))
	{
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
	}
}

#undef LOCTEXT_NAMESPACE
