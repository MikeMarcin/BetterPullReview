---
id: BPR-0395
title: The asset boundary verifier cannot find the renamed release binary
status: Done
rfc: RFC-BPR-0019
depends_on: []
---

# The asset boundary verifier cannot find the renamed release binary

`scripts/verify_dif_asset_boundaries.py::_discover_application` looks for the
application by two literal names:

```python
for name in ("better_pull_review", "better_pull_review.exe"):
    for path in build.rglob(name):
```

and raises unless it resolves to exactly one file. [[BPR-0328]] renamed the
tests-disabled macOS executable to `BetterPullReview`, so on a clean
`app-release` graph the binary is
`BetterPullReview.app/Contents/MacOS/BetterPullReview` and this finds **zero**:

```
lowercase hits: []
actual binary : build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
```

`pathlib.rglob` is case-sensitive on this filesystem, so the name is genuinely
absent rather than merely differently cased. The documented invocation in
`docs/development/build-and-test.md` passes `--app-build build/app-release`, and
that arm therefore fails with `selected application graph must contain one
application binary, found 0`.

## Why nobody noticed

Two reasons, and the second is the more interesting one.

It is not gated by ctest. `dif_assets_contract` imports the module and exercises
other paths; nothing runs `_discover_application` against a release graph.

And on a host that has built the release graph *both* before and after BPR-0328,
a stale `better_pull_review.app` sits beside `BetterPullReview.app`, so the
lowercase rglob finds exactly one file and the check passes. That is how it looks
healthy here. The stale directory is masking the defect rather than causing one —
which is the same hazard [[BPR-0392]] noted from the other direction, where a
stale flat binary would have made the count two.

## Required final state

- The verifier finds the application in a clean tests-disabled graph as well as a
  clean tests-enabled one, on every platform the name differs across.
- It does not find it by accident. Whatever replaces the literal-name list must be
  derived from something the build actually states, or the next rename repeats
  this.
- Exactly-one remains the rule. Two candidates is still an error, because two
  binaries means one of them is stale and the check would silently audit the wrong
  bytes.
- A clean-graph run of the documented invocation is recorded, since a run on a
  tree carrying pre-rename output proves nothing about either.

## Not in scope

Renaming any executable, and changing what the boundary audit asserts once it has
found the binary.

## Verification

```sh
cmake --preset app-release
cmake --build --preset app-release
python3 scripts/verify_dif_asset_boundaries.py \
  --source src/adapters/qt/assets/dif \
  --dev-build build/dev --core-build build/core-only --app-build build/app-release
```

The `app-release` tree must be freshly configured and built, not reused: a tree
that also holds pre-BPR-0328 output cannot distinguish a pass from this defect.


## Result — 2026-08-19

### Derived, not listed

`_application_names` now reads the executable names the build actually states,
from two sources that answer for different graphs:

- `src/app/CMakeLists.txt`'s `OUTPUT_NAME` values, plus `bpr_add_executable`'s
  default. This covers every platform, bundled or flat, and is what makes a future
  rename self-describing rather than a repeat of this card.
- `CFBundleExecutable` from any `.app` in the build tree — the bundle stating its
  own executable's name.

On this repository that yields `BetterPullReview`, `better_pull_review`, and their
`.exe` forms. Exactly-one remains the rule, and the refusal now names both paths,
because two candidates means one is stale and auditing the wrong bytes is worse
than refusing.

### The clean-graph run the card demanded

`build/app-release` was deleted and rebuilt from a fresh configure, so the tree
carries no pre-rename output. On it:

| Lookup | Candidates found |
|---|---|
| the old literal-name list | **0** |
| the derived names | 1 — `BetterPullReview.app/Contents/MacOS/BetterPullReview` |

That is the defect reproduced and repaired on the same tree, which a host still
holding a stale `better_pull_review.app` could not have shown either way.

### Two registered cases, so this is covered rather than remembered

`dif_assets_contract` previously exercised `validate_profile` against synthetic
trees and never reached `_discover_application` with a renamed binary, which is why
the gap was invisible. Two cases now close that:

- a synthetic `app-release` tree whose only binary is a bundled `BetterPullReview`
  declaring itself through `CFBundleExecutable` — the lookup must find it;
- the same tree with the old flat binary left beside it — the lookup must refuse
  with `found 2`.


### The documented invocation, and what still blocks it

The full three-graph command does not yet pass, and the reason is **not** this
card. On freshly configured and built `dev`, `core-only`, and `app-release` graphs
it stops at:

```
DIF_ASSET_BOUNDARIES_ERROR: core-only selected Qt/app source: src/app/src/designated_live_macos_capsule_identity.cpp
```

That is a different, pre-existing violation: `5212885` ([[BPR-0354]]) added a
platform unit test that compiles an app source directly, and `core-only` builds
tests. It predates this work and is now [[BPR-0396]].

So this card's own arm is proven directly rather than through the aggregate run:
on the clean `app-release` graph the old lookup finds zero candidates and the
derived lookup finds exactly one. The `--app-build` arm is what this card owns, and
it works; the run as a whole waits on BPR-0396.

Worth naming, because it is the same shape twice in one run: both defects exist
because a documented gate has no automation behind it. The registered
`dif_assets_contract` exercises `validate_profile` against synthetic trees and
never touches a real graph, so neither the renamed binary nor the leaked app source
could surface until someone typed the command.
