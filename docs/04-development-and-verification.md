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
nine PNG reading/capture assets and an ignored `build\demo-review.png` sheet.
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

## Resource interpretation

The native harness measures a five-second controlled repaint workload with a
180x110 crop at a selected rate of 5 fps. It records received frames, process CPU
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
No commit, push, upload, auto-start, application installation or policy change
is part of this milestone. Human acceptance is pending.

The ignored `.agent-context` contains owned local continuity and must not enter
packages. Build/test outputs remain separate from first-party source.
