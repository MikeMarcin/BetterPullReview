---
schema: "implement-rfc.card.v1"
id: "BPR-0623"
title: "Add deterministic large-review workload fixtures"
status: "Ready"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Add deterministic large-review workload fixtures

RFC-0013 requires checked-in benchmark inputs for large file counts, large
files, many comments, many epochs and mixed binary/generated content. Retained
BPR-0080 exercises 2,000-line workbench interactions and BPR-0066 exercises
paged epoch models, but these do not supply a coherent large-review workload
for the ordinary application. The existing served PR recordings have two or
five files. Build this missing input boundary before deriving latency budgets.

## Bounded acceptance and ownership

- Add a deterministic synthetic workload recipe and compact checked-in Git pack
  with at least 512 changed source files, a 20,000-line comparison, binary and
  generated content, and 32 distinct head revisions sharing a stable base.
  Include 1,000 explicitly synthetic review comments in bounded pages with
  stable identities and valid source anchors. No real repository or personal
  content is used.
- Record exact counts, paths, object identities, content/shape expectations,
  provenance and recipe version. A regeneration helper uses only isolated
  local Git and fixed fixture identities. It must not run hooks, repository
  code, credential helpers, network requests or GitHub Actions.
- Verify the stored pack through actual Git import/fsck/tree/diff/blob reads,
  independently compare the fixture shape and revision graph, and validate
  comment page membership, identities and anchors. Reject fixture drift.
  The fixture contract must detect changed/corrupt stored input rather than
  only re-run its own generator.
- Own `scripts/build_large_review_fixture.py`, a focused script contract and
  CTest registration under `tests/`, the fixture under
  `tests/fixtures/github/pull-requests/large-review-v1/`, and fixture-catalog
  documentation. Use existing contained temporary-directory support.
- Run the isolated fixture contract, isolation manifest, documentation gate
  and `git diff --check`. This fixture boundary does not claim application
  responsiveness, memory ceilings, target-native latency budgets or RFC
  closeout. The next slice must serve these inputs through the real provider/
  installed-Git/application path and measure actual interactions under load.

RFC base remains `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; BPR-0617 is the
closeout anchor. Earlier live gates and explicit deferrals are unchanged.
