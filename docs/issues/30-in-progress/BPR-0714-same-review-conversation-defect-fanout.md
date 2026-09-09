---
schema: "implement-rfc.card.v1"
id: "BPR-0714"
title: "Refresh same-review conversation and defect projections"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0713"]
metadata: {"dispatch":"UX-2026-09-08-D6","mandate_revision":2}
---

# Refresh same-review conversation and defect projections

Mandate revision 2: finish and hand off this M6 slice, then stop. No next
milestone is authorized during this invocation.

## Bounded scope and acceptance

Continue AC-03/09: accepted private conversation receipts and defect changes
reach independent sibling views of the exact same review without replacing
comparison, file, semantic selection or scroll. Reload accepted durable defects
without correlating or writing against the recipient's selected comparison.
Queued invalidation after review replacement must not expose the old review.
Exercise the rendered composed path; do not infer full AC-03 from cache tests.

## Inventory and ownership

BPR-0713 already carries accepted-only, coalesced, exact-authority ledger
invalidations on the profile decision lane. Conversation invalidation calls
`publish_read_state`, but private invalidation omits the per-window
`DurableDefects` cache. The ordinary defect loader correlates and persists,
so sibling reload needs a separate read-only projection operation.

Own narrow composition reload and its public multi-window regression, with
architecture and BPR-0595 continuation. Preserve RFC-0028 Home/layout authority,
unscheduled browser tabs, full two-account isolation, native geometry/focus,
Windows activation transport, anonymous selected-comparison limitations,
package/accessibility and attended evidence, and canonical dependencies.

## Implementation and verification notes

Private-record invalidation now reloads accepted durable defects and presents
them directly. It never calls the ordinary loader or correlation path; failed
reads preserve the previous accepted projection. Existing exact review/account,
window lifetime and profile-generation admission remains in force.

Independent native medium review found no source blocker. It noted that
`shown_defect()` alone excludes anchor availability; the public regression is
strengthened to compare the rendered context label while the sibling reads a
different file. Direct write counts and two rendered historical epochs remain
unclaimed. Opposite-family review and the named integrator adapter were unavailable.

The initial four-check integration passed architecture, ledger composition and
composed-host inventory, but the older defect course failed `STARTUP_FAILED`
before reading any review. Its fixture directory used ordinary mkdir permissions;
the ledger course already applies owner-only permissions required by profile
admission. The same exact-root chmod is added to the defect fixture setup.

The first multi-window attempt failed its unread starting-state assertion
(76.59s). Inspection found the reading fixture's cloned comments were authored
by the current viewer, which correctly yields no unread posts. The bounded
reading-fixture overlay now supplies a distinct synthetic commenter; the
course uses the exact generated PR/thread identity and a shared Whole PR
conversation filter. These input/setup failures are not accepted evidence.
The next course passed conversation fanout, then exposed an old row-count
assumption: accepted read progress may remove `review.cpp` from actionable
rows. The test now addresses the required `secondary.cpp` by stable identity.
Different-file classification, command delivery and context preservation then
passed; the stale-delivery course's attempted reopen of a withdrawn defect was
invalid because withdrawal is terminal. Final coverage uses legal reclassification
followed by a queued withdrawal before review replacement.

The final actual multi-window course passes84.64s, after the preceding behavioral
pass84.78s. It exercises public Mark read and classification/reclassification,
exact defect and context-label equality with the recipient on `secondary.cpp`,
unchanged comparisons/selections/scroll, and queued withdrawal before PR
replacement. The replacement contains no prior-review defect. Evidence:
`build/dev/bpr0714-final-public-course.log`; six inspected PNGs under
`build/dev/bpr0714-rendered/`. Full-window captures show the different files;
the current sidebar clips the defect card below that viewport, so supplemental
renders of the actual two card widgets show critical classification and available
context. This is not acceptance of native layout, accessibility or visibility.

This slice covers private conversation read receipts and durable defects.
GitHub conversation snapshots still belong to each window read host; sharing
new/edited/deleted posts and resolution refreshes remains separate AC-03 work.

Final old defect composition course passes54.00s after its permission repair,
covering seed/reopen against both recorded server contracts. Initial unchanged
architecture9.63s, ledger composition38.61s and development host inventory1.88s
passed. Canonical format and changed-TU tidy pass. Tests-disabled Release builds;
documentation and its 62 regression tests pass. Logs remain ignored under build.

The final three-process `auth-seed` / `auth-advance` / `auth-restore` course
passes through the existing restoration helper, confirming independent saved
positions and the accepted final destinations with the updated commenter.
This is the selected three-process regression, not a rerun of all34 registered
restoration processes. The initial local wrapper used the CMake target name
instead of the output filename and started no child; the corrected wrapper
passes (`build/dev/bpr0714-short-restoration.log`). Tests-disabled production
inventory passes required30/forbidden7 (`build/app-release/bpr0714-inventory.log`).
