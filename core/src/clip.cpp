// SPDX-License-Identifier: Apache-2.0
#include "hanji/clip.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace hanji {

namespace {

constexpr std::array<std::string_view, kHeadingCount> kHeadingNames = {"e", "ne", "n", "nw", "w", "sw", "s", "se"};

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string frame_label(const Action& action, Heading heading, int index) {
    return action.name + "." + std::string(heading_name(heading)) + "[" + std::to_string(index) + "]";
}

}  // namespace

std::string_view heading_name(Heading heading) { return kHeadingNames[static_cast<std::size_t>(heading_index(heading))]; }

std::optional<Heading> parse_heading(std::string_view name) {
    const std::string wanted = lower(name);
    for (int i = 0; i < kHeadingCount; ++i)
        if (kHeadingNames[static_cast<std::size_t>(i)] == wanted) return heading_at(i);
    return std::nullopt;
}

Heading mirror_of(Heading heading) {
    switch (heading) {
        case Heading::E: return Heading::W;
        case Heading::W: return Heading::E;
        case Heading::NE: return Heading::NW;
        case Heading::NW: return Heading::NE;
        case Heading::SE: return Heading::SW;
        case Heading::SW: return Heading::SE;
        case Heading::N: return Heading::N;
        case Heading::S: return Heading::S;
    }
    return heading;
}

std::string_view box_kind_name(BoxKind kind) { return kind == BoxKind::Attack ? "attack" : "hurt"; }

std::optional<BoxKind> parse_box_kind(std::string_view name) {
    const std::string wanted = lower(name);
    if (wanted == "attack" || wanted == "hit") return BoxKind::Attack;
    if (wanted == "hurt") return BoxKind::Hurt;
    return std::nullopt;
}

int Action::frame_count() const {
    for (const Clip& clip : headings)
        if (clip.present) return static_cast<int>(clip.frames.size());
    return 0;
}

int Action::total_ms(Heading heading) const {
    int total = 0;
    for (const Frame& frame : clip(heading).frames) total += frame.duration_ms;
    return total;
}

const Action* ClipTable::find(std::string_view name) const {
    for (const Action& action : actions)
        if (action.name == name) return &action;
    return nullptr;
}

Action* ClipTable::find(std::string_view name) {
    for (Action& action : actions)
        if (action.name == name) return &action;
    return nullptr;
}

void Diagnostics::append(const Diagnostics& other) {
    errors.insert(errors.end(), other.errors.begin(), other.errors.end());
    warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
}

Diagnostics validate(const ClipTable& table) {
    Diagnostics d;
    if (table.image.empty()) d.error("the table names no atlas image");
    if (table.image_w <= 0 || table.image_h <= 0) d.error("the atlas image size is unknown");
    if (table.cell_w < 0 || table.cell_h < 0) d.error("the cell size is negative");
    if (table.cell_w > 0 && table.cell_h > 0 &&
        (table.pivot.x < 0 || table.pivot.y < 0 || table.pivot.x >= table.cell_w || table.pivot.y >= table.cell_h))
        d.error("the ground pivot (" + std::to_string(table.pivot.x) + ", " + std::to_string(table.pivot.y) +
                ") is outside the cell");
    if (table.metres_per_pixel < 0.0) d.error("metres per pixel is negative");
    if (table.actions.empty()) d.warning("the table has no actions");

    std::set<std::string> names;
    for (const Action& action : table.actions) {
        if (action.name.empty()) d.error("an action has no name");
        if (!names.insert(action.name).second) d.error("action '" + action.name + "' appears twice");

        const int count = action.frame_count();
        std::string missing;
        for (int h = 0; h < kHeadingCount; ++h) {
            const Clip& clip = action.clip(heading_at(h));
            if (!clip.present) {
                missing += missing.empty() ? "" : ", ";
                missing += heading_name(heading_at(h));
                continue;
            }
            if (static_cast<int>(clip.frames.size()) != count)
                d.error("action '" + action.name + "' has " + std::to_string(clip.frames.size()) + " frames in heading " +
                        std::string(heading_name(heading_at(h))) + " but " + std::to_string(count) + " in another heading");
        }
        if (count == 0) {
            d.error("action '" + action.name + "' has no frames");
            continue;
        }
        if (!missing.empty()) d.error("action '" + action.name + "' lacks headings: " + missing);

        if (action.cycle_cm < 0.0) d.error("action '" + action.name + "' has a negative cycle distance");
        if (action.contact_frame < -1 || action.contact_frame >= count)
            d.error("action '" + action.name + "' has contact frame " + std::to_string(action.contact_frame) +
                    " outside 0.." + std::to_string(count - 1));
        if (action.active_end_frame != -1) {
            if (action.contact_frame < 0)
                d.error("action '" + action.name + "' has an active end frame but no contact frame");
            else if (action.active_end_frame <= action.contact_frame || action.active_end_frame > count)
                d.error("action '" + action.name + "' has active end frame " + std::to_string(action.active_end_frame) +
                        "; it must be after the contact frame and at most " + std::to_string(count));
        }

        for (int h = 0; h < kHeadingCount; ++h) {
            const Heading heading = heading_at(h);
            const Clip& clip = action.clip(heading);
            if (!clip.present) continue;
            for (int i = 0; i < static_cast<int>(clip.frames.size()); ++i) {
                const Frame& frame = clip.frames[static_cast<std::size_t>(i)];
                const std::string label = frame_label(action, heading, i);
                if (frame.duration_ms < 0) d.error(label + " has a negative duration");
                if (frame.duration_ms == 0 && action.cycle_cm <= 0.0)
                    d.error(label + " has no duration and the action is not distance-driven");
                if (frame.source.w <= 0 || frame.source.h <= 0 || frame.source.x < 0 || frame.source.y < 0 ||
                    frame.source.x + frame.source.w > table.image_w || frame.source.y + frame.source.h > table.image_h)
                    d.error(label + " has a source rectangle outside the atlas image");
                for (const std::string& event : frame.events)
                    if (event.empty()) d.error(label + " has an unnamed event");
                for (const Hitbox& box : frame.boxes) {
                    if (box.box.w <= 0 || box.box.h <= 0) d.error(label + " has box '" + box.name + "' with no area");
                    if (box.box.x < 0 || box.box.y < 0 || box.box.x + box.box.w > frame.source.w ||
                        box.box.y + box.box.h > frame.source.h)
                        d.error(label + " has box '" + box.name + "' outside the cell");
                }
            }
        }
    }
    return d;
}

int fill_mirrored_headings(Action& action, int cell_w) {
    int filled = 0;
    for (int h = 0; h < kHeadingCount; ++h) {
        const Heading heading = heading_at(h);
        const Heading mirror = mirror_of(heading);
        if (action.clip(heading).present || mirror == heading || !action.clip(mirror).present) continue;
        Clip copy = action.clip(mirror);
        for (Frame& frame : copy.frames) {
            frame.mirror_x = !frame.mirror_x;
            const int width = cell_w > 0 ? cell_w : frame.source.w;
            for (Hitbox& box : frame.boxes) box.box.x = width - box.box.x - box.box.w;
        }
        action.clip(heading) = std::move(copy);
        ++filled;
    }
    return filled;
}

void spread_single_heading(Action& action) {
    const Clip* only = nullptr;
    int present = 0;
    for (const Clip& clip : action.headings)
        if (clip.present) {
            ++present;
            only = &clip;
        }
    if (present != 1) return;
    const Clip source = *only;
    for (Clip& clip : action.headings) clip = source;
}

}  // namespace hanji
