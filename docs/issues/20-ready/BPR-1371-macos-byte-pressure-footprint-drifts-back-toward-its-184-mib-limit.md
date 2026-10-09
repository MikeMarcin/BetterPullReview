---
schema: "implement-rfc.card.v1"
id: "BPR-1371"
title: "macOS: the byte-pressure course's footprint drifts back toward its 184 MiB limit while the live heap stays flat"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"owner, 2026-10-09: application_large_review_cache_bytes footprint median 178.8 MiB (155-190, one miss at 190.0) against 158.6 at BPR-1296's fix; Windows flaked too","reservation":"docs/issues/_reservations/BPR-1371--BPR-1371--a64e15ed93fa.json","related_cards":["BPR-1296","BPR-0967","BPR-1032","BPR-1327","BPR-1344","BPR-1360"]}
---

# BPR-1371: macOS byte-pressure footprint drifts back toward its 184 MiB limit

## Observation

On macOS `application_large_review_cache_bytes` (limit from [[BPR-1296]]:
footprint growth at most 184 MiB over `before_application`) is close to its
limit again. The live heap sampled at each selection is steady at 158.8 MiB,
but the footprint median is 178.8 MiB (range 155-190, one failure at 190.0)
against 158.6 at BPR-1296's fix. Windows also saw the row flake.

## Outcome

Find which change raised the footprint while the heap stayed flat. If the
cause is a real retention, fix it with a test. If it is measurement noise in
footprint against heap, propose a better-grounded assertion here and bring it
to the owner before the limit changes.

## Direct acceptance

- AC1: an alternated A/B on macOS, at least five runs each, of `main`,
  `3947245ca^` and BPR-1296's fix `fa3537d8c`, with the station's load
  recorded.
- AC2: the footprint rise is attributed to a change, with before/after
  measurement.
- AC3: a real retention is fixed with a test, or a better-grounded assertion
  is proposed here and taken to the owner before the limit changes.
