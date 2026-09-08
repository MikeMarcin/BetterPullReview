---
schema: "implement-rfc.card.v1"
id: "BPR-0709"
title: "Restore each window's accepted pull request"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0708"]
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
