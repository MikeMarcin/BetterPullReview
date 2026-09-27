---
schema: "implement-rfc.card.v1"
id: "BPR-0572"
title: "Prove RFC-BPR-0022 automated recovery and security invariants"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0571"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"Nine placement/capability cells; every durable state refuses ineligibility, four illegal observation enum domains and unknown events. Core policy and integrated host tests pass. See BPR-0572 card and m3-evidence.json."}},{"criterion":"AC2","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"66 reached before/after wire/witness/journal/cached-admission interruptions, each followed by3 fresh real-SQLite sessions; at most1 remote candidate per logical publication and no child retransmission. Deterministic interruption simulation, not OS kill/power loss. See BPR-0572 card and m3-evidence.json."}},{"criterion":"AC3","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"Generic422/403/429/503, ambiguous loss, cancellation and incompatible body session courses retain zero unauthorized fallback; strict classifier/wire tests pass; production classifier remains disabled. See BPR-0572 card and m3-evidence.json."}},{"criterion":"AC4","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"Fresh placement host uses no local DB/receipt; ordinary content reconstructs anchors and handles mixed-client edits/replies/resolution/deletion, invalid links and unavailable source. No actual uninstall claimed. See BPR-0572 card and m3-evidence.json."}},{"criterion":"AC5","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"Fallback-only rogue implementation/factory and fallback submission symbol rejected;24 guard counterexample tests; complete source inventory plus17 rebuilt nonproduction binaries pass positive-controlled scan. See BPR-0572 card and m3-evidence.json."}},{"criterion":"AC6","outcome":"pass","revision":"2311209476dd8d87cd10d07b7d33a934e5188046","evidence":{"kind":"observation","detail":"20/20 integrated focused selectors43.09s at final source, including full public native/fallback gesture/session/SQLite/server-refresh/selected-conversation course13.57s; exact production build passes. See BPR-0572 card and m3-evidence.json."}}]
---

# Prove RFC-BPR-0022 automated recovery and security invariants

Run and extend the complete automated hostile matrix across core, host, GitHub, SQLite, Qt, composition, and isolation boundaries.

## Acceptance

- Every decision-table cell and illegal observation is covered.
- Death/loss before and after every baseline, witness, rejection, fallback, receipt, and resolution transition leaves at most one remote candidate.
- Generic 422 and every ambiguous outcome prove zero fallback POST.
- Database-loss, mixed-client edit/reply/resolve/delete, invalid permalink, and uninstall-shaped reconstruction pass.
- Mutation-isolation positive counterexamples include the new fallback operation.
- Focused composition selectors and one direct selected/public path pass at the same head.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

## Implementation and evidence — milestone C, mandate r2

The integrated source retains the nine placement/capability decision cells and
adds refusal checks for every durable state with revoked eligibility, all four
illegal observation enum domains, and unknown events. No refused observation
has an action, representation, or admitted transition.

The real protocol-session/SQLite test now enumerates **66 interruption courses**:
46 before/after wire read/response and transmission-witness boundaries across
native and fallback success, 18 before/after durable native/fallback journal
transitions, and two before/after cached-evidence admissions. Each interruption
is reached explicitly, then three fresh sessions reopen the store. There is at
most one remote candidate for the logical publication and neither child is
blindly retransmitted. These are deterministic process-interruption simulations
with reopened real SQLite connections; they are not OS kill or power-loss tests.
Existing delayed-receipt, generic 422/403/429/503, malformed/ambiguous response,
wrong actor/head, revoked lease, cancellation and incompatible body courses
continue to prove no unauthorized fallback. The supported synthetic tuple is
explicit test input; the production unsupported classifier remains disabled.

Server-truth courses reconstruct a new placement host solely from an accepted
snapshot and exact retained source, with no local database or receipt input.
That database-loss/uninstall-shaped reconstruction covers external root edits,
foreign/removed/ambiguous links, reply inheritance, resolution, root deletion,
missing source/catalogue, and native non-regression. No actual application
uninstall or live service behavior is inferred from those fixtures.

Isolation counterexamples now introduce a rogue fallback-only port/factory and
its `submit_and_reconcile` symbol. Both are rejected by the existing guard;
the 24 guard tests pass. The source inventory remains complete. A scoped
manifest of 17 rebuilt executables, including the testing application and
required coordinator positive control, passes the binary scan. No exhaustive
unrelated executable sweep is claimed. An initial manifest named the production
bundle's capitalization instead of the testing bundle; its existence assertion
failed, and the corrected complete manifest is the accepted scan.

Final verification on this source:

- Focused target build: pass (`m3-security-build.log`).
- Core/host/cache/SQLite/GitHub/native HTTPS/Qt/isolation/public composition:
  **20/20 pass**, 43.09 seconds (`m3-final-focused-tests.log` and detail log).
  The permalink suite reports `publication-interruption-courses:66`; the
  composition contract requires the native2/file1/stored2/journal3/session2
  marker plus the original fourteen criteria.
- Complete source plus 17-binary isolation: pass (`m3-isolation-complete.log` and
  `m3-changed-executables.txt`).
- Tests-disabled production application: pass (`m3-production-exact-build.log`).
  This binds the inherited M1 build repair and completed C production source.
- Architecture plus composition confirmation: 2/2 pass, 21.50 seconds.
- Scoped clang-tidy: five product units, final application unit, and two changed
  security test units pass. Formatting passes for all 21 C++ files changed in C.
- Documentation gate and all 64 documentation-validator tests pass; whitespace
  passes. Final board/checkpoint documentation is gated again at handoff.

All logs above are ignored task-owned files under `build/rfc0022/`; the final
checkpoint records hashes and producing revisions. Unchanged M1/M2 admission,
legacy left-reply, whole-review schema compatibility and routing evidence is
retained, with affected integrated suites rerun here. Concurrent RFC-0027
logging/release-maintenance source is preserved. C performs no live mutation.
