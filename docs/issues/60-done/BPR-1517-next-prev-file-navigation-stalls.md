---
schema: "implement-rfc.card.v1"
id: "BPR-1517"
title: "Investigate Next/Previous File stalling while file tree navigation works"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"I haven't figured out how yet but I've gotten into a state where next/prev file do nothing although I can still navigate through files using the file tree","reproduction":"Trigger unknown; owner observed stalled Next/Previous File with working file tree","dispatch":"Implemented and verified in isolated codex/bpr1517-file-navigation; coordinator integration pending","milestone_plan":{"id":"bpr1517-file-navigation","path":"docs/rfcs/plans/bpr1517-file-navigation/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a08c7fcb8e1b2b069fd82445053c2ab06e4318e5","evidence":{"kind":"observation","detail":"Two representative composed failures recorded at exact revisions; arrow admission, tree reflection and source terminals traced. Original owner trigger remains explicitly unknown.","refs":["docs/issues/_checkpoints/BPR-1517/diagnosis.json","docs/issues/_checkpoints/BPR-1517/final-verification.json"]}},{"criterion":"AC2","outcome":"pass","revision":"a08c7fcb8e1b2b069fd82445053c2ab06e4318e5","evidence":{"kind":"observation","detail":"Both file arrows reach exact adjacent selected/tab/material identity after terminal and in-flight unavailable source, immediate tree selection, filtering and same-head epoch refresh; older-pair entry and no-changes cases remain covered.","refs":["docs/issues/_checkpoints/BPR-1517/diagnosis.json","docs/issues/_checkpoints/BPR-1517/final-verification.json"]}},{"criterion":"AC3","outcome":"pass","revision":"a08c7fcb8e1b2b069fd82445053c2ab06e4318e5","evidence":{"kind":"observation","detail":"Offered adjacent arrows admit ordinary file navigation without unavailable-source deadlock. Core first/last/no-file bounds, always-enabled active arrows and stale-context cancellation are preserved by the composed course and host tests.","refs":["docs/issues/_checkpoints/BPR-1517/diagnosis.json","docs/issues/_checkpoints/BPR-1517/final-verification.json"]}},{"criterion":"AC4","outcome":"pass","revision":"a08c7fcb8e1b2b069fd82445053c2ab06e4318e5","evidence":{"kind":"observation","detail":"Real composed QAction/tree course asserts exact selected file, active tab, accepted comparison and current rendered material, through installed Git/cache; unavailable and recovered full-window renders inspected.","refs":["docs/issues/_checkpoints/BPR-1517/diagnosis.json","docs/issues/_checkpoints/BPR-1517/final-verification.json"]}}]
---

# BPR-1517: Investigate Next/Previous File stalling while file tree navigation works

The owner observed a state where Next File and Previous File do nothing, while choosing files in the file tree still navigates successfully. The triggering sequence is not yet known. Investigate and fix the discrepancy without assuming a reproduction or treating tree navigation as a workaround that completes the card.

## Acceptance

- AC1: Diagnose the relevant action admission, selection identity and navigation sequence; establish a representative reproduction of the reported stalled state or record precise evidence and remaining uncertainty if not reproducible.
- AC2: Next/Previous File navigate when an eligible adjacent file exists, including after tree selection, epoch/comparison changes, refresh, filtering and in-flight file loads as applicable to the diagnosis.
- AC3: Action enabled state and actual admission agree; stale completions and unavailable material do not strand navigation. Legitimate endpoints remain correctly disabled without wrapping unless already specified.
- AC4: Verify a regression sequence through actual composed actions and file-tree navigation, asserting exact selected file identity and visible material, not only QAction activation.

## Coordination

Coordinate shared navigation/selection paths with BPR-1506 (automatic Seen) and BPR-1507/BPR-1515 (Accept control/hotkey). Preserve core navigation authority. Load frontend-design:frontend-design before any UX changes per AGENTS.md. Never operate or capture the owner’s live desktop. Use controlled fixtures and deterministic host delay where useful; no live remote mutation required.

## Diagnosis and result

A representative persistent stall is reproduced by opening an added file on an
older base/base comparison where it is absent from both revisions. The core still
offers adjacent file navigation, and the tree can choose those files, but the
composition required the unavailable file's rendered diff and reading selection
before admitting either arrow. A preceding tree choice could also invalidate a
later press while its tab reflection was queued, and an unavailable source terminal
unconditionally discarded a file press queued during that load.

Ordinary Review file navigation now uses the selected file and its exact accepted
comparison/order after loading settles, independently of source availability.
The composition reflects preceding tree choices before capturing the press and
retains ordinary file presses through source-failure terminals. Entry from another
modality still restores its exact reading and refuses unavailable source; no newer
comparison is borrowed. Core bounds and later-context cancellation are preserved.

The [diagnosis receipt](../_checkpoints/BPR-1517/diagnosis.json) binds two failing
regressions and the exact final source candidate. The [final checks](../_checkpoints/BPR-1517/final-verification.json)
pass the composed navigation course, workbench host and source architecture gate.
The course checks both directions after terminal and in-flight unavailable source,
rapid tree choice, filtering, same-head refresh, the 4×3×3 entry matrix, exact
selected file/tab/pair and visible material, bounds, and superseding contexts.
The offscreen full-window unavailable and recovered material were inspected.
Both boundary checks, 67 documentation verifier tests, focused clang-format 22 and
`git diff --check` pass. The UX contract records the clarified ordinary file path.

The owner's original trigger remains unknown. These representative reproduced
causes match the reported distinction; they do not establish which sequence the
owner encountered. No required human, native or independent acceptance remains.
