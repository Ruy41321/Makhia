// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Log category for the Gameplay Ability System layer (abilities, attributes, damage pipeline, input queue). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHAbility, Log, All);

/** Log category for the authentication layer (auth subsystem, auth providers, automatic login at start-up). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHAuth, Log, All);

/** Log category for the audio layer (music playback, submix routing, volume modulation, user audio settings). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHAudio, Log, All);

/** Log category for the cosmetics layer (skin catalog, chosen skin and its save slot, skins applied to characters). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHCosmetics, Log, All);

/** Log category for the shared data layer (project data settings, data subsystem, item/rarity definition lookups). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHData, Log, All);

/** Log category for the input layer (key profiles, player key remapping, input user settings). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHInput, Log, All);

/** Log category for the item layer (inventory, equipment, item generation, world pickups). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHInventory, Log, All);

/** Log category for the loading screen and the travel errors (UMKHLoadingScreenSubsystem). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHLoading, Log, All);

/** Log category for the character movement framework (movement components, movement state machine and states). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHMovement, Log, All);

/** Log category for the network diagnostics (NAT type, direct/relayed P2P route, delivery quality, ping summaries). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHNet, Log, All);

/** Log category for the online backend layer (backend choice, session/social/transport providers). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHOnline, Log, All);

/** Log category for the session layer (login, lobby lifecycle, matchmaking, level travel). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHSession, Log, All);

/** Log category for the social layer (friends list, presence, friend requests, lobby invites). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHSocial, Log, All);

/** Log category for the UI layer (menus, HUD widgets, credits). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHUI, Log, All);

/** Log category for the AI layer (pathfinding, behavior trees, decision making). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHAI, Log, All);

/** Log category for arena actors driven by the match (gates, crowd). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHArena, Log, All);

/** Log category for the voice chat (local voice state, permission rule, devices). */
MAKHIA_API DECLARE_LOG_CATEGORY_EXTERN(LogMKHVoice, Log, All);
