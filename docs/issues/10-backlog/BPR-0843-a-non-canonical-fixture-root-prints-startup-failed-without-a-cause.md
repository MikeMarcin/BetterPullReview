---
schema: "implement-rfc.card.v1"
id: "BPR-0843"
title: "A non-canonical epoch fixture root prints STARTUP_FAILED without a cause"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0830 macOS half","found_at":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0843: A non-canonical epoch fixture root prints STARTUP_FAILED without a cause

## Defect

While running [[BPR-0830]]'s macOS half, a `--self-test --github-fixture
github-com --epoch-fixture-root R` run whose `R` was spelled through a
symbolic link (`/var/folders/…` rather than `/private/var/folders/…`) printed
only `STARTUP_FAILED`, in all 10 seed and reuse phases. The same directory
spelled canonically passed
(`build/r2-logs/dsp3-0830-s2attempt1-noncanonical-*.log`,
`dsp3-0830-diag-canonical-seed.log`). The profile storage root requires a
canonical path (`src/adapters/sqlite/src/profile_storage_root.cpp` ~53). That
refusal is correct, but nothing in the output names it. [[BPR-0834]] found
the same bare refusal on Windows, where it needed a reverted diagnostic to
read `startup_failure=5`.

## Scope

- Make an automated run's startup refusal name its `startup_failure` value
  (for example `STARTUP_FAILED:registry` or a following line). Keep the
  refusal and the markers other courses match, and change no interactive
  behaviour.
- Check which courses match `STARTUP_FAILED` exactly, and keep them passing.

## Acceptance

- A non-canonical fixture root's run names the refusal's cause.
- The courses that match `STARTUP_FAILED` still pass on macOS.
