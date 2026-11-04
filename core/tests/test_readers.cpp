// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <string>

#include "hanji/readers.hpp"
#include "support.hpp"

using namespace hanji;
using hanji_test::read_fixture;

namespace {

std::string joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const auto& line : lines) out += line + "\n";
    return out;
}

}  // namespace

TEST_CASE("headings: names, parsing and mirrors") {
    CHECK(heading_name(Heading::E) == "e");
    CHECK(heading_name(Heading::SE) == "se");
    CHECK(parse_heading("NW") == Heading::NW);
    CHECK_FALSE(parse_heading("north").has_value());
    CHECK(mirror_of(Heading::E) == Heading::W);
    CHECK(mirror_of(Heading::NE) == Heading::NW);
    CHECK(mirror_of(Heading::SW) == Heading::SE);
    CHECK(mirror_of(Heading::N) == Heading::N);
    CHECK(mirror_of(Heading::S) == Heading::S);
    for (int h = 0; h < kHeadingCount; ++h) CHECK(mirror_of(mirror_of(heading_at(h))) == heading_at(h));
}

TEST_CASE("pose-atlas contract v2: actions, headings, cells, durations and keys") {
    const LoadResult r = load_pose_contract(read_fixture("pose-atlas-v2/pose-contract.json"));
    INFO(joined(r.diagnostics.errors));
    REQUIRE(r.diagnostics.ok());
    REQUIRE(r.table);
    const ClipTable& t = *r.table;
    CHECK(t.image == "atlas.png");
    CHECK(t.cell_w == 256);
    CHECK(t.cell_h == 256);
    CHECK(t.image_w == 10 * 256);
    CHECK(t.image_h == 8 * 256);
    CHECK(t.pivot == IntPoint{128, 208});
    CHECK(t.metres_per_pixel > 0.0);
    REQUIRE(t.actions.size() == 2);

    const Action* idle = t.find("idle");
    const Action* run = t.find("run");
    REQUIRE(idle);
    REQUIRE(run);
    CHECK(idle->frame_count() == 4);
    CHECK(run->frame_count() == 6);
    CHECK(idle->loop);
    CHECK_FALSE(idle->has_contact());

    // Rows are views in heading order, columns are poses; run starts after idle's four columns.
    const Frame& f = run->clip(Heading::NW).frames[2];
    CHECK(f.source == IntRect{(4 + 2) * 256, 3 * 256, 256, 256});
    CHECK(f.key.rfind("run.nw@", 0) == 0);
    CHECK_FALSE(f.mirror_x);
    for (int h = 0; h < kHeadingCount; ++h) {
        CHECK(idle->total_ms(heading_at(h)) == idle->total_ms(Heading::E));
        CHECK(run->total_ms(heading_at(h)) == run->total_ms(Heading::E));
    }
    CHECK(idle->total_ms(Heading::E) > 0);
}

TEST_CASE("pose-atlas contract v1 still loads, without frame keys") {
    const LoadResult r = load_pose_contract(read_fixture("pose-atlas-v1/pose-contract.json"));
    INFO(joined(r.diagnostics.errors));
    REQUIRE(r.diagnostics.ok());
    const Action* run = r.table->find("run");
    REQUIRE(run);
    CHECK(run->frame_count() == 8);
    CHECK(run->clip(Heading::SE).frames[7].source == IntRect{15 * 256, 7 * 256, 256, 256});
    CHECK(run->clip(Heading::SE).frames[7].key.empty());
}

TEST_CASE("the contract reader and the Aseprite reader agree on the same pose-atlas output") {
    for (const char* dir : {"pose-atlas-v1", "pose-atlas-v2"}) {
        CAPTURE(dir);
        const LoadResult contract = load_pose_contract(read_fixture(std::string(dir) + "/pose-contract.json"));
        const LoadResult aseprite = load_aseprite_json(read_fixture(std::string(dir) + "/atlas.json"));
        INFO(joined(aseprite.diagnostics.errors));
        REQUIRE(contract.diagnostics.ok());
        REQUIRE(aseprite.diagnostics.ok());
        const ClipTable& a = *contract.table;
        const ClipTable& b = *aseprite.table;
        CHECK(a.image == b.image);
        CHECK(a.image_w == b.image_w);
        CHECK(a.image_h == b.image_h);
        CHECK(a.cell_w == b.cell_w);
        CHECK(a.pivot == b.pivot);
        REQUIRE(a.actions.size() == b.actions.size());
        for (const Action& action : a.actions) {
            const Action* other = b.find(action.name);
            REQUIRE(other);
            for (int h = 0; h < kHeadingCount; ++h) {
                const Clip& x = action.clip(heading_at(h));
                const Clip& y = other->clip(heading_at(h));
                REQUIRE(x.frames.size() == y.frames.size());
                for (std::size_t i = 0; i < x.frames.size(); ++i) {
                    CHECK(x.frames[i].source == y.frames[i].source);
                    CHECK(x.frames[i].duration_ms == y.frames[i].duration_ms);
                }
            }
        }
    }
}

TEST_CASE("Aseprite JSON: tags give actions and headings, slices give the pivot and boxes") {
    // Five headings drawn (e, se, s, nw, n); w, sw and ne come from their mirrors. A tag without a heading
    // suffix spreads to all eight.
    const std::string text = R"({
      "frames": [
        {"filename": "a", "frame": {"x": 0,  "y": 0, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 100},
        {"filename": "b", "frame": {"x": 32, "y": 0, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 120},
        {"filename": "c", "frame": {"x": 64, "y": 0, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 100},
        {"filename": "d", "frame": {"x": 96, "y": 0, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 120},
        {"filename": "e", "frame": {"x": 0,  "y": 48, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 80},
        {"filename": "f", "frame": {"x": 32, "y": 48, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 80},
        {"filename": "g", "frame": {"x": 64, "y": 48, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 80},
        {"filename": "h", "frame": {"x": 96, "y": 48, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 80},
        {"filename": "i", "frame": {"x": 0,  "y": 96, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 90},
        {"filename": "j", "frame": {"x": 32, "y": 96, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 90},
        {"filename": "k", "frame": {"x": 64, "y": 96, "w": 32, "h": 48}, "sourceSize": {"w": 32, "h": 48}, "duration": 200}
      ],
      "meta": {
        "image": "hero.png", "size": {"w": 128, "h": 144},
        "frameTags": [
          {"name": "jab.e",  "from": 0, "to": 1},
          {"name": "jab.se", "from": 2, "to": 3},
          {"name": "jab.s",  "from": 4, "to": 5},
          {"name": "jab.nw", "from": 6, "to": 7},
          {"name": "jab.n",  "from": 8, "to": 9},
          {"name": "pose",   "from": 10, "to": 10}
        ],
        "slices": [
          {"name": "foot", "keys": [{"frame": 0, "bounds": {"x": 0, "y": 0, "w": 32, "h": 48}, "pivot": {"x": 16, "y": 44}}]},
          {"name": "hit:fist", "keys": [
            {"frame": 1, "bounds": {"x": 20, "y": 10, "w": 10, "h": 6}},
            {"frame": 2, "bounds": {"x": 0, "y": 0, "w": 0, "h": 0}}
          ]}
        ]
      }
    })";
    const LoadResult r = load_aseprite_json(text);
    INFO(joined(r.diagnostics.errors));
    REQUIRE(r.diagnostics.ok());
    const ClipTable& t = *r.table;
    CHECK(t.image == "hero.png");
    CHECK(t.pivot == IntPoint{16, 44});
    CHECK(t.cell_w == 32);

    const Action* jab = t.find("jab");
    REQUIRE(jab);
    CHECK(jab->frame_count() == 2);
    CHECK(jab->clip(Heading::E).frames[1].duration_ms == 120);
    CHECK_FALSE(jab->clip(Heading::E).frames[0].mirror_x);
    CHECK(jab->clip(Heading::W).frames[0].mirror_x);
    CHECK(jab->clip(Heading::W).frames[0].source == jab->clip(Heading::E).frames[0].source);
    CHECK(jab->clip(Heading::SW).frames[1].source == jab->clip(Heading::SE).frames[1].source);
    CHECK(jab->clip(Heading::NE).frames[0].source == jab->clip(Heading::NW).frames[0].source);
    CHECK(jab->clip(Heading::NE).frames[0].mirror_x);
    CHECK_FALSE(jab->clip(Heading::N).frames[0].mirror_x);

    // The fist box is on frame 1 of jab.e only (the empty key at frame 2 ends it); mirrored into w.
    REQUIRE(jab->clip(Heading::E).frames[1].boxes.size() == 1);
    const Hitbox& box = jab->clip(Heading::E).frames[1].boxes[0];
    CHECK(box.name == "fist");
    CHECK(box.kind == BoxKind::Attack);
    CHECK(box.box == IntRect{20, 10, 10, 6});
    CHECK(jab->clip(Heading::E).frames[0].boxes.empty());
    REQUIRE(jab->clip(Heading::W).frames[1].boxes.size() == 1);
    CHECK(jab->clip(Heading::W).frames[1].boxes[0].box == IntRect{32 - 20 - 10, 10, 10, 6});

    const Action* pose = t.find("pose");
    REQUIRE(pose);
    for (int h = 0; h < kHeadingCount; ++h) CHECK(pose->clip(heading_at(h)).present);
}

TEST_CASE("Aseprite JSON: a heading with no drawn mirror is reported, not invented") {
    const LoadResult r = load_aseprite_json(R"({
      "frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}, "duration": 100},
                 {"filename": "b", "frame": {"x": 8, "y": 0, "w": 8, "h": 8}, "duration": 100}],
      "meta": {"image": "x.png", "size": {"w": 16, "h": 8},
               "frameTags": [{"name": "walk.e", "from": 0, "to": 0}, {"name": "walk.s", "from": 1, "to": 1}]}
    })");
    const std::string all = joined(r.diagnostics.errors);
    CHECK(all.find("lacks headings: ne, n, nw, sw, se") != std::string::npos);
    LoadOptions strict;
    strict.allow_mirror = false;
    const LoadResult no_mirror = load_aseprite_json(R"({
      "frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}, "duration": 100}],
      "meta": {"image": "x.png", "size": {"w": 8, "h": 8}, "frameTags": [{"name": "walk.e", "from": 0, "to": 0}]}
    })", strict);
    CHECK(joined(no_mirror.diagnostics.errors).find("lacks headings: ne, n, nw, w, sw, s, se") != std::string::npos);
}

TEST_CASE("validation reports missing headings, unequal frame counts and bad landmarks") {
    ClipTable t;
    t.image = "x.png";
    t.image_w = 64;
    t.image_h = 64;
    t.cell_w = 32;
    t.cell_h = 32;
    t.pivot = {16, 30};
    Action a;
    a.name = "jab";
    a.contact_frame = 3;
    a.active_end_frame = 2;
    for (int h = 0; h < kHeadingCount; ++h) {
        Clip& c = a.clip(heading_at(h));
        c.present = h != heading_index(Heading::N);
        const int n = h == heading_index(Heading::S) ? 3 : 2;
        if (c.present)
            for (int i = 0; i < n; ++i) c.frames.push_back(Frame{IntRect{0, 0, 32, 32}, 100, false, "", {}, {}});
    }
    t.actions.push_back(a);
    const Diagnostics d = validate(t);
    const std::string all = joined(d.errors);
    CHECK(all.find("lacks headings: n") != std::string::npos);
    CHECK(all.find("3 frames in heading s") != std::string::npos);
    CHECK(all.find("contact frame 3") != std::string::npos);
}

TEST_CASE("validation: no duration is an error only for time-driven actions") {
    ClipTable t;
    t.image = "x.png";
    t.image_w = 32;
    t.image_h = 32;
    Action a;
    a.name = "walk";
    for (int h = 0; h < kHeadingCount; ++h) {
        a.clip(heading_at(h)).present = true;
        a.clip(heading_at(h)).frames.push_back(Frame{IntRect{0, 0, 32, 32}, 0, false, "", {}, {}});
    }
    t.actions.push_back(a);
    CHECK_FALSE(validate(t).ok());
    t.actions[0].cycle_cm = 180.0;
    CHECK(validate(t).ok());
}

TEST_CASE("hanji-clips overlay adds landmarks, events and boxes, and the table round-trips") {
    LoadResult r = load_pose_contract(read_fixture("pose-atlas-v2/pose-contract.json"));
    REQUIRE(r.diagnostics.ok());
    ClipTable t = *r.table;
    const Diagnostics d = apply_hanji_overlay(t, R"({
      "format": "hanji-clips", "version": 1,
      "actions": [
        {"name": "run", "loop": false, "contact_frame": 2, "active_end_frame": 4,
         "events": {"0": ["footstep"], "2": ["hit", "whoosh"]},
         "hitboxes": [
           {"name": "fist", "kind": "attack", "frames": [2, 3], "box": [140, 120, 30, 20], "heading": "e"},
           {"name": "body", "kind": "hurt", "frames": "all", "box": [100, 80, 56, 128]}
         ]}
      ]
    })");
    INFO(joined(d.errors));
    REQUIRE(d.ok());
    const Action* run = t.find("run");
    REQUIRE(run);
    CHECK_FALSE(run->loop);
    CHECK(run->contact_frame == 2);
    CHECK(run->active_end_frame == 4);
    CHECK(run->clip(Heading::SW).frames[2].events == std::vector<std::string>{"hit", "whoosh"});
    CHECK(run->clip(Heading::E).frames[2].boxes.size() == 2);
    CHECK(run->clip(Heading::W).frames[2].boxes.size() == 1);  // the fist is only in heading e
    CHECK(run->clip(Heading::W).frames[5].boxes[0].name == "body");

    const LoadResult again = load_hanji_clips(write_hanji_clips(t));
    INFO(joined(again.diagnostics.errors));
    REQUIRE(again.diagnostics.ok());
    CHECK(write_hanji_clips(*again.table) == write_hanji_clips(t));
}

TEST_CASE("the overlay rejects actions the table lacks and frames out of range") {
    LoadResult r = load_pose_contract(read_fixture("pose-atlas-v2/pose-contract.json"));
    ClipTable t = *r.table;
    const Diagnostics d = apply_hanji_overlay(t, R"({"format": "hanji-clips", "version": 1, "actions": [
        {"name": "kick", "contact_frame": 1},
        {"name": "idle", "events": {"9": ["hit"]}}
    ]})");
    const std::string all = joined(d.errors);
    CHECK(all.find("action 'kick', which the table lacks") != std::string::npos);
    CHECK(all.find("events at frame '9'") != std::string::npos);
}

TEST_CASE("readers report malformed input instead of throwing") {
    CHECK_FALSE(load_pose_contract("{").diagnostics.ok());
    CHECK_FALSE(load_pose_contract(R"({"format": "something-else"})").diagnostics.ok());
    CHECK_FALSE(load_aseprite_json(R"({"frames": []})").diagnostics.ok());
    CHECK_FALSE(load_hanji_clips(R"({"format": "hanji-clips"})").diagnostics.ok());
}
