---
id: BPR-0233
title: Exercise the recorded UIforETW diff and intraline pixels through the selected public path
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0227]
---

# Exercise the recorded UIforETW diff and intraline pixels through the selected public path

The recorded `google/UIforETW#74` fixture is now served and its generator is
wire-exact, but the registered public-path test only opens the pull request and
saves a window grab. It never requests a selected diff. `RENDER_OK` means the
PNG save succeeded, not that it contains comparison or intraline content.

## Required final state

- Run the recorded fixture through BPR-0227's real file-selection path.
- Assert the selected comparison uses the recording's expected base/head blobs
  and contains the known near-identical replacement pairs.
- Assert nonempty intraline removed/added ranges and the exact semantic role
  applied to their pane layout.
- Capture the affected pixels in Light, Dark, High-contrast Light, and
  High-contrast Dark only after the deterministic range/role assertions pass.
- Bind an attended verdict to those exact captures before BPR-0214 or BPR-0210
  closes.

Fixture serving is complete in BPR-0187. It is not the remaining blocker.

## Implemented state

`application_uiforetw_selected_diff_contract` now runs the recorded fixture
through the shipped Files drawer and BPR-0227's selected-diff composition. It
selects `UIforETW/Resource.h` and `UIforETW/UIforETW.rc`, binds the generated
base/head commits and exact file blobs, and requires the known `136` → `137`,
`464,28` → `464,24`, and `464,42` → `464,36` intraline pairs. The course reads
the pane's non-full-width selections back under the removed/added intraline
semantic roles before it can capture evidence.

Four isolated processes capture Light, Dark, High-contrast Light, and
High-contrast Dark at 1280 × 1000. A fifth process cuts the real Qt selection
wire and must fail while the fixture-diff helper remains compiled and reports
zero invocations. The deterministic product and capture contract is green;
the card remains open until the exact captures receive the required attended
verdict.

## Attended checkpoint — awaiting user

The deterministic implementation is complete at producing commit `ff75096`.
The exact four-profile batch is available at
`docs/verification/reviews/uiforetw-intraline-acceptance-v001/offline-package/index.html`
and is bound as
`bpr:/attended-review/betterpullreview/7be85b53158def375f406b1a`. BPR-0233 is
parked in Backlog solely while awaiting the exported human verdict; no
downstream closure claim is enabled by the package.

## Verification

```sh
ctest --preset dev -R '^application_uiforetw_selected_diff_contract$' --output-on-failure
ctest --preset dev -R '^(github_recorded_fixture_contract|qt_diff_workbench_scale_100|application_workbench_composition_contract)$' --output-on-failure
```
