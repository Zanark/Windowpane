"""Build README media from explicitly exported native synthetic demo surfaces."""

import argparse
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
BACKGROUND = "#111825"
FOREGROUND = "#e6eef9"
MUTED = "#a4b7cd"
ACCENT = "#50e1c2"
SIZE = (720, 410)


def font(size):
    return ImageFont.truetype("segoeui.ttf", size)


def native_bitmap(path):
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    header, width, height, planes, depth, compression = struct.unpack_from("<IiiHHI", data, 14)
    if (
        data[:2] != b"BM"
        or header != 40
        or not 1 <= width <= 4096
        or not -4096 <= height < 0
        or planes != 1
        or depth != 32
        or compression != 0
        or len(data) != offset + width * -height * 4
    ):
        raise ValueError(f"Expected a bounded top-down native BGRA export: {path}")
    # Native layered-window pixels are premultiplied; PNG/GIF composition needs straight RGB.
    pixels = bytearray(data[offset:])
    for i in range(0, len(pixels), 4):
        alpha = pixels[i + 3]
        for channel in range(3):
            pixels[i + channel] = min(255, (pixels[i + channel] * 255 + alpha // 2) // alpha) if alpha else 0
    return Image.frombytes("RGBA", (width, -height), bytes(pixels), "raw", "BGRA")


def checkerboard(size):
    image = Image.new("RGBA", size, "#1e2837")
    draw = ImageDraw.Draw(image)
    for y in range(0, size[1], 16):
        for x in range(0, size[0], 16):
            if (x // 16 + y // 16) % 2 == 0:
                draw.rectangle((x, y, x + 15, y + 15), fill="#364151")
    return image


def card(source, title, detail, state):
    image = Image.new("RGB", SIZE, BACKGROUND)
    draw = ImageDraw.Draw(image)
    draw.text((28, 20), "WindowPane", font=font(18), fill=ACCENT)
    draw.text((28, 52), title, font=font(29), fill=FOREGROUND)
    draw.text((28, 96), detail, font=font(17), fill=MUTED)
    backing = checkerboard((664, 236))
    backing.alpha_composite(source, ((664 - source.width) // 2, (236 - source.height) // 2))
    image.paste(backing.convert("RGB"), (28, 132))
    draw.text((28, 380), state, font=font(15), fill=FOREGROUND)
    return image


def frames(directory, family, count):
    paths = [directory / f"{family}-{index:03}.bmp" for index in range(count)]
    return [native_bitmap(path) for path in paths]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "build" / "demo-frames")
    args = parser.parse_args()
    destination = ROOT / "docs" / "assets"
    destination.mkdir(parents=True, exist_ok=True)
    groups = {
        "live": frames(args.input, "live", 8),
        "opacity": frames(args.input, "opacity", 17),
        "key": frames(args.input, "key", 3),
        "resize": frames(args.input, "resize", 13),
    }
    definitions = {
        "live": (
            "Keep a region in view",
            "New source pixels arrive in the borderless live pane.",
            ["Actual WGC capture | synthetic source repaints green / red"] * 8,
            550,
        ),
        "opacity": (
            "Fade the whole pane",
            "Overall opacity changes every pixel, including the background.",
            [f"Native opacity slider: {100 - 10 * min(i, 16 - i)}%" for i in range(17)],
            150,
        ),
        "key": (
            "Remove a background color",
            "Pick blue, keep the other colors, then fade what remains.",
            ["Color key off | opacity 100%", "Blue keyed out | opacity 100%", "Blue keyed out | opacity 50%"],
            1600,
        ),
        "resize": (
            "Resize without stretching",
            "The crop keeps its proportions; unused margins stay transparent.",
            [f"Actual native pane surface: {frame.width} x {frame.height}" for frame in groups["resize"]],
            200,
        ),
    }
    palette = Image.new("RGB", (360, 205 * sum(len(group) for group in groups.values())), BACKGROUND)
    palette_index = 0
    for family, sources in groups.items():
        title, detail, labels, _ = definitions[family]
        for source, label in zip(sources, labels):
            preview = card(source, title, detail, label).resize((360, 205), Image.Resampling.NEAREST)
            palette.paste(preview, (0, palette_index * 205))
            palette_index += 1
    palette = palette.quantize(colors=256)
    for family, sources in groups.items():
        title, detail, labels, duration = definitions[family]
        images = [card(source, title, detail, label) for source, label in zip(sources, labels)]
        encoded = [image.quantize(palette=palette, dither=Image.Dither.NONE) for image in images]
        encoded[0].save(
            destination / f"demo-{family}.gif",
            save_all=True,
            append_images=encoded[1:],
            duration=duration,
            loop=0,
            disposal=2,
            optimize=False,
        )
    stills = [
        ("pane-original.png", "live", 1),
        ("pane-opacity.png", "opacity", 5),
        ("pane-keyed.png", "key", 1),
        ("pane-keyed-faded.png", "key", 2),
        ("pane-aspect-fit.png", "resize", 0),
    ]
    for name, family, index in stills:
        title, detail, labels, _ = definitions[family]
        card(groups[family][index], title, detail, labels[index]).save(destination / name)
    for name in ("default", "picker", "keyed"):
        native_bitmap(args.input / f"controls-{name}.bmp").save(destination / f"controls-{name}.png")
    hero = Image.new("RGB", (1440, 820), BACKGROUND)
    for index, (name, _, _) in enumerate(stills[:4]):
        with Image.open(destination / name) as image:
            hero.paste(image, ((index % 2) * 720, (index // 2) * 410))
    hero.save(destination / "showcase.png")
    review = Image.new("RGB", (1440, 820), BACKGROUND)
    for index, family in enumerate(groups):
        title, detail, labels, _ = definitions[family]
        frame_index = 4 if family == "live" else len(groups[family]) // 2
        review.paste(
            card(groups[family][frame_index], title, detail, labels[frame_index]),
            ((index % 2) * 720, (index // 2) * 410),
        )
    review.save(ROOT / "build" / "demo-review.png")
    print(f"Exported 4 GIFs and 9 PNGs to {destination}")


if __name__ == "__main__":
    main()
