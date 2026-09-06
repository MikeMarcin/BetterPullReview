---
schema: "implement-rfc.card.v1"
id: "BPR-0631"
title: "Compile exact large-review epoch replays"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0630"]
platforms: ["mac"]
---

# Compile exact large-review epoch replays

The large-review fixture contains 32 distinct Git heads, but the composed course
currently opens only the latest and therefore creates one application epoch.
RFC-BPR-0013 requires many-epoch benchmark and stale-selection evidence.

## Bounded acceptance and ownership

- Add compact deterministic per-head file deltas alongside the existing stored
  32-head pack. Preserve existing pack/manifest/comment bytes. Independently
  compare every delta and ordered head identity with installed Git; malformed
  or substituted deltas must fail.
- Extend the test-only Glaze replay compiler to serve any exact stored head,
  with matching REST/GraphQL/check identities and file blob metadata. Keep the
  default latest-head 29-request script identical. Reject absent/noncanonical
  heads; never use a prefix, wildcard, live request or implicit fallback.
- Verify each head through exact paged replay, using typed normalized response
  checks and negative identity/page/cursor cases. This is fixture input evidence;
  BPR-0632 owns actual 32-epoch composition, timings and application claims.
- Own scripts/build_large_review_fixture.py, its independent script tests,
  the new epoch-deltas.json and fixture README, src/app/src/large_review_fixture.hpp,
  and tests/end_to_end/large_review_replay_tests.cpp. Reuse existing tests/build
  registration. Do not change production protocol behavior or source authority.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work is local, network-denied and synthetic. No Actions, workflow dispatch,
push, paid CI, credential access or production app/account operation. Existing
human/live gates and Windows/Linux work remain open.
