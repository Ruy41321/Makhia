# Performance Optimization

## Target

| | |
|---|---|
| Minimum GPU | NVIDIA GTX 1650 (4 GB) |
| Target | 1920×1080, **60 FPS stable** in a full 1v1 match |
| GPU budget | ≤ 12 ms per frame (leaves headroom under 16.6 ms) |
| Measured on | Packaged **Shipping** build — never PIE (two PIE instances double the cost and the editor adds its own) |

Makhia is a 1v1 duel in small, static arenas: the look relies on readable lighting and clean
silhouettes, not on dynamic global illumination. The pipeline is chosen for that.

## Rendering pipeline

Set in `Config/DefaultEngine.ini` → `[/Script/Engine.RendererSettings]`:

| Setting | Value | Why |
|---|---|---|
| `r.AllowStaticLighting` | `True` | Lighting is baked into lightmaps (Lightmass): near-zero runtime cost |
| `r.DynamicGlobalIlluminationMethod` | `0` (None) | Lumen GI was the single largest GPU cost |
| `r.ReflectionMethod` | `2` (Screen Space) | Lumen reflections replaced by SSR + reflection captures |
| `r.Shadow.Virtual.Enable` | `0` | Virtual Shadow Maps replaced by classic shadow maps |
| `r.GenerateMeshDistanceFields` | `False` | Only needed by Lumen / distance-field effects |
| `r.RayTracing.RayTracingProxies.ProjectEnabled` | `False` | No ray tracing |

The previous values were the UE5 template defaults (Lumen + VSM), designed for current-generation
GPUs. Changing any of them needs an editor restart and a shader recompile.

## Lighting rules for levels

- Global lights (directional, sky light) are **Static** or **Stationary**; build lighting
  (Build → Build Lighting Only, Production) after any change to geometry or lights.
- Every playable level has a **Lightmass Importance Volume** around the playable area.
- Decorative point lights (braziers, glows) are **Static** unless they must flicker or move; a
  Movable light should not cast shadows. Where a light must also light the characters (Static lights reach them
  only through the volumetric lightmap), use **Stationary without shadows** (e.g. Makhion's room torches).
- No volumetric clouds or volumetric fog; sky is the cheap sky sphere.
- Post process: no motion blur; exposure fixed (Manual or Min = Max) so lightmaps look the same
  on every machine.
- Static meshes need a valid lightmap UV channel (Generate Lightmap UVs on import).
- **No Static/Stationary lights inside Child Actors.** A `ChildActorComponent` destroys and respawns
  its actor on every load, with a new light GUID, so its baked lighting is always reported as
  unbuilt ("LIGHTING NEEDS TO BE REBUILT"). Place a plain light actor in the level instead and
  clear the child actor class on that instance. `DumpUnbuiltLightInteractions` lists the culprits.

History: CastleArena once used Movable lights because, after the lobby → arena ServerTravel,
precomputed lighting appeared black. That happened while `r.AllowStaticLighting` was `False`
(baked data ignored). With static lighting enabled the travel must be re-tested after each
lighting build.

### Character lighting standard (2026-10-10)

Characters must look the same in every map. The reference is `Lvl_ThirdPerson`: one neutral sun with shadows
(3 lux, 6500 K) as the **dominant** light, a sky fill a few times weaker, fixed exposure (EV100 0), no white
balance shift. Measured with the Abyss skin (`Gideon_Undertow`) placed in each map:

- **The sun must dominate the sky light.** With the sky stronger than the sun, characters lose their shape and
  colour (flat, grey-beige). Rough target: a sunlit surface ~3–4× brighter than the same surface in shade.
- **Exposure and lights go together.** Compare maps by *light ÷ exposure*, not by raw intensity: CastleArena keeps
  bias −3 at EV100 0.8 (so its emissive sky sphere, barriers, fire and VFX keep their look), which is why its
  lights are ~28× the ThirdPerson values. Exposure is fixed with Min = Max (no eye adaptation between rooms).
- **Colour only on the lights, once.** Sun at 5800 K (slightly warm, as the art direction asks), sky light white,
  no `WhiteTemp` in the post process. Stacking warm sun + warm sky + `WhiteTemp` 7200 cancelled the blues of the
  skins and made them look sepia.
- **Lobby extra lights stay accents.** The backdrop key/rim (see `LobbyPresentation.md`) are a fraction of the
  level's directional light.

Changing a Stationary light's intensity or colour shows at once, but the baked bounce follows only after
Build Lighting.

### Current state of the levels (2026-10-10)

| Level | Lights | Other changes |
|---|---|---|
| CastleArena | Directional **Stationary** (shadows on) **85 lux, 5800 K** (was 3 lux, colour 255/217/168: far weaker than the sky), SkyLight **Stationary** cubemap at intensity **24**, white (was 80, tinted 255/224/191); exposure fixed at EV100 0.8, bias −3 (was auto 0.55–1); `WhiteTemp` and local exposure overrides removed. Point lights (4 braziers, GrateGlow, 4 `TorchLight_*` in the antechambers) **Static**. The antechamber torches (`BP_PROP_torch_standing_dungeon`) have their child light cleared; the `TorchLight_*` actors replace it with the same settings | Volumetric fog off, motion blur 0. Before: 8 Movable shadow-casting point lights, each re-rendering the scene six times per frame |
| Lobby | Directional Stationary **3.5 lux, 5800 K** (was 10 lux), SkyLight Stationary captured (unchanged), backdrop key 1 / rim 2.5 (were 3 / 6); manual exposure bias 0.3 (unchanged) | VolumetricCloud removed, lighting built (`Lobby_BuiltData`). Since 2026-10-07 the background is an unlit picture plane plus two Movable shadowless directional lights (`AMKHLobbyBackdrop`); fog, wall and visible floor removed (see `LobbyPresentation.md`) |
| Makhion (2026-10-14, in review; **2026-10-17: sun 5400 K, sky light 1.0, fog 0.06 / max 0.28 pale blue, Mie 0.005, bias −2.8, saturation 1.08, contrast 1.12, white balance 6800 K**: the exterior view light, chosen for the whole level, `Handoff/MakhionExteriorPlan.md` V1) | Directional Stationary **85 lux, 5200 K**, pitch −50 (across the duel axis), atmosphere sun; SkyLight Stationary captured from the SkyAtmosphere + cloud dome at **1.3**; exposure EV100 0.8 / bias **−2.7** (+0.3 EV, New World high-key look), bloom 1.0. 12 room torches **Stationary without shadows** (40 cd, indirect 0.5: Static ones left the fighters black in the rooms), 4 braziers **Static** | Same sun and exposure as CastleArena, checked on PIE screenshots (editor SceneCapture shots ignore the PPV exposure and look ~2 stops brighter: `MakhionHandoff.md`, phase 4). Golden height fog 0.15 from 5 m (max opacity 0.45), SkyAtmosphere Mie × 3, no volumetrics; translucent unlit cloud dome, no volumetric clouds. Phase 5 (2026-10-15): Fab statues reduced by LODs (placed triangles 3.3M → 1.5M), Fab textures capped at 2K / 1K (~800 → ~290 MB estimated), fires culled when not rendered (`NET_MKN_Fire`), Production light build (lightmaps 60 MB + shadowmaps 14 MB); details in `Handoff/MakhionHandoff.md` |

`Lvl_ThirdPerson` (editor start-up test map) is not shipped and was left as is. **CastleArena was deleted on
2026-10-16** (Makhion replaced it): its rows here and measurements #3, #4, #7, #8 stay as reference.

## Quality levels and first launch

`UMKHGameUserSettings` (see `SettingsArchitecture.md`):

- **First launch** of a packaged game: `RunFirstLaunchAutoDetect()` (called from
  `UMKHGameInstance::OnStart`) runs the engine hardware benchmark once, applies and saves the
  detected scalability levels. Without it the engine default is Epic for everyone.
  Skipped in the editor and on dedicated servers; guarded by `bHasAutoDetectedQuality`.
- **Frame rate**: unlimited is replaced by `DefaultFrameRateLimit` (144). An uncapped game keeps
  the GPU at 100% even in menus.
- **Reset to defaults** on the video page re-runs the benchmark instead of returning to Epic.
- **Steam Deck**: on a handheld PC both of the above stage the handheld profile instead of the benchmark
  (Medium, 40 FPS, borderless at the native resolution). Values and how to tune them: `SteamDeck.md`.

## Measuring

Console commands in a Shipping/Test or Development packaged build:

| Command | Use |
|---|---|
| `stat unit` | Frame / Game / Draw / GPU time — tells which thread is the bottleneck |
| `stat gpu` | GPU time per pass (shadows, base pass, post process, translucency…) |
| `ProfileGPU` | One-frame detailed GPU capture |
| `stat scenerendering` | Draw calls and primitives |
| Unreal Insights (`-trace=default,gpu`) | Full timing over a match |

Measure the lobby and a full round in CastleArena at each quality preset and record the results
in the table below.

### Summary (`stat unit`)

| # | Date | Build | Hardware | Preset | Scene | Render res | Frame ms | Game ms | Draw ms | RHIT ms | GPU ms | ≈ FPS | VRAM | Draws | Prims |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2026-10-07 | Old build (months ago, pre-optimization, Lumen + VSM + volumetric clouds) | RTX 5070 | Epic (presumed, no settings page yet) | Lobby | 72.9% (1400×788) as reported by `stat unit` | 4.48 | 2.60 | 4.47 | 2.32 | 3.94 | ~223 (uncapped) | 2.36 GB | 165 | 96.7K |
| 2 | 2026-10-07 | New build (lightweight renderer, baked lighting) | RTX 5070 | Epic | Lobby | 100% (1920×1080) | 3.04 | 2.69 | 3.05 | 1.33 | 2.59 | ~329 (uncapped) | 1.14 GB | 189 | 183.6K |
| 3 | 2026-10-07 | New build | RTX 5070 | Epic | CastleArena, vs bot, preparation phase (antechamber, looking through the barrier) | 100% (1920×1080) | 3.40 | 3.36 | 2.09 | 1.32 | 2.52 | ~294 (uncapped) | 1.50 GB | 290 | 245.8K |
| 4 | 2026-10-07 | New build | RTX 5070 | Medium | CastleArena, same view as #3 | 100% (1920×1080) | 3.38 | 3.39 | 2.04 | 1.10 | 1.63 | ~296 (uncapped) | 1.42 GB | 285 | 245.7K |
| 5 | 2026-10-15 | Development package, CSV profiler (`Tools/Perf/Measure-Arena.ps1`) | RTX 5070, Ryzen 7 9700X | Epic | Makhion, vs bot, preparation room looking through the gate | 100% (1920×1080) | 3.36 | 1.58 | 3.36 | 1.22 | 3.04 | ~298 (uncapped) | — | 488 | 881K |
| 6 | 2026-10-15 | same as #5 | RTX 5070 | Medium | Makhion, same view as #5 | 100% | 2.56 | 1.55 | 2.56 | 1.27 | 2.11 | ~391 | — | 473 | 770K |
| 7 | 2026-10-15 | same as #5 | RTX 5070 | Epic | CastleArena, same view as #3 | 100% | 2.83 | 1.50 | 2.82 | 1.23 | 2.51 | ~353 | — | 295 | 330K |
| 8 | 2026-10-15 | same as #5 | RTX 5070 | Medium | CastleArena, same view as #3 | 100% | 2.28 | 1.46 | 2.28 | 1.08 | 1.65 | ~439 | — | 273 | 292K |
| 9 | 2026-10-17 | same as #5 | RTX 5070 | Epic | Makhion with the exterior always loaded (visual pass V1-V5), same view as #5 | 100% | 7.53 | 1.88 | 7.53 | 1.25 | 7.16 | ~133 | — | 1244 | 3.13M |
| 10 | 2026-10-17 | same as #5 | RTX 5070 | Epic | Makhion, exterior loaded on demand (not in the duel), same view as #5 | 100% | 3.52 | 1.74 | 3.51 | 1.27 | 3.18 | ~284 | — | 771 | 819K |

### GPU breakdown (`stat gpu`, avg ms)

**#1 — old build, Lobby, Epic, RTX 5070.** Graphics queue total 2.97, async compute queue total 2.07
(the two overlap, which is why GPU time is 3.94 and not their sum).

| Graphics queue | ms | Compute queue | ms |
|---|---|---|---|
| Postprocessing | 1.18 | LumenScreenProbeGather | 1.10 |
| TemporalSuperResolution | 0.95 | LumenSceneLighting | 0.61 |
| RenderDeferredLighting | 0.80 | Postprocessing | 0.15 |
| Shadow Depths | 0.34 | TemporalSuperResolution | 0.15 |
| Lights | 0.29 | | |
| Shadow Projection | 0.25 | | |
| SubsurfaceScattering | 0.19 | | |
| LumenReflections | 0.15 | | |
| DistanceField Shadows | 0.07 | | |
| MotionBlur | 0.06 | | |
| Basepass | 0.06 | | |
| VolCloud (compose + reconstruction) | 0.03 | | |

Notes on #1:
- On an RTX 5070 the GPU is not the limit: Draw (render thread, 4.47 ms) is as long as the whole
  frame. GPU numbers on this card are too small to judge; the GTX 1650 is several times slower, so
  the same passes would dominate there.
- In the Lobby Lumen costs ~1.9 ms (ScreenProbeGather + SceneLighting + Reflections + SceneUpdate) and
  post processing + TSR ~2.3 ms — together most of the GPU frame.

**#2 — new build, Lobby, Epic, RTX 5070.** Graphics queue total 2.55, async compute queue total 0.82.

| Graphics queue | ms | Compute queue | ms |
|---|---|---|---|
| Postprocessing | 1.21 | Postprocessing | 0.76 |
| TemporalSuperResolution | 0.97 | TemporalSuperResolution | 0.76 |
| RenderDeferredLighting | 0.50 | | |
| Composition PreLighting | 0.22 | | |
| ScreenSpace AO | 0.21 | | |
| SubsurfaceScattering | 0.17 | | |
| Lights | 0.14 | | |
| Basepass | 0.14 | | |
| MotionBlur | 0.08 | | |
| Translucent Lighting | 0.07 | | |
| Shadow Projection | 0.07 | | |
| Reflection Environment | 0.07 | | |
| Shadow Depths | 0.04 | | |

#1 → #2 (Lobby, Epic, RTX 5070):
- GPU time 3.94 → **2.59 ms (−34%)**, with every Lumen pass gone (~1.9 ms). If #1 really rendered at
  72.9%, #2 draws ~1.9× more pixels for less GPU time, so the real per-pixel gain is larger.
- VRAM 2.36 → **1.14 GB (−52%)**: matters for the 4 GB of the GTX 1650.
- Render thread (Draw) 4.47 → 3.05 ms, RHI thread 2.32 → 1.33 ms: the frame is no longer bound by Draw.
- Not comparable: Prims 96.7K → 183.6K and Mem 1.38 → 3.77 GB reflect the newer content, not the renderer.
- What is left: **post processing + TSR ≈ 2.2 ms, ~85% of the graphics queue**. Next levers: cheaper
  anti-aliasing / TSR quality on Low–Medium, motion blur still on in the Lobby (0.08 ms; only
  CastleArena turns it off), SSAO (0.21 ms).

**#3 / #4 — new build, CastleArena, RTX 5070, Epic vs Medium.**

| Pass (graphics queue) | Epic #3 | Medium #4 |
|---|---|---|
| **Queue total** | **2.49** | **1.59** |
| Postprocessing | 1.11 | 0.51 |
| TemporalSuperResolution | 1.01 | 0.41 |
| RenderDeferredLighting | 0.51 | 0.36 |
| Basepass | 0.19 | 0.15 |
| SubsurfaceScattering | 0.19 | 0.13 |
| Composition PreLighting | 0.15 | 0.14 |
| ScreenSpace AO | 0.14 | 0.13 |
| Lights | 0.12 | 0.10 |
| Reflection Environment | 0.09 | 0.08 |
| Translucent Lighting | 0.08 | 0.00 |
| Translucency | 0.07 | 0.05 |
| Shadow Projection | 0.05 | 0.04 |
| Shadow Depths | 0.04 | 0.03 |
| Fog | 0.04 | 0.04 |
| MotionBlur | — (off) | — (off) |
| **Compute queue total** (Postprocessing + TSR) | **0.79** | **0.57** |

Notes on #3 / #4:
- Medium cuts GPU time by 35% (2.52 → 1.63 ms), almost all of it from post processing and TSR
  (2.12 → 0.92 ms). Lighting, shadows and base pass are already cheap: baked lighting works.
- **On this machine CastleArena is now game-thread bound**: Game ≈ Frame (3.4 ms) on both presets,
  so Medium does not raise the FPS here. Game thread is higher than in the Lobby (2.69 ms): worth a
  look with Unreal Insights (`-trace=default`) during a real fight — a weaker CPU paired with a
  GTX 1650 will feel it before the GPU does.
- Rough GTX 1650 estimate (≈ 5–6× slower than an RTX 5070 in raster; a ratio, not a measurement):
  Epic ≈ 13–15 ms GPU (borderline 60 FPS), Medium ≈ 8–10 ms (comfortable 60 FPS). To be confirmed
  on real hardware.
- Both shots are in the preparation phase: no fight, few effects on screen. A measurement during a
  duel (both characters, abilities, fire Niagara) is still missing.

**#5–#8 — Makhion vs CastleArena (2026-10-15, after the Makhion phase 5 pass).** Same build, same method:
`Measure-Arena.ps1 -Map <map> -Preset <0-3>` starts the packaged game on `<map>?Mode=Bot` (no lobby, no Steam),
applies the preset, uncaps the frame rate and records a CSV (`-csvCaptureFrames`, `-csvGpuStats`);
`Tools/Perf/csv_summary.py` averages it. Draw = render-thread time. The CSVs and screenshots are in `Saved/Perf`.

| GPU pass (ms) | Makhion Epic #5 | CastleArena Epic #7 | Makhion Medium #6 | CastleArena Medium #8 |
|---|---|---|---|---|
| **GPU total** | **3.04** | **2.51** | **2.11** | **1.65** |
| TemporalSuperResolution | 0.98 | 0.98 | 0.42 | 0.42 |
| Lights | 0.33 | 0.06 | 0.30 | 0.06 |
| Basepass | 0.29 | 0.19 | 0.26 | 0.15 |
| Postprocessing | 0.18 | 0.18 | 0.15 | 0.15 |
| SubsurfaceScattering | 0.15 | 0.16 | 0.09 | 0.09 |
| SSAO | 0.14 | 0.14 | 0.14 | 0.14 |
| ReflectionEnvironment | 0.11 | 0.08 | 0.10 | 0.08 |

Notes on #5–#8:
- Makhion costs **+0.5 ms GPU** over CastleArena on both presets (+21% Epic, +28% Medium). Most of it is
  **Lights (+0.27 ms)**: the 6 visible Stationary shadowless torches of the preparation room (40 cd, 7 m radius,
  chosen so the fighters are not black there, `MakhionHandoff.md` phase 4); then the base pass (+0.1 ms: 881K vs
  330K primitives drawn, 488 vs 295 draw calls).
- Both arenas are **render-thread bound** on this PC (Draw = Frame); the game thread is ~1.5 ms in both. #3 (game
  thread 3.4 ms) was measured with the old method and an older build: not comparable.
- Rough GTX 1650 estimate (≈ 5–6× slower in raster, a ratio, not a measurement): Makhion Epic ≈ 15–18 ms GPU (below
  60 FPS), **Medium ≈ 10.5–12.5 ms (60 FPS)**; CastleArena Epic ≈ 12.5–15, Medium ≈ 8–10 ms. The first-launch
  benchmark should put a GTX 1650 on Medium or lower.
- Not measured yet: the arena floor during a fight (the preparation countdown lasts 60 s), and VRAM.
- Levers if Makhion needs them: smaller radius or fewer room torches (Lights), TSR / AA on Low–Medium (roadmap 4).

**#9 — Makhion exterior always loaded (2026-10-17).** Same view as #5 (preparation room, looking through the gate):
GPU 3.04 → **7.16 ms**, draws 488 → 1244, primitives 881K → 3.13M. Top passes: base pass 1.94, prepass 1.21, TSR
1.00, shadow projection 0.79 ms. The exterior is behind the stands and the room walls, but its ~42k instances
(HISM, mostly small) are not occlusion-culled per instance and still go through prepass / base pass; on a GTX 1650
(≈ 5–6×) that would be far over the 16.6 ms budget. The packaged build also reported 180 objects with unbuilt
lighting.

**#10 — exterior on demand (2026-10-17, decision).** `Makhion_Exterior` is now a dynamic streaming level (not
loaded in the duel), streamed in by `LS_MKN_ExteriorViews`: same view GPU **3.18 ms** (#9 7.16, #5 3.04), 819K
primitives. Only 757 frames passed the summary filter in this capture (the others in #5 / #9: ~5900); the
averages agree with #5 within noise. Draw calls 771 vs #5 488 (content added since #5 inside the arena).

## Roadmap

1. ~~Lightweight renderer settings~~ (done)
2. ~~First-launch auto-detect, default frame cap~~ (done)
3. ~~Convert CastleArena and Lobby to baked lighting~~ (done) — still to verify: look in game after
   the lobby → arena ServerTravel, brazier light pools, sun shadows on characters
   **Paused 2026-10-07** until dedicated GTX 1650-class hardware is available. When resuming:
   measure a real duel (not the preparation phase), profile the game thread with Unreal Insights
   (CastleArena is CPU-bound on the RTX 5070), then the GPU quick wins below.
4. Scene content pass: project-wide motion blur off, cheaper AA / TSR on Low–Medium, SSAO on low
   presets, translucency, Niagara budgets
5. Character LODs and animation cost (URO)
6. Optional: 60 FPS cap in menus, reduced rendering when the window is in background
7. Measure on GTX 1650-class hardware, tune `DefaultScalability.ini` if needed
