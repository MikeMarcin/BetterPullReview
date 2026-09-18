# Overview and conversation decisions evidence

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-002`; producer
  `/root/rfc0037_m2`; mandate revision `1`.
- Binding: `overview-conversation-decisions` for BPR-0918, BPR-0920,
  BPR-0923 and BPR-0926, recorded in
  `assignment-rfc0037-dispatch-002.json` in this directory.
- Reconciliation: the prior `source-update-review-entry` return remains
  accepted. Its three cards are Done with their original revision-bound
  evidence and final milestone commits present. The handoff helper reports
  `reconcile` solely because completion changed the live card fingerprints
  captured before those cards moved to Done; no contradicting source,
  acceptance record or active writer was found.
- Current candidate: allocation commit `f021c138`; BPR-0928 and BPR-0929 are
  reserved canonically for the two expected runtime follow-ups. All four
  design cards are In Progress.
- Owned paths: the four bound design cards, this evidence/assignment location,
  canonical ID reservation artifacts, necessary linked follow-up cards, and
  scoped source/tests/docs selected after investigation.
- Completed checks: live RFC queue inspected; prior assignment, return,
  acceptance JSON and evidence reconciled; the new assignment binds valid,
  resolved source obligations and the accepted prerequisite milestone; the
  four cards were promoted and claimed; ID reservation history/counter were
  committed. The first thin Qt pass passed the Overview, conversation panel,
  conversation cards, workbench, review editor and review-submission public
  path selectors. `qt_composed_github_scale_100` failed at an existing
  native-host/CLI-warning assertion; a read-only scout owns classification of
  that negative result.
- Active owners/commands: `/root/rfc0037_m2/overview_layout` owns only
  `overview-description-layout-study.md`; `/root/rfc0037_m2/conversation_design`
  owns only `conversation-surface-layout-study.md`. No long-running command is
  known outside their disposable probes. `/root/rfc0037_m2/composed_qt_failure`
  is read-only and owns the composed-test failure diagnosis.
- Pending obligations: consume both investigations; adjudicate the selected
  designs; create and link BPR-0928/BPR-0929 with runtime acceptance; record
  integrated direct evidence and complete the four design cards.
- Next discriminating step: exercise the existing composed Qt selectors while
  the bounded workers trace their independent surfaces.

Historical source/update/entry evidence remains in
`milestone-source-update-review-entry-evidence.md`; this milestone does not
repeat or reopen that accepted engineering.
