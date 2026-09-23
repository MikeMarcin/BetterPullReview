---
schema: "implement-rfc.card.v1"
id: "BPR-0597"
title: "Verify and close RFC-BPR-0025"
status: "Backlog"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0596","BPR-0981"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0025"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md"}
---

# Verify and close RFC-BPR-0025

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0025](../../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 3 of 4.
Planned cards: BPR-0596, BPR-0597, BPR-0981. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

Planning reconciliation on 2026-09-22 ([record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md))
adds the one required slice BPR-0981 to the planned cards and dependencies.
The owner-locked RFC-BPR-0028 direction supersedes this RFC's layout and
focus-mode chrome where they conflict and leaves browser-style file tabs
unscheduled; each row below names the surviving check or the superseding
decision. A superseded row is recorded as such at closeout, never as passed.

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped review surface has independently collapsible Reviews, Files, and Conversations drawers, compact center chrome, and reversible focus mode with window-specific restoration. | Re-run `qt_workbench_window_scale_100` (drawer toggles, collapse returns space, strip modalities, repositories rail) and `application_profile_window_layout_public_path` (per-window drawer layout restoration) | Layout/focus mode superseded by RFC-BPR-0028 (owner lock 2026-09-06): repositories drawer, strip/overview/epoch line, per-window modalities; surviving drawer independence not yet re-run | None |
| AC-02: Independent fuzzy filters find PRs by repository/title/number and files by filename/path, preserve matching context, and expose honest empty results without provider effects or lost active views. | BPR-0981: `qt_files_tree`, `qt_repositories_drawer`, composed gesture path in `qt_composed_github_scale_100` with an empty intent vector | Not run; allocated to BPR-0981 | None |
| AC-03: File clicks and previous/next file navigation reuse the active tab and preserve its comparison; Cmd-click, Ctrl-click equivalents, and Open in New Tab create a distinct background view. | Single view: `core_workbench_interaction` actionable-order navigation and a composed assertion that the epoch selection is unchanged across file click and next/previous file | Cmd/Ctrl-click and Open in New Tab superseded/unscheduled by RFC-BPR-0028; single-view part not yet run | None |
| AC-04: Duplicate Tab preserves the exact initial view while allowing independent epochs, positions, and selections even for the same file in multiple tabs. | None: Duplicate Tab is wholly tab-specific | Superseded/unscheduled by RFC-BPR-0028 browser-tab non-goal; not implemented; not passed | None |
| AC-05: Each window's PR retains its tab order and active view across PR switching and restart; closing tabs or PRs follows the specified neighbor/landing behavior without deleting private review work. | Single view: `application_profile_window_review_restoration_public_path` (BPR-0709), reading restoration (BPR-0710), close-to-Home and private ledger retention (BPR-0703/0704) | Tab order/active tab superseded; per-window restoration and close behaviour not yet re-run | None |
| AC-06: New revisions, removed files, pruned epochs, and unavailable comparisons never silently retarget an exact view; named-comparison updates require explicit acceptance in the affected tab. | `core_epoch_policy` exact-pair retention through rebase/prune/unavailability; source inspection that `refresh_epochs_intent` leaves only the explicit Refresh gesture; BPR-0710 unavailable without substitution | Tab-scoped update indication superseded; single-view retention not yet re-run | None |
| AC-07: Late work cannot populate another profile, window, PR, tab, or comparison, and inactive tabs retain bounded lightweight state without eager full-diff loading. | `application_workbench_host` and `application_workbench_snapshot_observation` late/stale admission; RFC-0024 BPR-0976/0977/0978 exact-authority evidence | Tab identity and bounded inactive tabs superseded; profile/window/PR/comparison exactness not yet re-run | None |
| AC-08: The existing diff mapping, linked scrolling, selected-line identity, tuned appearance, review-draft authority, and focused-review space behavior remain intact in the composed tabbed workbench. | `qt_composed_github_scale_100`, `qt_diff_workbench_scale_100` and `application_diff_workbench_public_path` at the closeout head | Read as the shipped RFC-BPR-0028 composition; not yet run | None |
| AC-09: Keyboard, focus, accessibility, long labels, narrow-window layouts, and 100–200% scaling keep tabs, filters, drawers, comparison controls, and review actions reachable. | BPR-0981 focus-order rows in `qt_workbench_window_scale_100/125/200`; `qt_composed_github_scale_100/125/200`; drawer width floor and narrow Read course | Tab strip superseded; filters/drawers/comparison/review reachability not yet run | None |

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

The owner activated the ordered workstream in the canonical index; RFC-0018
and RFC-0024 are Auto-Verified. [[BPR-0596]] recorded the 2026-09-22 planning
reconciliation: RFC-0028's superseding layout and unscheduled browser-style
file tabs are mapped per criterion above, and [[BPR-0981]] is the one
allocated slice. This closeout becomes startable when BPR-0981 is Done; it
then re-runs the surviving checks at the closeout head, records superseded
rows as superseded, and reconciles the RFC and index. This record claims no
tab implementation or acceptance.
