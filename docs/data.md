# Data: clip tables and readers

The core reads one character's atlas metadata into a **clip table**: actions (idle, run, jab ...), each in the eight screen headings `e, ne, n, nw, w, sw, s, se` (pose-atlas row order), each a list of frames. A frame has its rectangle in the atlas image, a display duration in whole milliseconds, a mirror flag, an optional stable key, the events raised when it begins, and the hitboxes active while it is shown. An action has a loop flag, an optional contact frame and active-end frame, and, for distance-driven locomotion, the distance one pass of its frames covers. The table also holds the image path and size, the cell size, the ground pivot in cell pixels and metres per pixel.

Durations stay in milliseconds as the source gives them. Nothing is rounded to a flipbook frame rate; the [clock](clock.md) plays them exactly.

## Readers

| Reader | Source | Notes |
|---|---|---|
| `load_pose_contract` | pose-atlas `pose-contract.json`, versions 1 and 2 | Rectangles from the contract's cell, columns and view rows; keys from version 2; a distance cadence becomes the action's cycle distance. |
| `load_aseprite_json` | Aseprite's JSON export, `json-array` or `json-hash` | Actions and headings from frame tags (`run.se`), or from frame names when every name is `<action><index>.<heading>` (pose-atlas's own `atlas.json`). A tag without a heading (`pose`) means the same frames in every heading. The `foot` slice, or the first slice with a pivot, is the ground pivot. Slices named `hit:<name>` or `hurt:<name>` are boxes on the frames their keys cover; a key with an empty rectangle ends the box. |
| `load_hanji_clips` | Hanji's own `hanji-clips` JSON | The whole table, frames included; `write_hanji_clips` writes it, and reading it back gives the same table. |

For both pose-atlas outputs, the contract reader and the Aseprite reader produce the same rectangles and durations; the tests check this.

## Mirrors

A heading the source lacks is filled from its mirror heading (`w` from `e`, `nw` from `ne`, `sw` from `se`, and the other way round), with the frames flagged to be drawn flipped and their boxes flipped around the cell width. `n` and `s` have no mirror. An action drawn in one named heading (`walk.e` alone) is not copied to the other seven: its missing headings are reported. Pass `allow_mirror = false` to require every heading to be drawn.

## Overlay

Landmarks, events and boxes rarely come from the renderer. A `hanji-clips` overlay adds them to a table read from any source:

```json
{
  "format": "hanji-clips", "version": 1,
  "actions": [
    {"name": "jab", "loop": false, "contact_frame": 3, "active_end_frame": 5,
     "events": {"0": ["whoosh"], "3": ["hit"]},
     "hitboxes": [
       {"name": "fist", "kind": "attack", "frames": [3, 4], "box": [150, 110, 30, 20], "heading": "e"},
       {"name": "body", "kind": "hurt", "frames": "all", "box": [100, 80, 56, 128]}
     ]}
  ]
}
```

Events apply to the frame in every heading. A box without `heading` applies to every heading at the same cell position; with `heading`, only to that one, and a box placed on a mirrored heading is flipped. Boxes are in cell pixels from the cell's top-left corner, `[x, y, w, h]`.

## Validation

Every reader leaves the table checked by `validate`, which reports, by action and frame:

- a missing heading, or headings with different frame counts;
- a frame with no duration in a time-driven action;
- a rectangle outside the image, or a box outside its cell or with no area;
- a contact frame out of range, or an active-end frame not after the contact frame;
- a pivot outside the cell, a duplicated action name.

Readers never throw on bad input: malformed JSON, a wrong `format` or missing fields become diagnostics.
