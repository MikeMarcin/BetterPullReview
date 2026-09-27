---
schema: "implement-rfc.card.v1"
id: "BPR-0601"
title: "Verify and close RFC-BPR-0027"
status: "Backlog"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-0600","BPR-0602","BPR-0603","BPR-1215","BPR-1216","BPR-1217","BPR-1218","BPR-1219"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0027"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0027

## Objective

Reconcile [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) against all linked
implementation cards, direct evidence and the shipped diagnostic path.

RFC base: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Requested RFC set: RFC-BPR-0027; workstream position: 1 of 1.
Planned cards: BPR-0600, BPR-0601, BPR-0602, BPR-0603. Add subsequently allocated required slices to
this list and dependencies before closeout. Registration is not implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data. | Composed Diagnostics controls, four detail/privacy combinations, restart and invalid-setting fixtures | Not run; implementation deferred | None |
| AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged. | Credential-free native/provider-to-host-to-file failure fixture with unfamiliar code and useful sanitized message; compare UI/authority | Not run; implementation deferred | None |
| AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure. | Secret/private-content canaries across all sinks, queues, ring, export and failure fallbacks | Not run; implementation deferred | None |
| AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context. | Manual-executor mode-change, Off, Clear, sign-out and queued-event transition cases | Not run; implementation deferred | None |
| AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically. | Preview/snapshot export course with joins, mixed modes, malformed lines and zero-network observation | Not run; implementation deferred | None |
| AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out. | Bounded overload/error-storm, concurrent-process, rotation, retention and disk-full fixture | Not run; implementation deferred | None |
| AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash. | Startup/cancel/quiescence, partial final line and sink-failure course | Not run; implementation deferred | None |
| AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports. | Architecture guards, composed lifetime, fake filesystem attacks and profile-scope partition cases | Not run; implementation deferred | None |
| AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence. | Pinned dependency/license/packaging inspection plus native logging/UI lanes on each claimed platform | Not run; implementation deferred | None |

## Completion rules

Record exact executed commands, results and producing commits for each criterion.
All required linked cards must be done, including slices allocated after entry.
Use focused changed-surface checks and a composed public path, with native and
attended claims limited to actual evidence. Preserve unsupported-platform and
crash-durability limitations. Run the documentation gate and git diff hygiene,
then reconcile RFC/index status only when canonical order and dependencies allow.

## Foundation evidence and resume

BPR-0602 completed the owner-authorized phase 1 in `3d109fd4`. Its card records
passing native/provider/sanitizer, file persistence/cap, blocked-stderr cleanup,
composed application and dependency/boundary checks. Those establish the basic
subset of AC-02, AC-03 and AC-06 through AC-09; they do not complete the full
numbered criteria above. No new live sign-in ran and no live root cause is claimed.

Historical resume note: BPR-0600 and BPR-0602 completed the foundation.
The owner has now requested BPR-0603's full advanced phase under mandate r2
below; this closeout remains pending, with every full criterion retained.

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).
The bounded BPR-0602 confidentiality/lifetime review resolved all three findings;
no blocker remains in that subset. Opposite-family review was unavailable.

## Active continuation — mandate r2, 2026-09-26

The owner requests full RFC implementation and landing on main after each
accepted milestone. The old advanced-phase deferral is superseded; full criteria
and immutable RFC base remain unchanged. No live sign-in or PR mutation is
authorized. BPR-1215–1219 are the required source-owned milestones. Current owner:
`/root/rfc0027_m3`, sole writer in worktree `57ed`, branch
`codex/rfc0027-advanced`; current milestone `diagnostic-controls`.

Historical foundation and BPR-0971 evidence remain preserved. Earlier deferred
rows describe their original not-run state; advanced acceptance stays pending
until its own evidence is recorded.

### M1 checkpoint

Source candidate: `ac64e5ad0dc4694dad4c6db3a40f12479b11503e`.
[Evidence and commands](../../verification/RFC-BPR-0027.md);
[plan](../../rfcs/plans/rfc-bpr-0027-advanced/MILESTONES.md);
[bound assignment](../../rfcs/plans/rfc-bpr-0027-advanced/m1-binding.json).
Changed surfaces: logging adapter/CMake, maintenance JSONL classifier, direct
adapter/public logging tests, format-dependent profile/benchmark assertions,
and owning architecture/privacy/operator documentation. No provider sanitizer,
core policy, UI controls or dependency version changed.

BPR-1215 supplies the structured-event subset of AC-02/03/06/07/08. All full
criteria in the table remain pending their advanced tests; no native/human gap
is waived. Next: BPR-1216 / `session-retention`, then controls, richer exports,
and native/attended closeout. No protected attended candidate or review server
was created for M1. All configure/build/test/tidy/documentation commands have
completed and no task-owned subprocess or descendant remains. The preserved
strategy and binding sit beside this continuation; the coordinator starts a
fresh implementer for the next milestone after the main push is recorded.

M1 landed and pushed as `857d93624aa3acb1e7f590969ae25aecf93a8993` (main and
origin/main matched after push). Source behavior is `ac64e5ad`; the intermediate
merge preserved concurrent review-tour documentation from `1248c0b1`. BPR-1215
is Done. [Completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m1-observation.json)
binds all M1 obligations/exits to that landed candidate. All commands are stopped;
there is no review URL, session, event cursor or protected candidate to transfer.
The appended handoff commit carries documentation only. The next engineer can
reuse this worktree and its offline `build/dev` cache, promote/claim BPR-1216,
and bind `session-retention` with `structured-events` accepted. Preserve the
full RFC criteria, mandate r2 and the existing BPR-0971 privacy boundary.

### M2 checkpoint — accepted and landed

Mandate r2; assignment `/root/rfc0027_m2`, BPR-1216 / `session-retention`.
Source candidate: `3381a02c5a78f4611fd7ab593cc0f6086ec32208`. Main `6e8ca8a8`
was synchronized before claim `fb27f251`; immutable RFC base is unchanged.
[Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m2-binding.json) and
[evidence/commands](../../verification/RFC-BPR-0027.md) bind this result.

Changed paths: logging adapter/CMake (native session files and limits), shared
pure filename grammar, SQLite maintenance enumeration, logger/retention and
public logging/profile/maintenance tests, registrations/support, and owning
architecture/operator docs. The provider sanitizer and core policy are unchanged.

Final exact-source integration passed 15/15, format 15/15, all three touched
product units passed tidy, and the documentation gate plus 64 tests passed.
The evidence ledger retains actual failures and fixes, partial-tail/resource
boundary self-review, and scope limits. BPR-1216 supplies the M2 retention and
lifecycle subset; all full-RFC criteria above remain pending later milestones.
No live service was used. Native Windows/Linux, packaged and human acceptance
remain open; the real native write failure uses RLIMIT_FSIZE, not a filled disk.

All owned build/test/tidy commands finished. There is no review URL/session,
event cursor, protected candidate or server. The source/acceptance landing was pushed as `8ff94031e1279c913cdb59512c2a9609e0779410`;
main and origin/main matched after push. This final documentation checkpoint
releases `/root/rfc0027_m2` writer ownership. The
[completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m2-observation.json)
binds every M2 obligation and exit to that landed candidate. Next eligible scope is
BPR-1217 / `diagnostic-controls` for a fresh implementer. Preserve mandate r2,
M1/M2 evidence and the full native/human criteria.

### M3 checkpoint — accepted, landing pending

Mandate r2; assignment `/root/rfc0027_m3`, BPR-1217 / `diagnostic-controls`.
Source candidate `75700f4dd0ce88f86e59940cddf690a6a3e2f45b`; merged candidate
`6089117d7bd13ad52d2b34c49713b842cd0f3954` preserves concurrent main `2deb889b`.
Claim `b56d0b2f`; immutable RFC base unchanged.
[Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m3-binding.json) and
[evidence/commands](../../verification/RFC-BPR-0027.md) define this checkpoint.

Changed paths: application diagnostic ports/settings host and metadata classifier;
logging encoder/queues/ring/native Clear; SQLite preferences/purge inventory;
Qt Settings/Help and composition lifecycle/sign-out; the fixed Verbose provider
producer; direct/provider/public tests and owning architecture/operator docs.
No credential-owner, core authority or dependency version change.

Affected backend/provider gates pass 9/9 and public composition 3/3, all 26 C++
files pass format, all 12 product units have passing tidy, documentation and its
64 tests pass. The ledger preserves the initial failures, fixes and earlier long
profile/lifecycle/180-second age checks. The merged-source rebuild passed and focused
public integration passed 5/5 in 35.90 seconds at `6089117d`. BPR-1217 is accepted;
main push and the final completion observation remain.

Current writer remains `/root/rfc0027_m3`; all task-owned build/test/tidy commands
have completed. No review URL/session/event cursor, protected
candidate or server exists. Required full-RFC rich export and native/human
acceptance remain open. Next after accepted main landing: a fresh implementer
for BPR-1218 / `diagnostic-export`, preserving mandate r2 and all prior evidence.
