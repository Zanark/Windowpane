# WindowPane memory

> WindowPane desktop-engineering base. Preserve decisions and honesty across sessions.

## People

- The user selected WindowPane, rejected custom-app sign-in, and requested a
  generic visual mirror rather than a site-specific consumption scraper.

## Conventions

- Follow the user-designated development/documentation manual.
- Numbered TODO workstreams have all eight required fields, with separate
  technical evidence and user acceptance. Use a development ledger.
- Keep implementation, automated fixture verification, environmental
  qualification, human acceptance and publication distinct.
- Share full absolute artifact paths in handoffs.
- Keep source and fixtures public-safe; do not reproduce the supplied internal
  portal screenshots in repository documentation.
- Developer-only tracking is adopted separately from the no-network app:
  scripts\todo_sync.py plus scripts\todo-sync.json, mappings/baselines in
  TODO\github-sync-state.json, guide docs\github-tracking-adoption.md.
  Use the external shared credential runner; never access its secret file.
  Preview then explicit apply after approved workstream updates.
- IDs/managed markers survive renames; index 00 and perpetual ledger 05 are
  excluded. Local workstreams own managed content/completion; human remote
  edits conflict, not silent overwrite. Keep pending progress and never infer
  remote deletion. No scheduler or two-way automation is installed.

## Gotchas

- Mirroring does not refresh website data. The selected source window/tab must
  remain alive and rendering. A geometric crop is not a DOM/text anchor.
- PowerToys Crop And Lock was tried by the user. Installed 0.102.3 has no
  implemented opacity property despite a conflicting DSC documentation example.
- Basic DWM thumbnails do not provide the requested per-pixel color masking;
  the first implementation uses explicit capture/composition instead.
- WGC may queue differently sized frames across minimize/restore. Do not equate
  ContentSize mismatch alone with a true user resize; check source geometry.
- Source capture is whole-window GPU memory, crop-only CPU readback. Do not
  claim OS capture excludes all pixels outside the crop.
- Tests use original fixture HWND guards and safe compositing sample points;
  never remove them to make a run pass or capture a private desktop for docs.
- Build with scripts\build.ps1 -Test; native startup/tray smoke is
  scripts\smoke-launch.ps1. Export only fixture imagery with
  scripts\export-test-images.ps1. CMake does not require external packages.
- Windows composition expects already-premultiplied BGRA. Do not multiply by
  original source alpha again when applying opacity/key coverage.
- Color-keying can remove any matching foreground color, not only backgrounds.
- Invalid-source recovery must remain clickable even if click-through was
  selected. Its button uses client hit testing on the drawn layered surface.
- Cancelling re-snipping after resize/closure must restore the unavailable
  placeholder, not start capture on an invalid crop or dead HWND. Typed source
  phases distinguish that path from cancellation of a healthy live pane.
- README contextual GIFs are authored illustrations; native-output GIFs are
  actual synthetic compositor exports. Never describe either as mouse recordings.
- The corner grip must remain a separate owned layered HWND: content opacity,
  color-keying and WS_EX_TRANSPARENT must not erase or disable the drag/options
  anchor. Hide during snipping/picking and yield to overlapping compact recovery.
- Frosted-look grip means procedural translucent tint, not true backdrop blur
  or desktop sampling. Native content-only GIFs omit this separate HWND.
- ExtractIconEx requesting large and small handles returned 2 locally; verify
  a positive result plus both handles, not a result of exactly 1.
- PowerShell casts IntPtr.Zero to true. Native readiness checks must compare
  handles explicitly with [IntPtr]::Zero rather than use Boolean truthiness.
- CaptureRates is shared by capture validation and controls; its maximum is the
  default target. Native TimeSpan intervals retain sub-millisecond precision.
  The resource fixture explicitly selects 5 fps, separate from the 60 fps default.
- Cross-platform release checksums must use explicit LF, not PowerShell's Windows
  default CRLF; GNU sha256sum can interpret carriage returns as filename bytes.
- Hosted release CI runs pixel tests only; native WGC/UI qualification requires
  a local interactive desktop. Do not equate headless CI with capture acceptance.
- gh project list can fail "unknown owner type" with this credential despite
  direct GraphQL Project reads/writes succeeding; do not infer missing access.
- GitHub uses existing In Progress casing for the logical In progress stage.
  Keep actual option/node IDs in the configuration, never guess from labels.

## Context-offload protocol

- Store is `.agent-context` in this designated base. Siblings remain read-only.
- Update STATE and newest-first JOURNAL at milestones; never exceed twelve
  active hours without offload. Refresh Context Mesh after a coherent batch.
- Session identity is `3ebcd26e-abc2-4e29-a4ee-e5d874d8a1bb`.
