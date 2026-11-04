// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <string_view>

#include "hanji/clip.hpp"

namespace hanji {

struct LoadOptions {
    /// Fill headings that the source lacks from their mirror heading (e from w and so on).
    bool allow_mirror = true;
};

struct LoadResult {
    std::optional<ClipTable> table;  ///< Set when the source parsed; may still fail validate().
    Diagnostics diagnostics;
};

/// pose-atlas `pose-contract.json`, versions 1 and 2. Frame rectangles come from the contract's cell,
/// columns and view order; the image size is what those imply.
LoadResult load_pose_contract(std::string_view json_text, const LoadOptions& options = {});

/// Aseprite JSON export (`json-array` or `json-hash`). Actions and headings come from the frame tags
/// (`run.se` = action `run`, heading `se`; `run` alone = every heading) or, when every frame is named
/// `<action><index>.<heading>`, from the frame names. The `foot` slice (or the first slice with a pivot)
/// gives the ground pivot; slices named `hit:<name>` / `hurt:<name>` give boxes for the frames their keys
/// cover.
LoadResult load_aseprite_json(std::string_view json_text, const LoadOptions& options = {});

/// Hanji's own `hanji-clips` JSON, as `write_hanji_clips` writes it: the whole table, frames included.
LoadResult load_hanji_clips(std::string_view json_text);

/// Applies a `hanji-clips` overlay: per action, `loop`, `contact_frame`, `active_end_frame`, `cycle_cm`,
/// `events` ({"<frame>": ["hit", ...]}) and `hitboxes` ([{name, kind, frames, box, heading?}]). Actions in the
/// overlay that the table lacks are errors; headings in the overlay are not read.
Diagnostics apply_hanji_overlay(ClipTable& table, std::string_view json_text);

/// Serialises the whole table as `hanji-clips` JSON. `load_hanji_clips` reads it back losslessly.
std::string write_hanji_clips(const ClipTable& table);

}  // namespace hanji
