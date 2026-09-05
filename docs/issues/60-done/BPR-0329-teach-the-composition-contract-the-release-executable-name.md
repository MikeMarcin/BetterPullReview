---
schema: "implement-rfc.card.v1"
id: "BPR-0329"
title: "Teach the composition contract the renamed release executable"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0328"]
---

# Teach the composition contract the renamed release executable

[[BPR-0328]] gave the macOS Qt bundle `OUTPUT_NAME "BetterPullReview"`, which is
the right name for a shipped application. It did not update
`tests/end_to_end/composition_contract_tests.py`, whose
`find_better_pull_review_executable` searches only for `better_pull_review` and
`better_pull_review.exe`. The tests-disabled Release case builds successfully
and then fails to find what it built:

```
AssertionError: expected exactly one built BetterPullReview executable; found none
```

`application_composition_contract` has been red since that commit. The build is
fine; only the finder is stale.

## Why this is its own card

Two sessions share this checkout. The rename landed while unrelated appearance
work was in flight, and the failure surfaced in that work's closeout matrix
rather than in its own. Folding the repair into either slice would attach it to
a change that did not cause it, so it gets its own card and its own commit.

## Required final state

- The finder accepts the shipped macOS name as well as the historical ones, and
  keeps rejecting an ambiguous result rather than picking one arbitrarily.
- `application_composition_contract` passes from a clean build.
- No production or packaging behaviour changes. This card touches test support
  only; [[BPR-0328]] owns the name itself.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure -R '^application_composition_contract$'
python3 scripts/verify_documentation.py --root .
```

## Result

`EXECUTABLE_NAMES` now lists both the target name and the shipped macOS name,
and the ambiguity guard is unchanged: two matches still fail rather than one
being chosen. `application_composition_contract` passes.

No production or packaging file was touched.
