// SPDX-License-Identifier: Apache-2.0
#include "hanji/readers.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>

namespace hanji {

namespace {

using json = nlohmann::json;

std::optional<json> parse(std::string_view text, Diagnostics& d) {
    try {
        return json::parse(text.begin(), text.end());
    } catch (const json::exception& ex) {
        d.error(std::string("the JSON does not parse: ") + ex.what());
        return std::nullopt;
    }
}

const json* member(const json& object, const char* key) {
    if (!object.is_object()) return nullptr;
    const auto it = object.find(key);
    return it == object.end() || it->is_null() ? nullptr : &*it;
}

std::optional<int> as_int(const json* value) {
    if (value == nullptr || !value->is_number()) return std::nullopt;
    return value->get<int>();
}

std::optional<double> as_double(const json* value) {
    if (value == nullptr || !value->is_number()) return std::nullopt;
    return value->get<double>();
}

std::optional<std::string> as_string(const json* value) {
    if (value == nullptr || !value->is_string()) return std::nullopt;
    return value->get<std::string>();
}

std::optional<IntRect> as_rect(const json* value) {
    if (value == nullptr) return std::nullopt;
    if (value->is_array() && value->size() == 4 && std::all_of(value->begin(), value->end(), [](const json& v) { return v.is_number(); }))
        return IntRect{(*value)[0].get<int>(), (*value)[1].get<int>(), (*value)[2].get<int>(), (*value)[3].get<int>()};
    if (value->is_object()) {
        const auto x = as_int(member(*value, "x")), y = as_int(member(*value, "y"));
        const auto w = as_int(member(*value, "w")), h = as_int(member(*value, "h"));
        if (x && y && w && h) return IntRect{*x, *y, *w, *h};
    }
    return std::nullopt;
}

std::optional<IntPoint> as_point(const json* value) {
    if (value == nullptr) return std::nullopt;
    if (value->is_array() && value->size() == 2 && (*value)[0].is_number() && (*value)[1].is_number())
        return IntPoint{(*value)[0].get<int>(), (*value)[1].get<int>()};
    if (value->is_object()) {
        const auto x = as_int(member(*value, "x")), y = as_int(member(*value, "y"));
        if (x && y) return IntPoint{*x, *y};
    }
    return std::nullopt;
}

Action& action_named(ClipTable& table, const std::string& name) {
    if (Action* found = table.find(name)) return *found;
    table.actions.push_back(Action{});
    table.actions.back().name = name;
    return table.actions.back();
}

/// After a reader has placed every frame it found: copy the actions the source gave without a heading into
/// all eight headings, fill mirrors, validate. An action drawn in one named heading is not spread: its
/// missing headings are reported.
void finish(ClipTable& table, const std::set<std::string>& without_heading, const LoadOptions& options, Diagnostics& d) {
    for (Action& action : table.actions) {
        if (without_heading.count(action.name) != 0) spread_single_heading(action);
        if (options.allow_mirror) fill_mirrored_headings(action, table.cell_w);
    }
    d.append(validate(table));
}

// ---------------------------------------------------------------- pose-atlas contract

LoadResult read_pose_contract(const json& root, const LoadOptions& options) {
    LoadResult result;
    Diagnostics& d = result.diagnostics;
    if (as_string(member(root, "format")) != std::optional<std::string>("pose-atlas-contract")) {
        d.error("not a pose-atlas contract: format is not \"pose-atlas-contract\"");
        return result;
    }
    const int version = as_int(member(root, "version")).value_or(0);
    if (version != 1 && version != 2) d.warning("pose-atlas contract version " + std::to_string(version) + " is not 1 or 2; reading it as 2");

    ClipTable table;
    const auto cell = as_point(member(root, "cell"));
    const auto pivot = as_point(member(root, "pivot"));
    const auto columns = as_int(member(root, "columns"));
    const json* views = member(root, "views");
    if (!cell || !pivot || !columns || views == nullptr || !views->is_array()) {
        d.error("the contract lacks cell, pivot, columns or views");
        return result;
    }
    table.cell_w = cell->x;
    table.cell_h = cell->y;
    table.pivot = *pivot;
    table.metres_per_pixel = as_double(member(root, "meters_per_pixel")).value_or(as_double(member(root, "metres_per_pixel")).value_or(0.0));
    table.image_w = *columns * table.cell_w;
    table.image_h = static_cast<int>(views->size()) * table.cell_h;
    if (const json* files = member(root, "files"))
        if (const json* atlas = member(*files, "atlas")) table.image = as_string(member(*atlas, "file")).value_or("");
    if (table.image.empty()) table.image = "atlas.png";

    std::map<std::string, int> view_rows;
    for (int row = 0; row < static_cast<int>(views->size()); ++row) {
        const auto name = as_string(&(*views)[static_cast<std::size_t>(row)]);
        if (!name || !parse_heading(*name)) {
            d.error("view " + std::to_string(row) + " is not one of the eight headings");
            return result;
        }
        view_rows[*name] = row;
    }

    struct ActionInfo { int first_column = 0; int pose_count = 0; };
    std::map<std::string, ActionInfo> infos;
    if (const json* actions = member(root, "actions"); actions != nullptr && actions->is_array()) {
        for (const json& entry : *actions) {
            const auto name = as_string(member(entry, "name"));
            if (!name) { d.error("an action in the contract has no name"); continue; }
            Action& action = action_named(table, *name);
            ActionInfo& info = infos[*name];
            info.first_column = as_int(member(entry, "first_column")).value_or(0);
            info.pose_count = as_int(member(entry, "pose_count")).value_or(0);
            action.loop = as_string(member(entry, "end_point")).value_or("exclude") == "exclude";
            if (const json* clocks = member(entry, "clocks"))
                if (const json* runtime = member(*clocks, "runtime")) {
                    if (as_string(member(*runtime, "cadence")).value_or("") == "distance")
                        action.cycle_cm = as_double(member(*runtime, "cycle_m")).value_or(0.0) * 100.0;
                }
        }
    }

    const json* frames = member(root, "frames");
    if (frames == nullptr || !frames->is_array()) {
        d.error("the contract has no frames array");
        return result;
    }
    for (const json& entry : *frames) {
        const auto action_name = as_string(member(entry, "action"));
        const auto view = as_string(member(entry, "view"));
        const auto pose_index = as_int(member(entry, "pose_index"));
        if (!action_name || !view || !pose_index || view_rows.count(*view) == 0) {
            d.error("a frame lacks action, view or pose_index: " + entry.dump());
            continue;
        }
        Action& action = action_named(table, *action_name);
        const ActionInfo& info = infos[*action_name];
        Clip& clip = action.clip(*parse_heading(*view));
        clip.present = true;
        if (static_cast<int>(clip.frames.size()) <= *pose_index) clip.frames.resize(static_cast<std::size_t>(*pose_index) + 1);
        Frame& frame = clip.frames[static_cast<std::size_t>(*pose_index)];
        frame.source = IntRect{(info.first_column + *pose_index) * table.cell_w, view_rows[*view] * table.cell_h, table.cell_w, table.cell_h};
        frame.duration_ms = as_int(member(entry, "duration_ms")).value_or(0);
        frame.key = as_string(member(entry, "key")).value_or("");
    }
    for (Action& action : table.actions) {
        const int expected = infos[action.name].pose_count;
        if (expected > 0 && action.frame_count() != expected)
            d.error("action '" + action.name + "' lists " + std::to_string(expected) + " poses but the frames give " +
                    std::to_string(action.frame_count()));
    }

    std::set<std::string> without_heading;
    if (views->size() == 1)
        for (const Action& action : table.actions) without_heading.insert(action.name);
    finish(table, without_heading, options, d);
    result.table = std::move(table);
    return result;
}

// ---------------------------------------------------------------- Aseprite JSON

struct AsepriteFrame {
    std::string name;
    IntRect source;
    IntRect sprite_source;  // where the trimmed cell sits in the canvas
    int canvas_w = 0, canvas_h = 0;
    int duration_ms = 0;
    bool trimmed = false;
};

std::vector<AsepriteFrame> aseprite_frames(const json& root, Diagnostics& d) {
    std::vector<AsepriteFrame> out;
    const json* frames = member(root, "frames");
    if (frames == nullptr) {
        d.error("the Aseprite JSON has no frames");
        return out;
    }
    auto read = [&](const std::string& name, const json& entry) {
        AsepriteFrame f;
        f.name = name;
        const auto rect = as_rect(member(entry, "frame"));
        if (!rect) {
            d.error("frame '" + name + "' has no frame rectangle");
            return;
        }
        f.source = *rect;
        f.sprite_source = as_rect(member(entry, "spriteSourceSize")).value_or(IntRect{0, 0, rect->w, rect->h});
        if (const json* size = member(entry, "sourceSize")) {
            f.canvas_w = as_int(member(*size, "w")).value_or(rect->w);
            f.canvas_h = as_int(member(*size, "h")).value_or(rect->h);
        } else {
            f.canvas_w = rect->w;
            f.canvas_h = rect->h;
        }
        f.trimmed = member(entry, "trimmed") != nullptr && member(entry, "trimmed")->is_boolean() && member(entry, "trimmed")->get<bool>();
        f.duration_ms = as_int(member(entry, "duration")).value_or(0);
        out.push_back(std::move(f));
    };
    if (frames->is_array()) {
        for (const json& entry : *frames) read(as_string(member(entry, "filename")).value_or(""), entry);
    } else if (frames->is_object()) {
        for (auto it = frames->begin(); it != frames->end(); ++it) read(it.key(), it.value());
    } else {
        d.error("the Aseprite frames are neither an array nor an object");
    }
    return out;
}

struct Placement { std::string action; std::optional<Heading> heading; int index = 0; };

/// Splits "run.se" into (run, se) and "run" into (run, none).
std::pair<std::string, std::optional<Heading>> split_tag(const std::string& name) {
    const std::size_t dot = name.find_last_of(".-_");
    if (dot != std::string::npos)
        if (const auto heading = parse_heading(name.substr(dot + 1))) return {name.substr(0, dot), heading};
    return {name, std::nullopt};
}

LoadResult read_aseprite(const json& root, const LoadOptions& options) {
    LoadResult result;
    Diagnostics& d = result.diagnostics;
    const std::vector<AsepriteFrame> frames = aseprite_frames(root, d);
    if (frames.empty()) {
        if (d.ok()) d.error("the Aseprite JSON has no frames");
        return result;
    }

    ClipTable table;
    const json* meta = member(root, "meta");
    table.image = meta ? as_string(member(*meta, "image")).value_or("") : "";
    if (meta)
        if (const json* size = member(*meta, "size")) {
            table.image_w = as_int(member(*size, "w")).value_or(0);
            table.image_h = as_int(member(*size, "h")).value_or(0);
        }
    if (table.image_w <= 0 || table.image_h <= 0) {
        for (const AsepriteFrame& f : frames) {
            table.image_w = std::max(table.image_w, f.source.x + f.source.w);
            table.image_h = std::max(table.image_h, f.source.y + f.source.h);
        }
        d.warning("the Aseprite JSON gives no image size; using the extent of the frames");
    }
    const bool same_canvas = std::all_of(frames.begin(), frames.end(), [&](const AsepriteFrame& f) {
        return f.canvas_w == frames[0].canvas_w && f.canvas_h == frames[0].canvas_h;
    });
    if (same_canvas) {
        table.cell_w = frames[0].canvas_w;
        table.cell_h = frames[0].canvas_h;
    }
    if (std::any_of(frames.begin(), frames.end(), [](const AsepriteFrame& f) { return f.trimmed; }))
        d.warning("some frames are trimmed; boxes and the pivot are read in canvas pixels, and the runtime draws the trimmed cell at its sprite source offset");

    // Where each global frame goes: by frame name when every name is "<action><index>.<heading>", else by tags.
    std::vector<std::optional<Placement>> placements(frames.size());
    const std::regex named(R"(^(.+?)(\d+)[._-](e|ne|n|nw|w|sw|s|se)$)", std::regex::icase);
    bool all_named = true;
    for (std::size_t i = 0; i < frames.size() && all_named; ++i) {
        std::smatch m;
        if (std::regex_match(frames[i].name, m, named))
            placements[i] = Placement{m[1].str(), parse_heading(m[3].str()), std::stoi(m[2].str())};
        else
            all_named = false;
    }
    if (!all_named) {
        std::fill(placements.begin(), placements.end(), std::nullopt);
        const json* tags = meta ? member(*meta, "frameTags") : nullptr;
        if (tags == nullptr || !tags->is_array() || tags->empty()) {
            d.error("the Aseprite JSON has no frame tags and its frame names are not <action><index>.<heading>");
            return result;
        }
        for (const json& tag : *tags) {
            const auto name = as_string(member(tag, "name"));
            const auto from = as_int(member(tag, "from")), to = as_int(member(tag, "to"));
            if (!name || !from || !to || *from < 0 || *to < *from || *to >= static_cast<int>(frames.size())) {
                d.error("a frame tag is malformed: " + tag.dump());
                continue;
            }
            const auto [action, heading] = split_tag(*name);
            for (int f = *from; f <= *to; ++f) {
                auto& slot = placements[static_cast<std::size_t>(f)];
                if (slot) {
                    d.error("frame " + std::to_string(f) + " is in tags '" + slot->action + "' and '" + *name + "'");
                    continue;
                }
                slot = Placement{action, heading, f - *from};
            }
        }
    }

    std::set<std::string> without_heading;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (!placements[i]) continue;  // untagged frames are left out
        const Placement& p = *placements[i];
        Action& action = action_named(table, p.action);
        if (!p.heading) without_heading.insert(p.action);
        auto place = [&](Heading heading) {
            Clip& clip = action.clip(heading);
            clip.present = true;
            if (static_cast<int>(clip.frames.size()) <= p.index) clip.frames.resize(static_cast<std::size_t>(p.index) + 1);
            Frame& frame = clip.frames[static_cast<std::size_t>(p.index)];
            frame.source = frames[i].source;
            frame.duration_ms = frames[i].duration_ms;
        };
        if (p.heading) place(*p.heading);
        else place(Heading::E);  // spread to the other headings by finish()
    }

    // Slices: pivot and boxes.
    if (meta)
        if (const json* slices = member(*meta, "slices"); slices != nullptr && slices->is_array()) {
            bool pivot_set = false;
            for (const json& slice : *slices) {
                const std::string name = as_string(member(slice, "name")).value_or("");
                const json* keys = member(slice, "keys");
                if (keys == nullptr || !keys->is_array() || keys->empty()) continue;
                const json& first = (*keys)[0];
                if (const auto pivot = as_point(member(first, "pivot")); pivot && (!pivot_set || name == "foot")) {
                    const auto bounds = as_rect(member(first, "bounds")).value_or(IntRect{});
                    table.pivot = IntPoint{bounds.x + pivot->x, bounds.y + pivot->y};
                    pivot_set = true;
                }
                const std::size_t colon = name.find_first_of(":/");
                const auto kind = parse_box_kind(colon == std::string::npos ? name : name.substr(0, colon));
                if (!kind) continue;
                const std::string box_name = colon == std::string::npos ? name : name.substr(colon + 1);
                std::vector<std::pair<int, IntRect>> ranges;
                for (const json& key : *keys) {
                    const auto frame = as_int(member(key, "frame"));
                    const auto bounds = as_rect(member(key, "bounds"));
                    if (frame && bounds) ranges.emplace_back(*frame, *bounds);
                }
                std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                for (std::size_t r = 0; r < ranges.size(); ++r) {
                    const int begin = ranges[r].first;
                    const int end = r + 1 < ranges.size() ? ranges[r + 1].first : static_cast<int>(frames.size());
                    if (ranges[r].second.w <= 0 || ranges[r].second.h <= 0) continue;  // an empty key ends the box
                    for (int f = std::max(begin, 0); f < std::min(end, static_cast<int>(frames.size())); ++f) {
                        const auto& p = placements[static_cast<std::size_t>(f)];
                        if (!p) continue;
                        Action& action = action_named(table, p->action);
                        auto add = [&](Heading heading) {
                            Clip& clip = action.clip(heading);
                            if (p->index < static_cast<int>(clip.frames.size()))
                                clip.frames[static_cast<std::size_t>(p->index)].boxes.push_back(Hitbox{box_name, *kind, ranges[r].second});
                        };
                        if (p->heading) add(*p->heading);
                        else add(Heading::E);
                    }
                }
            }
        }

    for (const std::string& name : without_heading) {
        const Action* action = table.find(name);
        int present = 0;
        for (const Clip& clip : action->headings) present += clip.present ? 1 : 0;
        if (present > 1) d.error("action '" + name + "' has a tag without a heading and tags with headings");
    }
    finish(table, without_heading, options, d);
    result.table = std::move(table);
    return result;
}

// ---------------------------------------------------------------- hanji-clips

json rect_json(const IntRect& r) { return json::array({r.x, r.y, r.w, r.h}); }

LoadResult read_hanji_clips(const json& root) {
    LoadResult result;
    Diagnostics& d = result.diagnostics;
    if (as_string(member(root, "format")) != std::optional<std::string>("hanji-clips")) {
        d.error("not a hanji-clips file: format is not \"hanji-clips\"");
        return result;
    }
    if (as_int(member(root, "version")).value_or(0) != 1) d.warning("hanji-clips version is not 1; reading it as 1");
    ClipTable table;
    table.image = as_string(member(root, "image")).value_or("");
    if (const auto size = as_point(member(root, "image_size"))) { table.image_w = size->x; table.image_h = size->y; }
    if (const auto cell = as_point(member(root, "cell"))) { table.cell_w = cell->x; table.cell_h = cell->y; }
    table.pivot = as_point(member(root, "pivot")).value_or(IntPoint{});
    table.metres_per_pixel = as_double(member(root, "metres_per_pixel")).value_or(0.0);
    if (const json* actions = member(root, "actions"); actions != nullptr && actions->is_array())
        for (const json& entry : *actions) {
            const auto name = as_string(member(entry, "name"));
            if (!name) { d.error("an action has no name"); continue; }
            Action& action = action_named(table, *name);
            if (const json* loop = member(entry, "loop"); loop && loop->is_boolean()) action.loop = loop->get<bool>();
            action.contact_frame = as_int(member(entry, "contact_frame")).value_or(-1);
            action.active_end_frame = as_int(member(entry, "active_end_frame")).value_or(-1);
            action.cycle_cm = as_double(member(entry, "cycle_cm")).value_or(0.0);
            const json* headings = member(entry, "headings");
            if (headings == nullptr || !headings->is_object()) continue;
            for (auto it = headings->begin(); it != headings->end(); ++it) {
                const auto heading = parse_heading(it.key());
                if (!heading) { d.error("action '" + *name + "' has an unknown heading '" + it.key() + "'"); continue; }
                Clip& clip = action.clip(*heading);
                clip.present = true;
                const json* frames = member(it.value(), "frames");
                if (frames == nullptr || !frames->is_array()) continue;
                for (const json& f : *frames) {
                    Frame frame;
                    frame.source = as_rect(member(f, "source")).value_or(IntRect{});
                    frame.duration_ms = as_int(member(f, "duration_ms")).value_or(0);
                    if (const json* m = member(f, "mirror"); m && m->is_boolean()) frame.mirror_x = m->get<bool>();
                    frame.key = as_string(member(f, "key")).value_or("");
                    if (const json* events = member(f, "events"); events && events->is_array())
                        for (const json& e : *events) frame.events.push_back(as_string(&e).value_or(""));
                    if (const json* boxes = member(f, "boxes"); boxes && boxes->is_array())
                        for (const json& b : *boxes) {
                            Hitbox box;
                            box.name = as_string(member(b, "name")).value_or("");
                            box.kind = parse_box_kind(as_string(member(b, "kind")).value_or("attack")).value_or(BoxKind::Attack);
                            box.box = as_rect(member(b, "box")).value_or(IntRect{});
                            frame.boxes.push_back(std::move(box));
                        }
                    clip.frames.push_back(std::move(frame));
                }
            }
        }
    d.append(validate(table));
    result.table = std::move(table);
    return result;
}

}  // namespace

LoadResult load_pose_contract(std::string_view json_text, const LoadOptions& options) {
    LoadResult result;
    const auto root = parse(json_text, result.diagnostics);
    if (!root) return result;
    return read_pose_contract(*root, options);
}

LoadResult load_aseprite_json(std::string_view json_text, const LoadOptions& options) {
    LoadResult result;
    const auto root = parse(json_text, result.diagnostics);
    if (!root) return result;
    return read_aseprite(*root, options);
}

LoadResult load_hanji_clips(std::string_view json_text) {
    LoadResult result;
    const auto root = parse(json_text, result.diagnostics);
    if (!root) return result;
    return read_hanji_clips(*root);
}

Diagnostics apply_hanji_overlay(ClipTable& table, std::string_view json_text) {
    Diagnostics d;
    const auto root = parse(json_text, d);
    if (!root) return d;
    if (as_string(member(*root, "format")) != std::optional<std::string>("hanji-clips")) {
        d.error("not a hanji-clips overlay: format is not \"hanji-clips\"");
        return d;
    }
    const json* actions = member(*root, "actions");
    if (actions == nullptr || !actions->is_array()) {
        d.error("the overlay has no actions array");
        return d;
    }
    for (const json& entry : *actions) {
        const auto name = as_string(member(entry, "name"));
        if (!name) { d.error("an overlay action has no name"); continue; }
        Action* action = table.find(*name);
        if (action == nullptr) { d.error("the overlay names action '" + *name + "', which the table lacks"); continue; }
        const int count = action->frame_count();
        if (const json* loop = member(entry, "loop"); loop && loop->is_boolean()) action->loop = loop->get<bool>();
        if (const auto v = as_int(member(entry, "contact_frame"))) action->contact_frame = *v;
        if (const auto v = as_int(member(entry, "active_end_frame"))) action->active_end_frame = *v;
        if (const auto v = as_double(member(entry, "cycle_cm"))) action->cycle_cm = *v;

        auto frames_of = [&](const json* spec, std::vector<int>& out) {
            if (spec == nullptr) return;
            if (spec->is_string() && spec->get<std::string>() == "all") { for (int i = 0; i < count; ++i) out.push_back(i); return; }
            if (spec->is_number()) { out.push_back(spec->get<int>()); return; }
            if (spec->is_array()) for (const json& v : *spec) if (v.is_number()) out.push_back(v.get<int>());
        };
        auto for_each_heading = [&](const json* heading_spec, auto&& fn) {
            if (const auto heading_name_text = as_string(heading_spec)) {
                if (const auto heading = parse_heading(*heading_name_text)) fn(*heading);
                else d.error("action '" + *name + "' overlay names an unknown heading '" + *heading_name_text + "'");
                return;
            }
            for (int h = 0; h < kHeadingCount; ++h) fn(heading_at(h));
        };

        if (const json* events = member(entry, "events"); events && events->is_object())
            for (auto it = events->begin(); it != events->end(); ++it) {
                int index = -1;
                try { index = std::stoi(it.key()); } catch (...) {}
                if (index < 0 || index >= count) { d.error("action '" + *name + "' overlay has events at frame '" + it.key() + "', outside 0.." + std::to_string(count - 1)); continue; }
                std::vector<std::string> names;
                if (it.value().is_array()) for (const json& v : it.value()) names.push_back(as_string(&v).value_or(""));
                else if (it.value().is_string()) names.push_back(it.value().get<std::string>());
                for (int h = 0; h < kHeadingCount; ++h) {
                    Clip& clip = action->clip(heading_at(h));
                    if (!clip.present || index >= static_cast<int>(clip.frames.size())) continue;
                    auto& target = clip.frames[static_cast<std::size_t>(index)].events;
                    target.insert(target.end(), names.begin(), names.end());
                }
            }

        if (const json* boxes = member(entry, "hitboxes"); boxes && boxes->is_array())
            for (const json& b : *boxes) {
                Hitbox box;
                box.name = as_string(member(b, "name")).value_or("");
                box.kind = parse_box_kind(as_string(member(b, "kind")).value_or("attack")).value_or(BoxKind::Attack);
                const auto rect = as_rect(member(b, "box"));
                if (!rect) { d.error("action '" + *name + "' overlay has a hitbox without a box"); continue; }
                box.box = *rect;
                std::vector<int> indices;
                frames_of(member(b, "frames"), indices);
                if (indices.empty()) { d.error("action '" + *name + "' overlay hitbox '" + box.name + "' names no frames"); continue; }
                for_each_heading(member(b, "heading"), [&](Heading heading) {
                    Clip& clip = action->clip(heading);
                    if (!clip.present) return;
                    for (int index : indices) {
                        if (index < 0 || index >= static_cast<int>(clip.frames.size())) { d.error("action '" + *name + "' overlay hitbox '" + box.name + "' at frame " + std::to_string(index) + " is out of range"); continue; }
                        Frame& frame = clip.frames[static_cast<std::size_t>(index)];
                        Hitbox placed = box;
                        if (frame.mirror_x) {
                            const int width = table.cell_w > 0 ? table.cell_w : frame.source.w;
                            placed.box.x = width - placed.box.x - placed.box.w;
                        }
                        frame.boxes.push_back(std::move(placed));
                    }
                });
            }
    }
    d.append(validate(table));
    return d;
}

std::string write_hanji_clips(const ClipTable& table) {
    json root;
    root["format"] = "hanji-clips";
    root["version"] = 1;
    root["image"] = table.image;
    root["image_size"] = json::array({table.image_w, table.image_h});
    root["cell"] = json::array({table.cell_w, table.cell_h});
    root["pivot"] = json::array({table.pivot.x, table.pivot.y});
    root["metres_per_pixel"] = table.metres_per_pixel;
    json actions = json::array();
    for (const Action& action : table.actions) {
        json a;
        a["name"] = action.name;
        a["loop"] = action.loop;
        a["contact_frame"] = action.contact_frame;
        a["active_end_frame"] = action.active_end_frame;
        a["cycle_cm"] = action.cycle_cm;
        json headings = json::object();
        for (int h = 0; h < kHeadingCount; ++h) {
            const Clip& clip = action.clip(heading_at(h));
            if (!clip.present) continue;
            json frames = json::array();
            for (const Frame& frame : clip.frames) {
                json f;
                f["source"] = rect_json(frame.source);
                f["duration_ms"] = frame.duration_ms;
                if (frame.mirror_x) f["mirror"] = true;
                if (!frame.key.empty()) f["key"] = frame.key;
                if (!frame.events.empty()) f["events"] = frame.events;
                if (!frame.boxes.empty()) {
                    json boxes = json::array();
                    for (const Hitbox& box : frame.boxes)
                        boxes.push_back({{"name", box.name}, {"kind", std::string(box_kind_name(box.kind))}, {"box", rect_json(box.box)}});
                    f["boxes"] = boxes;
                }
                frames.push_back(std::move(f));
            }
            headings[std::string(heading_name(heading_at(h)))] = {{"frames", frames}};
        }
        a["headings"] = headings;
        actions.push_back(std::move(a));
    }
    root["actions"] = actions;
    return root.dump(2) + "\n";
}

}  // namespace hanji
