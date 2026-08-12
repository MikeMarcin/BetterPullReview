---
id: BPR-0260
title: Restore compact-navigation source formatting
status: Ready
rfc: RFC-BPR-0006
depends_on: [BPR-0259]
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
