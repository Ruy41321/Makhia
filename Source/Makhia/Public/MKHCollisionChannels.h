// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "Engine/EngineTypes.h"

/** Custom "CameraObstacle" channel (Config/DefaultEngine.ini): the probe channel of the player's camera boom. */
inline constexpr ECollisionChannel ECC_MKHCameraObstacle = ECC_GameTraceChannel1;

/** Custom "Weapon" channel (Config/DefaultEngine.ini). */
inline constexpr ECollisionChannel ECC_MKHWeapon = ECC_GameTraceChannel2;
