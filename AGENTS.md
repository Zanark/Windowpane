# WindowPane contributor contract

WindowPane is a native, local-only Windows window-region mirror. Keep it generic:
no browser login, cookie/token extraction, website scraping, telemetry, network
access, hidden recording, startup task or permission bypass.

Read `TODO\00_INDEX.todo` and the relevant workstream before implementation.
Workstreams use all eight fields: Status, Milestone, Dependencies, Purpose,
Tasks, Acceptance criteria, Completion evidence and Completion log. Keep actual
evidence distinct from environmental qualification and user acceptance.

Use C++20, Win32/C++/WinRT, the Windows SDK, CMake and the repository formatter.
Use precise explicit errors, RAII and the existing status/diagnostic paths.
Do not introduce a browser runtime or third-party dependency for a small feature.
The source-window capture surface is in memory; do not write user frames to disk.

Run `scripts\build.ps1 -Test` for behavioral changes. The native integration
harness is allowed to capture only its original fixture HWND. Do not remove its
source guard, capture private desktop/browser contents or weaken tests to make
them pass. Documentation images must be owned synthetic surfaces.

Update the affected guide, numbered workstream and causal development ledger
alongside the code. Private `.agent-context` is ignored and excluded from public
source and packages. Only the current base's context store is writable.

Do not commit, push, publish, install, sign, enable auto-start or change system
security settings without explicit authorization. A local preview is not a
released or universally compatible application.

## Developer-only GitHub tracking

The application still has no network access. The explicitly adopted maintainer
tracking tool is separate: read `docs\github-tracking-adoption.md` and
`scripts\todo-sync.json` before synchronizing. Local numbered workstreams own
managed descriptions/completion; preserve stable IDs and `TODO\github-sync-state.json`.
One adopted workstream maps to one issue and one issue-containing Project item.
Index 00 and ledger 05 are excluded. Review public-safe content, preview, then
explicitly apply after authorized workstream changes, completion, reopening or
cancellation. Unknown new files require deliberate configuration/adoption.
Use the shared credential runner, never inspect/copy its credential file.
No scheduler or Actions sync exists. Human conflicts block that item; preserve
pending progress, comments and unmanaged body text. Never infer remote deletion.
Backlinks/mappings remain local until a separately authorized commit/push.

Future authorized commits must use repository-local `user.name=Zanark` and
`user.email=32354314+Zanark@users.noreply.github.com`; never change global identity.
Check both `git var GIT_AUTHOR_IDENT` and `git var GIT_COMMITTER_IDENT` immediately
before committing, including possible environment overrides. Preserve required
co-author/session trailers. Attribution configuration is not commit authority.
