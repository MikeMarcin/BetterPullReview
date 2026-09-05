---
id: "RFC-BPR-0025"
title: "Review Workspace Navigation and Browser-Style Tabs"
status: "Deferred"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0003","RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0006","RFC-BPR-0024"]
pack_version: "post-v2"
---

# RFC-BPR-0025: Review Workspace Navigation and Browser-Style Tabs

## Status and scope

**Design accepted; registration only; implementation requires a later request.**
The product owner approved the navigation and browser-tab behavior on
2026-09-04. This deferred post-pack RFC follows RFC-BPR-0024 and precedes
RFC-BPR-0026 in the requested workstream beginning with RFC-BPR-0018. Registration
waives no eligibility, dependency order, existing authority, or evidence gate.

Registration baseline: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Entry: [[BPR-0596]]. Closeout: [[BPR-0597]]. These cards remain in backlog.

## Problem and outcome

The current selected-file workbench has useful text diffs and tuned colors,
but session configuration and secondary controls consume review space. A
reviewer also needs several retained views of one PR: two files, two epochs of
the same file, or distant sections of one file. Tabs keyed uniquely by filename
cannot represent that workflow, and a PR tab strip would compete with file tabs.

Provide separate Reviews and Files drawers, browser-style viewing tabs scoped
to each window's PR, and a compact center above the existing diff. The unit of
a tab is an independent viewing context, not a unique file.

## Accepted layout

From left to right: **Reviews drawer**, **Files drawer**, **review center**, and
**Conversations drawer**. Each drawer collapses independently, retaining its
width and visibility in the window. All toggles remain reachable by keyboard
and visible controls; focus mode collapses secondary navigation and detail
chrome and restores the prior arrangement when exited.

Reviews groups this window's open PRs under repositories. It displays PR title,
number, state, and attention indication; host names disambiguate otherwise
identical repositories. Its fast fuzzy text filter matches repository, title,
and PR number. Files lists the active PR's files and has an independent fast
fuzzy filter matching filename and repository-relative path. Filters operate on
resident projections, not network search, and retain parent context for matches.
Empty results expose a clear-filter action without closing the current view.

The center has a compact PR identity/header, the PR's tab strip, the active
tab's comparison controls and review/navigation actions, and the existing diff.
Exact OIDs and history maintenance remain reachable behind secondary details;
server configuration moves to RFC-BPR-0018. The Conversations drawer continues
to serve the active view's existing discussion/defect/agent surfaces. Changing
tabs cannot silently retarget an existing draft or in-flight publication.

## Browser-style tab contract

Each window retains a tab collection for each open PR. A tab owns a distinct
identity plus file target, semantic comparison selection, resolved exact
anchors, reading position, and selection. Two tabs can contain identical
targets and still remain independently navigable.

- Clicking a file reuses the active tab. Previous/next file also changes that
  tab, using the existing actionable-file navigation contract and preserving
  its comparison selection. Opening the first file creates the initial tab.
- Cmd-click opens the chosen file in a new background tab on macOS; Ctrl-click
  is the equivalent elsewhere. The context menu's **Open in New Tab** performs
  the same operation. The new tab inherits the current comparison, starts at
  that file's normal initial position, and does not change the original tab.
- **Duplicate Tab** opens and activates a new tab with the same file, exact
  comparison and position. Subsequent scroll, selection, and epoch changes are
  independent. There is no filename-based deduplication.
- Comparison controls belong to the active tab. The first tab defaults to
  Whole PR; changing its epochs has no effect on sibling tabs or windows.
- Switching PRs restores that window's active tab and tab collection for the
  chosen PR. Opening an already-open PR activates it in the receiving window.
  RFC-BPR-0026 routes external locations into this operation.
- Closing a tab selects its next neighbor, or its previous neighbor when it
  was last. Closing the final tab leaves the PR open with its file chooser.
  Closing a PR closes only this window's views and returns to Home when
  composed, otherwise the existing landing surface.

Tab labels show filenames with enough path/comparison context to distinguish
duplicates. Full target information is accessible without relying on truncation
or color. A merged or closed PR remains in open reviews until the user closes
it; its state is visible. Closing a PR/tab changes no durable draft, note,
receipt, or following membership owned by RFC-BPR-0024 and the review domains.

## Revisions, persistence, and failure handling

Persist tab order, active tab, independent comparisons and reading anchors
under profile/window/PR authority. Reuse RFC-BPR-0024's session-store ownership;
do not create a second restoration authority. Restore code positions against
their exact file/revision; changed or unavailable material cannot masquerade as
the same view. Missing files or pruned epochs expose an unavailable state and
an explicit recovery choice instead of silently selecting another comparison.

Exact historical pairs never rebase themselves onto newly observed revisions.
For named comparisons, retain the displayed exact pair until the user accepts
an offered update, then resolve through the existing epoch host for that tab.
The update indication distinguishes newly available material from the material
actually displayed. Sibling tabs retain their own choices.

Keep inactive tabs as lightweight contexts with bounded caches. Creating many
tabs must not load every full diff, instantiate a full widget tree for each,
or duplicate profile-wide provider polling. Switching away cancels or detaches
view-specific work while preserving shared jobs with other consumers. Results
carry exact profile/window/PR/tab/request/comparison identity and can publish
only into the matching still-live context. A failed tab does not clear others.

## Architecture and interface changes

Core owns typed tab identity, navigation/close/duplicate/update intents,
comparison bindings, command legality, and compact per-view state. File identity
remains the existing diff identity; tab identity never substitutes for it.
The host owns tab residency, async loading, caching, fuzzy-match observations,
session persistence, and stable incremental projections. The core continues to
decide navigation from bounded facts rather than retaining full file lists.

Qt owns drawers, the tab strip, comparison presenters, focus, and event
translation. Qt neither parses provider URLs nor performs Git/provider/store
effects. The existing semantic diff, alignment-space scroll authority, link
map, palette inheritance, and appearance host remain the rendering authority.
Moving controls and adding view identities does not authorize replacing them.

## Non-goals

Horizontal PR tabs, a single file tab strip spanning PRs, filename-deduplicated
tabs, simultaneous split tab groups, cross-profile tab moves, new diff rendering,
new color tuning, and new review/publication semantics are outside this RFC.
Home discovery and URL parsing belong to RFC-BPR-0026; shared profile data and
conflict handling belong to RFC-BPR-0024 and existing domain hosts.

## Acceptance criteria

- AC-01: The shipped review surface has independently collapsible Reviews, Files, and Conversations drawers, compact center chrome, and reversible focus mode with window-specific restoration.
- AC-02: Independent fuzzy filters find PRs by repository/title/number and files by filename/path, preserve matching context, and expose honest empty results without provider effects or lost active views.
- AC-03: File clicks and previous/next file navigation reuse the active tab and preserve its comparison; Cmd-click, Ctrl-click equivalents, and Open in New Tab create a distinct background view.
- AC-04: Duplicate Tab preserves the exact initial view while allowing independent epochs, positions, and selections even for the same file in multiple tabs.
- AC-05: Each window's PR retains its tab order and active view across PR switching and restart; closing tabs or PRs follows the specified neighbor/landing behavior without deleting private review work.
- AC-06: New revisions, removed files, pruned epochs, and unavailable comparisons never silently retarget an exact view; named-comparison updates require explicit acceptance in the affected tab.
- AC-07: Late work cannot populate another profile, window, PR, tab, or comparison, and inactive tabs retain bounded lightweight state without eager full-diff loading.
- AC-08: The existing diff mapping, linked scrolling, selected-line identity, tuned appearance, review-draft authority, and focused-review space behavior remain intact in the composed tabbed workbench.
- AC-09: Keyboard, focus, accessibility, long labels, narrow-window layouts, and 100–200% scaling keep tabs, filters, drawers, comparison controls, and review actions reachable.

## Direct verification scenarios

| Criteria | Direct surface and discriminating check |
|---|---|
| AC-01, AC-02 | Shipped Qt window with fixture PRs across repositories: filter each drawer, collapse/reopen independently, resize, enter/exit focus mode, and assert retained selection, layout, and zero provider search requests. |
| AC-03, AC-04 | Real file gestures and actions: navigate A to B in one tab, background-open another file, duplicate a tab, change only one pair of epochs, and compare the other tabs' exact identities and positions. |
| AC-05 | Two windows with overlapping PRs: alter tab order and active views, switch PRs, restart, close neighbors/final tabs/PRs, and check restoration and retained draft/ledger records. |
| AC-06, AC-07 | Manual-executor host plus public Qt projection: deliver A/B/A completions out of order, close a loading tab, advance the head, remove a file, and prune an epoch; assert no wrong-target render or silent fallback. |
| AC-08 | Existing representative diff/appearance fixtures through the new selected path: exercise link map, scroll, line selection, review-draft binding, theme preview/cancel, and focused-review geometry. Reuse unchanged evidence only when its selected-path inputs remain unchanged. |
| AC-09 | Rendered window and keyboard course at 100%, 125%, 150%, and 200%, with long duplicate labels and narrow sizes; inspect focus return, accessible identity, and control reachability in supported appearances. |

The entry card records exact focused selectors before implementation, including
one composed application path; verification here is planned, not run. Subjective
layout acceptance uses the existing attended workflow on rendered application
states, not a mockup substituted for runtime evidence.

## Implementation entry and documentation impact

[[BPR-0596]] inventories selected-view identity, action binding, drawer projection,
session persistence, and draft targeting before allocating bounded slices.
[[BPR-0597]] anchors final acceptance. Update the workbench specification and
keyboard/navigation documentation as the selected public path becomes real;
until then, reference this RFC only as future design.
