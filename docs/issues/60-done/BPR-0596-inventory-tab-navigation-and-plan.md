---
schema: "implement-rfc.card.v1"
id: "BPR-0596"
title: "Inventory tab navigation and plan RFC-BPR-0025"
status: "Done"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0595"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
acceptance: [{"criterion":"Re-audit the live RFC dependencies, accepted scope, governing architecture, and current code/verification paths; record concrete ownership and migration facts.","outcome":"pass","revision":"2d594729","evidence":{"kind":"observation","detail":"Re-audited at 9097723a: RFC-0028 owner lock supersedes the RFC-0025 layout/focus mode and leaves browser tabs unscheduled; files filter is a literal substring, repositories tree has no filter and does not retain its row across apply, neither filter is a declared focus stop, refresh_epochs_intent is explicit-gesture only, core_epoch_policy retains exact pairs; RFC-0024 BPR-0709/0710/0703/0704 hold per-window restoration and private-record retention. Recorded on the card and in docs/rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md.","path":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md"}},{"criterion":"Map every numbered RFC acceptance criterion to a bounded implementation slice and a direct planned check, naming the smallest existing selectors or the focused new check required. Include one composed public path for UI behavior.","outcome":"pass","revision":"2d594729","evidence":{"kind":"observation","detail":"AC-01 through AC-09 mapped in the RFC's planning-reconciliation section and BPR-0597's checklist: AC-02 and the filter half of AC-09 to BPR-0981 (qt_files_tree, qt_repositories_drawer, qt_composed_github_scale_100 composed gesture path, qt_workbench_window_scale_100/125/200); AC-01/03/05/06/07/08 to named existing selectors re-run at closeout with their superseded parts recorded; AC-04 wholly superseded/unscheduled. No criterion removed, weakened or accepted.","path":"docs/rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md#planning-reconciliation--2026-09-22"}},{"criterion":"Allocate only needed slice cards, link their dependencies and affected RFC, and update the closeout's planned cards/checklist without claiming tests ran.","outcome":"pass","revision":"2d594729","evidence":{"kind":"command","detail":"Main integrator reserved exactly BPR-0981 (942fbf66, incorporated 44a733b3); card created in Backlog depending on BPR-0596 with rfcs RFC-BPR-0025; BPR-0597 depends on BPR-0981 and lists planned cards BPR-0596/0597/0981; plan regrouping check-amendment passed b8a5a46f... to 3d629290... (build/rfc0025-plan-amendment.json), plan-rfc-0025 definition fingerprint unchanged; verify_documentation.py DOCUMENTATION_OK (build/rfc0025-plan-docs.log), issue_board validate valid over 973 cards, git diff --check clean. No test ran as evidence.","path":"docs/issues/10-backlog/BPR-0597-close-rfc-0025.md#closeout-checklist"}}]
---

# Inventory tab navigation and plan RFC-BPR-0025

## Objective

Trace the current selected-file intent, exact comparison identity, diff-host lifetime, drawer projection, draft binding, and window session persistence. Define the smallest tab-context slice without duplicating file identity or diff semantics. Allocate bounded implementation cards with exact existing regression selectors and one public gesture path.

Governing design: [RFC-BPR-0025](../../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md).
Closeout: [BPR-0597](../30-in-progress/BPR-0597-close-rfc-0025.md).

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

## Acceptance

- Re-audit the live RFC dependencies, accepted scope, governing architecture, and
  current code/verification paths; record concrete ownership and migration facts.
- Map every numbered RFC acceptance criterion to a bounded implementation slice
  and a direct planned check, naming the smallest existing selectors or the
  focused new check required. Include one composed public path for UI behavior.
- Allocate only needed slice cards, link their dependencies and affected RFC,
  and update the closeout's planned cards/checklist without claiming tests ran.

## Non-goals

Product implementation, RFC closeout, live authentication or publication, broad
validation matrices, and redesigning the approved interaction model are not part
of this entry card. Later implementation authority does not waive the RFC's
existing dependency/eligibility rules.

## Verification record

Not run; entry work and product implementation are deferred. When authorized,
run the documentation gate on the planning slice, record exact focused selectors
for its implementation consumers, and commit with explicit paths. This card is
not satisfied merely by the initial RFC registration.

## Scoped reconnaissance, 2026-09-08

A read-only scout reconciled RFC0025 against RFC0028. Existing modalities,
three drawers, strip navigation and file/repository selection remain the shipped
authority. Browser-style file tabs, the old Reviews drawer and separate focus
mode remain superseded or unscheduled under the index direction.

Remaining resident filtering: `files_tree_model::rebuild` uses literal
case-insensitive path substring matching; repositories_tree_model has no text
filter. Plan one bounded resident fuzzy matcher, repository filter control,
ancestor retention, explicit empty/clear state and selection restoration by
semantic identity. No provider search or replacement active comparison may
result from filtering. The file tree currently resets on each filter; no measured
latency failure is claimed. Choose matcher ownership against the core/host/Qt
contract during this entry, then allocate the bounded slice.

Relevant selectors: core_workbench_interaction, application_workbench_host,
application_workbench_snapshot_observation, qt_files_tree,
qt_repositories_drawer, qt_workbench_window_scale_100/125/200 and
qt_composed_github_scale_100. No new test or product implementation ran for this
reconnaissance; entry and terminal dependencies remain in force.

## Planning inventory, 2026-09-22

Re-audited at `9097723a` under the owner's mandate ("Planning stage can
revise/update the rfc if it's gone stale") and recorded in the
[planning reconciliation](../../rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md).
The owner-locked RFC-BPR-0028 direction shipped the drawers, strip,
modalities and epoch line and names browser-style file tabs a non-goal that
stays registered but unscheduled; the adopted pack plan bounds this RFC to
resident filtering and selection restoration.

Ownership facts: the files filter is a literal substring in
`files_tree_model::rebuild` and the files tree already restores its selected
file identity across rebuilds; the repositories tree has no filter, does not
retain its current row across `apply`, and dispatches an open from
`currentChanged`; neither filter is a declared focus stop; no test types into
`filesFilter`. `refresh_epochs_intent` is dispatched only by the explicit
Refresh gesture, so no host path re-resolves a named comparison silently, and
`core_epoch_policy` retains exact explicit pairs through rebase, prune and
unavailability. Per-window restoration, close-to-Home and private-record
retention are RFC-0024's BPR-0709/0710/0703/0704.

Matcher ownership: the filter is presentation over projections the trees
already hold, decides nothing and performs no effect, so one Qt-owned
resident matcher shared by both trees is the bounded choice, consistent with
BPR-0658's placement; no core or host change is needed.

Every criterion is mapped in the RFC's reconciliation section and on
[[BPR-0597]]: AC-02 and the filter half of AC-09 are allocated to
[[BPR-0981]] (reserved `942fbf66`, incorporated `44a733b3`) with selectors
`qt_files_tree`, `qt_repositories_drawer`, `qt_composed_github_scale_100`
(the composed public gesture path) and `qt_workbench_window_scale_100/125/200`;
AC-01/03/05/06/07/08 cite existing direct evidence the closeout re-runs and
name their superseded parts; AC-04 is wholly superseded/unscheduled. The
plan regrouping passed (`b8a5a46f…` to `3d629290…`, `close-rfc-0025` only).
No product code changed, no test ran as evidence, and nothing is accepted
by this inventory.
