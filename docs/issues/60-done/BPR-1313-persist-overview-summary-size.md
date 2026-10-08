---
schema: "implement-rfc.card.v1"
id: "BPR-1313"
title: "Persist each window summary fraction through its existing host"
status: "Done"
rfcs: ["RFC-BPR-0063"]
depends_on: ["BPR-1312"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"94c01a937","evidence":{"kind":"command","detail":"application_workbench_host (0.57s), sqlite_workbench_settings_storage (1.05s), application_profile_window_layout_public_path (23.88s): held startup/reset intent merges into loaded record, coalesces/drains; separate fractions restored in two real windows/restart, new window natural default, full-reader return retained. Qt clamp/empty/reset course passes without overwriting preferred fraction."}},{"criterion":"C2","outcome":"pass","revision":"94c01a937","evidence":{"kind":"command","detail":"Qt Overview authority replacement clears admitted body and reading state using existing admission path; same-body refresh preserves document and Files identity. Schema15 stores only a bounded summary fraction per window; migrations1–14 preserve profile preferences, profile defaults exclude fraction. Format, changed-line warnings-as-errors tidy, documentation gate pass."}}]
---

# Persist each window summary fraction through its existing host

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Explicit preferred fraction survives refresh/navigation/full reader/restart per window; clamp does not overwrite intent, reset clears it and empty body preserves it.
- C2: Authority replacement clears old text/reading before new content; stale results cannot repopulate it; only size is persisted.

## Composition reseal review (2026-10-08)

441d55e45 ("Persist the Overview summary fraction per window") is this card's
change, and it changed `src/app/src/github_application.cpp` without resealing
the round-trip composition pins. It adds one dispatch:
`overview_page()->set_summary_resize_dispatch` calls
`WorkbenchHost->resize_summary(fraction)`, guarded by `Closing`. The host
rejects 0 and anything at or above 10000, holds an intent that arrives before
the layout loads, and stores only that per-window fraction in the existing
workbench settings record. Profile defaults never take it. It reads only local
settings and reaches no provider, grant, target, credential, GitHub write,
mutation or publication path. The mutation-branch pin is unchanged. The pins
were resealed with BPR-1308's and BPR-1316's changes in the commit that adds
this note.
