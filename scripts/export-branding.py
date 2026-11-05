"""Generate WindowPane's original overlapping-window mark and Windows icon."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SHAPES = [
    ((4, 4, 124, 124), 28, "#111825", None, 0),
    ((23, 24, 83, 82), 10, "#172f3d", "#50e1c2", 5),
    ((43, 45, 105, 103), 10, "#e6eef9", "#111825", 5),
    ((52, 54, 96, 64), 3, "#2a756f", None, 0),
    ((52, 71, 96, 94), 4, "#50e1c2", None, 0),
]


def main():
    destination = ROOT / "assets"
    destination.mkdir(parents=True, exist_ok=True)
    scale = 8
    image = Image.new("RGBA", (128 * scale, 128 * scale))
    draw = ImageDraw.Draw(image)
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128" role="img" aria-labelledby="title">',
        "  <title id=\"title\">WindowPane - a little window into any window</title>",
    ]
    for (x1, y1, x2, y2), radius, fill, outline, width in SHAPES:
        draw.rounded_rectangle(
            (x1 * scale, y1 * scale, x2 * scale, y2 * scale),
            radius=radius * scale, fill=fill, outline=outline, width=width * scale,
        )
        stroke = f' stroke="{outline}" stroke-width="{width}"' if outline else ""
        svg.append(
            f'  <rect x="{x1}" y="{y1}" width="{x2-x1}" height="{y2-y1}" rx="{radius}" fill="{fill}"{stroke}/>'
        )
    svg.append("</svg>")
    (destination / "windowpane.svg").write_text("\n".join(svg) + "\n", encoding="utf-8")
    image.resize((512, 512), Image.Resampling.LANCZOS).save(destination / "windowpane.png")
    image.save(
        destination / "favicon.ico", format="ICO",
        sizes=[(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 128, 256)],
    )
    print(f"Generated original SVG, PNG and multi-resolution ICO in {destination}")


if __name__ == "__main__":
    main()
