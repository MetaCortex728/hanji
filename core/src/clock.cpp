// SPDX-License-Identifier: Apache-2.0
#include "hanji/clock.hpp"

#include <algorithm>
#include <cmath>

namespace hanji {

namespace {

// Positions are compared with this slack so that a landmark computed as a ratio of times lands on its
// frame, not a hair before it.
constexpr double kEpsilon = 1e-9;

bool finite_positive(double v) { return std::isfinite(v) && v > 0.0; }

}  // namespace

bool GameplayTiming::valid() const {
    return finite_positive(startup_s) && finite_positive(active_s) && finite_positive(recovery_s);
}

ActionTimeline::ActionTimeline(const Action& action, Heading heading) {
    const Clip& clip = action.clip(heading);
    start_s_.reserve(clip.frames.size());
    double t = 0.0;
    for (const Frame& frame : clip.frames) {
        start_s_.push_back(t);
        t += std::max(frame.duration_ms, 0) / 1000.0;
    }
    total_s_ = t;
}

double ActionTimeline::frame_start_s(int frame) const {
    if (frame <= 0 || start_s_.empty()) return 0.0;
    if (frame >= frame_count()) return total_s_;
    return start_s_[static_cast<std::size_t>(frame)];
}

double ActionTimeline::position_at_native(double t) const {
    const int count = frame_count();
    if (count == 0) return 0.0;
    if (total_s_ <= 0.0) return 0.0;
    if (t <= 0.0) return 0.0;
    if (t >= total_s_) return static_cast<double>(count);
    // The last frame whose start is at or before t (with slack, so a start computed by division counts).
    const auto it = std::upper_bound(start_s_.begin(), start_s_.end(), t + kEpsilon);
    const int i = static_cast<int>(it - start_s_.begin()) - 1;
    const double begin = start_s_[static_cast<std::size_t>(i)];
    const double end = frame_start_s(i + 1);
    const double fraction = end > begin ? std::clamp((t - begin) / (end - begin), 0.0, 1.0) : 0.0;
    return static_cast<double>(i) + fraction;
}

double ActionTimeline::position_native(double elapsed_s, double speed, bool loop) const {
    const int count = frame_count();
    if (count == 0 || total_s_ <= 0.0 || !std::isfinite(elapsed_s) || elapsed_s <= 0.0) return 0.0;
    const double t = elapsed_s * (finite_positive(speed) ? speed : 1.0);
    if (!loop) return position_at_native(std::min(t, total_s_));
    const double cycles = std::floor(t / total_s_);
    return cycles * count + std::min(position_at_native(t - cycles * total_s_), static_cast<double>(count));
}

double ActionTimeline::position_retimed(double elapsed_s, const GameplayTiming& timing, int contact, int active_end) const {
    const int count = frame_count();
    if (count == 0 || !timing.valid() || !std::isfinite(elapsed_s) || elapsed_s <= 0.0) return 0.0;
    contact = std::clamp(contact, 0, count - 1);
    active_end = active_end < 0 ? contact + 1 : std::clamp(active_end, contact + 1, count);
    const double contact_t = frame_start_s(contact);
    const double active_end_t = frame_start_s(active_end);
    double t;
    if (elapsed_s < timing.startup_s)
        t = contact_t * (elapsed_s / timing.startup_s);
    else if (elapsed_s < timing.startup_s + timing.active_s)
        t = contact_t + (active_end_t - contact_t) * ((elapsed_s - timing.startup_s) / timing.active_s);
    else
        t = active_end_t + (total_s_ - active_end_t) * std::min((elapsed_s - timing.startup_s - timing.active_s) / timing.recovery_s, 1.0);
    // Landmarks are exact: at startup the contact frame begins, at startup + active the active-end frame.
    if (std::abs(elapsed_s - timing.startup_s) <= kEpsilon) return static_cast<double>(contact);
    if (std::abs(elapsed_s - timing.startup_s - timing.active_s) <= kEpsilon) return static_cast<double>(active_end);
    return position_at_native(t);
}

double ActionTimeline::position_distance(double distance_cm, double cycle_cm) const {
    const int count = frame_count();
    if (count == 0 || !finite_positive(cycle_cm) || !std::isfinite(distance_cm) || distance_cm <= 0.0) return 0.0;
    return distance_cm / cycle_cm * count;
}

// ---------------------------------------------------------------- Player

Player::Player(const ClipTable& table) : table_(&table) {}

bool Player::play(std::string_view name, const PlayOptions& options) {
    const Action* action = table_->find(name);
    if (action == nullptr || action->frame_count() == 0) return false;
    if (options.timing && (!options.timing->valid() || !action->has_contact())) return false;
    if (action == action_ && !options.restart_if_same) {
        options_.speed = options.speed;  // a running loop can change speed without restarting
        return true;
    }
    action_ = action;
    options_ = options;
    timeline_ = ActionTimeline(*action, heading_);
    elapsed_s_ = 0.0;
    distance_cm_ = 0.0;
    const bool cyclic = action->loop || action->cycle_cm > 0.0;
    const double phase = cyclic && std::isfinite(options.start_phase) ? options.start_phase - std::floor(options.start_phase) : 0.0;
    start_offset_ = phase * action->frame_count();
    last_fired_ = static_cast<long long>(std::floor(start_offset_ + kEpsilon)) - 1;
    ++play_id_;
    events_.clear();
    fire_through(position());
    return true;
}

double Player::position() const {
    if (action_ == nullptr) return 0.0;
    if (action_->cycle_cm > 0.0) return start_offset_ + timeline_.position_distance(distance_cm_, action_->cycle_cm);
    if (options_.timing) return timeline_.position_retimed(elapsed_s_, *options_.timing, action_->contact_frame, action_->active_end_frame);
    // Native playback: offset the start by the phase, in time.
    const double offset_s = timeline_.frame_count() > 0 ? start_offset_ / timeline_.frame_count() * timeline_.native_s() : 0.0;
    const double speed = finite_positive(options_.speed) ? options_.speed : 1.0;
    return timeline_.position_native(elapsed_s_ + offset_s / speed, speed, action_->loop);
}

void Player::fire_through(double pos) {
    if (action_ == nullptr) return;
    const int count = action_->frame_count();
    const bool cyclic = action_->loop || action_->cycle_cm > 0.0;
    long long reached = static_cast<long long>(std::floor(pos + kEpsilon));
    if (!cyclic) reached = std::min<long long>(reached, count - 1);
    // At most one cycle per call: a long hitch fires each frame's events once, not once per lost cycle.
    const long long first = std::max(last_fired_ + 1, reached - count + 1);
    const Clip& clip = action_->clip(heading_);
    for (long long n = first; n <= reached; ++n) {
        const int index = static_cast<int>(((n % count) + count) % count);
        for (const std::string& name : clip.frames[static_cast<std::size_t>(index)].events)
            events_.push_back(FiredEvent{name, index, play_id_});
    }
    last_fired_ = std::max(last_fired_, reached);
}

void Player::advance(double real_dt_s, double moved_cm) {
    events_.clear();
    if (action_ == nullptr || !std::isfinite(real_dt_s) || real_dt_s < 0.0) return;
    // Split the step into the part under hit-stop and the part after it.
    const double stopped = std::min(real_dt_s, stop_remaining_s_);
    const double scale_avg = real_dt_s > 0.0 ? (stopped * stop_scale_ + (real_dt_s - stopped)) / real_dt_s : 1.0;
    elapsed_s_ += stopped * stop_scale_ + (real_dt_s - stopped);
    stop_remaining_s_ -= stopped;
    if (stop_remaining_s_ <= 0.0) {
        stop_remaining_s_ = 0.0;
        stop_scale_ = 1.0;
    }
    if (action_->cycle_cm > 0.0 && std::isfinite(moved_cm)) {
        const double travel = std::max(moved_cm, 0.0) * scale_avg;
        if (travel > teleport_cm) {
            // A teleport is not a stride: restart the cycle without firing footsteps.
            distance_cm_ = 0.0;
            start_offset_ = 0.0;
            last_fired_ = 0;
            return;
        }
        distance_cm_ += travel;
    }
    fire_through(position());
}

void Player::hit_stop(const HitStopPolicy& policy) {
    if (!finite_positive(policy.duration_s)) return;
    const double scale = std::isfinite(policy.time_scale) ? std::clamp(policy.time_scale, 0.0, 1.0) : 0.0;
    if (hit_stopped()) {
        switch (policy.merge) {
            case HitStopPolicy::Merge::Ignore: return;
            case HitStopPolicy::Merge::Extend: stop_remaining_s_ = std::max(stop_remaining_s_, policy.duration_s); break;
            case HitStopPolicy::Merge::Restart: stop_remaining_s_ = policy.duration_s; break;
        }
        stop_scale_ = std::min(stop_scale_, scale);
        return;
    }
    stop_remaining_s_ = policy.duration_s;
    stop_scale_ = scale;
}

int Player::frame() const {
    if (action_ == nullptr) return 0;
    const int count = action_->frame_count();
    long long n = static_cast<long long>(std::floor(position() + kEpsilon));
    const bool cyclic = action_->loop || action_->cycle_cm > 0.0;
    if (!cyclic) return static_cast<int>(std::clamp<long long>(n, 0, count - 1));
    return static_cast<int>(((n % count) + count) % count);
}

const Frame& Player::current() const {
    static const Frame kNone{};
    if (action_ == nullptr) return kNone;
    return action_->clip(heading_).frames[static_cast<std::size_t>(frame())];
}

double Player::phase() const {
    if (action_ == nullptr) return 0.0;
    const double count = static_cast<double>(action_->frame_count());
    const double p = position() / count;
    const bool cyclic = action_->loop || action_->cycle_cm > 0.0;
    return cyclic ? p - std::floor(p) : std::clamp(p, 0.0, 1.0);
}

bool Player::finished() const {
    if (action_ == nullptr || action_->loop || action_->cycle_cm > 0.0) return false;
    if (options_.timing) return elapsed_s_ + kEpsilon >= options_.timing->total_s();
    const double speed = finite_positive(options_.speed) ? options_.speed : 1.0;
    return elapsed_s_ * speed + kEpsilon >= timeline_.native_s();
}

bool Player::in_active_window() const {
    if (action_ == nullptr || !action_->has_contact()) return false;
    const int f = frame();
    const int end = action_->active_end_frame < 0 ? action_->contact_frame + 1 : action_->active_end_frame;
    return f >= action_->contact_frame && f < end && !finished();
}

}  // namespace hanji
