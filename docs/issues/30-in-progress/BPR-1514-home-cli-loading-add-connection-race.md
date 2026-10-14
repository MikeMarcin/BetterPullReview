---
schema: "implement-rfc.card.v1"
id: "BPR-1514"
title: "Prevent Add CLI reentry while Home loads a CLI connection"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Home while loading cli shows the new user add buttons and gets into a weird state if you try to add cli while it's loading cli","milestone_plan":{"id":"bpr1514-cli-loading","path":"docs/rfcs/plans/bpr1514-cli-loading/milestone-plan.json"}}
---

# BPR-1514: Prevent Add CLI reentry while Home loads a CLI connection

While Home is loading a CLI connection, it currently displays the new-user Add controls. Trying Add CLI during that load can leave Home in an inconsistent state. Project an accurate loading state and prevent conflicting connection enrollment from racing the in-flight load.

## Acceptance

- AC1: Reproduce the reported sequence with controlled delayed CLI connection loading and record the actual conflicting transitions.
- AC2: Home represents CLI connection loading accurately, without presenting new-user Add controls as though initial connection loading had finished.
- AC3: Repeated Add CLI gestures or queued connection intents during loading cannot corrupt connection selection, account identity or Home state. Core policy and host admission enforce this beyond widget disabling.
- AC4: Successful, failed and cancelled loads reach coherent recoverable states; retry/add works when appropriate. Late completions cannot replace a newer authority.
- AC5: Verify a deterministic delayed composed startup/CLI path, reentry, failure and recovery, plus an offscreen rendering of the loading state. Do not switch global gh accounts or mutate remote data.

## Coordination

Read remembered-connection and CLI authority contracts and relevant RFCs before edits. Coordinate Home layout overlap with BPR-1505. Load frontend-design:frontend-design before any UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Required human judgement uses short attended review and an exact frozen candidate.

## Engineering checkpoint — 2026-10-14

Mandate: the owner's testing note above, revision `owner-testing-note-2026-10-14`.
Baseline `a1130fccb`; isolated branch `codex/bpr1514-cli-loading`, claim
`ad747247f`. Coordinator-authorized tracker-only imports through `2ab3ac18a`
bring NEXT_ID to BPR-1519; no independent allocation changes.

The adopted [milestone plan](../../rfcs/plans/bpr1514-cli-loading/milestone-plan.json)
binds AC1–AC5; [binding](../_checkpoints/bpr1514-cli-loading/binding.json).
Preflight is `ready_pinned`; this does not certify host loading. The implementer
is the only writer in this worktree. BPR-1505 owns separate Home Cancel layout
hunks in its own worktree; the coordinator will integrate.

Design: preserve the native palette, font and Home card rhythm. A left-aligned
connection heading and one sentence replace the first-time buttons while the
session validates. The existing session controls retain cancellation/sign-out;
terminal failure restores the established recovery actions. The frontend-design
skill was read before UX work; repository native-style rules govern colors and
fonts, so no new palette, font, QSS or decorative motion is introduced.

Current diagnosis: `update_home_landing` selects first-time `sign_in` until an
account is accepted, ignoring `establishing_session`. CLI discovery has no
session-in-progress admission. Discovery may mint a new profile for the saved
account; its activation can retire the pending remembered validation. A controlled
fixture `/user` gate and composed reproducer are being built before repair.

Commands: task-owned configure/build only, logs `/tmp/bpr1514-configure.log` and
`/tmp/bpr1514-build.log`. No live GitHub calls, desktop automation, review server,
background agent or protected attended candidate. Next: run the delayed baseline,
implement narrow core/host admission and loading projection, then verify exact
candidate success/failure/cancellation/recovery and offscreen pixels.

### Candidate and diagnosis update

Product candidate `b0da7b4aeff080fa583ddbb25f29dfdf9bc5322a` fixes the initial
projection gap as well as admission. The delayed baseline course reported
`ui-status=0` (`no_profile`), `landing=0` (`sign_in`), Add CLI visible, and
`remembered=2:replacement=4:inventory=3` after Add CLI during the held `/user`
response. Discovery was queued behind validation on the serial provider lane;
once released, it enrolled a new profile for the remembered account. Baseline
log: `/tmp/bpr1514-baseline.log`, recorded test output in the private build tree.

The application host now replays its pending identity/status/commands to a window
that attaches after validation began. Core disables user setup during validation
and rejects repeat activation of the same pending profile/mode. Composition
checks that policy before discovery, queued activation, device sign-in and Home
setup, and before discovery can register profiles. Qt shows a native connecting
card with no first-time buttons. The existing device-capable card also obeys the
same activation admission. Credential enrollment/resume has a distinct core
revalidation event: a native grant rotation must still restart same-profile
validation. The first focused run exposed that intentional renewal behavior;
the final candidate preserves it explicitly. The restart fixture helper waits
for startup to settle before it deliberately replaces an account; the new held
course separately exercises rejected early gestures.

The first repaired composed delayed course passed success, forced queued
CLI/activation/Home gestures, delayed failure with same-process retry and Quit
cancellation/reopen. Its offscreen render `/tmp/bpr1514-cli-loading.png` was
visually inspected: heading and one status sentence, no Add or Sign in controls.
The first integration pass passed both architecture gates and Qt composition;
final exact-candidate native-renewal/core/restart checks are in progress. An
initial core-only build overlapped the event-header correction and linked against
mismatched intermediate event definitions; that failed attempt is not evidence.
A fresh dependency-aware rebuild against the committed candidate replaces it.

Current commands: dev and core-only build/test lanes only, all task-owned.
No review server, agents, live GitHub, credential inspection or desktop capture.
The documentation gate currently refuses the stale local tracker counter
(NEXT_ID BPR-1519 vs shared historical high water BPR-1520); coordinator import
of the committed tracker note is requested. Next: finish final checks, refresh
tracker by authorized import, run documentation gate, then canonical finish and
record command quiescence. No human criterion is required by AC1–AC5.

### Final unattended frontier

Source `b0da7b4ae` and test-only follow-up `4802241ec` have no known in-scope
engineering failure. The final enterprise restart course passes 1/1 (29.65s),
completing the seven selected dev checks across the final integration pass and
its narrowly affected recheck. Core-only is 4/4; documentation contracts 67/67;
format 22 and whitespace pass. The enterprise course now waits for a real
remembered-startup terminal before another-server enrollment, rather than
mistaking initial `no_profile` for readiness. The held composed course separately
proves that early reentry is refused.

[Acceptance records](../_checkpoints/bpr1514-cli-loading/acceptance.json),
[verification](../_checkpoints/bpr1514-cli-loading/verification.json), and
[durable handoff](../_checkpoints/bpr1514-cli-loading/handoff.md) bind the exact
assignment, revisions, changed paths, commands and pending obligation. All owned
commands are finished and writer ownership is released; no review server,
background agent, protected candidate or required human decision exists.

Tracker closeout remains pending coordinator authorization/import of existing
`9b551b738`, followed by the documentation gate and canonical finish. No counter
was changed independently. The plan's documentation/closeout exit remains pending;
passing behavior records are not a claim that the whole milestone is closed.
