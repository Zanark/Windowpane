# WindowPane state

> **Last updated:** 2026-11-06 (Asia/Kolkata) - Tracking adoption complete and live idempotence verified.
> **Last context offload:** 2026-11-06T18:02:53.8820760+05:30
> WindowPane desktop-engineering base. Resume the same evidence-grounded working identity.

## Current focus

User supplied the complete updated manual/tracking migration prompt, authorizing
developer tracking, repository-local attribution and issue/Project writes, not
commits/pushes/dispatch/schedules/releases or deferred product features.
Stale-handoff reconciliation is ep_1062. Starting clean main was 11ddfe0
(First prototype); another actor committed the prior product/workflow files.
This migration's tool/config/docs/backlinks remain local, unstaged.

Reviewed shared credential runner only, never its secret file. Runner-backed
GitHub account is Zanark, admin/push on public Zanark/Windowpane,
repository R_kgDOVkY5XQ. Initial complete inventory: zero issues, no linked board,
one unrelated owner Project BowserBOY (untouched). gh project list fails unknown
owner type; GraphQL works.
Created private-default owner Project 2 WindowPane, linked to this repository:
https://github.com/users/Zanark/projects/2
ID PVT_kwHOAe2wCs4BqePp. Default fields/options/IDs preserved; added Backlog,
Ready and In review. Logical In progress uses existing In Progress casing.

Implemented reviewed dependency-free scripts\todo_sync.py, scripts\todo-sync.json,
TODO\github-sync-state.json and docs\github-tracking-adoption.md. Preview default,
explicit --apply, stable IDs/markers, exact status mapping, atomic recovery state,
human-edit conflict isolation, no deletion or unattended automation.
22 focused offline tests pass. wp-01 pilot issue 1/item identity/body/lifecycle/
Done/backlink verified; rerun unchanged updatedAt 2026-11-06T12:25:25Z.
Backfill created 11 further issues, reused pilot; adoption issue 12 subsequently
completed. Final independent GraphQL inventory: 12 issues (0 open, 12 closed/
completed), 12 unique issue-containing Project items all Done, 12 exact persistent
backlinks. Full apply rerun: 12 reused/no-ops, 0 creates, unchanged issue updatedAt
and item IDs. No conflicts or pending operations. Exclude index00 and ledger05.
All 11 historical workstreams unchanged except two metadata lines/newlines.
Public-payload/mapping/new-file-whitespace and git diff --check pass. No
credential/context file enters public changes. Guide/index/ledger all updated.
No current tracking blocker. Next authorized product task is not yet selected.
Final verified adoption handoff persisted as ep_1063.
User-requested reference-thread reply windowpane-tracking-lessons-20261106
adds this adoption's verified example and recovery/privacy limits to the open
TODO/issue/Project discussion. This is local MeshBoard publication only, not a
new remote GitHub inventory or source publication.
No native behavior or release YAML changed; sealed release bundle untouched.
Repository-local attribution and both effective identities verified; recheck
before any future separately authorized commit. No global identity change.
Active feed baseline eight messages, no owned attention; Nov6 check-in retained.

### Previous product/release handoff (historical)

The first native desktop preview is implemented, locally qualified and ready
for user review. Workstreams 01-12 are DONE for the preview and visual guide.
User chose manual workflow runs only for release automation. Added
C:\devdesk\homedesk\Windowpane\.github\workflows\release.yml and tracked
C:\devdesk\homedesk\Windowpane\docs\portable-README.txt.
The action builds/tests/packages Windows x64, then a separate contents-write
job creates a fresh exact-source tag, verifies uploaded draft downloads and
publishes an unsigned preview. Its hosted execution was not verified here.
At the earlier workflow handoff the account was read-only; the current runner
has write access but no dispatch/push authority in this migration.
User requested a release; earlier GitHub access was read-only. User chose
local release files for manual upload. v0.1.0-preview.1 is prepared at
C:\devdesk\homedesk\Windowpane\build\releases\v0.1.0-preview.1.
Includes portable ZIP, standalone EXE, explicit public-source ZIP, checksums,
release notes and manual instructions. No GitHub release was published.
Default update rate is now the maximum selectable 60 fps target; alternatives
are 1/5/10/30 fps. Actual cadence depends on source rendering and Windows timing.
README now includes two contextual animated illustrations, four native demo
GIFs, eleven screenshots/reading cards and the original SVG logo. Original
multi-size icon is embedded in the executable and used for controls/tray.
The upper-right frosted-look grip supports dragging and right-click options
even with zero-opacity, keyed or click-through content. It is an independent
owned layered popup, not desktop blur. Resized/closed-source placeholders
include a directly clickable Snip again button; compact recovery takes priority
over an overlapping grip.
Media provenance and reproduction are documented; no private content is used.
No commit, push, tag, publication, installation or startup task was performed.
Those source features are now in 11ddfe0; this migration is the uncommitted work.

## Threads

- Product name: WindowPane. A small window into any of your windows.
- Native Windows, local-only live crop mirror. Not an authenticated consumption
  client; no browser integration, cookie extraction, login, telemetry or network.
- First vertical slice: source-region snipping, borderless always-on-top view,
  drag/resize/hide, opacity, color picker with color-key tolerance/soft edges.
- Implemented: C++20/Win32/C++/WinRT Windows Graphics Capture, D3D11 cropped
  readback and CPU premultiplied layered-window composition. No browser/runtime
  package dependencies. One pane at a time; settings are session-only.
- Actual controls: snip, caption-free drag/edge resize/snap, opacity, picked key,
  tolerance/softness, click-through, hide/show, tray and Ctrl+Alt+N/W/O.
- Resized/closed-source recovery: in-pane Snip again (compact Snip), mouse and
  Enter/Space, click-through temporarily suspended, cancellation preserves the
  old placeholder without recapturing an invalid/dead source.
- Corner grip: independent content alpha/input; actual pointer drag and native
  right-click menu. Hides with pane and during snipping/picking; yields if it
  would obscure compact recovery. Frosted-look tint, no actual backdrop blur.
  Closing the grip hides/releases the pane instead of destroying the handle.
- Run C:\devdesk\homedesk\Windowpane\build\Release\WindowPane.exe.
- Manual release bundle:
  C:\devdesk\homedesk\Windowpane\build\releases\v0.1.0-preview.1.
  EXE/portable ZIP match the qualified 450560-byte build; x64 header and unsigned
  status verified. Archive CRC/entry bytes checked, SHA256SUMS generated.
  Source ZIP covers 62 public files plus hashes/metadata, excludes context/
  git/build/cache, explicitly marks uncommitted working-tree snapshot.
  PUBLISHING.txt historically warned to commit/push matching source before
  creating a tag. Do not reinterpret its sealed snapshot as a newer package.
  This sealed bundle predates workflow changes; do not silently rewrite it.
- Release workflow: workflow_dispatch only, optional validated tag; blank uses
  preview-<run-id>. Read-only Windows build and headless 18 pixel checks, not
  native GUI/capture checks on hosted runners. Separate publisher uses automatic
  contents-write token; pinned action commits, existing tags never overwritten.
  Post-reservation failure may leave a tag/draft; inspect manually or use fresh
  run/tag. Local actionlint, actual Windows step execution, invalid-tag cases,
  ZIP contents, GNU checksum verification and Bash syntax pass. Hosted publish
  path unexecuted in this session; current tracking authority excludes dispatch.
- Build/check: .\scripts\build.ps1 -Test. Native fixture metadata:
  C:\devdesk\homedesk\Windowpane\build\native-test.txt.
- Current local result: 18 pure cases +47 native checks pass; optional demo
  passes 46 checks. scripts\smoke-launch.ps1 passed ordinary startup,
  close-to-tray, recovery and exit after the default-rate change.
  Formatting and whitespace checks pass. Actual synthetic images inspected.
- Current preview with default 60 fps is 450560 bytes, SHA256
  417895643CE9874223D0CAD81470E0192E152DB6D69AC06E57A11C481BAB9C3E.
  Earlier 391168-byte preview identity remains recorded in the first journal entry.
- Historical first-preview fixture: 180x110 at selected 5fps for 5016ms, 22 frames,
  process CPU 0.623006% of one logical core, working set 48328704 bytes, private
  bytes 30261248. This excludes external compositor/GPU cost and is not a budget.
- Capture is user-initiated. Test only original synthetic windows, not private
  desktop/browser contents. Never persist captured user frames.

## Open questions

- Real user acceptance and broad application compatibility are not established.
- Source must remain open; minimized/hidden sources clear old pixels and show
  explicit unavailable state. Restore/show resumes; resize requires re-snipping.
  Non-rendering/protected sources can freeze or fail; no policy bypass.
- Mirror never refreshes source website data or anchors to semantic page content.
- HDR, broad app compatibility, mixed-DPI/multi-monitor, clean-machine operation,
  persistence, multiple panes and signed packaging remain outside this milestone.

## Environment reminders

- Visual Studio 2022 Enterprise C++ tools and Windows SDK 10.0.26100.0 are present.
- Windows 10.0.26200.0; CMake is bundled under Visual Studio. Build script finds it
  with vswhere. Static C++ runtime, no extra runtime install.
- Test helpers are stopped. An existing ordinary preview process was found
  running during the media task and left untouched. It locked the normal build
  executable; source was first built/tested separately at
  C:\devdesk\homedesk\Windowpane\build\media\Release\WindowPane.exe.
  That original process later exited independently; the normal build\Release
  path was successfully rebuilt. No WindowPane process is left by this task.
- --demo-assets <report-path> is an explicit guarded-fixture export mode.
  scripts\export-demo-media.py uses already-available Pillow (12.2.0) to generate
  README assets from native BMPs. These tools do not record user windows.
- Extended demo teardown initially stalled on source destruction with capture
  active; hide/release capture before destroying the owned fixture fixes it.
  Earlier demo passed 27 checks before recovery additions; current standard
  suite passes 18 pure +47 native checks, optional demo 46.
- CaptureRates in src\capture.hpp is shared by controls and capture validation.
  Default is its maximum 60; intervals use native 100ns TimeSpan precision.
  New native checks verify selected default and live 30/60 fps source updates,
  not measured 60 fps throughput. Resource fixture explicitly selects 5 fps,
  then restores 60 fps before controls export. Refreshed default image inspected.
- scripts\export-illustrations.py draws original, labeled source-to-pane and
  workspace animations without capturing any app or reading the supplied
  screenshot. Decoded GIF samples reviewed; 16/26 states and 6600/7920ms loops.
- scripts\export-branding.py creates original SVG/PNG and ICO (nine sizes).
  ExtractIconEx with large and small handles returned 2 locally; require a
  positive extraction and both handles, not exactly 1. Resource/class/tray icons
  load from the executable. No runtime Pillow dependency.
- Startup smoke must compare HWND explicitly to IntPtr.Zero: PowerShell casts
  the zero struct to true, which previously could skip waiting on a slower run.
- Workflow checksum output must be explicit LF-only. Initial Windows Set-Content
  CRLF output made GNU sha256sum treat carriage returns as filename characters.
  Fixed with joined LF + Set-Content -NoNewline; actual GNU checks now pass.
- docs\assets\corner-grip.png combines actual native pane/grip surfaces at their
  recorded relative position, not a desktop screenshot. Other native GIFs are
  content-only; drawn illustrations include an illustrative grip. Updated
  decoded samples inspected; all 41 native GIF areas within 5/255 channel error.
- An initial restore bug came from queued animation-frame dimensions. Fixed by
  comparing current source geometry before discarding transient old-sized frames.
- Actual UI inspection caught low-contrast native checkbox text and incomplete
  child repaint; corrected and recaptured before handoff.
- Contributor contract is AGENTS.md; focused docs start at docs\README.md.
- Cross-base resumption episodics: ep_1027 registration, ep_1033 first-preview
  completion, ep_1034 visual README, ep_1035 recovery/contextual illustrations.
  Branding/grip milestone: ep_1037; latest default-rate milestone: ep_1039.
  Latest local manual-release preparation: ep_1041.
  Latest manual-only release workflow handoff: ep_1045.
  Final active board checked: no owned mentions/broadcast obligations;
  no archived conversations were read or revived.
- Publication needs separate explicit authority. Current migration permits
  issue/Project tracking writes only; no commit/push/tag/release/dispatch.
- Active board read at registration: three current general/root messages, no
  active broadcasts. No relevant incoming obligations for the new session.
- Canonical app work is here; the earlier AIConsumptionNotification directory
  was only an empty exploratory location and must not receive new writes.
