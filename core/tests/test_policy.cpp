// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <string>

#include "hanji/policy.hpp"
#include "hanji/readers.hpp"
#include "support.hpp"

using namespace hanji;

namespace {

std::string joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const auto& line : lines) out += line + "\n";
    return out;
}

}  // namespace

TEST_CASE("policies: states, timing and hit-stop read from JSON") {
    const PolicyLoad r = load_policies(R"({"format": "hanji-policies", "version": 1, "states": [
        {"state": "run", "stride": true, "keep_phase": true},
        {"state": "jab", "action": "jab_r", "lock_facing": true,
         "timing": {"startup_s": 0.2, "active_s": 0.1, "recovery_s": 0.3},
         "hit_stop": {"duration_s": 0.055, "time_scale": 0.1, "merge": "restart", "who": "both"}}
    ]})");
    INFO(joined(r.diagnostics.errors));
    REQUIRE(r.diagnostics.ok());
    const StatePolicy* run = r.table->find("run");
    REQUIRE(run);
    CHECK(run->action == "run");
    CHECK(run->stride);
    CHECK(run->keep_phase);
    const StatePolicy* jab = r.table->find("jab");
    REQUIRE(jab);
    CHECK(jab->action == "jab_r");
    CHECK(jab->lock_facing);
    REQUIRE(jab->timing);
    CHECK(jab->timing->total_s() == doctest::Approx(0.6));
    REQUIRE(jab->hit_stop);
    CHECK(jab->hit_stop->duration_s == doctest::Approx(0.055));
    CHECK(jab->hit_stop->time_scale == doctest::Approx(0.1));
    CHECK(jab->hit_stop->merge == HitStopPolicy::Merge::Restart);
    CHECK(jab->hit_stop->who == HitStopPolicy::Who::Both);

    const PlayOptions keep = play_options(*run, 0.4);
    CHECK(keep.start_phase == doctest::Approx(0.4));
    const PlayOptions timed = play_options(*jab, 0.4);
    CHECK(timed.start_phase == 0.0);
    CHECK(timed.timing.has_value());
}

TEST_CASE("policies are checked against the clip table") {
    LoadResult clips = load_pose_contract(hanji_test::read_fixture("pose-atlas-v2/pose-contract.json"));
    REQUIRE(clips.diagnostics.ok());
    const PolicyLoad r = load_policies(R"({"format": "hanji-policies", "version": 1, "states": [
        {"state": "run", "stride": true},
        {"state": "idle", "timing": {"startup_s": 0.1, "active_s": 0.1, "recovery_s": 0.1}},
        {"state": "kick"},
        {"state": "dash", "hit_stop": {"duration_s": 0.1, "merge": "sometimes"}}
    ]})", &*clips.table);
    const std::string all = joined(r.diagnostics.errors);
    CHECK(all.find("'run' is a stride state but action 'run' has no cycle distance") != std::string::npos);
    CHECK(all.find("'idle' is timed but action 'idle' has no contact frame") != std::string::npos);
    CHECK(all.find("action 'kick', which the clip table lacks") != std::string::npos);
    CHECK(all.find("merge 'sometimes'") != std::string::npos);
}
