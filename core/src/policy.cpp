// SPDX-License-Identifier: Apache-2.0
#include "hanji/policy.hpp"

#include <nlohmann/json.hpp>
#include <set>

namespace hanji {

namespace {

using json = nlohmann::json;

bool flag(const json& o, const char* key, bool fallback) {
    const auto it = o.find(key);
    return it != o.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

double number(const json& o, const char* key, double fallback) {
    const auto it = o.find(key);
    return it != o.end() && it->is_number() ? it->get<double>() : fallback;
}

std::string text(const json& o, const char* key, const std::string& fallback) {
    const auto it = o.find(key);
    return it != o.end() && it->is_string() ? it->get<std::string>() : fallback;
}

}  // namespace

const StatePolicy* PolicyTable::find(std::string_view state) const {
    for (const StatePolicy& s : states)
        if (s.state == state) return &s;
    return nullptr;
}

PolicyLoad load_policies(std::string_view json_text, const ClipTable* table) {
    PolicyLoad result;
    Diagnostics& d = result.diagnostics;
    json root;
    try {
        root = json::parse(json_text.begin(), json_text.end());
    } catch (const json::exception& ex) {
        d.error(std::string("the JSON does not parse: ") + ex.what());
        return result;
    }
    if (!root.is_object() || text(root, "format", "") != "hanji-policies") {
        d.error("not a hanji-policies file: format is not \"hanji-policies\"");
        return result;
    }
    const auto states = root.find("states");
    if (states == root.end() || !states->is_array()) {
        d.error("the policies have no states array");
        return result;
    }
    PolicyTable out;
    std::set<std::string> seen;
    for (const json& s : *states) {
        if (!s.is_object() || text(s, "state", "").empty()) {
            d.error("a state has no name");
            continue;
        }
        StatePolicy p;
        p.state = text(s, "state", "");
        p.action = text(s, "action", p.state);
        p.lock_facing = flag(s, "lock_facing", false);
        p.stride = flag(s, "stride", false);
        p.keep_phase = flag(s, "keep_phase", false);
        if (!seen.insert(p.state).second) d.error("state '" + p.state + "' appears twice");
        if (const auto t = s.find("timing"); t != s.end() && t->is_object()) {
            GameplayTiming timing{number(*t, "startup_s", 0.0), number(*t, "active_s", 0.0), number(*t, "recovery_s", 0.0)};
            if (!timing.valid()) d.error("state '" + p.state + "' has timing windows that are not all positive");
            p.timing = timing;
        }
        if (const auto h = s.find("hit_stop"); h != s.end() && h->is_object()) {
            HitStopPolicy stop;
            stop.duration_s = number(*h, "duration_s", stop.duration_s);
            stop.time_scale = number(*h, "time_scale", stop.time_scale);
            const std::string merge = text(*h, "merge", "extend");
            if (merge == "extend") stop.merge = HitStopPolicy::Merge::Extend;
            else if (merge == "restart") stop.merge = HitStopPolicy::Merge::Restart;
            else if (merge == "ignore") stop.merge = HitStopPolicy::Merge::Ignore;
            else d.error("state '" + p.state + "' has hit-stop merge '" + merge + "'; use extend, restart or ignore");
            const std::string who = text(*h, "who", "victim");
            if (who == "victim") stop.who = HitStopPolicy::Who::Victim;
            else if (who == "attacker") stop.who = HitStopPolicy::Who::Attacker;
            else if (who == "both") stop.who = HitStopPolicy::Who::Both;
            else d.error("state '" + p.state + "' has hit-stop who '" + who + "'; use victim, attacker or both");
            if (!(stop.duration_s > 0.0)) d.error("state '" + p.state + "' has a hit-stop without a positive duration");
            if (stop.time_scale < 0.0 || stop.time_scale > 1.0) d.error("state '" + p.state + "' has a hit-stop time scale outside 0..1");
            p.hit_stop = stop;
        }
        if (table != nullptr) {
            const Action* action = table->find(p.action);
            if (action == nullptr) d.error("state '" + p.state + "' plays action '" + p.action + "', which the clip table lacks");
            else {
                if (p.stride && !(action->cycle_cm > 0.0)) d.error("state '" + p.state + "' is a stride state but action '" + p.action + "' has no cycle distance");
                if (p.timing && !action->has_contact()) d.error("state '" + p.state + "' is timed but action '" + p.action + "' has no contact frame");
            }
        }
        out.states.push_back(std::move(p));
    }
    result.table = std::move(out);
    return result;
}

PlayOptions play_options(const StatePolicy& policy, double previous_phase) {
    PlayOptions o;
    o.timing = policy.timing;
    o.start_phase = policy.keep_phase ? previous_phase : 0.0;
    o.restart_if_same = false;
    return o;
}

}  // namespace hanji
