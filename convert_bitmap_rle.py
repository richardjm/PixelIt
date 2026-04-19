#!/usr/bin/env python3
"""
Convert PixelIt JSON between old flat uint16 bitmap format and new RLE+palette format.

Old bitmap format:
  "data": [0, 63488, 0, ...]   (array of uint16 RGB565 values)

New bitmap format (with palette, compact):
  "p":  [0, 63488],
  "c":  [0, 1, 0, ...],   (palette index per run)
  "n":  [1, 1, 1, ...]    (run length per run)

New bitmap format (without palette, direct colors):
  "c":  [0, 63488, 0, ...],   (direct RGB565 color per run)
  "n":  [1, 1, 1, ...]        (run length per run)

Old bitmapAnimation format:
  "data": [[0, 63488, ...], [...]]   (array of frames, each a uint16 array)

New bitmapAnimation format:
  "p": [0, 63488, ...],        (optional shared palette; if absent, c = direct RGB565)
  "data": [
    {"c": [0, 1, ...], "n": [1, 1, ...]},
    ...
  ]

Usage:
    python convert_bitmap_rle.py input.json [output.json]
    python convert_bitmap_rle.py input.json           # writes input_rle.json or input_flat.json
    python convert_bitmap_rle.py --inplace input.json # overwrites input.json
"""

import json
import sys
import os
import copy


MAX_PALETTE_SIZE = 256


def is_old_bitmap(obj: dict) -> bool:
    data = obj.get("data")
    return isinstance(data, list)


def is_new_bitmap(obj: dict) -> bool:
    return isinstance(obj.get("c"), list) and isinstance(obj.get("n"), list)


def is_old_animation(obj: dict) -> bool:
    data = obj.get("data")
    return isinstance(data, list) and bool(data) and isinstance(data[0], list)


def is_new_animation(obj: dict) -> bool:
    data = obj.get("data")
    return (
        isinstance(data, list)
        and bool(data)
        and isinstance(data[0], dict)
        and isinstance(data[0].get("c"), list)
        and isinstance(data[0].get("n"), list)
    )


def build_palette(frames: list[list[int]]) -> list[int]:
    """Collect unique colors from all frames, preserving first-seen order."""
    seen = {}
    palette = []
    for frame in frames:
        for color in frame:
            if color not in seen:
                if len(palette) >= MAX_PALETTE_SIZE:
                    continue
                seen[color] = len(palette)
                palette.append(color)
    return palette


def encode_rle(pixels: list[int], palette: list[int]) -> tuple[list[int], list[int]]:
    """Encode a flat pixel list as parallel indices and counts lists."""
    color_to_idx = {c: i for i, c in enumerate(palette)}
    if not pixels:
        return [], []

    indices = []
    counts = []
    current_idx = color_to_idx.get(pixels[0], 0)
    count = 1

    for color in pixels[1:]:
        idx = color_to_idx.get(color, 0)
        if idx == current_idx:
            count += 1
        else:
            indices.append(current_idx)
            counts.append(count)
            current_idx = idx
            count = 1
    indices.append(current_idx)
    counts.append(count)

    return indices, counts


def decode_rle(values: list[int], counts: list[int], palette: list[int] | None = None) -> list[int]:
    """Decode parallel c/n arrays to a flat pixel list."""
    pixels = []
    use_palette = palette is not None

    for value, count in zip(values, counts):
        color = palette[value] if use_palette and 0 <= value < len(palette) else (0 if use_palette else value)
        pixels.extend([color] * int(count))

    return pixels


def convert_single_bitmap(obj: dict) -> dict:
    """Convert a single bitmap object in either direction."""
    if is_old_bitmap(obj):
        pixels = [int(v) for v in obj["data"]]
        unique_colors = list(dict.fromkeys(pixels))  # preserve order, deduplicate

        if len(unique_colors) > MAX_PALETTE_SIZE:
            print(
                f"  WARNING: bitmap has {len(unique_colors)} unique colors (max {MAX_PALETTE_SIZE}), skipping conversion.",
                file=sys.stderr,
            )
            return obj

        palette = unique_colors
        indices, counts = encode_rle(pixels, palette)

        result = copy.copy(obj)
        del result["data"]
        result["p"] = palette
        result["c"] = indices
        result["n"] = counts
        return result

    if is_new_bitmap(obj):
        palette = [int(v) for v in obj.get("p", [])] if isinstance(obj.get("p"), list) else None
        values = [int(v) for v in obj["c"]]
        counts = [int(v) for v in obj["n"]]

        result = copy.copy(obj)
        result["data"] = decode_rle(values, counts, palette)
        result.pop("p", None)
        del result["c"]
        del result["n"]
        return result

    return obj


def convert_animation(obj: dict) -> dict:
    """Convert a bitmapAnimation object in either direction."""
    if is_old_animation(obj):
        frames = [[int(v) for v in frame] for frame in obj["data"]]
        palette = build_palette(frames)

        if len(palette) == 0:
            return obj

        skipped_colors = set()
        color_to_idx = {c: i for i, c in enumerate(palette)}
        for frame in frames:
            for color in frame:
                if color not in color_to_idx:
                    skipped_colors.add(color)

        if skipped_colors:
            print(
                f"  WARNING: animation has >{MAX_PALETTE_SIZE} unique colors; "
                f"{len(skipped_colors)} color(s) will map to palette index 0.",
                file=sys.stderr,
            )

        rle_frames = []
        for frame in frames:
            indices, counts = encode_rle(frame, palette)
            rle_frames.append({"c": indices, "n": counts})

        result = copy.copy(obj)
        result["p"] = palette
        result["data"] = rle_frames
        return result

    if is_new_animation(obj):
        palette = [int(v) for v in obj.get("p", [])] if isinstance(obj.get("p"), list) else None
        frames = []
        for frame in obj["data"]:
            values = [int(v) for v in frame["c"]]
            counts = [int(v) for v in frame["n"]]
            frames.append(decode_rle(values, counts, palette))

        result = copy.copy(obj)
        result["data"] = frames
        result.pop("p", None)
        return result

    return obj


def detect_doc_direction(doc: dict) -> str:
    """Return to_new, to_old, mixed, or unchanged for convertible content."""
    has_old = False
    has_new = False

    targets = [doc]
    if "setScreen" in doc and isinstance(doc["setScreen"], dict):
        targets.append(doc["setScreen"])

    for target in targets:
        if "bitmap" in target and isinstance(target["bitmap"], dict):
            has_old = has_old or is_old_bitmap(target["bitmap"])
            has_new = has_new or is_new_bitmap(target["bitmap"])

        if "bitmaps" in target and isinstance(target["bitmaps"], list):
            for bitmap in target["bitmaps"]:
                if isinstance(bitmap, dict):
                    has_old = has_old or is_old_bitmap(bitmap)
                    has_new = has_new or is_new_bitmap(bitmap)

        if "bitmapAnimation" in target and isinstance(target["bitmapAnimation"], dict):
            has_old = has_old or is_old_animation(target["bitmapAnimation"])
            has_new = has_new or is_new_animation(target["bitmapAnimation"])

    if has_old and has_new:
        return "mixed"
    if has_old:
        return "to_new"
    if has_new:
        return "to_old"
    return "unchanged"


def convert_doc(doc: dict) -> dict:
    """Convert all bitmap/bitmaps/bitmapAnimation keys in a JSON document."""
    doc = copy.deepcopy(doc)

    # Top-level or nested under "setScreen"
    targets = [doc]
    if "setScreen" in doc and isinstance(doc["setScreen"], dict):
        targets.append(doc["setScreen"])

    for target in targets:
        if "bitmap" in target and isinstance(target["bitmap"], dict):
            print("  Converting: bitmap")
            target["bitmap"] = convert_single_bitmap(target["bitmap"])

        if "bitmaps" in target and isinstance(target["bitmaps"], list):
            print(f"  Converting: bitmaps ({len(target['bitmaps'])} items)")
            target["bitmaps"] = [
                convert_single_bitmap(b) if isinstance(b, dict) else b
                for b in target["bitmaps"]
            ]

        if "bitmapAnimation" in target and isinstance(target["bitmapAnimation"], dict):
            print("  Converting: bitmapAnimation")
            target["bitmapAnimation"] = convert_animation(target["bitmapAnimation"])

    return doc


def default_output_path(input_path: str, direction: str) -> str:
    base, ext = os.path.splitext(input_path)

    if direction == "to_old":
        suffix = "_flat"
    elif direction == "to_new":
        suffix = "_rle"
    else:
        suffix = "_converted"

    return base + suffix + (ext or ".json")


def process_file(input_path: str, output_path: str) -> None:
    print(f"Reading: {input_path}")
    with open(input_path, "r", encoding="utf-8") as f:
        doc = json.load(f)

    direction = detect_doc_direction(doc)
    print(f"Detected: {direction}")
    converted = convert_doc(doc)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(converted, f, separators=(",", ":"))
    print(f"Written: {output_path}")


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        sys.exit(0)

    inplace = False
    if args[0] == "--inplace":
        inplace = True
        args = args[1:]

    if not args:
        print("Error: no input file specified.", file=sys.stderr)
        sys.exit(1)

    input_path = args[0]

    if inplace:
        output_path = input_path
    elif len(args) >= 2:
        output_path = args[1]
    if not os.path.isfile(input_path):
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if not inplace and len(args) < 2:
        with open(input_path, "r", encoding="utf-8") as f:
            output_path = default_output_path(input_path, detect_doc_direction(json.load(f)))

    process_file(input_path, output_path)


if __name__ == "__main__":
    main()
