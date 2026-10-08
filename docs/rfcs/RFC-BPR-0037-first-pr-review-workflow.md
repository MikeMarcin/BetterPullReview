---
id: "RFC-BPR-0037"
title: "First-PR Review Workflow: Navigation, Source Loading and Conversations"
status: "Proposed"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0003", "RFC-BPR-0008", "RFC-BPR-0013", "RFC-BPR-0015", "RFC-BPR-0028", "RFC-BPR-0029"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# RFC-BPR-0037: First-PR Review Workflow

## Mandate and status

The owner requested one RFC for their first-PR review feedback, with everything
except Markdown rendering linked to it. This proposal owns the long-description
work and all eight subsequent feedback cards, BPR-0918 through BPR-0926.
Registration baseline: `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.
Closeout is [[BPR-0927]].
The implementation, design follow-ups and target-native macOS review are now
technically accepted through BPR-0918 through BPR-0929. BPR-0927 records the
combined closeout candidate `e4096e05`. This RFC remains `Proposed`.
RFC-BPR-0008, RFC-BPR-0013, RFC-BPR-0015 and, since 2026-09-23, RFC-BPR-0029
are now `Auto-Verified`, but RFC-BPR-0028 (`Proposed`) is not; technical
completion does not waive dependency-ordered status.

Owner follow-up [[BPR-0934]], recorded on 2026-09-20 in attended decision
441, adds collapsible Overview folders. It extends this RFC as AC-10 and the
`overview-folder-collapse` milestone. The original nine criteria and their
technical acceptance remain intact. BPR-0934 is technically accepted at
`a88c0d19`: nested folders have retained, accessible native disclosure controls,
and focused plus rendered public-path checks pass. Merge `0ddf929b` landed it
on main on 2026-09-23, followed by test-only correction `b6e14081`. The
technical closeout result below records that landing.

Markdown rendering remains independently owned by RFC-BPR-0036 and BPR-0916/
BPR-0917. It is neither part of this RFC's delivery nor a prerequisite for fixing
plain-text layout, source loading or navigation. The shared body-rendering
boundary must accommodate later Markdown integration without duplicating policy.

The feedback followed publication of 0.1.3, but the running build, PR and machine
configuration were not independently captured. Do not treat reported symptoms
as reproduced defects or guesses about layout as established causes.

## Problem and intended outcome

A PR can now open, but the review journey is not usable end to end. The owner
reports source files refusing to load, missing update history, empty Discuss,
an empty Read landing from Start review, clipped long descriptions and threads,
slow nested scrolling, a confusing thread-reveal button and excessive review
composition UI in a narrow drawer. The conversation graph appears useful, but
its relationship to a second apparent rendering of the same thread is unclear.

The intended result is a coherent route from Overview to reviewing source or
reading PR-wide discussion. Each surface must have an understandable job, enough
space for its content, and honest loading/failure states. The source-loading
failure is the first functional blocker; layout work must not disguise it with
new wording or weakened exact-comparison guards.

## Linked work and criterion ownership

| Card | Work | Criterion |
|---|---|---|
| [BPR-0918](../issues/60-done/BPR-0918-long-overview-description-layout.md) | Long overview descriptions | AC-01 |
| [BPR-0919](../issues/60-done/BPR-0919-start-review-opens-review-mode.md) | Start review navigation | AC-02 |
| [BPR-0920](../issues/60-done/BPR-0920-redesign-conversations-drawer-review-composition.md) | Review composition and drawer redesign | AC-03 |
| [BPR-0921](../issues/60-done/BPR-0921-loaded-pr-update-history-empty.md) | Missing updates | AC-04 |
| [BPR-0922](../issues/60-done/BPR-0922-source-files-exact-comparison-unavailable.md) | Source-loading blocker | AC-05 |
| [BPR-0923](../issues/60-done/BPR-0923-clarify-show-review-threads.md) | Show review threads | AC-06 |
| [BPR-0924](../issues/60-done/BPR-0924-thread-body-clipping-and-slow-nested-scroll.md) | Clipped threads and slow nested scrolling | AC-07 |
| [BPR-0925](../issues/60-done/BPR-0925-duplicate-thread-below-conversation-graph.md) | Apparent duplicate thread rendering | AC-08 |
| [BPR-0926](../issues/60-done/BPR-0926-discuss-threads-without-file-selection.md) | Empty Discuss and file-independent reading | AC-09 |
| [BPR-0928](../issues/60-done/BPR-0928-expanded-overview-description-reader.md) | Implement retained full-body Overview reader | AC-01 runtime follow-up |
| [BPR-0929](../issues/60-done/BPR-0929-review-composition-and-pr-wide-discuss.md) | Implement dedicated composition and PR-wide Discuss | AC-03, AC-06 and AC-09 runtime follow-up |
| [[BPR-0934]] | Collapse Overview folders, including nested descendants | AC-10 |

## Source and update correctness

Investigate [[BPR-0922]] and [[BPR-0921]] together while preserving their separate
outcomes. The exact reported messages are:

> The selected file changed before it could be loaded. Review history could
> not be updated. The exact comparison is unavailable.

The update strip separately says “No updates loaded.” Trace first-PR acquisition,
history persistence, update identity, accepted comparison, object availability,
selected-file identity and presentation. Check actual CLI/Git versions and
capability refusals when relevant; no particular installed version is assumed.
An unavailable comparison is not evidence of a real file change, and an empty
strip is not evidence of empty remote history. Prove or disprove a common cause.

For supported stable PRs, source and genuine update history must load. Real
head changes, account/PR replacement and stale completions must still be handled
with exact identity and cancellation. Preserve RFC-BPR-0029's portable update
identity and RFC-BPR-0003's comparison integrity. Unsupported capabilities must
explain a useful recovery path without fabricating epochs or accepting stale code.

## Overview and mode navigation

Start review should enter **Review**, as explicitly requested, with an appropriate
selection or an honest empty/error state. It must not strand the reviewer in
**Read** with no file open. Continue review needs a separately specified retention
rule for the current place. Neither gesture sends a remote review.

The current overview body is a read-only plain-text widget capped at 110 pixels.
[[BPR-0918]] compares inline expansion, user resizing and a dedicated reading
surface, including page-level versus nested scrolling. Select the design from
reviewable examples with short, long and very long bodies; do not equate an
arbitrary larger pixel cap with full-body usability. Preserve access to Files,
selection/copy and reading position. Check provider truncation independently.

## Conversations and review composition

[[BPR-0920]] owns the design decision about where PR-wide review composition
belongs. Inventory the controls under “Review pull request” and “Choose a review
type and enter a summary”; the reported clipped boxes have not been identified.
Explain the blank gap as well as clipping. Compare the central Review surface
and a dedicated composition surface rather than assuming all controls belong
inside the narrow Conversations drawer. Retain local drafts and explicit
publication semantics when relocating them.

The effect of “Show review threads” must be traced before renaming or removing
it. [[BPR-0923]] should make loading, revealing, filtering and selection legible,
without requiring an unexplained button press for a seemingly empty surface.

For long thread bodies, distinguish resolved state, collapsed state and clipping.
Measure height allocation and scroll-event routing before attributing the slow
inner scroll to a fixed-height widget. The thread must be fully reachable with
usable wheel/trackpad/keyboard input. Compare the slow first representation with
the apparently faster lower representation; both observations are retained.

The graph is not itself reported broken. [[BPR-0925]] traces both apparent thread
copies to their identities and roles. Retain useful graph navigation and present
one coherent selected-thread detail, or clearly explain a necessary graph/detail
relationship. Avoid duplicate full bodies and incompatible legacy styling.

## Discuss without a selected file

[[BPR-0926]] first diagnoses why known threads do not appear. Evaluate PR-wide
Discuss as the default with optional file filtering: this is the owner's design
suggestion, not a locked final layout. Decide how top-level comments, review
summaries, inline threads, resolved threads and outdated anchors are presented.
A reviewer should be able to read discussion even when a code comparison cannot
load, with a clear route back to an anchored file when that capability is available.
Preserve private/local provenance and do not conflate filtering or loading errors
with a PR that has no conversation.

## Architecture and delivery

The core decides navigation, selection and exact request/comparison identity;
the application performs bounded provider, Git and storage effects and serializes
completion/cancellation; Qt presents layout, models, focus and input. Widgets
must not repair missing data by starting their own network or filesystem work.
Keep the existing publication and authentication boundaries unchanged.

1. Reproduce and repair source/update correctness using [[BPR-0922]] and
   [[BPR-0921]], and specify the entry transition in [[BPR-0919]].
2. Develop the overview and conversation layout decisions through [[BPR-0918]],
   [[BPR-0920]], [[BPR-0923]] and [[BPR-0926]]. These investigations may proceed
   independently of source-loading diagnosis. Record concrete alternatives and
   the selected design before changing broad surface placement.
3. Integrate the selected layouts, thread sizing/scrolling and graph/detail
   behavior through [[BPR-0924]] and [[BPR-0925]], plus allocated implementation
   follow-ups for design-only cards. Link every follow-up to this RFC and its
   owning criterion; completing an investigation alone does not prove a UI fix.
4. Exercise the combined Overview → Review and Overview → Discuss courses,
   reconcile criteria on [[BPR-0927]] and retain unavailable evidence explicitly.

Use synthetic descriptions and recorded GHES/CLI fixtures. No private PR content,
credentials or secret-bearing logs enter commits. Live inspection requires an
appropriate authorized target; this RFC does not authorize remote mutations.
Test responsive behavior at representative narrow/wide windows, larger fonts,
light/dark/high-contrast appearance and supported display scales. Measure the
reported scrolling problem rather than declaring success from a screenshot.

This proposal amends RFC-BPR-0028's affected navigation and presentation contracts
only as its design decisions are accepted and implemented. Existing history,
authority, performance and publication contracts remain governing constraints.
Canonical closeout order and predecessor eligibility remain unchanged.

## Acceptance criteria

- AC-01: Long PR descriptions have a demonstrated reading layout that exposes the complete admitted body, retains place and keeps Files reachable across representative window sizes and font scales.
- AC-02: Overview Start review enters Review rather than empty Read; Continue review preserves the intended position, and navigation publishes nothing.
- AC-03: Review composition has an agreed, implemented placement with usable input space, no unexplained large gaps or clipping, and preserved drafts and explicit submission semantics.
- AC-04: Available update history appears for the loaded PR; loading, empty, unavailable and failed states are distinct, and stale history cannot replace another PR’s updates.
- AC-05: Supported stable comparisons open the intended source for multiple files; genuine stale selections remain rejected and unavailable comparisons explain the actual cause and recovery path.
- AC-06: The thread action has an understandable visible effect or is replaced by a clear automatic/reachable flow; loading, filtering, empty results and failures are explicit.
- AC-07: Full admitted thread bodies are reachable without accidental nested scroll traps; wheel, trackpad and keyboard behavior is responsive and supported by observed or measured evidence.
- AC-08: The graph and thread detail form one understandable reading path with consistent body presentation and no unexplained duplicate full thread, while preserving identity and navigation.
- AC-09: Discuss displays known PR conversations without requiring a source file to load; its default scope and optional file filtering are decided explicitly, with clear thread-to-code navigation and loading/error states.
- AC-10: Each Overview folder has an accessible mouse and keyboard expand/collapse control that hides every descendant file; reopening preserves file identity and order, and same-authority refresh retains local expansion and sensible focus/selection without unintended file dispatch.

## Verification and closeout

Retain per-card diagnosis, producing commits and direct public-path evidence.
The combined course must start from loading a PR, reach its update history and
source, and show conversations without an accidental file-selection requirement.
Exercise initial loading, failures, empty content, refresh, PR/account replacement,
head changes and retained selection/drafts. Layout prototypes support decisions;
only implemented and exercised behavior satisfies the final runtime criteria.

Documentation and board validation must pass. Record target-native observations
for accessibility, layout and scrolling separately from deterministic host tests.
Do not declare unobserved platforms or private-service paths verified. Markdown
RFC status and its implementation/closeout cards remain separate.

## Technical closeout result

Candidate `e4096e050b856eed1f103c215701d3cf289d159f` completes the missing
combined recorded-fixture course. One network-isolated GHES/CLI composition now
loads the pull request, distinguishes refused acquisition from stale selection,
recovers two retained updates through the same durable store, enters Review from
Overview, renders exact content for `review.cpp` and `secondary.cpp`, reads exact
pathless and inline conversation identities in PR-wide Discuss before any source
selection, retains a local Compose-review draft and exact source selection across
navigation, and records zero publication effects. The focused five-selector
closeout batch passed 5/5 in 145.31 seconds.

AC-01 through AC-09 remain bound to their accepted per-card records and producing
revisions under
`docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/`. Target-native evidence is
macOS-only: attended session `c44d1fe1c8245aa53959afc9fc9f4b27` explicitly
accepted the Overview, Discuss and Compose candidates, and the native Cocoa
long-detail course ran at DPR 2. Windows and Linux, a live GitHub/GHES service,
a literal `gh` subprocess and private-repository behavior were not observed.
Successful in-process Refresh recovery was also not claimed; the recorded course
demonstrates healthy relaunch over the same durable store. RFC-BPR-0036 Markdown
rendering remains outside this result.

The later AC-10 candidate `a88c0d19e00cbf035bafd282c2701c862ddac623`
adds complete folder ancestry and stable incremental model indexes. Native
mouse/keyboard disclosure hides every descendant, preserves surviving folders'
local expansion across refresh, and clears that state with the owning authority.
Focused Qt tests cover nested folders, accessibility, selection/focus, structural
insertions/removals/reordering and authority reset. The rendered composed
first-PR course exercises collapse/reopen before continuing through the original
source, conversation and draft-retention route. Evidence and acceptance are in
`milestone-overview-folder-collapse-evidence.md` and `acceptance-BPR-0934.json`
under the plan directory. This new evidence uses offscreen Qt on macOS, not an
additional attended native-platform verdict. Broader sign-in-card and epoch-row
smoke failures also reproduce with the pre-change Overview implementation and
are recorded as baseline failures, not passing evidence.

The technical criteria and BPR-0927 may close while the RFC status remains
`Proposed`. A later status-only reconciliation may move this RFC and its index row
to `Auto-Verified` only after every listed dependency is `Auto-Verified`; it does
not require re-running unchanged RFC-BPR-0037 engineering evidence.

Merge `0ddf929b` landed `codex/rfc0037-overview-folders` on main on 2026-09-23
and preserved its five commits. Main had not changed the Overview page, its
focused test or this plan since the branch base `a578f8ec`. The only conflict
was this RFC's index row, and the branch allocated no issue IDs. Landing found
one regression: the masked window course
`test_identical_overview_skips_rebuild_but_restores_cleared_files` still counted
the page model's resets, which the retained folder tree never emits. The full
window run never reaches that course, because it stops earlier at BPR-0940's
epoch-row assertion. Test-only correction `b6e14081` counts every structural
signal and adds `qt_workbench_overview`, which runs the window's Overview
courses directly. Product code is unchanged from `a88c0d19`. The focused
integration pass shows only failures that main already had (BPR-0940,
BPR-0983 and BPR-0962); `milestone-overview-folder-collapse-evidence.md`
records the details. This landing changes no dependency status and asserts no
human or other-platform result.

## Adopted resizable summary amendment — 2026-10-07

The owner's resize request is registered as [RFC-BPR-0063](RFC-BPR-0063-resizable-overview-summary.md). It supersedes AC-01's normal compact/non-scrolling preview selection with a taller content-aware description/Files split and window-owned preferred fraction; the optional expanded reader and all reading, copy, authority clearing and file-navigation guarantees remain. The historical layout study, BPR-0928 and plan scope/exits stay unchanged as evidence for their exact design. BPR-1312–1315 owns new implementation and rendered/owner acceptance. This note neither closes this RFC nor supplies runtime evidence.
