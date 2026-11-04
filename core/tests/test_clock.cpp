// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <map>
#include <string>
#include <vector>

#include "hanji/clock.hpp"

using namespace hanji;

namespace {

/// A table with one action of the given durations in every heading, 32x32 cells in a row.
Action make_action(const std::string& name, const std::vector<int>& durations_ms, bool loop,
                   int contact = -1, int active_end = -1, double cycle_cm = 0.0,
                   const std::map<int, std::vector<std::string>>& events = {}) {
    Action a;
    a.name = name;
    a.loop = loop;
    a.contact_frame = contact;
    a.active_end_frame = active_end;
    a.cycle_cm = cycle_cm;
    for (int h = 0; h < kHeadingCount; ++h) {
        Clip& clip = a.clip(heading_at(h));
        clip.present = true;
        for (int i = 0; i < static_cast<int>(durations_ms.size()); ++i) {
            Frame f;
            f.source = IntRect{i * 32, h * 32, 32, 32};
            f.duration_ms = durations_ms[static_cast<std::size_t>(i)];
            if (auto it = events.find(i); it != events.end()) f.events = it->second;
            clip.frames.push_back(f);
        }
    }
    return a;
}

ClipTable make_table(std::vector<Action> actions) {
    ClipTable t;
    t.image = "x.png";
    t.image_w = 32 * 16;
    t.image_h = 32 * 8;
    t.cell_w = t.cell_h = 32;
    t.pivot = {16, 30};
    t.actions = std::move(actions);
    REQUIRE(validate(t).ok());
    return t;
}

std::vector<std::string> names(const std::vector<FiredEvent>& events) {
    std::vector<std::string> out;
    for (const auto& e : events) out.push_back(e.name);
    return out;
}

}  // namespace

TEST_CASE("timeline: native playback follows the authored millisecond durations") {
    const Action a = make_action("idle", {100, 200, 100, 100}, true);
    const ActionTimeline t(a, Heading::E);
    CHECK(t.native_s() == doctest::Approx(0.5));
    CHECK(t.position_native(0.0, 1.0, true) == doctest::Approx(0.0));
    CHECK(t.position_native(0.05, 1.0, true) == doctest::Approx(0.5));
    CHECK(t.position_native(0.1, 1.0, true) == doctest::Approx(1.0));
    CHECK(t.position_native(0.2, 1.0, true) == doctest::Approx(1.5));   // halfway through the 200 ms frame
    CHECK(t.position_native(0.35, 1.0, true) == doctest::Approx(2.5));   // frame 2 starts at 0.3 s
    CHECK(t.position_native(0.6, 1.0, true) == doctest::Approx(4.0 + 1.0));  // second cycle, frame 1
    CHECK(t.position_native(0.1, 2.0, true) == doctest::Approx(1.5));   // double speed
    CHECK(t.position_native(9.0, 1.0, false) == doctest::Approx(4.0));  // one-shots stop at the end
}

TEST_CASE("re-timing: the contact frame begins exactly at startup, at any attack speed") {
    // Seven frames; contact at 3, active end at 5. Durations deliberately uneven.
    const Action a = make_action("jab", {50, 80, 40, 30, 30, 120, 150}, false, 3, 5);
    const ActionTimeline t(a, Heading::E);
    for (double speed : {0.5, 1.0, 2.0}) {
        CAPTURE(speed);
        const GameplayTiming timing{0.20 / speed, 0.10 / speed, 0.30 / speed};
        // Just before startup: still before the contact frame; at startup: the contact frame.
        CHECK(t.position_retimed(timing.startup_s - 1e-6, timing, 3, 5) < 3.0);
        CHECK(t.position_retimed(timing.startup_s, timing, 3, 5) == doctest::Approx(3.0));
        CHECK(static_cast<int>(t.position_retimed(timing.startup_s, timing, 3, 5) + 1e-9) == 3);
        // At the end of the active window: the active-end frame.
        CHECK(t.position_retimed(timing.startup_s + timing.active_s, timing, 3, 5) == doctest::Approx(5.0));
        CHECK(t.position_retimed(timing.total_s(), timing, 3, 5) == doctest::Approx(7.0));
    }
}

TEST_CASE("re-timing keeps the authored proportions inside each window") {
    // Startup window holds frames 0 (100 ms) and 1 (300 ms): frame 1 begins a quarter into the startup.
    const Action a = make_action("kick", {100, 300, 50, 50, 200}, false, 2, 3);
    const ActionTimeline t(a, Heading::E);
    const GameplayTiming timing{0.4, 0.1, 0.2};
    CHECK(t.position_retimed(0.1 - 1e-6, timing, 2, 3) < 1.0);
    CHECK(t.position_retimed(0.1, timing, 2, 3) == doctest::Approx(1.0));
    CHECK(t.position_retimed(0.25, timing, 2, 3) == doctest::Approx(1.5));
    // Positions only grow.
    double previous = -1.0;
    for (int i = 0; i <= 700; ++i) {
        const double p = t.position_retimed(i / 1000.0, timing, 2, 3);
        CHECK(p >= previous);
        previous = p;
    }
}

TEST_CASE("player: frame events fire once each, in order, whatever the step size") {
    const ClipTable table = make_table({make_action("jab", {50, 50, 50, 50}, false, 2, 3, 0.0,
                                                    {{0, {"whoosh"}}, {2, {"hit"}}, {3, {"recover"}}})});
    Player p(table);
    REQUIRE(p.play("jab"));
    CHECK(names(p.events()) == std::vector<std::string>{"whoosh"});  // frame 0 begins at play
    p.advance(0.03);
    CHECK(p.events().empty());
    p.advance(0.2);  // a hitch: frames 1, 2 and 3 all begin in one step
    CHECK(names(p.events()) == std::vector<std::string>{"hit", "recover"});
    CHECK(p.finished());
    p.advance(0.1);
    CHECK(p.events().empty());
}

TEST_CASE("player: re-timed actions raise the contact event on the first active gameplay tick") {
    const ClipTable table = make_table({make_action("jab", {50, 80, 40, 30, 30, 120, 150}, false, 3, 5, 0.0,
                                                    {{3, {"hit"}}})});
    for (double speed : {0.5, 1.0, 2.0}) {
        CAPTURE(speed);
        Player p(table);
        PlayOptions o;
        o.timing = GameplayTiming{0.2 / speed, 0.1 / speed, 0.3 / speed};
        REQUIRE(p.play("jab", o));
        // Step at 60 Hz until the startup ends; the hit fires on the tick that crosses it, not later.
        const double tick = 1.0 / 60.0;
        int ticks = 0;
        bool fired = false;
        while (!fired && ticks < 1000) {
            p.advance(tick);
            ++ticks;
            fired = !p.events().empty();
            if (p.gameplay_elapsed_s() + 1e-9 < o.timing->startup_s) CHECK_FALSE(fired);
        }
        CHECK(fired);
        CHECK(p.gameplay_elapsed_s() >= o.timing->startup_s - 1e-9);
        CHECK(p.gameplay_elapsed_s() < o.timing->startup_s + tick);
        CHECK(p.frame() == 3);
        CHECK(p.in_active_window());
    }
}

TEST_CASE("player: hit-stop freezes the clock, never re-fires events, and restores itself") {
    const ClipTable table = make_table({make_action("jab", {100, 100, 100, 100}, false, 1, 2, 0.0,
                                                    {{1, {"hit"}}, {2, {"recover"}}})});
    Player p(table);
    REQUIRE(p.play("jab"));
    p.advance(0.1);
    CHECK(names(p.events()) == std::vector<std::string>{"hit"});
    HitStopPolicy stop;
    stop.duration_s = 0.07;
    stop.time_scale = 0.0;
    p.hit_stop(stop);
    CHECK(p.hit_stopped());
    CHECK(p.dilation() == 0.0);
    for (int i = 0; i < 4; ++i) {
        p.advance(1.0 / 60.0);
        CHECK(p.events().empty());
        CHECK(p.frame() == 1);
    }
    p.advance(0.02);  // crosses the end of the stop: 0.07 - 4/60 frozen, the rest runs
    CHECK_FALSE(p.hit_stopped());
    CHECK(p.dilation() == 1.0);
    CHECK(p.gameplay_elapsed_s() == doctest::Approx(0.1 + (0.02 - (0.07 - 4.0 / 60.0))));
    p.advance(0.1);
    CHECK(names(p.events()) == std::vector<std::string>{"recover"});
}

TEST_CASE("hit-stop merge policies: extend, restart, ignore") {
    const ClipTable table = make_table({make_action("idle", {100}, true)});
    HitStopPolicy first;
    first.duration_s = 0.1;
    HitStopPolicy second;
    second.duration_s = 0.05;

    auto remaining_after = [&](HitStopPolicy::Merge merge) {
        Player p(table);
        p.play("idle");
        p.hit_stop(first);
        p.advance(0.08);  // 0.02 left
        second.merge = merge;
        p.hit_stop(second);
        double frozen = 0.0;
        while (p.hit_stopped()) {
            p.advance(0.001);
            frozen += 0.001;
        }
        return frozen;
    };
    CHECK(remaining_after(HitStopPolicy::Merge::Extend) == doctest::Approx(0.05).epsilon(0.05));
    CHECK(remaining_after(HitStopPolicy::Merge::Restart) == doctest::Approx(0.05).epsilon(0.05));
    CHECK(remaining_after(HitStopPolicy::Merge::Ignore) == doctest::Approx(0.02).epsilon(0.1));

    // A slow-motion stop advances time at its scale.
    Player p(table);
    p.play("idle");
    HitStopPolicy crawl;
    crawl.duration_s = 0.1;
    crawl.time_scale = 0.05;
    p.hit_stop(crawl);
    p.advance(0.1);
    CHECK(p.gameplay_elapsed_s() == doctest::Approx(0.005));
}

TEST_CASE("stride clock: frames follow distance, footsteps land on their frames, teleports do not step") {
    // Six frames over a 180 cm cycle: one frame per 30 cm; footsteps on frames 0 and 3.
    const ClipTable table = make_table({make_action("run", {0, 0, 0, 0, 0, 0}, true, -1, -1, 180.0,
                                                    {{0, {"footstep"}}, {3, {"footstep"}}})});
    Player p(table);
    REQUIRE(p.play("run"));
    CHECK(names(p.events()) == std::vector<std::string>{"footstep"});
    p.advance(0.016, 29.0);
    CHECK(p.frame() == 0);
    CHECK(p.events().empty());
    p.advance(0.016, 1.0);
    CHECK(p.frame() == 1);
    p.advance(0.016, 60.0);
    CHECK(p.frame() == 3);
    CHECK(names(p.events()) == std::vector<std::string>{"footstep"});
    // Standing still: time passes, no frame changes, no events. This is what stops feet sliding.
    p.advance(1.0, 0.0);
    CHECK(p.frame() == 3);
    CHECK(p.events().empty());
    // A teleport restarts the cycle quietly.
    p.advance(0.016, 500.0);
    CHECK(p.frame() == 0);
    CHECK(p.events().empty());
    // Hit-stop slows travel too.
    HitStopPolicy stop;
    stop.duration_s = 1.0;
    p.hit_stop(stop);
    p.advance(0.016, 90.0);
    CHECK(p.frame() == 0);
}

TEST_CASE("start phase carries a cycle across a change of action") {
    const ClipTable table = make_table({make_action("walk", {0, 0, 0, 0, 0, 0, 0, 0}, true, -1, -1, 160.0),
                                        make_action("run", {0, 0, 0, 0, 0, 0, 0, 0}, true, -1, -1, 240.0)});
    Player p(table);
    p.play("walk");
    p.advance(0.1, 100.0);  // 100/160 of the cycle: frame 5
    CHECK(p.frame() == 5);
    PlayOptions o;
    o.start_phase = p.phase();
    p.play("run", o);
    CHECK(p.frame() == 5);
    CHECK(p.phase() == doctest::Approx(100.0 / 160.0));
}

TEST_CASE("headings change the drawn frame, not the time") {
    const ClipTable table = make_table({make_action("idle", {100, 100, 100}, true)});
    Player p(table);
    p.set_heading(Heading::E);
    p.play("idle");
    p.advance(0.15);
    CHECK(p.frame() == 1);
    CHECK(p.current().source.y == heading_index(Heading::E) * 32);
    p.set_heading(Heading::NW);
    CHECK(p.frame() == 1);
    CHECK(p.current().source.y == heading_index(Heading::NW) * 32);
}

TEST_CASE("loops fire at most one cycle of events per advance") {
    const ClipTable table = make_table({make_action("idle", {10, 10}, true, -1, -1, 0.0, {{0, {"a"}}, {1, {"b"}}})});
    Player p(table);
    p.play("idle");
    p.advance(10.0);  // 500 cycles in one hitch
    CHECK(p.events().size() == 2);
}

TEST_CASE("play refuses what it cannot honour") {
    const ClipTable table = make_table({make_action("idle", {100}, true), make_action("jab", {100, 100}, false, 1)});
    Player p(table);
    CHECK_FALSE(p.play("missing"));
    PlayOptions bad;
    bad.timing = GameplayTiming{0.1, 0.1, 0.1};
    CHECK_FALSE(p.play("idle", bad));  // no contact frame to re-time around
    bad.timing = GameplayTiming{0.0, 0.1, 0.1};
    CHECK_FALSE(p.play("jab", bad));   // invalid windows
    CHECK(p.action() == nullptr);
    CHECK(p.play("jab"));
}

TEST_CASE("playing the running loop again continues it; restart_if_same restarts it") {
    const ClipTable table = make_table({make_action("idle", {100, 100}, true)});
    Player p(table);
    p.play("idle");
    p.advance(0.15);
    const auto id = p.play_id();
    p.play("idle");
    CHECK(p.frame() == 1);
    CHECK(p.play_id() == id);
    PlayOptions restart;
    restart.restart_if_same = true;
    p.play("idle", restart);
    CHECK(p.frame() == 0);
    CHECK(p.play_id() == id + 1);
}
