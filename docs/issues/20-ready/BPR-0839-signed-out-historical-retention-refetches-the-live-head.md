---
schema: "implement-rfc.card.v1"
id: "BPR-0839"
title: "Signed-out historical retention refetches the live head with no haves"
status: "Ready"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0829 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0839: Signed-out historical retention refetches the live head with no haves

## Observation

While diagnosing [[BPR-0829]], the anonymous public preview fixture course
showed four smart-Git leases: the live epoch's pack, then one background
retention import per force-push occurrence on the timeline (events 91701,
91702 and 91703; `src/application/src/epoch_application_host.cpp`
`start_historical_import` ~6282, next import ~6503). Two of the three imports
fetch head `f9108913`, which is the live head already in the cache, and they
fetch with no haves (~6283). Each import took about 7.5 to 8 s in the fixture
at load 3 to 19 (`build/r2-logs/dsp3-0829-instrumented*.log`).

For a signed-out read this spends the shared unauthenticated budget and time
on bytes already present.

## Scope

- Confirm whether an occurrence whose after-head is already retained (or is
  the live head) needs an import at all, and whether imports should offer
  haves.
- If the import is skipped or narrowed, [[BPR-0829]]'s course pins
  `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4` and must change with it, with the
  reason.

## Acceptance

- The card records whether the refetch is needed; if not, the import no
  longer refetches an already retained head, the course's count is updated
  with its reason, and the epoch persistence and portable occurrence courses
  still pass on macOS.

**Platform timing (dsp-003).** With [[BPR-0829]]'s wait in place, the whole
course takes about 1.5 s on Linux (`bpr-linux`, 5 runs) and 33 to 36 s on
macOS, where each retention import took about 7.5 s. Whether the macOS cost
is the refetch, the sandboxed Git process or the fixture transport is not
established.
