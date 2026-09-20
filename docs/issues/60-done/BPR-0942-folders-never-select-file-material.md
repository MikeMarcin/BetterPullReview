---
schema: "implement-rfc.card.v1"
id: "BPR-0942"
title: "Keep folder gestures separate from semantic file selection"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r21-drawer-and-exact-material"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ae23c4d9","evidence":{"kind":"command","detail":"Model folder flags, mouse/branch and expansion keys emit no file intent; expanded-folder Right regression fails without interception."}},{"criterion":"AC2","outcome":"pass","revision":"ae23c4d9","evidence":{"kind":"command","detail":"Filtering, model rebuild, overview refresh, ordering and foreign-index unit checks retain exact actual file identity without dispatch."}},{"criterion":"AC3","outcome":"pass","revision":"ae23c4d9","evidence":{"kind":"artifact","detail":"Failure-before widget and composed checks; repaired visible-drawer course through installed Git retains exact context/result/both source panes and selection counter. 2/2 52.05s, full-window safe raster inspected."}}]
---

# Keep folder gestures separate from semantic file selection

Owner-observed signed r22 defects extend the current selected-diff-exact-epoch-pair
milestone. Prior BPR-0935..0939 acceptance remains historical; these are new
follow-ups, not reopened Done cards. Preserve BPR-0934 Overview folder collapse
as separate work and do not request another owner loading test.

## Acceptance

- AC1: Folder rows are structurally non-selectable as files; mouse, keyboard and expand/collapse gestures emit no semantic file selection and do not change the current comparison.
- AC2: Filtering, ordering, overview refresh and model rebuild preserve that boundary and exact surviving file identity; foreign indexes cannot alias files.
- AC3: Failure-before model/gesture checks and production-composed interaction evidence prove no folder-triggered diff, including after rebuild.

## Evidence

- Folder model flags inherited file selectability; focusing a folder also lost
  the last real file across model reset. Right on an already expanded folder
  could navigate into a child and dispatch a file. Folders now expose enabled
  only, own expansion keys, and cannot alias a file through a foreign index.
- Widget and actual composition fail before repair at folder selectability.
  The finer keyboard regression also caught the expanded-folder Right case.
  Final visible-drawer widget/composition pass: 2/2, 52.05 seconds, installed
  Git, 515-file safe fixture, exact result/context/source/counter retained.
- Filter, ordering, overview refresh, model rebuild, mouse/branch and keyboard
  paths preserve the last actual file projection without producing intents.
  Full-window `r23-rendered/folder-retains-exact-diff.png` was inspected locally;
  no screenshot or private material is committed.
