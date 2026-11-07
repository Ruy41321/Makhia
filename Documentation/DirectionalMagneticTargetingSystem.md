# Directional Magnetic Targeting System (DMT)

The Directional Magnetic Targeting (DMT) system provides an invisible assist for melee attacks. It ensures that attacks connect reliably by subtly adjusting the player's trajectory and distance when a valid target is detected within specific criteria, preventing "near-misses" that can feel frustrating in fast-paced combat.

![DMT On/Off Example](/Extra/DMT.gif)

## Core Functionality

The system operates on three primary pillars:

1.  **Intent Detection**: Identifies the intended target by evaluating candidates based on their distance and angular alignment relative to the player's camera or movement direction.
2.  **Alignment**: Once a target is selected, the system smoothly interpolates the player's rotation to face the enemy directly.
3.  **The "Magnet" Effect**: If the player is slightly outside of optimal strike range, the system applies a subtle forward pull to close the gap.

Unlike a hard-lock system, DMT only adjusts the initial trajectory upon ability activation and does not follow the target if they later dodge or move out of the way.

## Technical Implementation

### Target Selection Scoring
The system uses a weighted scoring algorithm in `UMKHAbilityTask_ApplyDMT` to determine the "best" target within a sphere trace. It combines **Alignment** (how close the target is to the center of the screen/forward vector) and **Distance**.

```cpp
// From MKHAbilityTask_ApplyDMT.cpp -> SelectBestTargetFromHits
// Alignment calculation (1.0 = perfectly aligned, 0.0 = on the edge of MaxAngle)
float Alignment = FMath::Clamp(1.f - (AngleDegrees / DMTMaxAngle), 0.f, 1.f);

// Distance calculation (1.0 = extremely close, 0.0 = on the edge of Radius)
float NormalizedDistance = 1.f - FMath::Clamp(Distance / DMTRadius, 0.f, 1.f);

float Score = (Alignment * DMTAlignmentWeight) + (NormalizedDistance * DMTDistanceWeight);
```

### Movement and Rotation Interpolation
The adjustment is performed over a short duration (defined by `DMTDuration`) using interpolation to ensure the movement feels organic.

```cpp
// From MKHAbilityTask_ApplyDMT.cpp -> UpdateRotation & UpdatePosition
// Rotation logic
const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, DMTInterpSpeedRotation);
Character->SetActorRotation(NewRotation);

// Position logic (closing the gap)
if (CurrentDist3D > DMTTargetStopDistance)
{
    FVector StopLocation = TargetLoc - (DirectionNormalized * DMTTargetStopDistance);
    FVector NewLocation = FMath::VInterpTo(CharLoc, StopLocation, DeltaTime, DMTInterpSpeedPosition);
    Character->SetActorLocation(NewLocation, true);
}
```

## Configuration

DMT settings are balancing data: they live in each ability's row of `DT_AbilityTuning` (`FMKHAbilityTuningRow`, see `AbilityTuning.md`) and can be customized per ability to match specific animation requirements (e.g., a lunging stab vs. a stationary slash).

| Property | Description |
| :--- | :--- |
| `bEnableDMT` | Master toggle for the system on the specific ability. |
| `DMTRadius` | The maximum range of the detection sphere trace. |
| `DMTMaxAngle` | The maximum horizontal angle (cone) for target validation. |
| `AttackRange` | The desired stopping distance from the target. |

### Ability Integration
The task is typically triggered via `TryActivateDMT()` when a melee ability is activated (e.g., during `OnMontageStarted` and `OnComboTriggered`).

```cpp
// From MKHMeleeAbility.cpp
void UMKHMeleeAbility::TryActivateDMT()
{
    const FMKHAbilityTuningRow& Tuning = GetAbilityTuning();
    if (!Tuning.bEnableDMT) return;

    UMKHAbilityTask_ApplyDMT* DMTTask = UMKHAbilityTask_ApplyDMT::ApplyDMT(
        this,
        Tuning.DMTRadius,
        Tuning.DMTMaxAngle,
        Tuning.AttackRange
    );

    if (DMTTask)
    {
        DMTTask->OnDMTFinished.AddDynamic(this, &UMKHMeleeAbility::OnDMTFinished);
        DMTTask->ReadyForActivation();
    }
}
```
