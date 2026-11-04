# Hanji

A runtime for eight-direction sprite characters in a 3D quarter-view world, for Unreal Engine 5.8. Hanji (한지) is Korean paper: the characters are flat, hand-finished images, and Hanji stands them up in a lit, three-dimensional scene and makes them behave in an action game.

Status: Stage 1 of the [roadmap](ROADMAP.md) is done. The engine-independent core (data, clock, geometry) is tested in CI; the Unreal 5.8 plugin is built and tested locally and was measured in a vertical slice with a real character ([docs/unreal.md](docs/unreal.md#measurements)). Occluder fade, sorting, silhouettes, lit-sprite setup and PaperZD interop come next.

## What it does

Hanji owns two things and overlaps freely on everything else.

- **One runtime clock.** Frame durations in milliseconds, as the atlas states them, not a flipbook frame rate. A distance-based stride clock, so feet do not slide and footsteps land. Re-timing of an action around its contact frame, so the frame the artist exaggerated lands on the first active gameplay tick at any attack speed. Hit-stop as local time dilation with the policy (who freezes, how long, what happens on re-entry) as data. Events fired over intervals, so a hitch loses none and a pause re-fires none. Hitbox windows resolved from the same clock that chose the displayed frame.
- **The 3D presentation.** A single quad with a masked material that survives TSR and TAA, a ground pivot that stays on the floor at every zoom step, depth bias so tall sprites do not sink into walls, eight-direction selection with hysteresis and a facing lock during attacks, a blob shadow, an attachment point for effects that follows the direction, and a thin follow camera that owns zoom and pixel snap together. Occluder fade, stencil sorting, a silhouette behind cover and a light-facing shadow billboard follow.

It reads [pose-atlas](https://github.com/MetaCortex728/pose-atlas) output (`pose-contract.json`, version 2) and Aseprite JSON. It does not import `.aseprite` files, run an animation state machine or edit hitboxes: Paper2D, PaperZD, Paper2DPlus and the Aseprite importers do those, and Hanji accepts their output and emits events their notifies can consume.

## Origin

Han wrote the first version of this runtime in October 2026 inside his own Unreal game prototype, with OpenAI Codex as a coding assistant and without version control: a fixed-angle quarter-view camera with zoom steps, pose-atlas playback with contact frames and re-timing, a distance-based stride clock, stop settling, an occlusion silhouette, a contact shadow and about 250 automation assertions. Those 18 files are dated 19 and 20 October 2026 and kept unchanged in [`prototype/`](prototype/README.md); [docs/provenance.md](docs/provenance.md) lists their dates and hashes and what each does and does not prove. Hanji rebuilds them as an engine-independent core plus a plugin, so that the parts tied to one game's camera, character and actions stay in that game.

## Layout

| Path | Content |
|---|---|
| `core/` | The engine-independent C++17 library: data (clip tables and readers), clock, geometry. Builds and tests on Linux and Windows without Unreal. |
| `unreal/Hanji` | The Unreal Engine 5.8 plugin: clip-table asset, sprite component, update subsystem, camera, materials. See [docs/unreal.md](docs/unreal.md). |
| `unreal/HanjiSample` | The project that builds the plugin and runs its automation tests (`unreal/hanji-unreal.ps1`). |
| `prototype/` | The October 2026 files, unchanged. |
| `docs/` | Design notes and provenance. |
| `third_party/` | Vendored doctest and JSON for Modern C++, each with its licence; see [NOTICE](NOTICE). |

## Build the core

Requires CMake 3.21 or later and a C++17 compiler.

```sh
cmake --preset default      # or clang, asan, msvc
cmake --build --preset default
ctest --preset default
```

The Unreal plugin needs an installed Unreal Engine 5.8 and is built and tested locally with `unreal\hanji-unreal.ps1`; GitHub's runners have no Unreal, so continuous integration covers the core only.

Documentation: [data](docs/data.md), [clock](docs/clock.md), [geometry](docs/geometry.md), [Unreal plugin](docs/unreal.md), [provenance](docs/provenance.md).

## Licence

Apache License 2.0; see [LICENSE](LICENSE) and [NOTICE](NOTICE).
