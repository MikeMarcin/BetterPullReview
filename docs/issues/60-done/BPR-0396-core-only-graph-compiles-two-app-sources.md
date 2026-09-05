---
schema: "implement-rfc.card.v1"
id: "BPR-0396"
title: "The core-only graph compiles two app sources, and its boundary check says so"
status: "Done"
rfcs: ["RFC-BPR-0019"]
depends_on: []
---

# The core-only graph compiles two app sources, and its boundary check says so

The documented Dif asset boundary invocation fails on a **freshly configured and
built** `core-only` graph:

```
DIF_ASSET_BOUNDARIES_ERROR: core-only selected Qt/app source: src/app/src/designated_live_macos_capsule_identity.cpp
```

`scripts/verify_dif_asset_boundaries.py:305-312` forbids any first-party source
under `src/adapters/qt/` or `src/app/` in the `core-only` compile database. Two
appear:

- `src/app/src/designated_live_macos_capsule_identity.cpp`
- `src/app/src/designated_live_macos_provenance.cpp`

## Where they come from

`tests/CMakeLists.txt:488-492`, added by `5212885` ([[BPR-0354]]):

```cmake
bpr_add_executable(
    test_better_pull_review_macos_capsule_identity_tests
    platform/macos_capsule_identity_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_capsule_identity.cpp"
)
```

A platform unit test compiles the app source it tests directly, rather than
linking a library. `macos_supervisor_provenance_tests` does the same with
`designated_live_macos_provenance.cpp`. Both are `APPLE`-guarded and both land in
`core-only` because that graph still builds tests — `BUILD_TESTING` is `ON` there;
what is `OFF` is `BPR_BUILD_QT`.

## Why nobody noticed

The registered `dif_assets_contract` exercises `validate_profile` against
**synthetic** build trees built by `make_selected_build`, which writes a
compile-commands file naming two or three hand-picked sources. It never sees the
real `core-only` graph. So the only thing that runs this rule against real output
is the manual invocation in `docs/development/build-and-test.md`, and nothing runs
that.

That is the same shape as [[BPR-0395]], found in the same run: a documented gate
that no automation executes, and therefore a rule that drifted from the tree
without anyone learning.

## The decision this card carries

Two readings, and they lead to different fixes:

1. **The graph is wrong.** `core-only` is meant to be the Qt-free, app-free tier,
   and a test target reaching into `src/app/src` breaks that regardless of what it
   is testing. The fix is to guard those two test targets out of `core-only`, or to
   give the sources a home the tier is allowed to consume.
2. **The rule is over-broad.** The boundary's purpose is that the *product* graph
   stays free of Qt and app code; a platform unit test compiling one app
   translation unit does not put that code in any product artefact. The fix is to
   scope the check to non-test translation units, and to say so.

Reading 2 is tempting because it is less work, and that is exactly why it should
not be chosen on those grounds. `core-only`'s value is that it proves the lower
tiers stand alone; the test tier's reach is part of what that proves.

## Required final state

- A clean `core-only` graph passes the documented invocation, or the rule states
  precisely which translation units it governs and why the excluded ones cannot
  reach a product artefact.
- Whichever reading is taken, `dif_assets_contract` gains coverage that would fail
  if the chosen boundary were violated again — against real graph contents, not a
  synthetic compile-commands file.
- The other two graphs' arms of the same invocation are unaffected.

## Not in scope

[[BPR-0395]]'s application-discovery repair, and moving or renaming the capsule
sources for any reason other than this boundary.

## Verification

```sh
cmake --preset core-only && cmake --build --preset core-only
python3 scripts/verify_dif_asset_boundaries.py \
  --source . \
  --dev-build build/dev \
  --core-build build/core-only \
  --app-build build/app-release
```

All three graphs must be freshly configured and built. A reused tree can hold
output from a different configuration and prove nothing either way.


## Result — 2026-08-19: reading 1, and why

The two targets are scoped to `BPR_BUILD_QT`, so `core-only` no longer compiles any
`src/app/` source. Reading 1, and the card's own warning is the reason: reading 2 is
tempting because it is less work, and weakening a boundary to make a gate pass is
the failure this repository has a whole class of cards about.

`BPR_BUILD_QT` rather than a new switch, because it already means "this graph
contains the application". The designated-live capsule these two exercise exists
only in a Qt application graph, so scoping them there costs no coverage — they run
in `dev`, which is where the capsule they test can exist.

### Evidence

| Check | Result |
|---|---|
| clean `core-only`: app/qt sources in `compile_commands.json` | 2 before, **0** after |
| `ctest --preset core-only -N` registrations for the two tests | 0 |
| `macos_capsule_identity`, `macos_supervisor_provenance`, `macos_service_slot_handshake` under `dev` | 3/3 passed |
| the documented three-graph invocation, all graphs freshly built | **`DIF_ASSET_BOUNDARIES_OK`** |

That last row is the first time the documented command has passed end to end in
this session, and it took repairing both this and [[BPR-0395]] to get there.

### The required-final-state bullet not taken

The card asks for coverage that would fail if the chosen boundary were violated
again, "against real graph contents, not a synthetic compile-commands file". That is
**not** delivered here and should not be claimed. `dif_assets_contract` builds
synthetic trees precisely so it need not configure a real graph, and making it
configure and build a real `core-only` would turn a 26-second contract into a
multi-minute one.

The honest position: the rule exists and is correct, the graph now satisfies it, and
the only thing that runs it against real output is still a documented manual
command. Automating that is the same gap [[BPR-0395]] names, it applies to every arm
of this verifier rather than to this rule alone, and it wants its own card and its
own decision about where a real-graph gate belongs in the matrix.
