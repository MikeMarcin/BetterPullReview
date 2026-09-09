---
schema: "implement-rfc.card.v1"
id: "BPR-0597"
title: "Verify and close RFC-BPR-0025"
status: "Backlog"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0596"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0025"]
---

# Verify and close RFC-BPR-0025

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0025](../../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 3 of 4.
Planned cards: BPR-0596, BPR-0597. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped review surface has independently collapsible Reviews, Files, and Conversations drawers, compact center chrome, and reversible focus mode with window-specific restoration. | Composed drawer/focus-mode and per-window layout restoration course | Not run; implementation deferred | None |
| AC-02: Independent fuzzy filters find PRs by repository/title/number and files by filename/path, preserve matching context, and expose honest empty results without provider effects or lost active views. | Resident fuzzy PR/file filters, matching parent context and zero-provider-search check | Not run; implementation deferred | None |
| AC-03: File clicks and previous/next file navigation reuse the active tab and preserve its comparison; Cmd-click, Ctrl-click equivalents, and Open in New Tab create a distinct background view. | Public file click, next/previous, Cmd/Ctrl-click and context-menu action course | Not run; implementation deferred | None |
| AC-04: Duplicate Tab preserves the exact initial view while allowing independent epochs, positions, and selections even for the same file in multiple tabs. | Duplicate same-file tabs with independent epoch, selection and scroll assertions | Not run; implementation deferred | None |
| AC-05: Each window's PR retains its tab order and active view across PR switching and restart; closing tabs or PRs follows the specified neighbor/landing behavior without deleting private review work. | Two-window per-PR tab persistence and close/neighbor/private-record retention course | Not run; implementation deferred | None |
| AC-06: New revisions, removed files, pruned epochs, and unavailable comparisons never silently retarget an exact view; named-comparison updates require explicit acceptance in the affected tab. | Head update, missing file and pruned epoch with explicit named-comparison update | Not run; implementation deferred | None |
| AC-07: Late work cannot populate another profile, window, PR, tab, or comparison, and inactive tabs retain bounded lightweight state without eager full-diff loading. | Out-of-order A/B/A loads, closed tabs and bounded inactive-tab residency tests | Not run; implementation deferred | None |
| AC-08: The existing diff mapping, linked scrolling, selected-line identity, tuned appearance, review-draft authority, and focused-review space behavior remain intact in the composed tabbed workbench. | Existing representative diff/scroll/appearance/draft/focused-space courses through tabbed composition | Not run; implementation deferred | None |
| AC-09: Keyboard, focus, accessibility, long labels, narrow-window layouts, and 100–200% scaling keep tabs, filters, drawers, comparison controls, and review actions reachable. | Rendered keyboard/accessibility, long-label, narrow-layout and 100–200% scale course | Not run; implementation deferred | None |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the ordered workstream in the canonical index. Its current
continuation is [[BPR-0595]]; this closeout remains dependent on the earlier work.
Before [BPR-0596](BPR-0596-inventory-tab-navigation-and-plan.md), reconcile
RFC-0028's superseding layout and unscheduled browser-style file tabs. This record
claims no tab implementation or acceptance.
