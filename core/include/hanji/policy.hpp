// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hanji/clip.hpp"
#include "hanji/clock.hpp"

namespace hanji {

/// How one gameplay state uses the clock. The game's state machine (its own, or PaperZD's) decides which
/// state is current; this table says what that state means for facing, travel and hits, so those rules
/// are data shared by every character instead of code in each one.
struct StatePolicy {
    std::string state;                      ///< The game's name for the state ("idle", "run", "jab").
    std::string action;                     ///< Action in the clip table; defaults to the state's name.
    bool lock_facing = false;               ///< Keep the heading chosen when the state began (attacks).
    bool stride = false;                    ///< Drive frames by distance travelled (needs a cycle distance).
    bool keep_phase = false;                ///< Start at the previous state's cycle phase (walk to run).
    std::optional<GameplayTiming> timing;   ///< Re-time to these windows (one-shots with a contact frame).
    std::optional<HitStopPolicy> hit_stop;  ///< Applied when this state's attack connects.
};

struct PolicyTable {
    std::vector<StatePolicy> states;
    const StatePolicy* find(std::string_view state) const;
};

struct PolicyLoad {
    std::optional<PolicyTable> table;
    Diagnostics diagnostics;
};

/// Reads `hanji-policies` JSON:
///
///     {"format": "hanji-policies", "version": 1, "states": [
///        {"state": "run", "stride": true, "keep_phase": true},
///        {"state": "jab", "lock_facing": true,
///         "timing": {"startup_s": 0.2, "active_s": 0.1, "recovery_s": 0.3},
///         "hit_stop": {"duration_s": 0.07, "time_scale": 0, "merge": "extend", "who": "victim"}}]}
///
/// With a clip table, also checks that each state's action exists, that stride states have a cycle
/// distance and that timed states have a contact frame.
PolicyLoad load_policies(std::string_view json_text, const ClipTable* table = nullptr);

/// The play options a state implies, given the phase of the state it follows.
PlayOptions play_options(const StatePolicy& policy, double previous_phase);

}  // namespace hanji
