# Makhìa — UE5 C++ Project

*Makhìa is a free 1v1 melee duel game made in **Unreal Engine 5** by a solo developer, coming to **Steam**
(Windows, Steam Deck). The focus of the project is a solid, scalable multiplayer combat architecture built on the
**Gameplay Ability System (GAS)**, with Steam lobbies, peer-to-peer matches, voice chat and an AI duel bot for
offline play.*

For a deep dive into the architecture, see the [Technical Documentation](#technical-documentation) below.

## 🚀 Project Status: Coming to Steam
The game is feature complete and in its final release phase. This repository is a **public mirror** of my private
development environment: it shows the C++ code and the technical documentation as of **7 November 2026**.
[Why a public mirror?](#why-a-public-mirror)

## ▶️ Video Showcase
[![Video Showcase](/Extra/MakhiaDemoGif.gif)](https://www.youtube.com/watch?v=YK1ubb74XVQ)
*(Click the gif to watch the full video on YouTube — an earlier version of the game)*

## ⚔️ Main Systems
- **Combat on GAS** — damage pipeline with a custom effect context and execution, permanent armor mitigation,
  status effects, ability queue and cancel windows, data-driven tuning of every attack.
- **Character framework** — one component per mechanic (dodge, sprint, crouch, jump, blocking) driven by a movement
  state machine, shared by players and the bot.
- **Hit detection & targeting** — hit scans driven by the attack animations and a directional "magnetic" targeting
  system.
- **Items** — server-generated items with rolls and rarities, pickups, spawn points, inventory and equipment with
  validated server RPCs, gamepad-ready inventory UI written in C++.
- **Online** — backend-independent layer (`UMKHOnlineBackend`) with a Steam implementation: sign-in, lobbies,
  invites, friends, matchmaking, P2P matches over Steam networking, network diagnostics.
- **Voice chat** — native UE VoIP with push-to-talk, server-side permissions and its own audio output stream.
- **Duel bot AI** — StateTree-driven opponent with attack choice, difficulty adjustment and an offline career mode.
- **Match flow** — rounds, preparation phase, scoreboard, rematch, loading screens and travel error handling.
- **Presentation** — settings, localization (English, Italian, French), full gamepad navigation, Steam Deck
  profile, Discord Rich Presence, crash reporting.

## Why a Public Mirror?
- **Licensing:** the private repository contains third-party assets (Fab/Marketplace and others) that cannot be
  redistributed.
- **Portfolio focus:** only the source code and the documentation are published, so the work can be reviewed without
  downloading gigabytes of binary data.

## Technologies Used
- **Engine:** Unreal Engine 5.8
- **Languages:** C++20, Blueprint
- **Core systems:** Gameplay Ability System, Enhanced Input, StateTree, UMG, Online Subsystem Steam, Steam Sockets
- **Architecture:** component-based gameplay, state machines, data-driven tuning, backend-independent online layer

## Development Team
- **Code, design and technical art:** Luigi Pennisi (solo developer).
- **Art and audio:** mostly third-party and open-source packages (listed in the in-game Credits), plus some VFX and
  animations of my own.

## 📄 License and legal
- The **source code in this repository** is licensed under the [MIT License](LICENSE). It does not include the
  game's art, audio or other content, which are not open source.
- The game itself: [Privacy Policy](https://ruy41321.github.io/Makhia/privacy-policy/) ·
  [End User License Agreement](https://ruy41321.github.io/Makhia/eula/) (also in Italian and French).
- Support for the game: support@luigipennisi.dev

---

## Technical Documentation
**Core gameplay**
- [Character & Movement System](Documentation/CharacterSystemArchitecture.md)
- [Gameplay Ability System (GAS)](Documentation/GASArchitecture.md)
- [Damage and Status Effects](Documentation/DamageAndStatusEffects.md) · [Armor Mitigation](Documentation/ArmorMitigation.md) · [Ability Tuning](Documentation/AbilityTuning.md)
- [Ability Queue](Documentation/QueueAbilitySystem.md) · [Ability Cancel](Documentation/AbilityCancelSystem.md)
- [Hit Detection](Documentation/HitDetectionArchitecture.md) · [Directional Magnetic Targeting](Documentation/DirectionalMagneticTargetingSystem.md)
- [Pickups](Documentation/PickupSystemArchitecture.md) · [Item Spawning](Documentation/ItemSpawnSystemArchitecture.md)

**Match, AI and online**
- [Match Flow](Documentation/MatchFlowArchitecture.md)
- [Duel Bot AI](Documentation/DuelBotAI.md) · [Offline Modes](Documentation/OfflineModes.md) · [Offline Architecture](Documentation/OfflineModeArchitecture.md)
- [Online Backends](Documentation/OnlineBackends.md) · [Voice Chat](Documentation/VoiceChat.md) · [Network Diagnostics](Documentation/NetworkDiagnostics.md)

**UI, audio and platform**
- [Inventory Dashboard](Documentation/InventoryDashboard.md) · [HUD Quick Slots](Documentation/HUDQuickSlots.md) · [Settings](Documentation/SettingsArchitecture.md)
- [Gamepad Menu Navigation](Documentation/GamepadMenuNavigation.md) · [Localization](Documentation/Localization.md) · [UI Typography](Documentation/UITypography.md)
- [Audio System](Documentation/AudioSystemArchitecture.md) · [Performance Optimization](Documentation/PerformanceOptimization.md) · [Steam Deck](Documentation/SteamDeck.md)

**Overview**
- [Design Patterns](Documentation/DesignPatternsUsed.md) · [System Analysis](Documentation/SystemAnalysis.md)

---
*For professional inquiries: contact@luigipennisi.dev*
