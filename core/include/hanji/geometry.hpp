// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "hanji/clip.hpp"

namespace hanji {

// Unreal's conventions, so the plugin passes values straight through: centimetres, X forward, Y right,
// Z up, and a view rotation as pitch, yaw and roll in degrees (FRotator order).

struct Vec2 {
    double x = 0.0, y = 0.0;
};

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
};

double dot(const Vec3& a, const Vec3& b);
double length(const Vec3& v);

struct ViewRotation {
    double pitch_deg = -30.0;
    double yaw_deg = 45.0;
    double roll_deg = 0.0;
};

/// The camera's axes: forward (into the screen), right and up, as Unreal's FRotationMatrix gives them.
struct ViewBasis {
    Vec3 forward, right, up;
};
ViewBasis view_basis(const ViewRotation& view);

// ---------------------------------------------------------------- headings

/// The screen heading a ground direction looks like from this camera: nearest of eight 45-degree sectors
/// (e is screen right, n screen up), measured on screen, not in world yaw. A direction too short to have
/// a heading keeps `previous`. With `hysteresis_deg` > 0 the previous heading is kept until the direction
/// is that many degrees past the sector boundary, so a stick held on a boundary does not flicker.
Heading select_heading(const Vec3& ground_direction, const ViewRotation& view, Heading previous,
                       double hysteresis_deg = 0.0);

/// The ground direction that appears as `screen` (x right, y up) from this camera, unit length on the
/// ground plane; zero when the camera looks straight down the up axis.
Vec3 ground_from_screen(const Vec2& screen, const ViewRotation& view);

/// The screen direction of a heading's sector centre (e = (1, 0), n = (0, 1)).
Vec2 heading_screen_direction(Heading heading);

/// Turning from one heading to another over `t` in 0..1 by the shortest screen arc, eased (smoothstep),
/// quantised to a heading. An exact half-turn always takes the negative arc. Use it to turn into an
/// attack over its first frames instead of snapping.
Heading turn_heading(Heading from, Heading to, double t);

/// Movement on the ground from screen-relative input (x right, y up) for a camera of this yaw. Keeps
/// analogue magnitude, caps it at 1, never adds height.
Vec3 ground_direction(const Vec2& screen_input, double view_yaw_deg);

// ---------------------------------------------------------------- pixels and pivots

/// World offset from the ground pivot to a pixel of the cell, on the camera's view plane.
Vec3 contact_offset(const Vec2& pixel, const IntPoint& pivot, double units_per_pixel, const ViewRotation& view);

/// Moves a point along the view direction so it lies on the plane through `front` facing the camera:
/// same place on screen, in front of the target's depth (for effects on a hit).
Vec3 onto_front_plane(const Vec3& point, const Vec3& front, const Vec3& view_forward);

/// Depth offset that lifts a sprite whose lowest opaque pixel is `pixels_below_pivot` under the pivot
/// clear of flat ground, without moving it on screen.
Vec3 ground_clearance_offset(double pixels_below_pivot, double units_per_pixel, const ViewRotation& view);

/// Rounds a point to the camera's pixel lattice on the view plane (right and up), keeping its depth.
Vec3 snap_view_plane(const Vec3& point, const Vec3& origin, const ViewRotation& view, double units_per_pixel);

/// As snap_view_plane, then slides along the view direction back to the point's own height, so snapped
/// feet stay on the floor.
Vec3 snap_ground_anchor(const Vec3& foot, const Vec3& origin, const ViewRotation& view, double units_per_pixel);

// ---------------------------------------------------------------- camera

/// Frame-rate independent exponential follow with a half-life; zero half-life snaps.
Vec3 follow(const Vec3& current, const Vec3& target, double dt_s, double half_life_s);

/// A zoom preference eased toward its target. The value is a view-width ratio: 0.9 is closer, 1.1 wider.
struct Zoom {
    double current = 1.0;
    double target = 1.0;
    double min = 0.9;
    double max = 1.1;
    double half_life_s = 0.06;

    void set(double value);   ///< Clamped to [min, max]; non-finite values are ignored.
    void tick(double dt_s);   ///< Settles exactly on the target once within 1e-4.
    double width(double base_width) const { return base_width * current; }
};

/// Room bounds for the camera focus when a wider view shows more around it: the bounds shrink by the
/// extra half-width (and its 16:9 height), never below zero; closer views keep the base bounds.
Vec2 zoom_focus_bounds(const Vec2& base_half_extent, double base_width, double render_width);

}  // namespace hanji
