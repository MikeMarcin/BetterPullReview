---
schema: "implement-rfc.card.v1"
id: "BPR-1369"
title: "The prepared-open rule judges work avoided, not a wall-time race"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1360's Linux station run, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1369--BPR-1369--7cac3359b2b2.json","related_cards":["BPR-1360","BPR-1327","BPR-1344"],"acceptance":["BPR-1369:old-rule","BPR-1369:work-avoided","BPR-1369:wall-time","BPR-1369:control","BPR-1369:rows"]}
acceptance: []
---

# BPR-1369: The prepared-open rule judges work avoided, not a wall-time race

## Finding

`application_large_review_prepared_open` ([[BPR-1327]]) requires that every
prepared open appear sooner than every unprepared one
(`tests/end_to_end/large_review_benchmark.cpp`, `run_prepared_open`).
[[BPR-1360]] made a cold open read both sides of its comparison at once. On
the local, complete-clone fixture both sides now come from the cache in one
actor job, so unprepared opens got about 8 ms faster (38.6-42.6 ms in
Release) while prepared opens stayed at 40-44 ms. The two distributions
overlap and the row fails about 8 of 10 times in Release. The prepared opens
did not get slower.

On this fixture a cold open is cheap, so a race between two wall times no
longer says whether preparation did its job.

## Outcome

The rule says what preparation is for:

- **Work avoided.** A prepared open reads no blob of its comparison, from Git
  or the network, and computes no diff for it. Every unprepared open reads
  its comparison. Judged from counts, not from wall time.
- **Wall time.** Prepared opens are no slower than unprepared ones beyond a
  small, justified tolerance, so a prepared open cannot turn into a slow one
  unnoticed.

## Acceptance

- The old rule fails on `main` with [[BPR-1360]]: the row run 10 times in
  Release, failures counted.
- The new rule passes 10 of 10 in Release and 10 of 10 in Debug.
- A control that breaks preparation fails the new rule, and is reverted.
- The large-review rows, `application_epoch_host` and
  `test_network_isolation_manifest` pass; format and documentation gates
  pass.

## Boundaries

Only the prepared-open course's assertion changes. If no existing count
attributes reads to the selected open, the coordinator pre-approved
(2026-10-09) the smallest testing-only fixture-access counter in `src/app`,
with no production behaviour change and the round-trip pins resealed in the
same commit.
