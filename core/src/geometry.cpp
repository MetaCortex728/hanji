// SPDX-License-Identifier: Apache-2.0
#include "hanji/geometry.hpp"

#include <algorithm>
#include <cmath>

namespace hanji {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kLn2 = 0.6931471805599453;

double radians(double degrees) { return degrees * kPi / 180.0; }

double ease_alpha(double dt_s, double half_life_s) { return 1.0 - std::exp(-kLn2 * dt_s / half_life_s); }

/// Signed difference a - b wrapped to (-180, 180].
double wrap_degrees(double d) {
    d = std::fmod(d, 360.0);
    if (d <= -180.0) d += 360.0;
    if (d > 180.0) d -= 360.0;
    return d;
}

}  // namespace

double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(const Vec3& v) { return std::sqrt(dot(v, v)); }

ViewBasis view_basis(const ViewRotation& view) {
    const double sp = std::sin(radians(view.pitch_deg)), cp = std::cos(radians(view.pitch_deg));
    const double sy = std::sin(radians(view.yaw_deg)), cy = std::cos(radians(view.yaw_deg));
    const double sr = std::sin(radians(view.roll_deg)), cr = std::cos(radians(view.roll_deg));
    ViewBasis b;
    b.forward = {cp * cy, cp * sy, sp};
    b.right = {sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp};
    b.up = {-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp};
    return b;
}

Heading select_heading(const Vec3& ground_direction, const ViewRotation& view, Heading previous, double hysteresis_deg) {
    const ViewBasis b = view_basis(view);
    const Vec3 ground{ground_direction.x, ground_direction.y, 0.0};
    const double x = dot(ground, b.right);
    const double y = dot(ground, b.up);
    if (!std::isfinite(x) || !std::isfinite(y) || x * x + y * y < 1e-10) return previous;
    const double angle = std::atan2(y, x) * 180.0 / kPi;
    const int sector = ((static_cast<int>(std::floor(angle / 45.0 + 0.5)) % kHeadingCount) + kHeadingCount) % kHeadingCount;
    if (sector == heading_index(previous) || !(hysteresis_deg > 0.0)) return heading_at(sector);
    const double from_previous = std::abs(wrap_degrees(angle - heading_index(previous) * 45.0));
    return from_previous <= 22.5 + hysteresis_deg ? previous : heading_at(sector);
}

Vec3 ground_from_screen(const Vec2& screen, const ViewRotation& view) {
    const ViewBasis b = view_basis(view);
    const Vec3 up_ground{b.up.x, b.up.y, 0.0};
    const double up_sq = dot(up_ground, up_ground);
    if (up_sq < 1e-8) return {};
    const Vec3 v = b.right * screen.x + up_ground * (screen.y / up_sq);
    const double len = std::sqrt(v.x * v.x + v.y * v.y);
    return len < 1e-12 ? Vec3{} : Vec3{v.x / len, v.y / len, 0.0};
}

Vec2 heading_screen_direction(Heading heading) {
    const double a = heading_index(heading) * kPi / 4.0;
    return {std::cos(a), std::sin(a)};
}

Heading turn_heading(Heading from, Heading to, double t) {
    if (!std::isfinite(t)) return from;
    t = std::clamp(t, 0.0, 1.0);
    const int source = heading_index(from);
    const int delta = (heading_index(to) - source + 12) % kHeadingCount - 4;
    const double weight = t * t * (3.0 - 2.0 * t);
    const int sector = static_cast<int>(std::floor(source + delta * weight + 0.5));
    return heading_at(((sector % kHeadingCount) + kHeadingCount) % kHeadingCount);
}

Vec3 ground_direction(const Vec2& screen_input, double view_yaw_deg) {
    const double c = std::cos(radians(view_yaw_deg)), s = std::sin(radians(view_yaw_deg));
    const Vec3 forward{c, s, 0.0};
    const Vec3 right{-s, c, 0.0};
    Vec3 d = forward * screen_input.y + right * screen_input.x;
    const double len = length(d);
    if (len > 1.0) d = d * (1.0 / len);
    return d;
}

Vec3 contact_offset(const Vec2& pixel, const IntPoint& pivot, double units_per_pixel, const ViewRotation& view) {
    const ViewBasis b = view_basis(view);
    return (b.right * (pixel.x - pivot.x) + b.up * (pivot.y - pixel.y)) * units_per_pixel;
}

Vec3 onto_front_plane(const Vec3& point, const Vec3& front, const Vec3& view_forward) {
    return point + view_forward * dot(front - point, view_forward);
}

Vec3 ground_clearance_offset(double pixels_below_pivot, double units_per_pixel, const ViewRotation& view) {
    const ViewBasis b = view_basis(view);
    if (!(pixels_below_pivot > 0.0) || !(units_per_pixel > 0.0) || std::abs(b.forward.z) < 0.01) return {};
    return b.forward * ((pixels_below_pivot * units_per_pixel * b.up.z + 0.5) / b.forward.z);
}

Vec3 snap_view_plane(const Vec3& point, const Vec3& origin, const ViewRotation& view, double units_per_pixel) {
    if (!(units_per_pixel > 0.0)) return point;
    const ViewBasis b = view_basis(view);
    Vec3 result = point;
    for (const Vec3& axis : {b.right, b.up}) {
        const double along = dot(point - origin, axis);
        result = result + axis * (std::round(along / units_per_pixel) * units_per_pixel - along);
    }
    return result;
}

Vec3 snap_ground_anchor(const Vec3& foot, const Vec3& origin, const ViewRotation& view, double units_per_pixel) {
    Vec3 result = snap_view_plane(foot, origin, view, units_per_pixel);
    const Vec3 forward = view_basis(view).forward;
    if (std::abs(forward.z) > 1e-12) result = result + forward * ((foot.z - result.z) / forward.z);
    return result;
}

Vec3 follow(const Vec3& current, const Vec3& target, double dt_s, double half_life_s) {
    if (!(dt_s > 0.0)) return current;
    if (!(half_life_s > 0.0)) return target;
    const double a = ease_alpha(dt_s, half_life_s);
    return current + (target - current) * a;
}

void Zoom::set(double value) {
    if (std::isfinite(value)) target = std::clamp(value, min, max);
}

void Zoom::tick(double dt_s) {
    if (!std::isfinite(dt_s) || dt_s <= 0.0) return;
    current = half_life_s > 0.0 ? current + (target - current) * ease_alpha(dt_s, half_life_s) : target;
    if (std::abs(current - target) < 1e-4) current = target;
}

Vec2 zoom_focus_bounds(const Vec2& base_half_extent, double base_width, double render_width) {
    const double extra_half_width = std::max(0.0, render_width - base_width) * 0.5;
    return {std::max(0.0, base_half_extent.x - extra_half_width), std::max(0.0, base_half_extent.y - extra_half_width * 9.0 / 16.0)};
}

}  // namespace hanji
