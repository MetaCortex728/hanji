# The clock

One `hanji::Player` per character decides which frame is shown, which events fire and which boxes are live. It is advanced with real time and the distance the character moved; it never reads a wall clock, so the engine layer and the tests drive it the same way.

## Position

The clock tracks one number per play: the **position**. Its integer part is the frame shown, counting on across loop cycles (frame = floor(position) mod count); its fraction is how far through that frame. Within one play the position only grows. Events fire for every integer the position crosses, so each frame's events fire exactly once per pass however large or small the steps are: a hitch fires the skipped frames' events in order (at most one cycle's worth), and a frozen clock fires nothing.

## Three ways to advance

| Mode | When | Position from |
|---|---|---|
| Native | Default | Gameplay time × speed, against the authored durations in milliseconds. Durations are never rounded to a flipbook frame rate. |
| Re-timed | `PlayOptions::timing` on a one-shot with a contact frame | Gameplay time against three windows: startup, active, recovery. The contact frame begins exactly at `startup_s`, the active-end frame exactly at `startup_s + active_s`. Inside each window the frames keep their authored proportions. |
| Distance | The action has a cycle distance (`cycle_cm`) | Distance travelled: one pass of the frames per cycle. Standing still holds the frame, so feet do not slide. Footstep events are ordinary frame events, so they land where the foot does. |

Re-timing is what lets combat timing live in the game while the art stays as drawn. A jab with startup 0.2 s, active 0.1 s and recovery 0.3 s at 1× and the same windows halved at 2× both show the contact frame on the first gameplay tick at or after `startup_s`; the tests check this at 0.5×, 1× and 2× on a 60 Hz tick.

The October prototype re-timed linearly over frame indices; Hanji maps native time instead, so a 300 ms frame in the startup window keeps three times the share of a 100 ms frame.

## Hit-stop

`Player::hit_stop(policy)` slows the clock for `duration_s` of real time at `time_scale` (0 freezes; 0.05 crawls). Travel is scaled with time, so a stride clock under hit-stop does not step. Because positions only grow, a stop never re-fires an event. The timer belongs to the clock and runs on real time, so it ends on schedule even when the actor's own time is frozen; this avoids the timer-under-dilation pitfall documented for engine timers.

A second hit during a stop follows `merge`: `extend` keeps the longer of the two remaining times, `restart` starts the new duration, `ignore` keeps the first. `who` (victim, attacker, both) is read by the engine layer, which decides which actors to stop; Japanese, Korean and Chinese recipes agree on the mechanism and disagree on this choice, so it is data.

## Headings

Changing the heading changes the frame drawn, not the time: the same frame index in the new heading. Every heading has the same frame count (the [data](data.md) layer checks this), and the timeline uses the durations of the heading current when the action started.

## States

`hanji-policies` JSON says what each gameplay state means for the clock: which action it plays, whether facing locks (attacks), whether frames follow distance (locomotion), whether the cycle phase carries over from the previous state (walk to run), the re-timing windows, and the hit-stop policy when the state's attack connects. The game's own state machine, or PaperZD's, decides which state is current. See `hanji/policy.hpp` for the format.
