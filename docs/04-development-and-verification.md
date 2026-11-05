# 4. Development and verification

## Build

Prerequisites: Windows x64, Visual Studio 2022 with C++ desktop tools, Windows SDK
10.0.26100.0 (including C++/WinRT headers), and its CMake component.
`scripts\build.ps1` finds the installation with `vswhere`; it does not install
tools, download packages, alter PATH or change system security settings.

```powershell
.\scripts\build.ps1 -Test
```

CMake builds with C++20, `/W4 /WX`, the static C++ runtime and an embedded
per-monitor-v2/asInvoker manifest. Artifacts are under `build\Release`.
The runtime uses system Windows DLLs; there is no bundled browser or separately
installed application framework.

Formatting uses the checked-in `.clang-format`. With Visual Studio's LLVM
component:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\bin\clang-format.exe' --dry-run --Werror src\app.cpp src\capture.cpp src\capture.hpp src\pixels.cpp src\pixels.hpp tests\pixels_tests.cpp
git diff --check
```

Paths to tools can differ by Visual Studio edition/installation.

## Evidence layers

`WindowPaneTests.exe` performs focused pure-function checks: reversed/clipped
crops, negative monitor coordinates, source bounds, opacity with existing alpha,
exact keying, feathering, aspect fit, scaled sampling and invalid buffers/sizes.

CTest also runs the actual `WindowPane.exe --self-test` path. It creates an
original synthetic Win32 source and sends messages through the real selection,
slider, color-picker, native hit-test, hide/show and recovery handlers. It uses
actual WGC frames and `UpdateLayeredWindow`; two guarded fixture-only screen
sample points verify desktop blending.

The native test checks source repaint, minimized/hidden restoration, resizing,
closure and explicit clearing of unavailable pixels. Its source guard forbids
capturing any HWND except the fixture. The harness does not capture an account,
browser, private desktop, live camera or protected application.

`scripts\smoke-launch.ps1` separately starts the ordinary executable (with its
tray/global-shortcut setup), closes its controls to the tray, requests controls
recovery and exits through its own menu command. It refuses to interfere with
an already-running WindowPane and cleans up only the process it started.

This is not a substitute for human mouse/keyboard usability review, broad
application testing, HDR testing, multi-monitor/mixed-DPI testing or clean-machine
installation. Hit-test/event-handler checks are not a claim that every possible
drag path has been physically exercised.

## Reproducible visual evidence

The native test creates these files under `build`:

- `native-test.txt`: pass/failure and bounded resource observations.
- `synthetic-original-pane.bmp`: native opaque captured crop.
- `synthetic-keyed-pane.bmp`: actual premultiplied keyed output.
- `synthetic-controls.bmp`: only WindowPane's owned controls surface.
- `synthetic-recovery-pane.bmp`: native resized-source message and Snip again action.
- `synthetic-grip-pane.bmp`, `synthetic-grip.bmp`, `synthetic-grip.json`:
  actual content/grip surfaces and their relative geometry.

Regenerate public-safe PNGs from those specific fixture files:

```powershell
.\scripts\export-test-images.ps1
```

The exporter retains alpha PNGs and creates checkerboard-backed reading copies.
It does not scan arbitrary screenshots or capture a desktop. Source and
derivatives remain distinguishable. Public images must never be replaced with
private account or desktop captures.

### Animated README demonstrations

The explicit `--demo-assets` mode runs the same guarded native fixture checks
and then exports live, opacity-slider, picker and aspect-fit sequences, plus
three owned-controls captures. Only this mode creates `demo-frames`; ordinary
application use does not export imagery.

After building, run from the repository root:

```powershell
$report = Join-Path $PWD 'build\demo-test.txt'
$process = Start-Process .\build\Release\WindowPane.exe `
    -ArgumentList '--demo-assets', ('"' + $report + '"') -PassThru
if (-not $process.WaitForExit(45000)) {
    Stop-Process -Id $process.Id
    throw 'Owned synthetic demo exceeded 45 seconds.'
}
if ($process.ExitCode -ne 0) { throw 'Read build\demo-test.txt for the failure.' }
python .\scripts\export-demo-media.py
if ($LASTEXITCODE -ne 0) { throw 'Documentation media encoding failed.' }
```

The optional encoder requires Python and Pillow (locally used: Pillow 12.2.0);
neither is required to run WindowPane. It reads explicitly named BMPs from
`build\demo-frames`, unpremultiplies native BGRA, and creates four looping GIFs,
ten PNG reading/capture assets and an ignored `build\demo-review.png` sheet.
It also reads the named grip surfaces/geometry beside `demo-frames`, validates
their bounds and combines them into `docs\assets\corner-grip.png`.
The [asset provenance](assets/README.md) records source roles and GIF timing.

Native exports are real capture/compositor states driven by app event handlers
and window-size calls, not physical mouse recordings. Checkers, labels and
presentation timing belong to the documentation. Do not infer update latency
or performance from GIF playback.

The 2026-11-06 media work used a separate `build\media` CMake output because the
ordinary preview executable was already running and Windows refused to replace
it (`LNK1104`). That process was left untouched. The extended export initially
stalled when destroying its fixture with capture still active. Its teardown now
explicitly hides the demo pane to release capture before destroying the owned
source. The ordinary native suite independently retains source-closure coverage.
The successful demo run passed 27 checks; the standard suite still passed its
18 pure cases and 26 native checks.

### In-pane recovery regression

The subsequent recovery-button change extends the native suite to **36 checks**.
It exercises actual client hit testing, mouse down/up and release outside,
Enter/Space activation, minimum-size layout, temporary click-through suspension,
selection cancellation after source resize/closure, and a successful fresh snip.
The synthetic recovery surface is exported by the ordinary native suite and
converted by `scripts\export-test-images.ps1`.

The standard 18 pure cases and 36 native checks passed on the development machine.
These event-path checks and the inspected native recovery screenshot do not
replace human physical interaction or broad mixed-DPI qualification.

### Contextual animated illustrations

```powershell
python .\scripts\export-illustrations.py
```

This optional documentation-only Pillow script draws fictional source/workspace
windows, build-progress data, selection outlines, connector lines and a floating
pane. It never captures an app or reads the user-supplied reference screenshot.
The two GIFs are explicitly labeled illustrations, not recordings or native
exports. Their frame sequence/timing is authored rather than measured.
Decoded-sample inspection and total-duration/motion checks qualify these files
as documentation artwork, not as app-behavior evidence.

### Branding and independent corner grip

```powershell
python .\scripts\export-branding.py
```

This documentation/build-time Pillow script creates the original SVG/PNG logo
and ICO at 16, 20, 24, 32, 40, 48, 64, 128 and 256 pixels. The executable embeds
the ICO and loads branded controls/tray icons; no external artwork or runtime
image package is required.

The branding/grip milestone extended the native suite to **44 checks**, retaining
the 18 pure cases. Its new
checks cover executable icon extraction, actual grip/pane drag movement and
anchoring, Escape cancellation, a real right-click-generated options popup,
interaction/dragging with zero-opacity click-through content, and
close/hide/show/snipping/picking lifecycle. Compact recovery must remain unobstructed.
These synthetic native event paths are not human physical-interaction approval.
The local Release run passed 18 pure cases and 44 native checks; the optional
media run passed 43 checks, and ordinary startup/tray/recovery/exit smoke passed.
The startup helper compares handles explicitly with `IntPtr.Zero`; PowerShell's
Boolean conversion treats that zero-valued struct as true, which previously
could skip its readiness wait on a slower launch.

The native grip card combines two actual exported surfaces rather than taking
a desktop screenshot. Other native pane GIFs export only captured-content
surfaces, not the separate grip HWND. Both authored contextual animations now
include an illustrative grip. Its frosted-look styling is a translucent tint,
not true desktop blur.

### Default update rate

The default and highest selectable target is **60 fps**, with 1, 5, 10 and
30 fps also available. Capture and controls share the allowed-rate list.
The capture session requests intervals in native 100-nanosecond units, avoiding
whole-millisecond truncation at 30/60 fps. UI polling still uses a best-effort
Windows timer; this does not establish measured throughput.

The rate update passed **18 pure cases and 47 native checks**, including
new-session 60 fps selection and live 30/60 fps changes delivering repainted
fixture pixels. Optional demo export passed 46 checks; ordinary startup/tray/
recovery/exit smoke passed. Refreshed controls imagery shows 60 fps selected.

## Resource interpretation

The native harness measures a five-second controlled repaint workload with a
180x110 crop at an explicitly selected rate of 5 fps, then restores the 60 fps
default before exporting controls. It records received frames, process CPU
time as a percentage of one logical core, working-set bytes and private bytes.
Capture initialization, DWM/GPU work outside this process, larger sources and
different devices can have different costs.

The observed first-preview values belong to that local fixture run, not a
performance budget or universal guarantee. Refer to the dated workstream
evidence and current `build\native-test.txt` for exact measurements.

The 2026-11-06 local run on Windows version 10.0.26200.0 passed 18 pure cases and
26 native integration checks. Its 5016ms workload received 22 frames, observed
0.623006% CPU relative to one logical core, 48,328,704 working-set bytes and
30,261,248 private bytes. The local unsigned executable was 391,168 bytes.
Do not treat these measurements as a clean-machine or whole-system GPU budget.

## Delivery boundaries

The output is a local unsigned executable, not an installer or public release.
The user subsequently requested a release, then selected local manual-upload
files because the current GitHub access is read-only. The prepared
`build\releases\v0.1.0-preview.1` directory contains a portable ZIP, standalone
EXE, explicit public-source snapshot, checksums, release notes and publishing
instructions. No commit, push, tag, upload, auto-start, installation or policy
change was performed. Human acceptance is pending.

The source snapshot captured public changes at preparation time and excludes
private context, build outputs, git internals and caches. Before publishing a
version tag, commit/push the matching source with an authorized account; do not
tag the older branch head as though it contains these features.

The ignored `.agent-context` contains owned local continuity and must not enter
packages. Build/test outputs remain separate from first-party source.

## Manual GitHub release workflow

`.github\workflows\release.yml` uses **workflow_dispatch only**. After an
authorized maintainer commits/pushes it to the default branch, open **Actions →
Manual preview release → Run workflow**, select the source branch/ref, and
optionally enter a new tag. Blank input becomes `preview-<run-id>`.

The Windows 2022 job builds with the existing script and runs only CTest's
`pixels` test. The inspected runner-image manifest lists Visual Studio 2022
and SDK 10.0.26100.0. It is not treated as an interactive capture desktop:
`scripts\build.ps1 -Test` and startup/tray smoke remain local checks.

The read-only build packages `WindowPane.exe`, the tracked
`docs\portable-README.txt` and a portable ZIP, then produces SHA256SUMS.txt.
The separate publisher alone receives `contents: write` through GitHub's
automatic token; no personal access token is required. Repository/organization
policy must permit those operations. Third-party actions are pinned to commits.
The checksum manifest explicitly uses LF line endings so the Linux publisher
does not interpret Windows carriage returns as part of asset filenames.

Publication creates a fresh tag at the exact built commit; an existing tag
fails rather than being reused or overwritten. It creates a draft pre-release,
uploads the EXE/ZIP/checksums, downloads and verifies them, then publishes.
GitHub's automatic source archives therefore refer to the built commit.
Generated release notes summarize committed changes; the preamble identifies
unsigned preview status and headless-only CI qualification.

A failure after tag reservation can leave a tag or draft behind, but not a
successfully published incomplete release from this workflow. Inspect it
manually; start a new run (new automatic tag) or choose a fresh custom tag.
Rerunning the same failed run after it has reserved its tag intentionally does
not overwrite that tag. Workflow logs expose failures; no automatic cleanup
deletes release history.

Writing this workflow locally does not install it remotely or authorize an
authentication switch. The current account has read-only GitHub access; no
push, live workflow run or publication has been performed here.

Local qualification ran the actual Windows build/test/packaging blocks,
accepted automatic/custom tags and rejected invalid/path-like inputs, inspected
ZIP contents, and verified the manifest with GNU sha256sum. actionlint 1.7.12,
publisher Bash syntax and documentation/whitespace checks pass. These are local
checks, not a successful hosted build or GitHub publication receipt.
