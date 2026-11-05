# Visual asset provenance

Native references use original WindowPane synthetic-fixture or owned-controls
output. Two additional authored animations illustrate fictional windows and
demo build-progress data. No authenticated portal, user desktop, private account
or third-party GIF is used. This is a bounded feature reference, not a complete
GUI inventory or human acceptance record.

## Sources and derivatives

| Files | Origin and meaning |
|---|---|
| `controls.png` | Earlier owned-controls `PrintWindow` capture from the native integration test |
| `synthetic-*-alpha.png` | Earlier native premultiplied compositor exports converted to alpha PNG |
| `synthetic-original-pane.png`, `synthetic-keyed-pane.png` | Earlier reading copies; checkerboard composited behind native pixels |
| `controls-default.png`, `controls-picker.png`, `controls-keyed.png` | Actual owned-controls captures from `--demo-assets` |
| `demo-live.gif` | Eight real WGC frames; the owned fixture repaints green/red before each frame is accepted |
| `demo-opacity.gif` | Seventeen native compositor states produced through the actual opacity-slider handler, 100% to 20% and back |
| `demo-key.gif` | Native unkeyed, blue-keyed and blue-keyed/50%-opacity states produced through picker/slider handlers |
| `demo-resize.gif` | Thirteen actual native pane sizes; aspect fit preserves the source proportions |
| `pane-*.png` | Five individual native-output states in annotated reading cards |
| `showcase.png` | Four reading cards combined; a documentation montage, not one desktop screenshot |
| `recovery-pane.png` | Actual native resized-source surface with its in-pane Snip again action |
| `corner-grip.png` | Actual exported content/grip surfaces combined at recorded relative geometry; blue removed and content at 50% opacity |
| `illustration-snip.gif` | Original drawn source window, growing selection outline, connectors, floating crop and synchronized fictional updates |
| `illustration-workspace.gif` | Original drawn workspace with an illustrated pane moving, fading and removing blue |

Titles, state labels and checkerboards are added by the documentation encoder.
They do not appear in the borderless pane. Window controls images are not
reconstructed or redrawn by that encoder.

GIF presentation is deliberately paced: live frames at 550 ms, opacity at
150 ms, key states at 1,600 ms and resize at 200 ms. These timings do not claim
capture latency, measured frame rate or real mouse movement. Resize is driven
by native window-size calls, not a recorded physical drag. GIF uses a 256-color
palette; the PNG stills retain the richer-color reading copies.

The two `illustration-*.gif` files are **not native exports or screen recordings**.
Their artwork is drawn by `scripts\export-illustrations.py`. They follow the
requested source-outline/connectors/floating-pane explanation but do not copy
the supplied account screenshot, identifiers, financial values, logos or website
layout. Cursor motion, window positions, example values and timing are
illustrative. Frames start at 220 ms; identical adjacent frames may be combined
by the encoder without changing their total duration.
The illustrated small corner grips represent the real drag/options control;
the workspace cursor uses that grip during the movement sequence.

The native `demo-*.gif` and `pane-*.png` export the captured-content compositor
surface only; the grip is a separate HWND. `corner-grip.png` combines the two
actual surfaces, not a desktop screenshot, and adds a checkerboard/arrow.
Its frosted-look shading is native translucent artwork, not backdrop blur.

The original logo assets live in `assets` at the repository root:
`windowpane.svg`, `windowpane.png` and `favicon.ico`. The procedural original
shapes are defined in `scripts\export-branding.py`; the ICO is also embedded in
the native executable and used for its controls/tray.

## Reproduce

Follow [Development and verification](../04-development-and-verification.md#reproducible-visual-evidence).
The exporter uses Python and Pillow for documentation only; they are not app
runtime dependencies. Raw native BMP inputs stay in ignored `build` output.

The native harness accepts only its fixture HWND as a capture source.
Controls are captured with `PrintWindow`; it does not take a desktop screenshot.
The encoder accepts only the explicitly named bounded top-down BGRA inputs.
It unpremultiplies native channels before compositing onto a checkerboard.

Generate the two authored illustrations independently (no capture or app needed):

```powershell
python .\scripts\export-illustrations.py
```

This uses Python/Pillow and the Windows Segoe UI font, like the native-media
reading-card encoder. Inspect decoded GIF output, not only the drawn input
frames. Original review sheets stay in ignored `build`.

## Review scope

2026-11-06: visually inspected the four-state montage, all three full controls
captures, the live/opacity/key/aspect-fit representative states and sampled
decoded GIF frames. Structural checks cover decoding, expected frame counts,
durations, dimensions, loops, changing frames and README image paths.
The GIF palette is derived from all sequence states. Decoded native-output
areas were compared with full-color references (at most 12/255 channel error);
this caught and corrected an initial dark-color quantization problem.
The source-to-pane/workspace illustrations were separately reviewed as decoded
GIF samples covering selection, connectors, updates, motion, opacity and keying.
The native recovery screenshot was inspected with its actual Snip again action.
The subsequent branding/grip update inspected the original logo, branded native
controls, actual two-surface grip composite and decoded updated illustration
samples. All nine ICO sizes decode; the current 41 native GIF output areas stay
within 5/255 channel error of full-color reference.
This does not establish compatibility with real dashboards, HDR displays,
every application or human mouse/keyboard usability.
