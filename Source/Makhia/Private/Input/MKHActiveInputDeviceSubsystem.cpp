// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Input/MKHActiveInputDeviceSubsystem.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "MKHLogChannels.h"

namespace MKHInputDevice
{
	/** Stick or trigger deflection below which an analog event is treated as drift, not as use. */
	constexpr float AnalogUseThreshold = 0.5f;

	/** Mouse travel (pixels) below which a move event is treated as jitter, not as use. */
	constexpr float MouseUseThreshold = 2.0f;
}

/** Reports every input to the owning subsystem without consuming it. */
class FMKHInputDeviceProcessor : public IInputProcessor
{
public:
	explicit FMKHInputDeviceProcessor(UMKHActiveInputDeviceSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Report(InKeyEvent.GetKey().IsGamepadKey() ? EMKHInputDevice::Gamepad : EMKHInputDevice::KeyboardMouse);
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	{
		if (InAnalogInputEvent.GetKey().IsGamepadKey() && FMath::Abs(InAnalogInputEvent.GetAnalogValue()) >= MKHInputDevice::AnalogUseThreshold)
		{
			Report(EMKHInputDevice::Gamepad);
		}
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetCursorDelta().SizeSquared() >= FMath::Square(MKHInputDevice::MouseUseThreshold))
		{
			Report(EMKHInputDevice::KeyboardMouse);
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Report(EMKHInputDevice::KeyboardMouse);
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		Report(EMKHInputDevice::KeyboardMouse);
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("MKHInputDeviceProcessor"); }

private:
	void Report(EMKHInputDevice Device) const
	{
		if (UMKHActiveInputDeviceSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->ReportInput(Device);
		}
	}

	TWeakObjectPtr<UMKHActiveInputDeviceSubsystem> Owner;
};

// ============================================================
// Lifecycle
// ============================================================

void UMKHActiveInputDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	InputProcessor = MakeShared<FMKHInputDeviceProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
}

void UMKHActiveInputDeviceSubsystem::Deinitialize()
{
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

void UMKHActiveInputDeviceSubsystem::ReportInput(EMKHInputDevice Device)
{
	if (Device == ActiveDevice)
	{
		return;
	}

	ActiveDevice = Device;
	UE_LOG(LogMKHInput, Verbose, TEXT("ActiveInputDevice: now %s"), *UEnum::GetValueAsString(Device));
	OnActiveInputDeviceChanged.Broadcast(Device);
}
