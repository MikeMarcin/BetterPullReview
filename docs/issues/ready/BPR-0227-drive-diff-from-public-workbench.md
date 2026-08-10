---
id: BPR-0227
title: Drive selected-file diff requests from the public workbench
status: Ready
rfc: RFC-BPR-0005
depends_on: [BPR-0226]
---

# Drive selected-file diff requests from the public workbench

At the reviewed head, the only caller of the composition's `request_diff()` is
the `BPR_BUILD_TESTING` fixture-access helper. The green selected-diff public
test calls that helper directly; no window gesture or workbench selection can
request a comparison.

## Required final state

- Accepted file observations retain the exact identities and blob facts needed
  to form `file_comparison_key` without path guessing or fixture knowledge.
- Selecting a file through the shipped window follows the semantic workbench
  intent/change path and requests the exact current comparison.
- Repository, PR, epoch, anchor, blob, file, options, algorithm, and generation
  identity are all revalidated before publication.
- Binary, missing, oversized, cancelled, stale, and failed states remain typed
  and visible.
- The direct public trace contains no
  `github_application_fixture_access::request_fixture_diff` call. Removing the
  real gesture-to-request wire fails it while the fixture helper still exists.
