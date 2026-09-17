---
schema: "implement-rfc.card.v1"
id: "BPR-0838"
title: "The anonymous smart-Git acquisition counter is read across threads without synchronization"
status: "Backlog"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0829 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0838: The anonymous smart-Git acquisition counter is read across threads without synchronization

## Defect

The anonymous smart-Git lease factory counts leases in a plain `std::size_t`
(`Acquisitions`, `src/app/src/github_application.cpp` ~1921). Historical
retention imports acquire leases on a worker thread, and `status()` reads the
count on the Qt thread (~1959, ~13081) for `PublicPreviewSmartAcquisitions`.
That is a data race. [[BPR-0829]] made the course read the count only after
every import finished, which avoids the race in that course but leaves the
counter itself unsynchronized.

## Scope

- Make the counter safe to read from the Qt thread (an atomic, or a snapshot
  through the completion lane), without changing what it counts.
- The file is inside the round-trip contract's pinned scope: reseal with a
  recorded review.

## Acceptance

- The counter is synchronized, `application_anonymous_public_preview_public_path`
  still passes on macOS, and the round-trip contract passes after a reviewed
  reseal.
