---
schema: "implement-rfc.card.v1"
id: "BPR-0882"
title: "The startup exception boundaries collapse every cause into one word, even in test builds"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review: the Claude/fable src/app partition and Codex independently","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0882: The startup exception boundaries collapse every cause into one word, even in test builds

## Defect

Two reviews found this separately, and this dispatch paid for it directly:
`bpr-win` had to add a temporary print to `github_application.cpp` just to
learn which startup failure its five red rows were hitting ([[BPR-0874]]).

Three boundaries each throw the cause away:

- `src/app/src/github_application.cpp:12633-12655` — the `catch (...)` around
  `github_profile_runtime` construction, `start_services()` and `open_window`
  (which throws `runtime_error{"initial window unavailable"}`) collapses to
  `github_application_startup_failure::unavailable`, and `main.cpp:8565`
  prints only `startup_failure=unavailable`. A window-wiring failure is
  indistinguishable from a workspace-root throw at `:4866`.
- `src/app/main.cpp:8708` — the desktop completion callback's boundary emits
  only `STARTUP_FAILED`. The **outer** handler at `:8741` deliberately prints
  `error.what()` in test builds, but nothing caught by the inner boundary ever
  reaches it. So the diagnostic that exists is unreachable for this class.
- `src/app/src/profile_desktop.cpp:734` and `:788` — construction and
  window-wiring collapse arbitrary exceptions into `unavailable` too.

[[BPR-0874]] names the sibling `registry` arm, which hides seven distinct
causes. This card is the same disease in the other arms, and the two should
probably be fixed in one pass.

## Scope

- Carry `what()` through to the stderr line **in developer and test builds**,
  where it costs nothing and is what every automated course reads. Production
  can keep the single word if there is a reason to, but say what the reason
  is.
- Give the arms distinct names where the causes are genuinely distinct, rather
  than one catch-all per boundary.
- `tests/end_to_end/CMakeLists.txt:2295-2296` pins
  `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`. When BPR-0874 splits
  that arm, **this pin has to move with it** or it will fail for the right
  reason. Coordinate the two.

## Acceptance

- A startup failure in a developer or test build names its cause, shown in a
  real failing run.
- The noncanonical-root pin still passes, against whatever name replaces
  `registry`.
- No production diagnostic leaks a path, token or repository name that it did
  not leak before; see the redaction properties [[BPR-0891]] covers.
