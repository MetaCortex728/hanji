// SPDX-License-Identifier: Apache-2.0
// Ported from the October prototype's automation tests (prototype/Tests), plus hysteresis.
#include <doctest.h>

#include <cmath>

#include "hanji/geometry.hpp"

using namespace hanji;

namespace {

constexpr double kPi = 3.14159265358979323846;

bool near(double a, double b, double eps) { return std::abs(a - b) <= eps; }
bool near(const Vec3& a, const Vec3& b, double eps) { return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps); }

}  // namespace

TEST_CASE("view basis matches Unreal's FRotationMatrix for a quarter view") {
    const ViewBasis b = view_basis({-30.0, 45.0, 0.0});
    const double c30 = std::cos(kPi / 6), s30 = 0.5, h = std::sqrt(0.5);
    CHECK(near(b.forward, Vec3{c30 * h, c30 * h, -s30}, 1e-12));
    CHECK(near(b.right, Vec3{-h, h, 0.0}, 1e-12));
    CHECK(near(b.up, Vec3{s30 * h, s30 * h, c30}, 1e-12));
}

TEST_CASE("headings: the nearest screen sector, not world yaw (prototype ScreenDirections)") {
    for (double pitch : {-30.0, -35.0})
        for (double yaw : {0.0, 45.0, 137.0}) {
            const ViewRotation view{pitch, yaw, 0.0};
            const ViewBasis b = view_basis(view);
            for (int degrees = 0; degrees < 360; ++degrees) {
                CAPTURE(pitch);
                CAPTURE(yaw);
                CAPTURE(degrees);
                const double a = degrees * kPi / 180.0;
                const Vec3 ground = ground_from_screen({std::cos(a), std::sin(a)}, view);
                const double px = dot(ground, b.right), py = dot(ground, b.up);
                const double n = std::sqrt(px * px + py * py);
                CHECK(near(px / n, std::cos(a), 1e-5));
                CHECK(near(py / n, std::sin(a), 1e-5));
                CHECK(heading_index(select_heading(ground, view, Heading::E)) == ((degrees + 22) / 45) % 8);
            }
        }
    CHECK(select_heading({}, {-30, 45, 0}, Heading::S) == Heading::S);
    CHECK(select_heading({0, 0, 100}, {-30, 45, 0}, Heading::N) == Heading::N);
}

TEST_CASE("heading hysteresis holds the previous heading near a boundary") {
    const ViewRotation view{-30, 45, 0};
    auto at = [&](double degrees) {
        const double a = degrees * kPi / 180.0;
        return ground_from_screen({std::cos(a), std::sin(a)}, view);
    };
    // 23.5 degrees is past the e/ne boundary by 1 degree.
    CHECK(select_heading(at(23.5), view, Heading::E, 0.0) == Heading::NE);
    CHECK(select_heading(at(23.5), view, Heading::E, 3.0) == Heading::E);
    CHECK(select_heading(at(26.0), view, Heading::E, 3.0) == Heading::NE);
    // Across the wrap at 0/360.
    CHECK(select_heading(at(-24.0), view, Heading::E, 3.0) == Heading::E);
    CHECK(select_heading(at(-26.0), view, Heading::E, 3.0) == Heading::SE);
    // A noisy sweep around a boundary flips at most once with hysteresis.
    Heading h = Heading::E;
    int flips = 0;
    for (int i = 0; i < 400; ++i) {
        const double noise = std::sin(i * 1.7) * 1.5 + std::sin(i * 0.31) * 0.8;  // within +-2.3 degrees
        const Heading next = select_heading(at(22.5 + noise), view, h, 3.0);
        flips += next != h ? 1 : 0;
        h = next;
    }
    CHECK(flips <= 1);
    int raw_flips = 0;
    h = Heading::E;
    for (int i = 0; i < 400; ++i) {
        const double noise = std::sin(i * 1.7) * 1.5 + std::sin(i * 0.31) * 0.8;
        const Heading next = select_heading(at(22.5 + noise), view, h, 0.0);
        raw_flips += next != h ? 1 : 0;
        h = next;
    }
    CHECK(raw_flips > 10);  // without hysteresis the same input flickers
}

TEST_CASE("turning into an attack takes the shortest screen arc (prototype StrikeEntryDirection)") {
    for (int from = 0; from < 8; ++from)
        for (int to = 0; to < 8; ++to) {
            CHECK(turn_heading(heading_at(from), heading_at(to), 0.0) == heading_at(from));
            CHECK(turn_heading(heading_at(from), heading_at(to), 1.0) == heading_at(to));
            for (int frame = 1; frame < 4; ++frame) {
                const int delta = (to - from + 12) % 8 - 4;
                const double t = frame / 4.0;
                const int expected = (static_cast<int>(std::floor(from + delta * t * t * (3 - 2 * t) + .5)) + 8) % 8;
                CHECK(heading_index(turn_heading(heading_at(from), heading_at(to), t)) == expected);
            }
        }
}

TEST_CASE("contact offsets land on screen where the pixel is (prototype ContactOffset)") {
    for (double density : {1.0, 2.0, 3.25}) {
        const ViewRotation view{-30, 45, 0};
        const ViewBasis b = view_basis(view);
        const Vec3 offset = contact_offset({40.5, 12.25}, {32, 56}, density, view);
        CHECK(near(dot(offset, b.right), 8.5 * density, 1e-6));
        CHECK(near(dot(offset, b.up), 43.75 * density, 1e-6));
        const Vec3 front{75, 20, 180};
        const Vec3 projected = onto_front_plane(offset, front, b.forward);
        const Vec3 moved = projected - offset;
        const Vec3 cross{moved.y * b.forward.z - moved.z * b.forward.y, moved.z * b.forward.x - moved.x * b.forward.z,
                         moved.x * b.forward.y - moved.y * b.forward.x};
        CHECK(length(cross) < 1e-6);
        CHECK(std::abs(dot(projected - front, b.forward)) < 1e-6);
    }
}

TEST_CASE("ground clearance moves only in depth (prototype GroundClearanceOffset)") {
    for (double pitch : {-30.0, -35.0}) {
        const ViewRotation view{pitch, 45, 0};
        const ViewBasis b = view_basis(view);
        const Vec3 offset = ground_clearance_offset(6.0, 3.25, view);
        CHECK(near(dot(offset, b.right), 0.0, 1e-8));
        CHECK(near(dot(offset, b.up), 0.0, 1e-8));
        CHECK(near(offset.z - 6.0 * 3.25 * b.up.z, 0.5, 1e-8));
    }
}

TEST_CASE("view-plane snapping (prototype RenderPlaneSnap)") {
    const Vec3 origin{17.25, -8.5, 23.75};
    for (double pitch : {-30.0, -35.0}) {
        const ViewRotation view{pitch, 45, 0};
        const ViewBasis b = view_basis(view);
        constexpr double q = 3.25;
        for (int step = -100; step <= 100; ++step) {
            const Vec3 point = origin + b.right * (step * 0.17) + b.up * (step * 0.29) + b.forward * 47.25;
            const Vec3 snapped = snap_view_plane(point, origin, view, q);
            for (const Vec3& axis : {b.right, b.up}) {
                const double pixel = dot(snapped - origin, axis) / q;
                CHECK(near(pixel, std::round(pixel), 1e-5));
                CHECK(std::abs(dot(snapped - point, axis)) <= q * 0.5 + 1e-5);
            }
            CHECK(near(dot(snapped - point, b.forward), 0.0, 1e-5));
            CHECK(near(snap_view_plane(snapped, origin, view, q), snapped, 1e-5));
            const Vec3 ground = snap_ground_anchor(point, origin, view, q);
            CHECK(near(ground.z, point.z, 1e-5));
            CHECK(near(dot(ground - snapped, b.right), 0.0, 1e-5));
            CHECK(near(dot(ground - snapped, b.up), 0.0, 1e-5));
        }
    }
    CHECK(near(snap_view_plane(origin, {}, {0, 0, 0}, 0.0), origin, 0.0));
}

TEST_CASE("sprite quads keep their on-screen size at every tilt") {
    for (double pitch : {-30.0, -35.0, -55.0})
        for (double yaw : {0.0, 45.0, 137.0}) {
            const ViewRotation view{pitch, yaw, 0.0};
            const ViewBasis b = view_basis(view);
            for (double tilt : {0.0, 10.0, -pitch / 2, -pitch, 80.0}) {
                CAPTURE(pitch);
                CAPTURE(yaw);
                CAPTURE(tilt);
                const SpriteQuad q = sprite_quad(view, tilt);
                CHECK(q.tilt_deg <= -pitch + 1e-12);
                CHECK(near(length(q.up), 1.0, 1e-12));
                CHECK(near(dot(q.up, q.right), 0.0, 1e-12));
                CHECK(near(dot(q.normal, q.up), 0.0, 1e-12));
                // Width runs along screen right; the scaled height covers one screen unit of height.
                CHECK(near(q.right, b.right, 1e-12));
                CHECK(near(dot(q.up * q.height_scale, b.up), 1.0, 1e-12));
                // The quad faces the camera side.
                CHECK(dot(q.normal, b.forward) < 0.0);
            }
            // Upright stands vertical; full tilt faces the camera.
            CHECK(near(sprite_quad(view, 0.0).up, Vec3{0, 0, 1}, 1e-12));
            CHECK(near(sprite_quad(view, -pitch).up, b.up, 1e-12));
            CHECK(near(sprite_quad(view, -pitch).height_scale, 1.0, 1e-12));
        }
}

TEST_CASE("follow is frame-rate independent and never overshoots (prototype FollowTiming)") {
    const Vec3 start{-40, 20, 80}, target{160, 220, 180};
    CHECK(near(follow(start, target, 0.08, 0.08), (start + target) * 0.5, 1e-4));
    CHECK(near(follow(start, target, 0.0, 0.08), start, 0.0));
    CHECK(near(follow(start, target, -0.1, 0.08), start, 0.0));
    CHECK(near(follow(start, target, 0.01, 0.0), target, 0.0));
    const Vec3 expected = follow(start, target, 1.0, 0.08);
    for (int rate : {30, 60, 120}) {
        Vec3 p = start;
        for (int i = 0; i < rate; ++i) p = follow(p, target, 1.0 / rate, 0.08);
        CHECK(near(p, expected, 1e-4));
    }
    const Vec3 spike = follow(start, target, 0.5, 0.08);
    CHECK(length(spike - target) < length(start - target));
    CHECK(dot(spike - start, target - spike) >= 0.0);
}

TEST_CASE("screen-relative input (prototype ScreenRelativeInput)") {
    const double h = std::sqrt(0.5);
    CHECK(near(ground_direction({0, 1}, 45), Vec3{h, h, 0}, 1e-4));
    CHECK(near(ground_direction({1, 0}, 45), Vec3{-h, h, 0}, 1e-4));
    CHECK(near(length(ground_direction({1, 1}, 45)), 1.0, 1e-4));
    CHECK(near(length(ground_direction({0.3, 0.4}, 45)), 0.5, 1e-4));
    CHECK(near(ground_direction({0, 0}, 45), Vec3{}, 0.0));
    for (double yaw : {0.0, 45.0, 90.0, -135.0}) CHECK(ground_direction({0.6, 0.8}, yaw).z == 0.0);
}

TEST_CASE("zoom steps ease, settle exactly and widen the view (prototype ZoomScale)") {
    Zoom zoom;
    CHECK(zoom.width(2080) == 2080.0);
    zoom.set(2);
    CHECK(zoom.target == 1.1);
    zoom.tick(0);
    CHECK(zoom.current == 1.0);
    zoom.tick(0.06);
    CHECK(near(zoom.current, 1.05, 1e-8));
    for (int rate : {30, 60, 120}) {
        Zoom z;
        z.set(1.1);
        for (int i = 0; i < rate; ++i) z.tick(1.0 / rate);
        CHECK(z.current == 1.1);
        CHECK(near(z.width(2080), 2288.0, 1e-8));
        const double density = z.width(2080) / 640;
        const ViewRotation view{-30, 45, 0};
        const Vec3 point{39.17, -21.23, 0}, origin{8.3, 4.5, 7.1};
        const Vec3 snapped = snap_ground_anchor(point, origin, view, density);
        CHECK(snapped.z == doctest::Approx(point.z).epsilon(1e-12));
        const ViewBasis b = view_basis(view);
        for (const Vec3& axis : {b.right, b.up}) {
            const double pixel = dot(snapped - origin, axis) / density;
            CHECK(near(pixel, std::round(pixel), 1e-6));
        }
        const Vec2 base{520, 270};
        const Vec2 bounds = zoom_focus_bounds(base, 2080, z.width(2080));
        CHECK(near(bounds.x + z.width(2080) * .5, base.x + 1040, 1e-6));
        CHECK(near(bounds.y + z.width(2080) * 9 / 32, base.y + 2080.0 * 9 / 32, 1e-6));
        z.set(.8);
        z.tick(2);
        CHECK(z.current == .9);
        CHECK(near(z.width(2080), 1872.0, 1e-8));
        const Vec2 close = zoom_focus_bounds(base, 2080, z.width(2080));
        CHECK(close.x == base.x);
        CHECK(close.y == base.y);
        const Vec2 tiny = zoom_focus_bounds({2, 2}, 2080, 2288);
        CHECK(tiny.x == 0.0);
        CHECK(tiny.y == 0.0);
        z.set(1);
        z.tick(2);
        CHECK(z.width(2080) == 2080.0);
    }
    zoom.set(std::nan(""));
    CHECK(zoom.target == 1.1);
}
