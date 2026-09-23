---
schema: "implement-rfc.card.v1"
id: "BPR-0891"
title: "application_logging is registered on UNIX only, so Windows has no evidence that logs exclude tokens"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable src/app and gates partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0891: application_logging is registered on UNIX only, so Windows has no evidence that logs exclude tokens

## Defect

`tests/end_to_end/CMakeLists.txt:2497-2505` wraps the `application_logging`
row in `if(UNIX)` **with no comment**. The test body
(`application_logging_tests.cpp:42-88`) uses only `std::filesystem`, a
`QTemporaryDir` and the composition — nothing POSIX-specific — so the gate
looks incidental rather than reasoned.

What it pins is a security property: that logs exclude the access token, the
refresh token, the data-root path and repository names (`:87-88`). **Windows
therefore has no automated evidence of any of it.**

Contrast `application_profile_activation_public_path` at `:2647`, whose
UNIX-only gate *is* justified — by the Windows stub at
`src/adapters/sqlite/src/profile_activation.cpp:270-275`. That is what a
reasoned platform gate looks like, and this one has no equivalent.

There is a second-order reason this matters now: [[BPR-0885]] records that the
diagnostics log does not open on Windows at all. If that is fixed, Windows
starts writing logs with no redaction contract behind them. The two should
land together, in that order.

## Scope

- Register the row on Windows. The owner-private root pattern it needs already
  exists at `github_application_liveness_tests.cpp:50-62` ([[BPR-0832]]).
- If it genuinely cannot run there, write the reason into the `if(UNIX)` as a
  comment, the way the activation row does, and say where the Windows evidence
  for redaction comes from instead.
- While there, check the other unexplained platform gates in the same file.
  An unexplained `if(UNIX)` around a security property is worth a sweep, not
  just a fix.

## Acceptance

- Windows has automated evidence that logs exclude the access token, refresh
  token, data-root path and repository names, or a recorded reason why not.
- The gate carries its reason in a comment either way.
- macOS and Linux keep the row.
