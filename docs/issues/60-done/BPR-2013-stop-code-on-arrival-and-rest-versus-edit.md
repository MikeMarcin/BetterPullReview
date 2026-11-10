---
schema: "implement-rfc.card.v1"
id: "BPR-2013"
title: "Show a stop's code on arrival and separate reading a stop from editing it"
status: "Done"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0025-stop-code-on-arrival-and-rest-versus-edit.md"}
depends_on: ["BPR-1991"]
acceptance: [{"criterion":"RFC-BPR-0106 AC01","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"Every arrival path shows the stop's code, including reopening the current stop; no Show in code","command":"ctest --preset dev -R 'qt_tour_stop_acceptance_card|application_review_tour_composition_public_path|application_review_tour_cold_import_public_path'"}},{"criterion":"RFC-BPR-0106 AC06","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"At rest only title, ring, lines link, text, Edit, chat and Accept while yellow; editing controls only while edited; every operation reachable from the context menu","command":"ctest --preset dev -R 'qt_tour_stop_acceptance_card|qt_tour_stop_card_cleanup|qt_workbench_window_scale_100'"}},{"criterion":"RFC-BPR-0106 AC07","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"No Suggest an outline or Insert outline; bpr tour outline unchanged; older drafts' suggested stops open Not accepted yet","command":"ctest --preset dev -R 'qt_review_tour_agent_setup_call|qt_review_tour_agent_failure|application_review_tour_writing|core_tour_stop_acceptance|sqlite_review_tour_storage'"}}]
---

# Show a stop's code on arrival and separate reading a stop from editing it

## Scope and acceptance

Implements
[RFC-BPR-0106](../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Make every way of arriving at a stop open its file in Review and reveal its
lines, remove the Show in code button, and make the stop's file-and-lines line
the link back to them. Reduce a stop at rest to its text, Edit, its side chat
and, while it is yellow, Accept with its reason; offer Use selected lines,
Enhance, Remove, Cancel and Save only while the stop is edited; keep route
operations in the stop's context menu; and remove Suggest an outline and
Insert outline from the application while leaving the `bpr tour outline`
commands as they are.

Covers AC01, AC06 and AC07 and the stop frames of the owner's look in AC08.
First establish which arrival paths fail to navigate today; the owner's extra
click was not reproduced in authoring. The Accept button's behaviour comes
from BPR-2014. Load frontend-design before the Qt work.

This is part of `tour-stops` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Evidence

Record: [RFC-BPR-0106 verification](../../verification/RFC-BPR-0106.md).
Frontend-design was loaded before the Qt work; the accepted stop mockup is the
design.

- Arrival paths that failed: reopening the current stop after its ring closed
  its card, Next changed stop on the current stop, and the open card (only
  Show in code led back). Each now shows the code; Show in code is gone and
  the file-and-lines line is the link back. AC01: `qt_tour_stop_acceptance_card`,
  `application_review_tour_composition_public_path`,
  `application_review_tour_cold_import_public_path`.
- AC06: `qt_tour_stop_acceptance_card` (at rest, edited, the stop's context
  menu), `qt_tour_stop_card_cleanup`, `qt_workbench_window_scale_*`.
- AC07: no Suggest an outline or Insert outline anywhere in the application
  (`qt_tour_stop_acceptance_card`, `qt_review_tour_agent_setup_call`,
  `qt_review_tour_agent_failure`, `application_review_tour_writing`); the
  `bpr tour outline` paths are unchanged; an older draft's outline-suggested
  stops open Not accepted yet (`core_tour_stop_acceptance`,
  `sqlite_review_tour_storage`).
- The stop frames of AC08 are asked in the same owner look as BPR-2014's ring
  states; the wait is recorded on BPR-2014.
