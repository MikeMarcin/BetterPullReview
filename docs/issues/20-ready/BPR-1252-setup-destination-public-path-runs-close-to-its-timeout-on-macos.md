---
schema: "implement-rfc.card.v1"
id: "BPR-1252"
title: "application_setup_destination_public_path takes 50 s of its 60 s timeout on macOS and times out in a full parallel run"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"found by the red-test backlog milestone integration pass, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1252--BPR-1252--d84fbad1d11b.json","related_cards":["BPR-1200","BPR-1213"]}
---

# BPR-1252: application_setup_destination_public_path takes 50 s of its 60 s timeout on macOS and times out in a full parallel run

## Observation

At `main` `f79d5a97d` on macOS arm64 (dev preset, Debug), the full
`ctest --test-dir build/dev -j 8 --timeout 900` run timed out
`application_setup_destination_public_path` at its own `TIMEOUT 60`
(`tests/end_to_end/CMakeLists.txt`). Run alone right after, it passed in
50.6 s. The main stabilization milestone's full run, the day before, passed it.
The course and its registration last changed on 2026-09-29 (BPR-1200's
refused-first-read course and BPR-1213's Overview folders).

## Scope

- Find where the 50 s go: the course's own waits, the fixture, or the
  composition. Measure before choosing.
- Either make the course do its work in less time, or give the row a timeout
  measured against the slowest supported station under a full parallel run,
  with the measurement recorded.

## Acceptance

- The row passes in a full `-j 8` dev run on macOS.
- The card records where the time goes and why the timeout is what it is.
