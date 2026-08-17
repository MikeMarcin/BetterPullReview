---
id: BPR-0330
title: Land the accepted dark intraline fill and the format gate it was missing
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0323]
---

# Land the accepted dark intraline fill and the format gate it was missing

[[BPR-0323]] closed the intraline emphasis sequence at `fb62f1e`, and its commit
message says what the owner accepted: "Dark brightens to #145E20 and #933030 in
the style of an owner-supplied reference." The card's own closing table says the
same, and the accepted attended subject
`criterion:/betterpullreview/bpr-0323/dark-green-strength` binds the render in
`02-after-green-at-30-7.png`.

The source never got there. `47b9308` set the dark fill to `#1E5D2A` — the
*before* of that comparison, the one measuring 25.2 — and no later commit
changed it. `fb62f1e` shipped the captures, the verdict, the verdict validation,
and three closed cards describing a colour the product did not paint.

So for four hours `HEAD` documented an accepted result it did not implement.
Nothing failed, because every gate that could have caught it measures the source
against itself: the headroom guard asserted `#1E5D2A`'s headroom of 5, the
captures were rendered before the commit that didn't include the change, and no
test compares a shipped palette against the hex in an accepted verdict.

The repair is the edit that was already sitting unstaged in the working tree,
made at 19:43 and 20:23 on 2026-08-16, after `47b9308` and before the three
commits that followed it.

## What this card lands

| File | Change |
|---|---|
| `src/adapters/qt/src/appearance_profile.cpp` | `diff_intraline_added_background` `#1E5D2A` → `#145E20` |
| `tests/adapters_qt/appearance_profile_tests.cpp` | Recorded dark headroom 5 → 1, and the comment that explains why every profile now sits on its floor |
| `tests/end_to_end/application_review_round_trip_composition_contract.cmake` | Both pinned digests re-stamped for the changed `src/app` bytes |
| `diff_workbench.hpp`, `diff_workbench.cpp`, `github_application.cpp`, `appearance_contrast.cpp`, `appearance_contrast_tests.cpp` | Formatting only |

The headroom is a measurement, not a decision. The guard records the tightest
emphasis pair in each profile, so changing the fill changes the number; 1 is
what `#145E20` produces, and `qt_appearance_profile` fails on any other value.
That is the guard working as [[BPR-0323]] intended — the numbers came from
verdicts, so moving one has to reopen the gate.

The formatting is not incidental either. `HEAD` did not pass
`_coding-standard/scripts/format_check.py` under the repository's configured
`clang-format`: six first-party files disagreed with `.clang-format`, and
`src/core/src/appearance_contrast.cpp` alone accounted for 41 of the 59
disagreeing lines, having been committed with hand-aligned declarations and
one-line switch arms that the configuration does not produce. Running the
repository formatter over the files this change touched is the board's own rule
for reaching `60-done/`, and it repaired those six as a side effect.

## Required final state

- The dark profile paints `#145E20`, the exact fill bound to the accepted
  attended subject, and `#933030` is unchanged.
- The recorded headroom agrees with the shipped palette rather than with the
  superseded one.
- The whole first-party tree passes the repository format check.
- The composition contract's pinned digests match the `src/app` bytes actually
  shipped.
- No appearance decision is reopened. This card lands an accepted result; it
  does not select one.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
python3 _coding-standard/scripts/format_check.py --root .
python3 scripts/verify_documentation.py --root .
```

## Result

[[BPR-0323]]'s own focused selector passes 6/6 from a clean `dev` build, in
89.21 seconds under the repository network-isolation wrapper:
`core_appearance_contrast`, `qt_diff_workbench_scale_100`,
`qt_diff_workbench_scale_125`, `qt_diff_workbench_scale_200`,
`qt_appearance_profile`, and `application_uiforetw_selected_diff_contract`.

`qt_appearance_profile` is the one that proves the fill: it asserts the recorded
headroom exactly rather than as a bound, so the pair `#145E20`/1 passes and the
pair `#1E5D2A`/1 does not. The palette and the guard cannot drift apart
silently, which is precisely the property [[BPR-0323]] asked the guard to have.

The format check passes across all 373 first-party files. `HEAD` before this
card did not.

No verdict was reopened, re-requested, or inferred. The accepted subject digest
in `human-verdict.json` is untouched, and this card claims only that the source
now paints what that verdict accepted.
