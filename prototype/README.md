# The October 2026 prototype

These 18 files are the quarter-view runtime of Han's Unreal Engine 5.8 game prototype, copied unchanged from its game module (`GyeolLab/Source/GyeolLab`) on 4 November 2026. They are kept here as the starting point of Hanji, not as code that builds in this repository: they depend on the game module (its `GYEOLLAB_API` export macro, generated headers, Paper2D and the module's lab pawn), and they are written for one game's camera, character and actions.

[docs/provenance.md](../docs/provenance.md) lists each file's creation and last-write time and SHA-256. The files are stored byte for byte (`prototype/** -text` in `.gitattributes`), so `sha256sum` on a checkout reproduces those hashes.

## What each file holds, and what Hanji takes from it

| File | In the prototype | Carried into Hanji |
|---|---|---|
| `QuarterView/QuarterViewMath.h` | Zoom with target and half-life easing, clamped to 0.9–1.1; sprite facing rotation for a fixed view; snapping of a point to the view plane's pixel grid; ground anchor snap that keeps feet on the floor; follow easing; ground direction from screen input | All of it, as the geometry layer of the core |
| `QuarterView/QuarterViewCamera.h/.cpp` | A camera actor at a fixed quarter-view rotation that follows a target with easing, room bounds and zoom steps | The thin camera |
| `QuarterView/StrideClock.h` | A distance-sampled gait clock: pose index from distance travelled, reset on teleport or when walking is not allowed | The stride clock |
| `QuarterView/LocomotionStop.h` | Visual settling after movement stops, bucketed by the last walk phase, on a 60 Hz clip | Stop transitions on the clock |
| `QuarterView/PoseAtlas.h` | Atlas and metadata parsing; eight screen sectors from a ground direction with a previous-sector tie rule; a native strike timeline with contact and active-end frames and re-timing that keeps those landmarks aligned with gameplay; attack entry from movement; contact offsets from pixels to world | The data and clock layers: contract parsing, sector selection, contact-preserving re-timing, contact offsets. The jab, kick and dash timelines themselves stay in the game |
| `QuarterView/PixelProbe.h/.cpp` | A lab sprite component with pixel-grid probes and the lab's sprite material handling | The render component's pivot and grid handling; the probes stay in the game |
| `QuarterView/PlayerOcclusion.cpp` | An occlusion-hint silhouette through custom depth, with the material built at run time through editor-only APIs | The silhouette, rebuilt as a shipped material asset (Stage 2) |
| `QuarterView/NormalProbe.cpp`, `QuarterView/StudyLighting.h` | Lighting studies for the lab | Nothing; lab-only |
| `QuarterView/QuarterViewLab.h/.cpp` | The lab pawn: command-line study modes, resolution checks, sprite inputs | Nothing directly; it shows how the pieces were driven |
| `Opening/ContactShadow.h/.cpp` | A contact shadow under the character's pivot | The blob shadow |
| `Tests/PoseAtlasTests.cpp`, `Tests/QuarterViewCameraTests.cpp`, `Tests/ContactShadowTests.cpp` | Unreal automation tests of the above (about 250 assertions) | Their cases, ported to the core's doctest suite where the logic moved |

"Dan" in these files is the game's working title; the character's art is not part of this repository.
