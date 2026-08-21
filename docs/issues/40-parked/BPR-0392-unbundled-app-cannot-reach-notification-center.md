---
id: BPR-0392
title: The unbundled test build cannot register with Notification Center
status: Parked
rfc: RFC-BPR-0010
parked_on: awaiting-owner-verdict
depends_on: []
---

# The unbundled test build cannot register with Notification Center

Found by the owner attempting the v002 notification packet. The course armed
correctly — `NATIVE_NOTIFICATION_QUIET_BASELINE_OK`,
`NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK`, the exact payload, then
`NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK` — and no banner ever appeared.
The owner checked System Settings → Notifications directly: **no
BetterPullReview entry exists at all**, and the appearance run's status dump
recorded `NOTIFICATION_PERMISSION:not-determined`.

The mechanism is structural. `build/dev/src/app/better_pull_review` is a
bare Mach-O with no bundle and no `Info.plist`, and macOS Notification
Center registers applications by bundle identity: an unbundled process
cannot request authorization, never gains a Settings entry, and its
delivery requests go nowhere. BPR-0328 gave the *tests-disabled* build its
bundle; the tests-enabled build that every attended packet binds never got
one. The deterministic preflight
(`application_native_notification_attended_contract`) passes because it
deliberately substitutes native display — the one fact it cannot check is
the one that was false. BPR-0168's own refrain applies a third time:
attendance was never what was missing; a banner no process identity can
produce cannot be clicked.

## Required final state

- The attended notification course runs from an executable with real bundle
  identity, so first launch triggers the macOS authorization prompt, the
  app appears under System Settings → Notifications, and a granted request
  produces a real banner.
- Whether that is the tests-enabled app becoming a `MACOSX_BUNDLE` (mirroring
  BPR-0328's tests-disabled shape, keeping documented paths stable via
  `OUTPUT_NAME` or updated packets) or a narrower attended-course bundling
  is decided by the slice from the build's own constraints, not assumed.
- The deterministic preflight gains the check that was missing: on macOS it
  fails when the running process has no bundle identity, so an unbundled
  build can never again present an attended banner packet.
- The BPR-0168 packet is regenerated against repaired bytes.

## Verification

```sh
ctest --preset dev -R '^(application_native_notification_attended_contract|application_attended_launch_contract)$' --output-on-failure
```

## The tests-enabled build is a bundle — 2026-08-19

### The decision, from the build's own constraints

`MACOSX_BUNDLE` for the tests-enabled macOS graph, `OUTPUT_NAME` left alone.
`src/app/CMakeLists.txt`'s `elseif(APPLE AND NOT BUILD_TESTING)` is now
`elseif(APPLE)` with an inner split on identity only; BPR-0328's tests-disabled
properties are byte-for-byte unchanged.

Four facts decided it, none of them a preference:

1. The narrower alternative has no home. The only post-build bundling precedent
   is the designated live macOS capsule, which raises `FATAL_ERROR` under
   `BUILD_TESTING` by design. A second bundled target would duplicate the whole
   composition graph.
2. Nothing registered had to change. All ~30 end-to-end registrations resolve the
   application through `$<TARGET_FILE:app_better_pull_review>`, which for a
   bundle target names the inner executable — the same distinction
   `$<TARGET_BUNDLE_DIR:...>` already relies on for the capsule.
3. `.vscode/launch.json:28` already named
   `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`
   for the dev macOS configuration, and was broken. This exact shape was
   anticipated; the build had simply never caught up.
4. `scripts/verify_dif_asset_boundaries.py` rglobs for the literal name
   `better_pull_review` and requires exactly one hit, so keeping `OUTPUT_NAME`
   keeps it working *for this graph* — and a stale flat Mach-O left in the build
   tree would have made it two. It does not find the tests-disabled graph's
   binary at all; see the measured note below and [[BPR-0395]].

The identities are deliberately different:

| Graph | `OUTPUT_NAME` | `CFBundleName` | `CFBundleIdentifier` |
|---|---|---|---|
| tests-enabled | `better_pull_review` | `BetterPullReview (tests)` | `io.github.MikeMarcin.BetterPullReview.tests` |
| tests-disabled | `BetterPullReview` | `BetterPullReview` | `io.github.MikeMarcin.BetterPullReview` |

Notification Center keys authorization by identifier. Sharing one would let a
tests-enabled build grant or consume the shipped application's permission, which
is a different defect wearing the same green marker.

### The check the preflight was missing, in both places

`notification_bundle_identity()` in `src/app/main.cpp` reads
`CFBundleGetIdentifier(CFBundleGetMainBundle())` — pure C CoreFoundation, already
included and already linked for this configuration, so no Objective-C++ entered
the tree. Note that `CFBundleGetMainBundle()` is non-null for an unbundled
executable too: CoreFoundation synthesizes a main bundle from the executable's
directory. The identifier is the fact that distinguishes them.

The notification course now answers it **before it arms anything**, prints
`NATIVE_NOTIFICATION_BUNDLE_IDENTITY:<identifier>`, and refuses with
`NATIVE_NOTIFICATION_REFUSED:bundle_identity_absent` when there is none. Refusal
rather than report, because a preflight that passes is claiming a banner is
possible.

`application_native_notification_attended_contract` adds the second gate: the
identifier must equal the exact expected one, which the course cannot check
without hard-coding its own answer. The status dump also reports
`NOTIFICATION_BUNDLE_IDENTITY:` beside `NOTIFICATION_PERMISSION:`, which is where
this defect was originally visible as a plausible `not-determined`.

### A wrong assumption, corrected rather than papered over

The first attempt put the marker only in the status dump. The notification course
never reaches it — `--native-notification-probe` runs its own tick loop and exits
through `QCoreApplication::exit` from its shutdown stage — so the contract had
nothing to read and failed. The card's own `NOTIFICATION_PERMISSION:not-determined`
evidence is attributed to the *appearance* run's status dump, which is consistent.
The marker now lives in the course.

### Evidence

| Check | Result |
|---|---|
| `cmake --preset dev` / `cmake --build --preset dev` | succeeded; links `src/app/better_pull_review.app/Contents/MacOS/better_pull_review` |
| `PlistBuddy` on the produced `Info.plist` | `CFBundleIdentifier=io.github.MikeMarcin.BetterPullReview.tests`, `CFBundleName=BetterPullReview (tests)`, `CFBundleExecutable=better_pull_review` |
| direct run of the bundled executable | `NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests` |
| `ctest -R '^(application_native_notification_attended_contract\|application_attended_launch_contract\|application_diff_workbench_public_path)$'` | 3/3 passed |
| identity forced to `none`, course re-run | `NATIVE_NOTIFICATION_REFUSED:bundle_identity_absent`, contract fails |
| identifier seeded to the shipped one, contract re-run | fails: `'io.github.MikeMarcin.BetterPullReview' rather than the expected` |
| both seeds reverted, re-run | 3/3 passed |
| `clang-format --dry-run --Werror src/app/main.cpp` | clean |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Documentation: the instructional flat paths in
`docs/development/build-and-test.md`, `docs/security/foundation-security.md`, and
`README.md` now name the bundled executable, with a subsection explaining the
mechanism, the two identities, why no registered test changed, and the
platform difference. Historical records under `docs/verification/` are
deliberately untouched: they record what was run at a past commit.

Three CMake `uninitialized variable` warnings for `MACOSX_BUNDLE_INFO_STRING`,
`MACOSX_BUNDLE_LONG_VERSION_STRING`, and `MACOSX_BUNDLE_COPYRIGHT` now appear on
the `dev` configure. They come from the shared `packaging/macos/Info.plist.in`
and the tests-disabled configure already emitted them; `uninitialized` is not in
the preset's `errors` list.

### The full matrix, and the one pin it moved

`ctest --preset dev` ran the complete 191-test matrix at this slice: **187
passed, 4 failed**, and all four are accounted for:

| Failure | Cause |
|---|---|
| `dependency_modernization_contract` | pre-existing at `HEAD`, [[BPR-0373]]'s missing `tempfile` import |
| `test_network_isolation_loopback` | pre-existing at `HEAD`, [[BPR-0394]]'s wrong keyword |
| `test_network_isolation_unrelated` | same |
| `application_composition_contract` | **this slice.** `main.cpp` changed, so the pinned production-app source digest drifted |

The first three were red before this slice touched anything and are fixed by
their own cards. The fourth is this slice's, and re-pinning it is the established
maintenance step: `bpr_verify_app_source_tree`
(`tests/end_to_end/application_review_round_trip_composition_contract.cmake:139`)
hashes eight named app sources and pins the manifest digest, so any edit to
`main.cpp` moves it.

The new value was computed independently rather than copied out of the failure
message, replicating the gate's own algorithm — CRLF and CR normalized to LF,
per-file SHA-256, then SHA-256 over the `path\ndigest\n` manifest — and it agreed
exactly: `14a14a2a5a402179237a5ad185307178da7b5ac6ea2ec92c0fd4e7bff28ef523`. The
pin is not weakened: `composition_contract_tests.py:690-709` prepends and appends
a hostile fragment to each pinned source and requires the drift diagnostic, so
the gate mutation-checks itself and still passes.

After all four repairs, a focused replay of exactly those four plus the new
`contained_temporary_usage_contract` passes 5/5. Every end-to-end test that
consumes the application binary — tests 155 through 191, including all three
public paths, the workbench, ledger, round-trip, defect, agent-invocation,
epoch-persistence, appearance, and attended courses — passed with the bundle in
place, which is the evidence that `$<TARGET_FILE:...>` absorbed the path change.

### Two things a review said were unclaimed, now measured

An independent review flagged two shifts the card asserted nothing about. Both were
checked directly rather than reasoned about.

**The application data root does not move.** The concern was that Qt resolves
`AppLocalDataLocation` from the bundle's `CFBundleName` on macOS, which would move
the tests-enabled default from `…/BetterPullReview` to `…/BetterPullReview (tests)`
and hide any pre-existing state. Probed with a minimal Qt 6.10.3 program run twice —
once flat, once inside a bundle carrying exactly this configuration's
`CFBundleName` and identifier:

```
flat executable    : /Users/jaeger/Library/Application Support/BetterPullReview
bundled executable : /Users/jaeger/Library/Application Support/BetterPullReview
```

Identical. Qt uses `QCoreApplication::applicationName()`, which `main.cpp` sets
explicitly to `BetterPullReview`, so `CFBundleName` does not participate. Nothing
moved, and no state became invisible.

**`verify_dif_asset_boundaries.py` finds this graph's binary but not the other
one.** The card's fourth reason said it "still finds it by name". True here, and
false for the tests-disabled graph, whose executable BPR-0328 renamed to
`BetterPullReview` while the verifier rglobs for `better_pull_review` only. On a
clean `app-release` tree that resolves to zero candidates.

It looks healthy on this host solely because a stale pre-rename
`better_pull_review.app` sits beside the current `BetterPullReview.app`, giving the
lowercase rglob exactly one hit. That is a pre-existing gap from BPR-0328 rather
than anything bundling introduced, and it is now [[BPR-0395]]. The build-and-test
sentence has been narrowed to say which graph it is true of.

### What remains, and what this does not claim

The bundle is neither signed nor notarized, and `macdeployqt` does not run. So
this establishes that the process *has* an identity Notification Center can
register — not that a banner appears. That is a person looking at a screen, and
it is [[BPR-0168]]'s, whose packet needs regenerating against these bytes.


## Parked on the operator — 2026-08-19

Every unattended bullet is done and the packet is regenerated:
`docs/verification/reviews/native-notification-delivery-v003/operator-packet.md`,
bound to the bundled executable
`sha256:6ad6ad39b7c7b593b222c2c7c20ee567e78331f3034f73ce8d48a6f201bf8753` and to
BPR-0251's implementation record.

The packet says plainly what this slice does and does not establish. Bundle
identity is *necessary* for Notification Center registration; the bundle is
neither signed nor notarized, so whether macOS then displays a banner for an
unsigned local build is what only a person looking at the screen can answer. The
packet therefore names "no banner appeared and no Settings entry exists even now"
as a specific and useful **rejection** rather than a retry condition — it would
mean identity is necessary but not sufficient and signing is the next question,
which is a materially different card from this one.

The Settings entry will read `BetterPullReview (tests)`, which is deliberate and
is stated in the procedure so it does not read as a defect.

## Regenerated once more — v004, 2026-08-19

v003 was never run. [[BPR-0390]]'s blob-read repair landed after it was written
and rebuilt the executable, so v003's bound digest no longer matches anything on
disk. That is the same failure mode this card was opened by — a packet whose
procedure is unexecutable by construction — caught before an operator hit it
rather than after.

The live packet is
`docs/verification/reviews/native-notification-delivery-v004/operator-packet.md`,
bound to
`sha256:4e93884097bc92d4c56eb8f12712245ed4e0c2b173e1bed7a441cc8072e20c64` at
51,050,832 bytes and to the unchanged BPR-0251 implementation record. v003 is
preserved as history.

BPR-0390 touched the diff classification path and nothing this packet decides,
so the procedure, the markers, the deliberate `BetterPullReview (tests)` Settings
name, and the "no banner even now" rejection condition are all unchanged. The
packet states that up front.

The card stays parked on the operator.

## Rebound once more, and this time the bytes are settled — 2026-08-19

[[BPR-0397]] bounded three unbounded waits in the epoch host and rebuilt the
executable, so the previous packet's digest no longer matched anything on disk.
Third rebuild in a row to invalidate a pending packet; caught before an operator
hit it rather than after.

The live packet is
`docs/verification/reviews/native-notification-delivery-v005/operator-packet.md`,
bound to
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93` at
51,085,872 bytes.

Why this one should hold where three did not: every card that could change these
bytes is in `60-done/`, and nothing in an active lane is executable without a
Windows or Linux host, a product decision, or an owner verdict. The bytes are
settled because the board is.

The preflights are stronger than any earlier version's, too — the complete
193-test `dev` matrix passed at exactly these bytes rather than a focused
selector set.

The card stays parked on the operator.

## Operator direction — will grant when prompted — 2026-08-20

Recorded verbatim:

> I will grant if prompted

So the remaining work splits, and only the smaller half is the owner's. Getting
the application to the point where macOS presents the Notification Center
permission dialog is unattended: launching the bundle, reaching the composed
notification path, and requesting authorization. The grant itself, and then
seeing the banner and clicking it, is the owner's.

[[BPR-0168]] owns the attended pass that the grant enables — banner content
limited to a name and counts, and a click that opens the correct review.

This card is what made a real banner possible at all: an unbundled binary cannot
reach Notification Center, and the fix was structural rather than cosmetic. The
grant is therefore the first time the repaired path can be observed end to end.

## The banner was delivered, read, and activated — 2026-08-20

The attended course passed. Exit 0, all eight of the packet's required markers
present, log
`sha256:307ec1f34a307a8d24e07d6165015ee1d40c5e8c3b70e8059301c5c911fbe9d4`:

```
NATIVE_NOTIFICATION_BUNDLE_IDENTITY:io.github.MikeMarcin.BetterPullReview.tests
NATIVE_NOTIFICATION_QUIET_BASELINE_OK
NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK
NATIVE_NOTIFICATION_PAYLOAD:octo-org/review-repository #17|The code moved after you reviewed it.
NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK
NATIVE_NOTIFICATION_TRAY_SIGNAL_OK
NATIVE_NOTIFICATION_ACTIVATION_OK
NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK
NATIVE_NOTIFICATION_SHUTDOWN_OK
NATIVE_NOTIFICATION_ATTENDED_RUN_OK
```

This is the first time this product has caused a real macOS banner and had its
activation land on the intended review. That is precisely what this card
existed for: an unbundled binary cannot reach Notification Center, and the fix
was structural rather than cosmetic.

### The first attempt reached the banner and lost the click

Worth recording, because it is a property of the medium rather than a defect.
The first run reached `NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK` with the
correct payload and then sat there: the banner had auto-dismissed into
Notification Center before the click landed, and no activation arrived. The run
was relaunched and the second banner was clicked while still on screen.

An attended notification gate therefore has a real time window that no log line
announces, and a run that misses it is indistinguishable from one where delivery
failed until you look at which markers are absent. Relaunching costs nothing and
consumes no packet, which is the right answer, but the packet should say so.

## Latest operator response is not a banner observation — 2026-08-20

The owner replied:

> I didn't see any notice a notification banner but I wasnt looking.

The course evidence above remains valid, but this statement cannot satisfy the
shared packet's display-content decision because the owner explicitly was not
observing the banner. It is not treated as a delivery failure or as approval.
This card remains parked with [[BPR-0168]] until the banner is described as
actually seen.
