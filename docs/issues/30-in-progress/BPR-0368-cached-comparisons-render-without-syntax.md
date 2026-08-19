---
id: BPR-0368
title: Cache-served comparisons render without syntax classification
status: In Progress
rfc: RFC-BPR-0006
depends_on: []
---

# Cache-served comparisons render without syntax classification

Found by the owner during BPR-0100's attended visual pass, minutes into
looking: `UIforETW/UIforETWDlg.cpp` — real C++ — renders monochrome in the
dark profile, no keyword, comment, or string coloring at all, while the
status line reports `Diff ready — 8 semantic changes (cached)` and every
composition marker stays green.

The mechanical facts, probed at the same binary:

- `SYNTAX_CLASSIFIER_COMPOSED_OK` — the classifier is composed.
- The KSyntaxHighlighting definitions are embedded in the executable.
- The automated `uiforetw-74` offscreen course reports
  `SYNTAX_CLASSIFICATION:1:2` — two submissions, one adopted — so even the
  green path adopts only half of what it submits.
- `adopt_classification` (`src/app/src/github_application.cpp:3993`) drops
  any publication whose blob no longer matches the current comparison's
  left/right key, deliberately and silently, and
  `adopt_token_spans` returning false is also uncounted.

The leading hypothesis: classification is submitted when a blob is
materialized, not when a comparison is displayed, so a comparison served
from the exact cache — a revisited file, or a course whose comparisons were
precomputed — renders with no spans and nothing resubmits them. The markers
ask whether the classifier was composed and whether some classification was
ever adopted; nobody asks whether the comparison on screen is classified.
This is BPR-0216's shape wearing syntax colors.

## Required final state

- The mechanism is identified from the live wiring: where classification is
  submitted, what clears adopted spans, and why a cache-served comparison
  ends up unclassified.
- A displayed comparison's panes carry the classification for their exact
  blobs regardless of whether the comparison was computed or cache-served,
  with stale-result rejection preserved.
- A deterministic check fails when the selected comparison renders
  unclassified for a file whose grammar is embedded — asking about the
  screen, not the composition.
- The attended visual pass can be re-presented against repaired bytes.

## Verification

```sh
ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|qt_diff_workbench_scale_100|application_workbench_composition_contract)$' --output-on-failure
```
