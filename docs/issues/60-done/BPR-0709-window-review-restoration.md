---
schema: "implement-rfc.card.v1"
id: "BPR-0709"
title: "Restore each window's accepted pull request"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0708"]
acceptance: [{"criterion":"Actual-process authenticated and anonymous accepted PR restoration","outcome":"pass","revision":"ce63c67b1e82987a6cbec7ac5b60fc7f47300917","evidence":{"kind":"command","detail":"application_profile_window_review_restoration_public_path passes 15.70s across fifteen processes. Exact stable-window PR17/18 snapshots return with fresh generations and retained synthetic vault input, zero new enrollment. Anonymous restart asserts zero raw six-vault and account/bootstrap factory attempts/calls. Missing-grant is the failed-vault-lookup counter negative control. build/dev/bpr0709-public-test.log."}},{"criterion":"Accepted-only persistence and exact Home, Close, Quit and fresh-window behavior","outcome":"pass","revision":"ce63c67b1e82987a6cbec7ac5b60fc7f47300917","evidence":{"kind":"observation","detail":"Same actual-process course independently inspects SQLite records: failed PR99 never replaces latest accepted PR18; Home retains destinations, exact Close removes only its row, Quit flushes latest, fresh monotonic windows remain Home without invented destination."}},{"criterion":"Explicit unavailable states without fallback, stale publication or allocator reset","outcome":"pass","revision":"ce63c67b1e82987a6cbec7ac5b60fc7f47300917","evidence":{"kind":"observation","detail":"Missing grant/server/read, corrupt/future optional store and configured GHES preview refusal leave usable Home and retained records. Held read terminals released across a queued decision barrier cannot replace Home or new PR18. Home false-toggle defect repaired with explicit no-accepted-review Home presentation; auth-home.png inspected."}},{"criterion":"Bounded storage, manual-executor lifecycle and invalid observations","outcome":"pass","revision":"ce63c67b1e82987a6cbec7ac5b60fc7f47300917","evidence":{"kind":"command","detail":"Expanded application_window_review_restoration passes0.32s: coalescing, conflicts, invalid load/save observations, delayed load vs newer read/Close, full32-window capacity/draining reservation and reentrant shutdown. Unchanged checkpoint core0.40s and SQLite0.44s bounds/interruption/profile evidence retained. build/dev/bpr0709-host-test.log."}},{"criterion":"Review, changed-TU checks and tests-disabled production composition","outcome":"pass","revision":"ce63c67b1e82987a6cbec7ac5b60fc7f47300917","evidence":{"kind":"observation","detail":"Prior checkpoint authority/lifetime review retained. Independent Astra-medium bounded Home/fixture review found no production blocker; its zero-attempt evidence gaps corrected, narrowly reviewed and final course rerun. Changed-TU tidy/format, strict inventory and hostile bypass pass. app-release configure/build and RFC0016/0017 test-seam absence scans pass. Documentation and62 verifier regression tests pass. Local logs build/dev/bpr0709-* and build/app-release/bpr0709-*. Native/full RFC and selected-comparison acceptance remain explicitly outside this completed destination slice."}}]
---

# Restore each window's accepted pull request

## Scope

Persist a bounded profile-private set of stable-window destinations containing
only accepted read mode, canonical hostname, repository owner/name and PR number.
No credentials, account claims, provider IDs/snapshots, cache paths or attempted
navigation text. Save only after that window accepts a read. Choosing Home over
an accepted review retains its destination; failed attempted replacement does
not replace it. A fresh window has no destination or fabricated Resume target.
Use a dedicated state/v1/window-restoration.sqlite3 under the profile root, at
most32 window records with revision-aware writes and explicit schema/bounds.

Restore through the exact window read host after the profile's accepted session
matches the saved configured hostname and mode. Core policy decides restoration
and cancellation; the host performs bounded persistence and typed observations;
Qt displays progress/error and translates gestures. Delay the desktop's automatic
remembered-sign-in choice until retained destinations are loaded, so a retained
public-preview session does not read credentials before selecting anonymous mode.
There is one shared profile session: select one startup authority deterministically
from the most-recent retained destination, never race per-window sign-ins or flip
modes repeatedly. Missing configured hosts fail explicitly. Destinations that
conflict with the accepted/chosen shared authority remain unavailable rather than
changing another window's session. Existing user activity wins over late startup
selection. With no saved destination, preserve normal remembered-sign-in startup.

Authenticated restoration may resume only an existing remembered binding, never
start setup/device authorization/enrollment. Public-preview restoration uses the
existing credential-free capability and policy, with no vault/account requests.
Missing grant, failed read, missing server, corrupt/future optional store or
unavailable capability leaves usable Home with an explicit error, and never resets
global window identities. Preserve stored destinations on failed restoration.
Home/new navigation cancels that window's pending restore. Every delayed load,
auth observation and read completion retains exact window context plus request
identity; no late completion may replace a newer user destination.

Coalesce bounded writes and flush latest accepted destinations on orderly Quit;
explicit Close erases its row after writers drain. Startup reconciles orphan rows
against the complete authoritative window set. Keep comparison/file/semantic
location and native geometry out of this slice. Browser tabs remain unscheduled;
RFC0026's later Home/Resume interaction is not claimed here.

Closeout [[BPR-0595]], immutable RFC_BASE_SHA
`e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns policy/port agreement, CMake, public tests, board, commits and acceptance.
Workers receive exclusive paths after the initial contract is established.

## Acceptance

Actual processes reopen two different accepted fixture PRs into their stable
windows with fresh generations and no new enrollment. A separate public-preview
restart performs anonymous reads with no vault/account request. Fresh windows
remain Home. Explicit Close removes only its destination; Quit preserves latest
accepted records. Failed navigation does not become the stored destination.
Configured-server/grant/store/capability failure is explicit without fallback or
allocator reset. Manual-executor tests prove bounded/coalesced persistence,
revision conflicts, missing/invalid observations, deterministic shared authority,
user cancellation, stale/reordered completion and shutdown safety. Storage tests
prove strict bounds, interruption/revision/cleanup and profile partitioning.
Focused public checks, changed-TU tidy, independent authority/lifetime review,
production composition guards and documentation pass. Full RFC/native acceptance
remains open.

## Evidence

BPR0707 restores identities and BPR0708 restores drawer layouts. Current restored
windows still show an unavailable-content notice. Existing setup destinations,
accepted read snapshots, validated server catalog and remembered/anonymous session
APIs provide the concrete path; none constitutes saved review restoration yet.


## Landed checkpoint — implementation paused at user request

The current implementation adds deterministic core startup/cancellation policy,
a bounded serialized restoration host, a lazy profile-private SQLite destination
store, and desktop composition for accepted reads and exact window retirement.
Remembered binding restoration now has bounded terminal callbacks and an exact
startup admission predicate so superseded startup cannot resume authentication.
No new device enrollment is part of restoration.

Focused checks pass: core restoration 0.40s, application restoration 0.33s,
SQLite restoration 0.44s, and the rebuilt GitHub host 0.61s. The independent
Astra medium review found no confirmed material blocker in authority,
cancellation, persistence or lifecycle handling. These checks do not establish
actual authenticated or anonymous process-restart acceptance.

The card remains In Progress. Resume by adding the planned actual-process
restoration courses: two authenticated PRs with retained fixture vault input,
anonymous restart with zero credential/account effects, missing grant/server/
capability/store errors, latest destination persistence, and stale user-navigation
refusal. Complete the remaining public/production checks before moving Done.
No further UX card was started after the request to land and stop.


Checkpoint integration: existing rendered four-process layout regression passes
19.98s. The identity-only restart course no longer requires the obsolete generic
unavailable-review notice when there is no saved destination; exact visible Home
and stale-generation assertions remain. Changed-TU tidy, strict composition
inventory and the hostile direct-bypass check (0.974s) pass. The tests-disabled
Release check remains part of the pending completion boundary.

Final rebuilt identity restart passes 7.24s; its changed test also passes tidy.
Documentation and whitespace gates pass. Task-created temporary check logs were
consumed and removed; existing rendered fixture artifacts remain under build/dev.

## Resumed milestone, 2026-09-08

Dispatch UX-2026-09-08-D1, mandate revision 1 resumes this card from clean
`af4acb4ab8477667068180a95ff24c149e5c840a` on main. The designated integrator
owns this continuation; the checkpoint has no prior active writer. The milestone
is actual-process accepted-destination restoration, including authenticated and
anonymous restart, exact Close/Quit persistence, failure visibility and stale
navigation refusal. Existing component/review evidence is reused where inputs
remain unchanged; the tests-disabled composition check remains required.
RFC-0028's Home and layout decisions remain authoritative and browser-style
file tabs remain unscheduled. Full RFC/native acceptance remains open.

## Accepted milestone

Source/test candidate `ce63c67b1e82987a6cbec7ac5b60fc7f47300917` completes this
bounded destination slice. `application_profile_window_review_restoration_public_path`
passes 15.70s across fifteen actual processes: two authenticated PRs return to
their stable windows with fresh generations using the retained synthetic grant
and no enrollment; anonymous restart makes zero vault/account/bootstrap attempts.
Raw counters include refused admissions, with missing-grant lookup as a negative
control. Fixture vault bytes travel only through bounded parent-memory pipes.
The course proves latest accepted writes, failed navigation preservation, Home
retention, exact Close versus Quit, fresh-window Home, missing grant/server/read,
future/corrupt store preservation and configured GHES preview refusal.

A held-terminal Home course exposed a production bug: Home was already checked
while restoration waited, so its false toggle skipped cancellation. Every Home
gesture now cancels the exact restoration and keeps Home visible when no review
is accepted. Released queued terminals cannot replace Home or a newer PR; the
stored destination survives Home cancellation. The first cancellation-only repair
stopped acceptance but left the empty review visible; the final course directly
checks both absence of stale content and visible Home after queued delivery.

Expanded manual-executor host checks pass 0.32s for malformed observations,
delayed load versus accepted navigation/Close, reentrant open/shutdown and full
window-capacity reservations. Unchanged core/storage/checkpoint authority review
evidence above is retained. A separate bounded Astra-medium review found no
production blocker and identified the zero-attempt instrumentation gap; its
correction was reviewed and the final course passed. Changed-TU tidy, format,
strict production inventory, hostile bypass regression, tests-disabled Release
build and device/public-preview absence scans pass. Documentation gate and its
62 regression tests pass.

Local evidence: `build/dev/bpr0709-public-{build,test,tidy,format-check}.log`,
`build/dev/bpr0709-host-{build,test,tidy}.log`, production inventory/bypass and
composition tidy logs with the same prefix; Release logs under
`build/app-release/bpr0709-*`. Inspected rasters are under
`build/dev/bpr0709-restoration-artifacts/`, including `auth-home.png`,
`missing-grant.png` and `anonymous-restore.png`. No artifacts or fixture vault
material are committed. The anonymous seed and restart both show the same
pre-existing unselected-comparison failure; this course proves accepted PR
metadata/file/comment restoration, not selected comparison or epoch usability.
Full RFC/native criteria remain open. Current workstream continuation is [[BPR-0595]].
