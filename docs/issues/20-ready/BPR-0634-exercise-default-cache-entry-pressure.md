---
schema: "implement-rfc.card.v1"
id: "BPR-0634"
title: "Exercise default cache entry pressure"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0633"]
platforms: ["mac"]
---

# Exercise default cache entry pressure

Host unit tests prove small-capacity eviction. The real application has not yet
been driven past its default 64-entry immutable diff/index cache capacity.

## Bounded acceptance and ownership

- Add an independent public large-review course selecting 65 distinct small
  files through the normal composition, provider/installed Git/cache actor and
  file drawer. Do not reduce configured cache limits or inject cached results.
- Verify exact blob/source identities, the growing cache up to its default
  capacity, eviction at the next unique selection, and weak ownership release
  of both the evicted raw diff and its prepared index. Revisit the evicted file
  through the same public selection path and verify correct rematerialized data.
- Record entry/byte/job and phase/input counters plus native memory samples;
  render the real window. Close/drain/destroy after pressure, using the existing
  ownership and preserved Git/catalog checks. Retain provisional Mac resource
  budgets and avoid portable timing claims.
- Own tests/end_to_end/large_review_benchmark.cpp and CMakeLists.txt. Reuse the
  current fixture/inspection ports; correct a contained demonstrated defect
  with focused product tests if needed. Keep original courses intact.
- Byte pressure remains separately required in BPR-0635; this card cannot close
  the broader cache-budget criterion by itself.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Local Mac, synthetic and network-denied only. No Actions, dispatch, push, paid
CI, credentials or production account/process operations. Earlier human/live
and Windows/Linux gates and deferred work remain open.
