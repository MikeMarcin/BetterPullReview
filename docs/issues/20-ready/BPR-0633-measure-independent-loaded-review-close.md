---
schema: "implement-rfc.card.v1"
id: "BPR-0633"
title: "Measure independent loaded-review close recovery"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0632"]
platforms: ["mac"]
---

# Measure independent loaded-review close recovery

BPR-0630 closes the public window after prune/cache deletion already releases
the large comparison. BPR-0632 closes with retained history but does not measure
memory. Neither establishes independent close recovery for a loaded large file.

## Bounded acceptance and ownership

- Add a separate Mac course in the existing real-composition large-review
  benchmark and CMake registration. Open the same 515-file / 1,000-comment
  fixture through ordinary provider/installed-Git paths, select and render the
  exact 20,000-line comparison, then close the public window while it still
  retains the raw diff, prepared index and native source documents.
- Observe the real last-window-close shutdown handoff and bounded drain.
  Verify zero derived cache/jobs, release of weak diff/index ownership and
  destruction of the window/source documents. Preserve retained Git/catalog
  identities and prove no prune/cache-delete action caused the release.
- Measure native RSS/physical footprint before load, with the large comparison,
  after close/drain and after application destruction. Reuse the documented
  provisional 160-MiB sampled-growth / 96-MiB cleanup-growth budgets from
  BPR-0630; distinguish allocator retention and sampled values from object
  lifetime and unsampled peaks. Keep broader cache pressure and platforms open.
- Own tests/end_to_end/large_review_benchmark.cpp and CMakeLists.txt, plus a
  contained demonstrated lifecycle defect and its focused tests if found.
  Preserve existing resource/default courses and the public composition.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work is local, synthetic and network-denied. No Actions, dispatch, push,
paid CI, credentials or production account/process operation. Earlier live and
human gates, Windows/Linux and explicitly deferred work remain open.
