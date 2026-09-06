---
schema: "implement-rfc.card.v1"
id: "BPR-0631"
title: "Compile exact large-review epoch replays"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0630"]
platforms: ["mac"]
acceptance: [{"criterion":"stored-epoch-inputs","outcome":"pass","revision":"d0047177aa0305666b7ee811d445f3ea0b14621f","evidence":{"kind":"observation","detail":"Fresh generator preserves every prior fixture byte; 31 compact exact deltas match installed Git before/after OIDs and sizes. Seven semantic/corruption negatives pass."}},{"criterion":"exact-paged-replays","outcome":"pass","revision":"d0047177aa0305666b7ee811d445f3ea0b14621f","evidence":{"kind":"observation","detail":"All 32 heads normalize via the production pager with complete 515-file/1000-thread counts and bound base/head/commit identities. Independent source-ordinal expectations verify REST blobs; unknown heads and broken chains/cursors/pages refuse."}},{"criterion":"compatibility-and-scope","outcome":"pass","revision":"d0047177aa0305666b7ee811d445f3ea0b14621f","evidence":{"kind":"observation","detail":"Original latest-head public app benchmark passes 22.17s; fixture and replay contracts pass 7.43s/5.84s, relevant builds and documentation/diff checks pass. BPR0632 remains open for composed many-epoch work; no remote/account activity."}}]
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

## Acceptance evidence

Source `d0047177aa0305666b7ee811d445f3ea0b14621f`. The 14,569-byte `epoch-deltas.json` records all 31 transitions
between the 32 stored review heads, with exact prior/current OIDs and changed
file blob identities/sizes. A fresh local generator run reproduced the existing
pack, manifest, recipe and ten comment pages byte-for-byte; only the new sidecar
was copied into the fixture. The independent installed-Git contract compares
all parent/head transitions and each before/after blob and size, including seven
negative courses (new head/blob/size/missing-delta corruption plus original
comment anchor/identity and pack corruption).

The test-only Glaze compiler accepts only an exact stored head, reconstructs its
file metadata by reversing later deltas, and binds REST PR/commit, GraphQL head
and check request identity to it. Earlier synthetic commit node IDs are distinct.
Default latest-head request and response contents are preserved. An independent
ordinal rule (source N changes once in head N+2) checks every earlier REST blob
against the Git-verified sidecar, separately from the reverse reconstruction.
Every head then passes through `perform_paged_github_read`, consuming all 28
PR-pager exchanges with the normal resource-scoped response headers. Each typed
snapshot retains the exact base/head/commit and all 515 files/1,000 threads.
The surrounding session /user remains in the full 29-exchange replay script.
Unknown/empty/base/abbreviated head requests and substituted chain/delta source
or missing transition data refuse. Existing strict REST page and GraphQL
cursor/noncanonical-body negatives remain passing.

Local network-denied validation on the same recorded macOS/arm64 Debug setup:

- `large_review_fixture_contract`: pass, 7.43s, including all seven negatives.
- `large_review_replay_contract`: pass, 5.84s, including all 32 production-pager
  snapshots and exact page/cursor/identity refusals. The first direct-pager test
  omitted the app fixture transport's required rate-limit headers; adding that
  existing transport decoration corrected the harness without changing product
  parsing. No product relaxation was made.
- `application_large_review_benchmark`: pass, 22.17s, through the unchanged
  default latest-head public application path, source/navigation/scroll and full
  window render. This remains one application epoch.
- Both changed dev targets build with the repository warning policy. Documentation
  structure gate and `git diff --check` pass. The documentation verifier's 62
  unchanged unit tests were already passing in BPR-0630; no core/Qt/product
  authority source changed that warrants repeating their unrelated matrices.

BPR-0631 is complete. BPR-0632 remains required for actual application refresh,
history/selection/cancellation, durable catalog/ref evidence and measurements
across all 32 heads. Synthetic thread text and line identities are workload
projections, explicitly not historical GitHub captures. No real account, network,
Actions, paid CI, push or production process was used. Task-owned generated
fixture copy, editing scripts, logs and measurement/render files are consumed
and removed after recording these observations.
