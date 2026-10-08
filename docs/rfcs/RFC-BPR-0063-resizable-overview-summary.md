---
id: "RFC-BPR-0063"
title: "Taller, Resizable PR Overview Summary"
status: "Proposed"
area: "BPR"
milestone: "Owner four-draft mandate"
depends_on: []
pack_version: "four-drafts-v1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# RFC-BPR-0063: Taller, Resizable PR Overview Summary

## Adoption and current obligations

Registered on main under mandate four-drafts-r1 at baseline `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. The [adopted plan](plans/bpr-four-drafts-v1/milestone-plan.json) preserves every acceptance criterion; the [import record](assets/bpr-four-drafts-v1.LIVE_IMPORT.md) retains provenance and exclusions. The implementation mandate adopts the bounded proposed behavior; required rendered/native/owner acceptance remains pending.

The source Handoff below is retained as authoring history, not current execution state. Current acceptance is tracked by the new issue cards and [ledger](../verification/RFC-BPR-0062-0065.md). Existing Done cards and plan exits remain historical evidence for their exact candidates. Formal dependencies use accepted capabilities: the already implemented native Markdown reader and window layout ownership, without claiming closure of RFC-BPR-0036 or 0037.

## Owner correction amendment — 2026-10-07

The owner rejected the first attended Overview candidate and requested a
compact enter/exit-full-view control at the reader's top-right, reclaiming the
space consumed below the body. View source moves to the reader context menu;
Overview no longer needs a separate Copy source control. Ordinary selection
copying yields Markdown source text rather than an HTML clipboard payload.
Exact source-mode byte copying and original provider-body authority remain.
Rendered partial selections may serialize their selected native document
fragment as Markdown; they do not claim byte-exact original offsets. Full
selection can retain the original body. Original AC01–AC05, resizing,
persistence and retained reading/file semantics remain in force, and a new
exact candidate still needs the required owner/native observations.

## Problem and grounded current behavior

The owner requested on 2026-10-07: “the summary panel on the pr overview is
way too small vertically and needs to be resizable.” This proposal interprets
the panel as the PR description above Files, not the author/branches line or
the right-hand “Since my last review” values.

In [the current Overview page](../../src/adapters/qt/src/pull_request_overview_page.cpp),
`overview_description_view::queue_layout` limits the normal description
viewport to four font-relative lines, fixes its height and disables vertical
scrolling. Files receives the remaining vertical stretch. “Read full
description” removes the height cap, but `set_description_expanded` hides
Files, the title/branches header and the side column. Reading more description
while keeping Files visible therefore has no user-controlled middle ground.
The [Overview tests](../../tests/adapters_qt/pull_request_overview_page_tests.cpp)
explicitly enforce that four-line, non-scrolling preview.

The [earlier layout study](plans/rfc-bpr-0037-first-pr-workflow/overview-description-layout-study.md)
considered and rejected a body/Files splitter as the primary route, selecting
the expanded reader implemented by
[BPR-0928](../issues/60-done/BPR-0928-expanded-overview-description-reader.md).
The owner's new requirement supersedes that choice for normal Overview.
[The proposed amendment](assets/bpr-four-drafts-v1/resizable-overview-summary/proposed-description-amendment.md) identifies the
affected adopted sources and preserved obligations.

## Proposed behavior and boundaries

### More room on arrival

Keep the normal Overview composition: title and branches above, description
and Files in the main column, participants and review facts beside them.
Place a visible horizontal resize handle between the description region and
the Files region. Moving it changes their heights while keeping both regions
available; it does not enter the expanded reader or change review modality.

For a long description with no saved size preference, allocate half of the
space shared by description and Files to each region, after accounting for
their controls and the fixed Overview header. A short description initially
fits its content without a tall empty well. An empty description remains the
compact “No description” state with no unused pane or resize handle. The
handle appears for every nonempty description, including short ones, so the
reviewer can choose more room independently of overflow detection.

The resize range keeps at least three description text lines and the Files
heading, table header and three file rows visible when the window has room.
Neither pane collapses to zero. In a smaller available area, respect the
existing window constraints and reduce the allocation without pushing
controls offscreen or requiring a larger minimum window. The full-description
action remains reachable for concentrated reading. Large fonts and display
scale use current font/row measurements rather than fixed pixel heights.

### Resizing and scrolling

Dragging the handle upward or downward adjusts the description height
immediately. The seam has a visible grip and the native vertical-resize cursor.
It is a keyboard focus stop named **Resize summary**; Up and Down adjust it by
one current description line, and its accessible value reports the current
description allocation. Its context menu offers **Reset summary size**;
double-click performs the same reset to content-aware initial sizing. Resizing
does not activate a file, replace the text document or move keyboard focus
into another pane.

The description becomes a normal read-only scrollable viewport when its
content exceeds its allocated height. Wheel, trackpad, page navigation,
selection and copy work there in both rendered Markdown and source fallback.
Files retains its own table scrolling. These are sibling viewports: scrolling
over one does not move the other, including at a scroll boundary. Do not add
an outer page scrollbar around both panes.

Keep **Read full description**, **Return to Files** and the existing local
Escape behavior. Expanded reading remains the optional full-area reader.
Returning restores the chosen split, the description's reading place and
Files selection/position. The normal and expanded presentations share the
existing accepted body and reader documents, including Markdown/source state;
there is no second copy of the description to reconcile.

### Remember the choice without remembering another PR's text

An explicit resize records a preferred fraction of the available shared
height for this window. Reuse it through Overview refresh, Home/Review/Discuss
navigation, opening another PR in that window, full-description round trips
and orderly restart. Other windows keep their own preference. With an
explicit preference, a short description does not automatically shrink the
chosen pane. An empty description temporarily hides it without erasing the
preference. Reset clears the preference and reapplies initial sizing.

Clamp the displayed allocation during window/font/scale changes without
overwriting the preferred fraction; enlarging the window restores the
reviewer's intended balance. Same-body refresh and resize retain text
selection and logical reading position. Body edits use the existing reader's
identity/update behavior. PR, host or account replacement clears old text and
reading state before new content appears, while the window's geometric
preference remains reusable. Stale data cannot repopulate the old body.

### Presentation and architecture

Use the existing Qt font, palette and Overview spacing. The grip supplies the
missing affordance; no new card styling or Settings page is needed. This
proposal uses `frontend-design:frontend-design` and follows the repository's
[native presentation rules](../../src/adapters/qt/AGENTS.md).

Qt owns the splitter, focus, local scroll/selection and incremental geometry.
The application host validates and persists the window preference through the
existing window-state ownership; widgets perform no store effects. Preserve
[the system architecture](../architecture/system-architecture.md), the
native Markdown reader's admission and resource/link policies, existing body
limits, file-model identity and review/publication semantics. Persist only the
size preference, not description text or a second canonical review state.
Exact widget/API and storage decomposition is implementation work.

## Acceptance criteria

- **AC01:** A long description initially occupies the defined larger share
  of normal Overview, with Files and the side summary visible; short and
  absent bodies follow the content-aware behavior instead of the four-line cap.
- **AC02:** Pointer and keyboard resizing change the normal description/Files
  allocation across the legal range, with discoverable reset, accessible
  state and usable controls at narrow heights and 100–200% display scale.
- **AC03:** The complete admitted description is reachable, selectable and
  copyable in the resized pane using either native Markdown or source fallback;
  description and Files scrolling remain independent without an outer scroll trap.
- **AC04:** Refresh, reflow and full-description round trips preserve the
  defined reading/file state and size choice; restart restores only that
  window's preference, and authority replacement cannot show old PR content.
- **AC05:** The composed application retains file activation, Start/Continue
  review and full-description return semantics without unnecessary model resets
  or provider writes; the owner judges the changed Overview in a fresh short
  attended session using a frozen build.

## Handoff

This is an unadopted draft outside the live RFC and issue board. The owner's
request authorizes the resize requirement; the proposed default allocation,
interaction details and persistence behavior are design proposals, not
recorded owner acceptance. No material competing outcome remains unresolved
for this bounded proposal. If “summary panel” meant another Overview region,
revise that scope before implementing its layout.

Implementation adopts the accompanying targeted amendment, updates the
Overview operator documentation and the obsolete compact-preview assertions,
and supplies rendered/public-path evidence. Historical study artifacts and
completed cards remain evidence for their exact accepted designs. Live IDs,
dependency assignment, issue decomposition and runtime qualification belong
to the subsequent authorized implementation workflow.
