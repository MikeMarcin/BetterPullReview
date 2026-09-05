---
schema: "implement-rfc.card.v1"
id: "BPR-0260"
title: "Restore compact-navigation source formatting"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0259"]
---

# Restore compact-navigation source formatting

The repository-wide formatting gate reports drift in
`src/app/src/github_application.cpp` after the compact-navigation branch was
merged. The affected translation unit is also a digest-bound production
composition source for BPR-0256, so its formatting must be repaired before the
attended live-app packet receives its final source and capsule identities.

## Required final state

- Apply the repository's canonical formatter to the one affected application
  translation unit without changing its behavior, ownership, or public path.
- Refresh only the exact integrity expectations that bind the resulting source
  bytes; do not weaken or remove any hostile composition cut.
- Re-run the compact-review regression selector and the production
  round-trip-composition contract.
- Regenerate BPR-0256's source manifest, capsule, approval subject, and operator
  packet after this card lands; no earlier digest may authorize the reformatted
  source.
- Pass the repository format, strict tidy, documentation, and diff gates.

## Non-goals

- Do not change compact navigation, selection, rendering, mutation authority,
  or live-lane policy.
- Do not absorb the separate Windows process formatting drift owned by
  in-progress BPR-0240.
- Do not create an approval file or execute the designated live mutation test.

## Verification

```sh
cmake --build --preset dev --target coding-format-check coding-tidy
ctest --preset dev --output-on-failure -R '^(qt_diff_workbench_scale_(100|125|200)|qt_appearance_profile|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_review_round_trip_composition_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Implementation checkpoint — 2026-08-11

Canonical formatting changed one line by sorting the designated public
observer include before the other GitHub adapter includes. No executable token,
authority boundary, navigation path, or rendering behavior changed. The exact
full-source and source-tree integrity digests were refreshed; the production
mutation-branch digest remained unchanged. The required selector passed 7/7 in
127.56 seconds, the 373-file formatting gate passed, documentation reported
`DOCUMENTATION_OK`, and `git diff --check` passed.

The strict repository tidy scan reached all 227 translation units and then
reported five older violations in three files owned by the BPR-0256 designated
live-review lane: one unsafe string-view pointer in the public observer, one
mutation-contract local naming violation, and three unsafe string-view pointers
in the approval bootstrap. This card remains in progress until that separately
owned gate drift is repaired and the exact full tidy target passes; none of
those findings changes this card's one-line formatting scope.

## Terminal closure — 2026-08-12

BPR-0256 repaired its separately owned strict-tidy findings without expanding
this card's source scope. From that stable tree, the combined repository gate
passed: formatting checked all 373 files and strict warnings-as-errors tidy
checked all 227 translation units. Documentation validation again reported
`DOCUMENTATION_OK`, and `git diff --check` passed. The one-line canonical
include ordering and its exact integrity digests are therefore terminal for
this card; BPR-0256 separately owns the regenerated attended packet.
