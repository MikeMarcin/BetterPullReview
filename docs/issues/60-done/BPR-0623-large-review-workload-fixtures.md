---
schema: "implement-rfc.card.v1"
id: "BPR-0623"
title: "Add deterministic large-review workload fixtures"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
acceptance: [{"criterion":"Synthetic workload","outcome":"pass","revision":"ee1717c29a1bff8d06898705718ce665ebec2377","evidence":{"kind":"observation","detail":"Stored recipe/pack/pages contain 515 changed files including 512 source files, 20,000-line large comparison, binary/generated content, 32 real head revisions and 1,000 valid source-anchored comments."}},{"criterion":"Independent stored-input validation","outcome":"pass","revision":"ee1717c29a1bff8d06898705718ce665ebec2377","evidence":{"kind":"observation","detail":"Network-isolated real Git import, strict fsck, exact graph/tree/blob/diff checks and all comment-page/anchor checks pass; wrong anchor, duplicate ID and corrupt pack refuse with updated digests."}},{"criterion":"Reproducible isolated generation","outcome":"pass","revision":"ee1717c29a1bff8d06898705718ce665ebec2377","evidence":{"kind":"observation","detail":"Separate contained regeneration matches all semantic identities and page digests; same-toolchain compressed bytes also match. Only local Git with disabled hooks/credentials/network was used."}},{"criterion":"Registration and documentation","outcome":"pass","revision":"ee1717c29a1bff8d06898705718ce665ebec2377","evidence":{"kind":"observation","detail":"large_review_fixture_contract and test_network_isolation_manifest pass 2/2 in 5.26s; documentation gate and git diff --check pass; fixture catalog/README/closeout retain exact limits and next application integration."}}]
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

## Implementation and verification

Source `ee1717c2` adds the version-one recipe, a 281,166-byte Git pack, an exact
515-file base/head inventory, 32 distinct head identities and ten 100-comment
pages. The fixture directory occupies about 1.1 MiB. All content, accounts,
comments and URLs are explicitly synthetic. Generation uses contained private
local Git storage, fixed commit identity/time, empty hooks/templates, disabled
credential helpers and forbidden network protocols. No source file is executed.

On macOS arm64, `large_review_fixture_contract` passes in 1.89s and
`test_network_isolation_manifest` passes in 3.37s (2/2, 5.26s total). The contract
imports the stored pack, performs strict fsck, checks the exact 33-commit graph,
all tree/blob IDs and sizes, 515 changed paths, actual line/binary counts and
every subsequent revision's changed file. Every page and source anchor is
checked. Wrong-anchor, duplicate-ID and corrupted-pack negatives all refuse
even after updating the corresponding manifest digest.

A separate invocation of the regeneration helper in a fresh contained directory
reproduced the recipe, base, heads, every file identity/change and every comment
page digest. With this same installed Git, pack bytes were also identical.
That equality is a local observation; compression identity across different
Git versions is not required. The regeneration directory was consumed by its
context manager. Documentation and `git diff --check` pass. The task configure/
test logs and finish decision are removed after these records are committed.
No GitHub Actions, push, network retrieval, paid CI or live agent was used.

This completes the defined fixture input boundary. It does not prove selected
provider paging, application responsiveness, memory/cache ceilings, native
platform latency or RFC closeout. The next Mac slice must connect this workload
to the real application path and measure source navigation/scroll/cancellation
while background work is active, then justify hardware-specific thresholds.
