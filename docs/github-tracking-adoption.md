# GitHub tracking adoption

## Initial inventory and authorized preview - 2026-11-06

Canonical public repository: **Zanark/Windowpane**, node `R_kgDOVkY5XQ`.
The starting worktree was clean at `11ddfe0` (`First prototype`).
The shared credential runner identified Zanark with repository admin/write
access. No credential file was inspected or copied. Complete paginated-capable
GraphQL reads returned zero issues and no repository-linked Projects.
The owner's only existing Project was BowserBOY, linked to another repository;
it is not an appropriate destination and remains unchanged.

No synchronizer, adoption record or mapping existed in this repository.
The manual supplies a credential runner, not synchronization. Adopt a small
standard-library Python/`gh` tool with explicit preview/apply and no scheduler.

| Workstreams | Reviewed local meaning | Planned issue / Project action |
|---|---|---|
| 01-04, 06-10 | Completed local implementation/qualification/documentation | Create 9 issues, closed/completed, Done |
| 11 | Completed **local portable preparation**, not release publication | Create 1 issue, closed/completed, Done |
| 12 | Completed **local manual-workflow authoring**, not hosted activation | Create 1 issue, closed/completed, Done |
| 13 (this adoption) | In progress until integration/backfill read-back | Create 1 issue, initially open/In progress; close only after completion |
| 00_INDEX | Navigation, not an actual feature | Exclude |
| 05_DEVELOPMENT_LEDGER | Perpetual causal history, not an actual feature | Exclude |

Planned new owner Project: **WindowPane**, linked to this repository. Preserve
default fields/options and add missing Backlog, Ready, In progress, In review
and Done stages. No conflicting historical workstreams were found.
Historical dates remain attributed body content; GitHub creation/closure dates
will be today's actual operations, not backdated.

The user authorizes tracking setup and issue/Project writes, not product work,
commits, pushes, workflow dispatch/activation, releases or remote secrets.
The application's no-network contract remains unchanged: this is optional
maintainer tooling, never application runtime behavior.

## Adopted destination and files

Project: **https://github.com/users/Zanark/projects/2** (owner Zanark, private
default visibility retained). Its repository association is `Zanark/Windowpane`.
Public-safe issue descriptions stay in the public repository; no private board
content is copied back into them. The canonical repository is checked by node
ID, not the checkout's name.

- Tool: `C:\devdesk\homedesk\Windowpane\scripts\todo_sync.py`
- Configuration: `C:\devdesk\homedesk\Windowpane\scripts\todo-sync.json`
- Durable mappings/baselines: `C:\devdesk\homedesk\Windowpane\TODO\github-sync-state.json`
- Contributor contract: `C:\devdesk\homedesk\Windowpane\AGENTS.md`
- This guide: `C:\devdesk\homedesk\Windowpane\docs\github-tracking-adoption.md`

The configuration records real repository/Project/Status/option IDs and an
explicit public workstream allowlist. Existing Todo, In Progress and Done
options/IDs are retained. The logical **In progress** stage uses GitHub's
existing **In Progress** spelling; Backlog, Ready and In review were added.
No other board fields, unrelated Projects or permissions were modified.

## Commands and invocation contract

Run from `C:\devdesk\homedesk\Windowpane`. Python's standard library and `gh`
are the only dependencies. The credential runner resolves its own external
ignored credential source; do not open it, copy it here, print tokens or
set global `GH_TOKEN`. Review the tool/configuration before credentialed use.

```powershell
# Read-only inventory and proposed operations (the default).
python "C:\devdesk\homedesk\skill-library\skills\development-documentation-manual\scripts\with_github_token.py" -- python "C:\devdesk\homedesk\Windowpane\scripts\todo_sync.py"

# Explicit authorized writes plus issue/body/state/item/status/backlink read-back.
python "C:\devdesk\homedesk\skill-library\skills\development-documentation-manual\scripts\with_github_token.py" -- python "C:\devdesk\homedesk\Windowpane\scripts\todo_sync.py" --apply

# Select a bounded pilot or independently reconciled workstream.
python "C:\devdesk\homedesk\skill-library\skills\development-documentation-manual\scripts\with_github_token.py" -- python "C:\devdesk\homedesk\Windowpane\scripts\todo_sync.py" --apply --only wp-01

# Offline behavior/failure tests, dummy fixtures only.
python -m unittest discover -s "C:\devdesk\homedesk\Windowpane\scripts\tests" -p test_todo_sync.py -v
```

Future agents invoke preview, inspect the source/public projection and conflicts,
then apply under this adopted issue/Project authority after substantive TODO
creation, description/task/evidence changes, completion, reopening or
cancellation. This authority never includes unrelated product work, secrets,
workflow activation, commit/push or release. A new numbered workstream must keep
all eight fields and be explicitly added with a unique, permanent `wp-NN` ID
after privacy/status review. Renames update the configured path, **not** the ID.
Repository renames require canonical-name review while retaining the node ID,
stable markers and mappings. Unknown files are reported unadopted, not exported.

Local metadata `Workstream-ID` and `GitHub-Issue` is excluded from descriptions,
preventing backlink feedback loops. All eight fields, task state and historical
dates are rendered in a bounded issue section. The disclosed public projection
replaces checkout-root absolute paths with repository-relative paths and adds a
source link; it is not a claim of byte-for-byte copying. Historical claims such
as earlier read-only access remain dated evidence, not current account claims.
The tool reads only configured TODO files, never private context or transcripts.
Pattern checks reject token-shaped text and personal absolute paths, but are
**not** comprehensive secret detection or permission to skip semantic review.

Backlinks and JSON mappings must travel with the corresponding TODO changes
when a commit/push is separately authorized. They are intentionally left local
by this migration; GitHub source links to unpushed changes are not publication.
Do not regenerate the sealed portable/source release bundle to include them.

## Exact local status policy

| Exact Status | Issue state / closure reason | Logical Project stage |
|---|---|---|
| NOT_STARTED or DEFERRED | Open | Backlog |
| READY | Open | Ready |
| IN_PROGRESS | Open | In progress |
| IN_REVIEW | Open | In review |
| DONE | Closed / completed | Done |
| CANCELLED or SUPERSEDED | Closed / not planned | Backlog, explicitly **not completed** |
| BLOCKED | Open | Explicit `Blocked-stage`, with `Blocker` description |

`BLOCKED` needs nonempty `Blocked-stage: Backlog|Ready|In progress|In review`
and `Blocker: ...` metadata in the workstream, mirrored into the managed body.
No substring matching is used. Composite/historical phrases stop for review;
only an explicitly reviewed exact-value entry in `reviewed_statuses` maps them
to a standard state. DONE covers the defined acceptance, not user approval,
hosted qualification or publication. Reopening updates both remote dimensions.
Closed-reason transitions use the introspected atomic state/reason API.

## Ownership, conflicts and recovery

Local TODOs own managed issue titles, bounded descriptions and lifecycle plus
Project Status. A baseline stores managed-body hashes and both status dimensions.
Human edits within those managed fields, manual closure/reopening, board moves,
removed/archived items, marker loss and duplicate IDs are conflicts, not orders
to rewrite files. The affected item stops; independent safe ones continue.
Comments, labels, unrelated body prefix/suffix and other board fields are
untouched. There is no two-way board-to-file automation.

Stable markers are `<!-- windowpane:wp-NN:begin -->` / `:end -->`. Mappings save
issue/node/item IDs, URL and baseline. Before mutations, atomic JSON persistence
records intended progress. A retry allows only the last baseline or that saved
intent; an interrupted create is discovered by stable marker before any create
retry. Already completed remote work is never destructively rolled back.
Backlink/state writes use same-directory temporary files and atomic replacement.

Read failures/permissions/rate limits are explicit errors, never empty
inventories. Read-only transient requests retry at most three times; mutations
never retry blindly. If an uncertain create/add is not yet visible, the item
stops. Rerun after eventual consistency settles. If repeated complete reads prove
the operation created nothing, an authorized maintainer may remove **only** the
specific `creating` or `adding_item` intent after recording that reconciliation;
do not clear the whole mapping or retry without this evidence.

For human conflicts, compare the TODO, live issue/Project and saved baseline.
Preserve the human content or deliberately reconcile the desired state; record
the decision, then update only that reviewed baseline component to the verified
live value before previewing/applying again. Never routinely reset baselines to
silence conflicts. Keep pending progress until reconciled and read back.
Deleted local files or removed configuration entries retain mappings; no remote
issue/item deletion is implemented or authorized.

An exclusive local `.todo-sync.lock` records the writer PID. A stale lock may be
removed only after confirming that exact process is no longer writing.
Atomic server-side compare-and-swap is not available across all GitHub
issue/Project mutations: an edit racing the last read can still occur.
Do not run competing remote writers; local locking is not a distributed lock.

The summary distinguishes creates/reuses, issue totals, no-ops and fully
verified mappings. A nonzero exit or `BLOCKED` means partial/incomplete, even if
some issues were created. Keep the receipt and rerun after resolving its cause.
No service, scheduler, webhook or Actions workflow was added/activated; this is
explicit invocation, not unattended synchronization.

## Pilot evidence - 2026-11-06

Offline suite: 22 tests passed, including idempotence, reopening, cancellation,
blocked stage, human-body preservation/conflicts, independent continuation,
uncertain/partial issue/Project/backlink recovery, pagination, missing files,
renames and no credential echo. Fixtures use dummy values.
Additional cases cover uncertain successful Project addition and unmapped
local backlinks, which must block before creating any replacement issue.

The all-workstream preview had zero conflicts and proposed 11 completed
historical issues plus the in-progress adoption issue. The bounded `wp-01`
pilot created **issue 1** and item `PVTI_lAHOAe2wCs4BqePpzhGjwTc`.
Managed content, closed/completed lifecycle, Done option and local backlink
matched read-back. The explicit pilot rerun reused the same IDs and left issue
`updatedAt` exactly `2026-11-06T12:25:25Z`: no issue content churn.
The subsequent backfill created 11 more issues and reused the pilot: 12 issues,
12 issue-containing Project items and 12 persistent local backlinks verified,
with zero conflicts. At that checkpoint 11 were closed/completed/Done and the
adoption issue was open/In Progress. A following all-workstream preview was
no-op. Final completion totals are recorded below after the adoption closes.

## Final reconciliation - 2026-11-06

The bounded adoption subsequently completed and synchronized issue 12 to
closed/completed/Done. A fresh complete GraphQL inventory and a full explicit
rerun verified:

| Result | Verified value |
|---|---|
| Included actual workstreams | 12: 11 historical plus this adoption |
| Issues created / preexisting reused | 12 / 0 |
| Backfill reuse after pilot | 1 pilot issue/item |
| Final open / closed issues | 0 / 12, all closed with completed reason |
| Project membership / Status | 12 unique issue-containing items / all Done |
| Persistent local backlinks | 12 exact stable-ID / issue-URL pairs |
| Full rerun | 12 reused, 12 no-ops, 0 creates, unchanged issue timestamps/item IDs |
| Exclusions | 00_INDEX.todo; 05_DEVELOPMENT_LEDGER.todo |
| Conflicts / partial operations / blockers | 0 / 0 / none |

Issue mapping is permanent in `TODO\github-sync-state.json`: wp-01 through wp-04
map to issues 1-4; wp-06 through wp-13 map to issues 5-12. Preserve IDs rather
than assuming issue numbers equal file numbers.

All 11 historical workstreams compare exactly with the starting commit after
removing only the two tracking metadata lines and normalizing line endings.
The eight-field descriptions, task/evidence history and completion boundaries
are preserved. The final public-payload/mapping checks found no pending writes,
token-shaped content, personal absolute paths, credential/context file in the
changes or edits to native source/release workflow. New-file whitespace and
`git diff --check` pass. The 22 offline tests are developer-tool behavior tests,
not hosted/native capture qualification; no app behavior changed.

Tool/configuration, guide, mappings and backlinks remain **local and unstaged**.
No commit, push, workflow activation/dispatch, release, remote secret or
scheduler was performed. The sealed preview bundle remains unchanged.
Repository-local Zanark attribution was set and both effective identities
verified; global identity/history remain untouched.
