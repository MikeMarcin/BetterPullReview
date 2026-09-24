---
schema: "implement-rfc.card.v1"
id: "BPR-1102"
title: "Windows: application_diff_workbench_public_path and the two appearance contracts (theme_customization, store_fallback) fail at DIFF_INTERACTION_FAILURE:capture-state"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's application sweep","found_at":"bcd57840","first_seen":"b9991925 (BPR-1022's rows)","cross_platform_at":"5660cf3b","related_cards":["BPR-1033","BPR-1073","BPR-1050","BPR-0265"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
---

# BPR-1102: Windows: application_diff_workbench_public_path and the two appearance contracts (theme_customization, store_fallback) fail at DIFF_INTERACTION_FAILURE:capture-state

## Defect

Three rows fail on Windows, and only on Windows, at the same line:

- `application_diff_workbench_public_path`
- `application_appearance_theme_customization_contract`
- `application_appearance_store_fallback_contract`

```text
DIFF_INTERACTION_FAILURE:capture-state
...
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:23
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
```

The line comes from the diff interaction evidence course in `src/app/main.cpp`
(`capture_diff_interaction_evidence`), which all three rows run. Each capture
settles a fixed four event passes and then requires four things together: the
scroll controller's retained frame, the link map's rendered frame, the
workbench's current context and its selection. The failure line does not say
which of the four is missing, or at which of the nineteen captures.

## Measured

| Station | Revision | Result |
| --- | --- | --- |
| bpr-main-win (MSVC 19.51 Debug, Qt 6.11.2, offscreen, `QFusionStyle`) | `b9991925` | diff workbench row fails at `capture-state` ([[BPR-1022]]'s rows, 11.24 s) |
| bpr-main-win, `build/windows-dev` | `bcd57840` | all three fail at `capture-state`, 4 of 4 runs each: 8.97 to 9.09 s, 11.95 to 12.08 s, 8.95 to 9.07 s |
| bpr-main-mac (Apple clang, Qt 6.11.2) | `5660cf3b` | all three pass: 12.56 s, 24.37 s, 12.81 s |
| bpr-main-linux (GCC 15.2, offscreen, load 0.30) | `5660cf3b` | all three pass: 2.78 s, 5.56 s, 3.29 s |

The macOS and Linux runs were made by the integrator and relayed to this
station; they print no `DIFF_INTERACTION_FAILURE` or other failure line. The
Windows logs are station records under `build/logs/bpr-1050-main-win/` and are
not committed.

## Scope

- Name the failing clause and the step first, as [[BPR-1033]] did for the
  UIforETW capture: one term per clause, with the facts the missing one waits
  on, printed after the existing marker so every earlier line keeps its place.
- Then fix the cause in the layer that owns it. Look first at what is
  Windows-specific on the capture's path.
- Keep the macOS and Linux behaviour: the rows pass there today.

## Acceptance

- AC1 A failing run names the capture step and which of its four clauses is
  false, with the facts that clause depends on, shown in a real Windows run.
- AC2 The cause is named and fixed in the layer that owns it, and the three
  rows pass on Windows in N of N runs (N >= 3 each).
- AC3 Negative control: with the fix disabled, the rows fail again on Windows
  at the clause AC1 names.
- AC4 macOS and Linux keep their path, or the change is run there.
