"""Render original, explicitly illustrative source-to-pane README animations."""

from functools import lru_cache
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SIZE = (1120, 700)
INK = "#e6eef9"
MUTED = "#a4b7cd"
MINT = "#50e1c2"
YELLOW = "#f3cc5b"
BLUE = (45, 126, 230, 255)


@lru_cache
def font(size):
    return ImageFont.truetype("segoeui.ttf", size)


def tile(value, keyed=False):
    image = Image.new("RGBA", (360, 128), (0, 0, 0, 0) if keyed else BLUE)
    draw = ImageDraw.Draw(image)
    draw.text((20, 13), "Build completion", font=font(16), fill="white")
    draw.text((20, 37), f"{value}%", font=font(38), fill="white")
    draw.text((20, 98), "Demo data - updates in the source", font=font(13), fill="white")
    return image


def grip(draw, x, y):
    draw.rounded_rectangle((x, y, x + 24, y + 24), radius=6, fill="#34485b", outline="#93b9bd")
    for row in (7, 12, 17):
        for column in (8, 15):
            draw.ellipse((x + column - 1, y + row - 1, x + column + 1, y + row + 1), fill="#defbf5")


def canvas(title, subtitle):
    image = Image.new("RGBA", SIZE, "#0b111c")
    draw = ImageDraw.Draw(image)
    draw.text((36, 20), "WindowPane  /  HOW IT WORKS", font=font(15), fill=MINT)
    draw.text((36, 49), title, font=font(32), fill=INK)
    draw.text((36, 97), subtitle, font=font(17), fill=MUTED)
    draw.line((36, 636, 1084, 636), fill="#29374a", width=1)
    draw.text(
        (36, 652), "ANIMATED ILLUSTRATION  |  Fictional windows and demo data, not an app recording.",
        font=font(14), fill=MUTED,
    )
    return image


def cursor(draw, x, y, clicking=False):
    if clicking:
        draw.ellipse((x - 13, y - 13, x + 13, y + 13), outline=YELLOW, width=3)
    points = [(x, y), (x + 3, y + 28), (x + 10, y + 20), (x + 18, y + 31), (x + 24, y + 27),
              (x + 16, y + 16), (x + 27, y + 14)]
    draw.polygon(points, fill="white", outline="#0b111c", width=2)


def connector(draw, start, end, progress=1.0):
    dx, dy = end[0] - start[0], end[1] - start[1]
    distance = math.hypot(dx, dy)
    for offset in range(0, int(distance * progress), 16):
        stop = min(offset + 9, distance * progress)
        draw.line(
            (start[0] + dx * offset / distance, start[1] + dy * offset / distance,
             start[0] + dx * stop / distance, start[1] + dy * stop / distance),
            fill=MINT, width=3,
        )
    draw.ellipse((start[0] - 4, start[1] - 4, start[0] + 4, start[1] + 4), fill=MINT)


def source_window(image, value):
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((420, 144, 1084, 594), radius=13, fill="#131d2b", outline="#34445a", width=2)
    draw.rounded_rectangle((421, 145, 1083, 184), radius=12, fill="#243248")
    draw.rectangle((421, 171, 1083, 184), fill="#243248")
    for x, color in [(442, "#f18e88"), (462, YELLOW), (482, MINT)]:
        draw.ellipse((x, 158, x + 10, 168), fill=color)
    draw.text((508, 151), "Demo dashboard - source window", font=font(16), fill=INK)
    draw.text((448, 203), "Overview       Activity       History", font=font(15), fill=MUTED)
    draw.line((448, 230, 1054, 230), fill="#34445a")
    draw.text((448, 244), "Build overview", font=font(25), fill=INK)
    draw.rounded_rectangle((468, 298, 1040, 430), radius=6, fill=BLUE)
    image.alpha_composite(tile(value), (470, 300))
    draw = ImageDraw.Draw(image)
    draw.text((861, 319), "Demo project", font=font(16), fill="white")
    draw.text((861, 349), "Running", font=font(23), fill="white")
    draw.text((448, 456), "Recent activity", font=font(18), fill=INK)
    for y, label in [(489, "Compile sources"), (526, "Run checks"), (563, "Prepare output")]:
        draw.text((456, y), label, font=font(15), fill=MUTED)
        draw.rounded_rectangle((688, y + 7, 1038, y + 14), radius=3, fill="#2b3b50")
        draw.rounded_rectangle((688, y + 7, 688 + value * 3, y + 14), radius=3, fill=MINT)


def snip_sequence():
    frames = []
    for index in range(30):
        value = 72 if index < 16 else 72 + min((index - 16) // 4, 2)
        image = canvas(
            "One region. A little floating window.",
            "Select part of a source window; keep its live mirror above your other apps.",
        )
        source_window(image, value)
        draw = ImageDraw.Draw(image)
        step = 0 if index < 8 else 1 if index < 16 else 2
        headings = ["1   SNIP A REGION", "2   FLOAT THE MIRROR", "3   FOLLOW LIVE UPDATES"]
        details = [
            ["Drag around only the bit", "you want to keep visible."],
            ["Same selected pixels.", "No title bar on the live pane."],
            ["The source changes.", "The little view changes too."],
        ]
        draw.text((40, 181), headings[step], font=font(20), fill=MINT)
        for line, text in enumerate(details[step]):
            draw.text((40, 224 + line * 27), text, font=font(18), fill=INK)
        if index < 8:
            progress = min(1.0, (index + 1) / 7)
            right, bottom = 470 + int(360 * progress), 300 + int(128 * progress)
            draw.rectangle((470, 300, right, bottom), outline=MINT, width=4)
            cursor(draw, right, bottom, index < 2)
            draw.text((40, 411), "Source region", font=font(17), fill=MUTED)
            draw.rectangle((40, 445, 340, 552), outline="#34445a", width=2)
            draw.text((62, 481), "Your pane appears here", font=font(16), fill=MUTED)
        else:
            draw.rectangle((470, 300, 830, 428), outline=MINT, width=4)
            progress = min(1.0, (index - 7) / 5)
            connector(draw, (470, 300), (340, 445), progress)
            connector(draw, (470, 428), (340, 552), progress)
            draw.text((40, 411), "Floating pane - always on top", font=font(17), fill=YELLOW)
            pane = tile(value).resize((300, 107), Image.Resampling.LANCZOS)
            image.alpha_composite(pane, (40, 445))
            draw = ImageDraw.Draw(image)
            grip(draw, 310, 451)
            draw.rectangle((38, 443, 342, 554), outline=YELLOW, width=3)
            draw.text((40, 572), "Outline and connectors are annotations.", font=font(14), fill=MUTED)
            if index < 14:
                cursor(draw, 331, 539)
        frames.append(image.convert("RGB"))
    return frames


def workspace_sequence():
    frames = []
    for index in range(36):
        stage = index // 12
        titles = ["Drag it out of the way.", "Fade the whole pane.", "Remove just the background."]
        subtitles = [
            "The small mirror stays above a different application while you keep working.",
            "Overall opacity fades both the text and its blue background.",
            "Picked-color transparency leaves the other colors visible over your workspace.",
        ]
        image = canvas(titles[stage], subtitles[stage])
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((36, 150, 1084, 608), radius=12, fill="#172334", outline="#34445a", width=2)
        draw.rectangle((37, 178, 1083, 196), fill="#243248")
        draw.rounded_rectangle((37, 151, 1083, 190), radius=11, fill="#243248")
        draw.text((58, 158), "A different app - example workspace", font=font(17), fill=INK)
        draw.text((64, 226), "Project notes", font=font(28), fill=INK)
        for y, text in [
            (285, "Review the latest changes"),
            (330, "Keep the build progress in sight"),
            (375, "Continue working without switching windows"),
            (453, "The original source stays open and rendering."),
            (493, "WindowPane does not refresh its data for you."),
        ]:
            draw.text((68, y), text, font=font(19), fill=MUTED)
        progress = min(index, 11) / 11
        x, y = 665 + int(50 * progress), 260 + int(165 * progress)
        opacity = 100
        if stage == 1:
            opacity = round(100 - 65 * (1 - abs((index - 12) / 11 * 2 - 1)))
        keyed = stage == 2 and index >= 27
        pane = tile(74, keyed=keyed).resize((300, 107), Image.Resampling.LANCZOS)
        if opacity != 100:
            pane.putalpha(pane.getchannel("A").point(lambda value: round(value * opacity / 100)))
        image.alpha_composite(pane, (x, y))
        draw = ImageDraw.Draw(image)
        grip(draw, x + 270, y + 6)
        draw.rectangle((x - 3, y - 3, x + 302, y + 109), outline=YELLOW, width=2)
        if stage == 0:
            label = "Drag the pane"
            cursor(draw, x + 282, y + 18, index == 0)
        elif stage == 1:
            label = f"Overall opacity: {opacity}%"
        else:
            label = "Blue removed; white text stays" if keyed else "Pick the blue background"
            if not keyed:
                cursor(draw, x + 265, y + 83, index == 26)
        draw.text((x - 3, y - 34), label, font=font(17), fill=YELLOW)
        draw.text((62, 565), "Yellow boundary = explanatory annotation, not a title bar.", font=font(15), fill=MUTED)
        frames.append(image.convert("RGB"))
    return frames


def save_sequence(frames, path, duration):
    sheet = Image.new("RGB", (280, 175 * len(frames)))
    for index, frame in enumerate(frames):
        sheet.paste(frame.resize((280, 175), Image.Resampling.NEAREST), (0, 175 * index))
    palette = sheet.quantize(colors=256)
    encoded = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    encoded[0].save(
        path, save_all=True, append_images=encoded[1:], duration=duration,
        loop=0, disposal=2, optimize=False,
    )


def main():
    destination = ROOT / "docs" / "assets"
    destination.mkdir(parents=True, exist_ok=True)
    sequences = [("illustration-snip", snip_sequence()), ("illustration-workspace", workspace_sequence())]
    review = Image.new("RGB", (1120, 1050), "#0b111c")
    for column, (name, frames) in enumerate(sequences):
        save_sequence(frames, destination / f"{name}.gif", 220)
        for row, index in enumerate([3, len(frames) // 2, len(frames) - 1]):
            review.paste(frames[index].resize((560, 350)), (column * 560, row * 350))
    review.save(ROOT / "build" / "illustration-review.png")
    print(f"Exported 2 original animated illustrations to {destination}")


if __name__ == "__main__":
    main()
