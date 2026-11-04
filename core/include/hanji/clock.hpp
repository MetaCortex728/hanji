// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hanji/clip.hpp"

namespace hanji {

/// Gameplay windows of a one-shot action, in seconds of gameplay time. The action's contact frame begins
/// exactly at `startup_s`, its active-end frame exactly at `startup_s + active_s`, and it ends at the sum.
struct GameplayTiming {
    double startup_s = 0.0;
    double active_s = 0.0;
    double recovery_s = 0.0;
    bool valid() const;
    double total_s() const { return startup_s + active_s + recovery_s; }
};

/// Where an action is in its frames, as one number: the integer part is the frame shown (counting on
/// across loop cycles, so frame = floor(position) mod count), the fraction is how far through that frame.
/// Positions only grow within one play, which is what makes events fire exactly once.
class ActionTimeline {
public:
    ActionTimeline() = default;
    ActionTimeline(const Action& action, Heading heading);

    int frame_count() const { return static_cast<int>(start_s_.size()); }
    double native_s() const { return total_s_; }        ///< Sum of the frame durations.
    double frame_start_s(int frame) const;              ///< Native start time of a frame, 0 <= frame <= count.

    /// Position at `elapsed_s` of playback at `speed` (native durations divided by speed).
    double position_native(double elapsed_s, double speed, bool loop) const;
    /// Position at `elapsed_s` of a one-shot re-timed to gameplay windows around `contact` and `active_end`.
    /// Within each window the frames keep their authored proportions.
    double position_retimed(double elapsed_s, const GameplayTiming& timing, int contact, int active_end) const;
    /// Position after `distance_cm` along a cycle of `cycle_cm`, frames evenly spaced.
    double position_distance(double distance_cm, double cycle_cm) const;

private:
    double position_at_native(double t) const;  // t in [0, total], one cycle
    std::vector<double> start_s_;               // native start time of each frame
    double total_s_ = 0.0;
};

/// What the clock does when a hit lands. Policy is data, so the same runtime serves games whose hit-stop
/// differs: who freezes, for how long, at what time scale, and what a second hit during a stop does.
struct HitStopPolicy {
    enum class Merge { Extend, Restart, Ignore };
    enum class Who { Victim, Attacker, Both };
    double duration_s = 0.07;  ///< Real (undilated) seconds.
    double time_scale = 0.0;   ///< 0 freezes; 0.05 crawls.
    Merge merge = Merge::Extend;
    Who who = Who::Victim;     ///< Read by the engine layer, which decides which actors to stop.
};

struct PlayOptions {
    double speed = 1.0;                     ///< Native playback rate; ignored when `timing` is set.
    std::optional<GameplayTiming> timing;   ///< Re-time a one-shot to these windows (needs a contact frame).
    double start_phase = 0.0;               ///< 0..1 into the cycle, for looping or distance-driven actions.
    bool restart_if_same = false;           ///< Playing the action already playing restarts it.
};

struct FiredEvent {
    std::string name;
    int frame = 0;
    std::uint64_t play_id = 0;
};

/// One character's clock: which action, which frame, which events fired, which boxes are live. Advanced
/// with real time and the distance the character moved; never reads a wall clock.
class Player {
public:
    explicit Player(const ClipTable& table);

    /// Starts an action. Returns false (and changes nothing) for an unknown action or for `timing` on an
    /// action without a contact frame. Playing the current action again continues it unless
    /// `restart_if_same` is set.
    bool play(std::string_view action, const PlayOptions& options = {});

    /// Changes the heading without touching time: the same frame index in the new heading.
    void set_heading(Heading heading) { heading_ = heading; }

    /// Advances by `real_dt_s` of real time and `moved_cm` of travel. Fires the events of every frame that
    /// began, in order, at most one cycle's worth per call. Hit-stop slows both time and travel.
    void advance(double real_dt_s, double moved_cm = 0.0);

    void hit_stop(const HitStopPolicy& policy);

    // State.
    const std::vector<FiredEvent>& events() const { return events_; }  ///< Fired by the last advance().
    const Action* action() const { return action_; }
    Heading heading() const { return heading_; }
    int frame() const;                    ///< Frame shown, 0..count-1.
    const Frame& current() const;         ///< The frame shown, in the current heading.
    double phase() const;                 ///< 0..1 through the cycle (loops and distance), or the one-shot.
    bool finished() const;                ///< A one-shot reached its end; it holds its last frame.
    bool in_active_window() const;        ///< contact <= frame < active end.
    bool hit_stopped() const { return stop_remaining_s_ > 0.0; }
    double dilation() const { return hit_stopped() ? stop_scale_ : 1.0; }
    double gameplay_elapsed_s() const { return elapsed_s_; }
    std::uint64_t play_id() const { return play_id_; }

    /// Travel per call above which the stride clock treats movement as a teleport and restarts its cycle.
    double teleport_cm = 100.0;

private:
    double position() const;
    void fire_through(double position);

    const ClipTable* table_;
    const Action* action_ = nullptr;
    ActionTimeline timeline_;
    PlayOptions options_;
    Heading heading_ = Heading::S;
    double elapsed_s_ = 0.0;     // gameplay time in this play (dilated)
    double distance_cm_ = 0.0;   // travel in this play (dilated)
    double start_offset_ = 0.0;  // phase offset in frames
    long long last_fired_ = -1;  // last frame (counting across cycles) whose events fired
    std::uint64_t play_id_ = 0;
    double stop_remaining_s_ = 0.0;
    double stop_scale_ = 1.0;
    std::vector<FiredEvent> events_;
};

}  // namespace hanji
