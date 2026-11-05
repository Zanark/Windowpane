# Visual asset provenance

All source imagery is original WindowPane synthetic-fixture or owned-controls
output. No authenticated portal, user desktop, private account or third-party
GIF is used. This is a bounded feature reference, not a complete GUI inventory
or human acceptance record.

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

Titles, state labels and checkerboards are added by the documentation encoder.
They do not appear in the borderless pane. Window controls images are not
reconstructed or redrawn by that encoder.

GIF presentation is deliberately paced: live frames at 550 ms, opacity at
150 ms, key states at 1,600 ms and resize at 200 ms. These timings do not claim
capture latency, measured frame rate or real mouse movement. Resize is driven
by native window-size calls, not a recorded physical drag. GIF uses a 256-color
palette; the PNG stills retain the richer-color reading copies.

## Reproduce

Follow [Development and verification](../04-development-and-verification.md#reproducible-visual-evidence).
The exporter uses Python and Pillow for documentation only; they are not app
runtime dependencies. Raw native BMP inputs stay in ignored `build` output.

The native harness accepts only its fixture HWND as a capture source.
Controls are captured with `PrintWindow`; it does not take a desktop screenshot.
The encoder accepts only the explicitly named bounded top-down BGRA inputs.
It unpremultiplies native channels before compositing onto a checkerboard.

## Review scope

2026-11-06: visually inspected the four-state montage, all three full controls
captures, the live/opacity/key/aspect-fit representative states and sampled
decoded GIF frames. Structural checks cover decoding, expected frame counts,
durations, dimensions, loops, changing frames and README image paths.
The GIF palette is derived from all sequence states. Decoded native-output
areas were compared with full-color references (at most 12/255 channel error);
this caught and corrected an initial dark-color quantization problem.
This does not establish compatibility with real dashboards, HDR displays,
every application or human mouse/keyboard usability.
