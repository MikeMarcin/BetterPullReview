---
id: BPR-0395
title: The asset boundary verifier cannot find the renamed release binary
status: Ready
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
