# Geometry

The core uses Unreal's conventions so the plugin passes values straight through: centimetres, X forward, Y right, Z up, and the view rotation as pitch, yaw and roll in degrees. `view_basis` gives the camera's forward, right and up axes exactly as `FRotationMatrix` does.

## Headings

`select_heading` picks the nearest of eight 45-degree sectors **as the direction appears on screen**, not by world yaw: east is screen right, north screen up. A direction too short to have a heading keeps the previous one, and height never changes the heading. With `hysteresis_deg` above zero the previous heading holds until the direction is that many degrees past the sector boundary; a stick held near a boundary then changes heading at most once instead of flickering. The tests sweep a noisy input across the east/north-east boundary: more than ten flips without hysteresis, at most one with 3 degrees.

`ground_from_screen` is the inverse: the ground direction that looks like a given screen direction, so movement and facing agree. `turn_heading` turns from one heading to another over an attack's first frames by the shortest screen arc, eased, so a character does not snap around. `ground_direction` turns screen-relative input into ground movement for a camera yaw, keeping analogue magnitude and never adding height.

## Pixels and pivots

`contact_offset` turns a pixel of the cell (a hit spark, a hand) into a world offset from the ground pivot on the view plane; `onto_front_plane` then moves an effect along the view direction in front of its target without moving it on screen. `ground_clearance_offset` lifts a sprite whose lowest pixels sit under the pivot clear of flat ground, again only in depth.

`snap_view_plane` rounds a point to the camera's pixel lattice on the view plane and keeps its depth; `snap_ground_anchor` then slides it back along the view direction to its own height, so snapped feet stay on the floor. With one atlas resampled at the 90 % and 110 % zoom steps, snapping keeps foot placement stable; it does not promise crisp 1:1 pixels at those steps.

## Camera

`follow` is a frame-rate independent exponential approach with a half-life; the tests check the same result at 30, 60 and 120 Hz and no overshoot on a frame spike. `Zoom` eases a view-width ratio toward its target (clamped to 0.9 to 1.1 by default) and settles exactly. `zoom_focus_bounds` shrinks the camera's room bounds when a wider view shows more around the focus, never below zero.

Every function here comes from the October prototype; [prototype/README.md](../prototype/README.md) lists where. Its automation tests are ported to `core/tests/test_geometry.cpp`.
