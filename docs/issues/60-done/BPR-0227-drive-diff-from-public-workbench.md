---
schema: "implement-rfc.card.v1"
id: "BPR-0227"
title: "Drive selected-file diff requests from the public workbench"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0226"]
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

## Implemented slice

- Bounded accepted file observations retain semantic identity, presentation
  path, provider-free current/previous repository paths, change kind, and
  content kind while the core continues to see only bounded cursor/order facts.
- The workbench host owns a cancellable, superseding selected-diff lifecycle.
  The epoch host authorizes its exact accepted comparison on the decision lane
  and resolves both selected endpoints independently on the already-retained
  cache actor. Rename candidates fail closed on ambiguity; reverse pairs and
  copied files do not inherit base-to-head provider direction. Multi-hop
  renames use one strict selected-pair record on that actor, so `a -> b -> c`
  resolves `b -> c` forward and `c -> b` in reverse without following history.
- Installed Git performs a literal `ls-tree -z` lookup for each present side,
  validating exact path, object format, blob type/mode, bounds, and
  cancellation. Added/deleted absence is representable; two required absent
  sides and non-blob entries fail closed.
- The Qt file model retains the opaque identity, emits semantic selection from
  the real list, and suppresses projection-driven false selection signals.
- Accepted epoch comparisons rearm an early or retained selection. An authority
  refresh first publishes empty rows/cursor state, so failure cannot expose a
  prior-generation row under replacement authority.
- `application_workbench_composition_contract` drives the real file-list
  selection into the existing diff host, verifies the complete comparison key,
  counts zero runtime fixture-helper calls, and proves an independent test-only
  cut of the real Qt-to-host selection wire fails while the helper still exists.

Focused evidence covers Git process/cache lookup, actual forward/reverse
multi-hop rename chains, epoch retained-actor resolution and revocation,
workbench supersession/replay/cancellation, Qt incremental selection at three
scales, and the shipped executable trace.
The final focused matrix passed 10/10, including the retained-cache contract
and public positive/cut-wire composition trace. Independent critical review
cleared the slice with no remaining blocking or high-risk finding.
