// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hanji {

/// The eight screen headings, in pose-atlas row order.
enum class Heading : std::uint8_t { E = 0, NE, N, NW, W, SW, S, SE };
inline constexpr int kHeadingCount = 8;

std::string_view heading_name(Heading heading);              // "e", "ne", ...
std::optional<Heading> parse_heading(std::string_view name);  // case-insensitive
Heading mirror_of(Heading heading);                           // e<->w, ne<->nw, se<->sw; n and s are their own mirror
inline Heading heading_at(int index) { return static_cast<Heading>(index); }
inline int heading_index(Heading heading) { return static_cast<int>(heading); }

struct IntRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const IntRect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};
struct IntPoint {
    int x = 0, y = 0;
    bool operator==(const IntPoint& o) const { return x == o.x && y == o.y; }
};

enum class BoxKind { Attack, Hurt };
std::string_view box_kind_name(BoxKind kind);
std::optional<BoxKind> parse_box_kind(std::string_view name);

/// A hitbox on one frame, in cell pixels from the cell's top-left corner.
struct Hitbox {
    std::string name;
    BoxKind kind = BoxKind::Attack;
    IntRect box;
};

/// One drawn frame of one action in one heading.
struct Frame {
    IntRect source;                   ///< Where the cell is in the atlas image.
    int duration_ms = 0;              ///< Display time; 0 only for distance-driven actions.
    bool mirror_x = false;            ///< Draw flipped: the frame is borrowed from the mirror heading.
    std::string key;                  ///< Optional stable key (pose-atlas: "run.se@0.400000").
    std::vector<std::string> events;  ///< Event names raised when this frame begins.
    std::vector<Hitbox> boxes;        ///< Boxes active while this frame is shown.
};

/// One action in one heading.
struct Clip {
    bool present = false;
    std::vector<Frame> frames;
};

/// One action (idle, run, jab ...) in up to eight headings.
struct Action {
    std::string name;
    bool loop = true;
    int contact_frame = -1;     ///< The frame the action lands on; -1 when it has none.
    int active_end_frame = -1;  ///< First frame after the active window; -1 when it has none.
    double cycle_cm = 0.0;      ///< Distance covered by one pass of the frames; 0 for time-driven actions.
    std::array<Clip, kHeadingCount> headings;

    const Clip& clip(Heading heading) const { return headings[static_cast<std::size_t>(heading_index(heading))]; }
    Clip& clip(Heading heading) { return headings[static_cast<std::size_t>(heading_index(heading))]; }
    int frame_count() const;           ///< Frames in the first present heading; 0 when none is present.
    int total_ms(Heading heading) const;
    bool has_contact() const { return contact_frame >= 0; }
};

/// Everything the runtime needs about one character's atlas.
struct ClipTable {
    std::string image;            ///< Atlas image path, as the source metadata states it.
    int image_w = 0, image_h = 0;
    int cell_w = 0, cell_h = 0;   ///< Cell size when every frame has the same untrimmed cell; 0 otherwise.
    IntPoint pivot;               ///< Ground pivot in cell pixels.
    double metres_per_pixel = 0.0;
    std::vector<Action> actions;

    const Action* find(std::string_view name) const;
    Action* find(std::string_view name);
};

struct Diagnostics {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool ok() const { return errors.empty(); }
    void error(std::string message) { errors.push_back(std::move(message)); }
    void warning(std::string message) { warnings.push_back(std::move(message)); }
    void append(const Diagnostics& other);
};

/// Checks the table against the rules every reader must leave it in: every action has all eight headings
/// with equal frame counts, every frame has a duration (or the action is distance-driven) and a source
/// rectangle inside the image, landmarks and boxes are in range.
Diagnostics validate(const ClipTable& table);

/// Fills headings that are missing from their mirror heading, flipping the frames and their boxes.
/// Returns how many headings were filled. Boxes are flipped around the cell width (or the frame width
/// when the table has no fixed cell).
int fill_mirrored_headings(Action& action, int cell_w);

/// Copies the one present heading of a single-view action into all eight.
void spread_single_heading(Action& action);

}  // namespace hanji
