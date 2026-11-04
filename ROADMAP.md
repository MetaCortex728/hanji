# Roadmap

Hanji owns one runtime clock and the 3D presentation of eight-direction sprite characters in a quarter-view world, and overlaps freely with the tools around it: it accepts their output (pose-atlas contracts, Aseprite JSON, Paper2D flipbooks) and emits events their notifies can consume. It does not import `.aseprite` files, run an animation state graph, or edit hitboxes.

## Stage 1: core, plugin and a vertical slice (done)

1. **Repository and CI:** Apache-2.0; the engine-independent core builds and tests with GCC, Clang, sanitizers and MSVC in CI; the October 2026 prototype is kept in `prototype/` with its provenance.
2. **Data:** clip tables read from pose-atlas contracts (v1 and v2), Aseprite JSON and `hanji-clips`; overlays for contact frames, events and hitboxes; mirrors filled with flipped boxes; validation.
3. **Clock:** millisecond durations played exactly; a distance-based stride clock; re-timing that keeps the contact frame on the startup tick at any speed and draws it on the tick that crosses it; hit-stop as local dilation with policy as data; events fired over intervals; hitbox windows; per-state policies.
4. **Geometry:** screen headings with hysteresis, view-plane snapping that keeps feet on the floor, follow, zoom steps, a quad that stands between upright and camera-facing with constant screen size.
5. **Unreal 5.8 plugin:** clip-table asset; a sprite component that never ticks (one world subsystem advances all sprites); a masked sprite material with Has Pixel Animation for TSR; a blob-shadow decal; effects attached to pixels; a quarter-view camera that owns zoom and pixel snap; depth tie-breaking between sprites.
6. **Vertical slice** in a private candidate of the author's game, with its character: measured contact timing at 0.5×, 1× and 2×, heading flicker, stride, hit-stop events, update cost, and TSR against no anti-aliasing (see [docs/unreal.md](docs/unreal.md#measurements)).

## Stage 2: presentation and interop (next)

7. **Occluder fade:** a screen-space circular dither in occluder materials around the character, as Octopath Traveler does, so the pose stays visible behind cover (about 3 hours).
8. **Sorting:** stencil sort priority between sprites, and the upright-depth correction for camera-facing quads that lean into walls (about 3 hours).
9. **Silhouette** behind cover through custom depth, as a shipped material (about 2 hours).
10. **Light-facing shadow billboard** with its own direction set, as an alternative to the blob shadow (about 3 hours).
11. **Lit sprites:** a tested lighting setup for the lit material (normal facing the camera, rim and fill), with captures (about 2 hours).
12. **Interop:** an optional PaperZD adapter (the component as playback source), a bridge raising Hanji events as PaperZD notifies and Paper2DPlus cues, and Paper2DPlus hitboxes as input (about 4 hours).
13. **Read-only clip viewer** in the editor: scrub a clip and see its frames, events and boxes (about 3 hours).
14. **Sample project** with pose-atlas's generated test rig, Blueprint documentation, and a packaged-build test (about 4 hours).

## Later

Pillars-style depth plates for flat painted backgrounds; 16 or 32 directions (the format already allows N); instanced rendering for crowds; proxy shadow casters; builds for Unreal 5.6 and 5.7; painted local motion (water, leaves) once its look is settled.
