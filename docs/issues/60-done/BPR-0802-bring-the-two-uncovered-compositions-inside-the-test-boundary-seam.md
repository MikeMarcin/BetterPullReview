---
schema: "implement-rfc.card.v1"
id: "BPR-0802"
title: "Bring the two compositions the test-boundary audit cannot cover inside the seam"
status: "Done"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"BPR-0797 named both gaps in its own AC4 evidence and its frontier rather than glossing them; d13 asked for one card between them; allocated on main in d14","acceptance":["BPR-0797 AC4 follow-up"]}
acceptance: [{"criterion":"AC1 gh_cli_git_smart_transport is a named enumerated exception asserting it links no bpr::app symbol","outcome":"pass","revision":"49d7df695ff7115dd1020b3dce330ec00a657c48","evidence":{"kind":"artifact","detail":"macOS half section: MACOS_UNSANDBOXED_TESTS entry with nm -C zero bpr::app symbol check and negative controls (build/r2-logs/dsp2-0802-*); Windows manifest dropped the raw add_test finding (docs/verification/BPR-0802-windows-confirmation.md); Linux manifest OK:configured=362."}},{"criterion":"AC2 in-process composition answers the boundary question where the composition is decided","outcome":"pass","revision":"49d7df695ff7115dd1020b3dce330ec00a657c48","evidence":{"kind":"command","detail":"github_application_liveness_tests asserts inside/recording browser/no CLI read; reader-removed mutation fails (dsp2-0802-r3-mutation-reader-removed.log); Linux prints github-application-boundary-tests:pass. Windows startup of that course is blocked by BPR-0832, unrelated to the reader."}},{"criterion":"AC3 test_network_isolation_manifest passes on the two findings this card owns","outcome":"pass","revision":"49d7df69 macOS; 711d3ddc Windows; 780d0a1c Linux","evidence":{"kind":"command","detail":"macOS TEST_NETWORK_ISOLATION_OK (dsp2-int5, dsp3-int1-ctest.log); Linux OK:configured=362; Windows 24 findings, exactly the previous 25 minus this card's, the rest owned by BPR-0812."}},{"criterion":"AC4 no test gains the ability to open a browser or spawn gh","outcome":"pass","revision":"49d7df695ff7115dd1020b3dce330ec00a657c48","evidence":{"kind":"observation","detail":"Marker, scrubs, sandbox profile and loopback denial unchanged; Windows ENVIRONMENT_MODIFICATION identical for all 354 rows before and after."}}]
---

# BPR-0802: Bring the two compositions the test-boundary audit cannot cover inside the seam

## Outcome

[[BPR-0797]] moved the automated-test boundary from a build-time define to a
run-time fact: `BPR_AUTOMATED_TEST_BOUNDARY=set:1` is injected by
`bpr_add_network_isolated_test`, the one registration seam every isolated test on
all three platforms goes through, and `verify_test_network_isolation.py` reads the
marker back out of CTest's own manifest for every configured row.

That argument covers the application binary and every row that goes through the
seam. It does not cover two places, and BPR-0797's AC4 evidence says so in its own
words rather than claiming a completeness it does not have:

1. **`gh_cli_git_smart_transport` escapes the seam.** It is a raw `add_test` at
   `tests/github_contract/CMakeLists.txt:1200`. `test_network_isolation_manifest`
   reports it by name today, with four separate findings: it lacks the
   `BPR_AUTOMATED_TEST_BOUNDARY=set:1` marker, it lacks the twenty pre-boundary
   environment scrubs, it lacks the `bpr_network_isolated` label, and its command
   does not start with `scripts/run_network_isolated_test.py` under
   `sandbox-exec`. It is harmless **today** for the boundary question, because it
   carries zero `bpr::app::` symbols and so cannot compose the application — but
   "harmless because of what it does not link" is a property nothing enforces.
2. **The marker is read in exactly one place.** `src/app/main.cpp` is the only
   reader, so `tests/end_to_end/github_application_liveness_tests.cpp:55`, which
   composes `github_application` in process with no fixture, is outside the
   boundary whatever its row's environment says. It presses nothing today. The
   seam argument therefore covers the application binary, not every composition
   root that can reach the application layer.

## Acceptance

- `gh_cli_git_smart_transport` is registered through
  `bpr_add_network_isolated_test`, or the reason it cannot be is recorded in the
  gate itself as a named, enumerated exception rather than as a finding the gate
  reports on every run. If it becomes an exception, the gate asserts the property
  that makes it safe — that it links no `bpr::app::` symbol — instead of trusting
  it.
- A composition root that reaches `github_application` in process answers the
  boundary question the same way `src/app/main.cpp` does, so
  `github_application_liveness_tests.cpp:55` cannot compose the real browser
  opener or the real GitHub CLI factory whatever its row says. Prefer moving the
  reader to where the composition is decided over adding a second reader that can
  drift from the first.
- `test_network_isolation_manifest` passes on the two findings this card owns. It
  is red on more than that today; see the note below, and do not claim this card
  fixed what it did not.
- No test gains the ability to open a browser or spawn `gh` on anyone's machine.
  That is the rule [[BPR-0797]] AC4 preserved and this card must not trade away
  while tidying the registration.

## Evidence

Evidence is **not run** at card authoring: nothing here has been implemented.

What a successor will actually see, so the red gate is not mistaken for this
card's whole scope. `test_network_isolation_manifest` at `e2095a3e` reports six
findings, and only the two `gh_cli_git_smart_transport` ones belong to this card:

- `gh_cli_git_smart_transport lacks the automated-test boundary marker` and its
  three companions — **this card**.
- `raw add_test ownership is [...]` and `designated live source registration must
  be exactly one empty fixed call: calls=2` — both lists contain
  `.claude/worktrees/bpr-dif-integration-rfc-3c2904/…` as well as the real path.
- `WINDOWS_UNCONTAINED appears 190 times in the sources, expected between one and
  105, the size of the pinned set`.

The last two are the gate scanning a second checkout that happens to sit inside
this one. That is an environment artifact of `.claude/worktrees`, not a defect in
the tracked sources, and it is the same checkout that stops `architecture_boundaries`.
Whether the gate should skip nested checkouts is a separate question from this
card; decide it deliberately rather than by widening a pinned set until the count
fits.

Where the existing evidence lives:

- [[BPR-0797]] AC4's evidence names both gaps, and its "Unresolved obligations"
  2 and 3 state them as follow-up work rather than acceptance.
- The gate: `scripts/verify_test_network_isolation.py`, whose 46 positive
  controls pass, six of them naming the boundary.
- The seam: `bpr_add_network_isolated_test` in `cmake/BprTestIsolation.cmake`,
  which uses `ENVIRONMENT_MODIFICATION` rather than `ENVIRONMENT` because a row's
  own `set_tests_properties(... ENVIRONMENT ...)` replaces the property after the
  seam returns.

## Boundaries

No GitHub or GHES mutation, no live device authorization and no credential access
is authorized by this card. Do not weaken the marker, the scrubs, the sandbox
profile or the loopback denial to make a row register more easily; a row that
cannot meet the seam's terms is a row whose terms need stating, not a seam that
needs loosening. Do not edit
`.claude/worktrees/bpr-dif-integration-rfc-3c2904`: it belongs to another
checkout.

## macOS half, 2026-09-17 (dsp-002)

Implemented by an `rfc-worker` slice and integrated on main as `49d7df695ff7115dd1020b3dce330ec00a657c48`.
Slice logs: `build/r2-logs/dsp2-0802-*`. Integrated run:
`build/r2-logs/dsp2-int5-ctest.log`.

**Criterion 1: a named, enumerated exception.** Under the seam's runner,
`gh_cli_git_smart_transport` fails at "fixture sandbox version launch".
The adapter wraps every `gh` launch in `/usr/bin/sandbox-exec`, and macOS
refuses nested sandboxes (`dsp2-0802-nesting-probe.log`). So the row is the
one `MACOS_UNSANDBOXED_TESTS` entry in
`scripts/verify_test_network_isolation.py`. On macOS the gate requires:

- the exact raw `add_test` (`NAME … COMMAND <target>`) in
  `tests/github_contract/CMakeLists.txt`, and no other raw registration;
- a direct executable with no wrapper or argument;
- exactly the `bpr_local_only_fixture` label;
- on every run, zero `bpr::app::` symbols in the built binary, found with
  `nm -C`. The check fails closed when the table cannot be read or holds no
  `bpr::` symbol;
- a stale-pin failure if the row ever appears under the runner.

The row still gets the seam's boundary marker and its 20 pre-boundary scrubs,
now from one function, `bpr_apply_automated_test_environment`, which the
seam itself also calls. Every other row's configured environment is
byte-identical (`dsp2-0802-r3-manifest-compare.log`).

Gate negative controls on the real build: a pin pointed at the liveness
binary is reported as linking 11215 `bpr::app::` symbols; an unlisted copy
of the row and an extra raw `add_test` are both reported. The structure
contract adds 12 controls; it passes 58 tests.

**Criterion 2: one reader, in the composition.** `github_profile_runtime`
now decides `InsideAutomatedTestBoundary` from the marker (or a selected
fixture), where the browser and the CLI factory are chosen. `main.cpp` and
`github_application_config` no longer carry it; `github_application_status`
reports it. `github_application_liveness_tests` sets the marker in
`main()` and asserts an in-process composition with no fixture is inside,
binds the recording browser and offers no CLI read. With the reader removed,
that check and the new single-reader contract test both fail
(`dsp2-0802-r3-mutation-reader-removed.log`). The Release byte scan now
forbids the marker's name. The outside direction is covered only by the
designated `application_composition_capabilities_product_run` row, which
the gate can see, not by an in-process test.

**Criterion 3.** `test_network_isolation_manifest` passes on macOS
(`TEST_NETWORK_ISOLATION_OK:configured=374`).

**Criterion 4.** No test gained the ability to open a browser or spawn `gh`.
The marker, scrubs, sandbox profile and loopback denial are unchanged.

**Integrated run at `49d7df695ff7115dd1020b3dce330ec00a657c48`.** These pass:

- `architecture_boundaries`;
- both isolation gates;
- `gh_cli_git_smart_transport`;
- `application_composition_contract` (167.80 s);
- `application_review_round_trip_composition_contract`, with this finish
  commit's reseal;
- the panel course and its twin;
- `application_device_authorization_browser_handoff`;
- `qt_composed_github_scale_100`;
- `composed_application_hosts`.

`application_poller_liveness` prints `github-application-boundary-tests:pass`,
then fails on the tick subtest, which was already red at HEAD before this
change: [[BPR-0827]].

**Round-trip pins resealed in this card's commit** for the reviewed range
`99712ec3..49d7df695ff7115dd1020b3dce330ec00a657c48`. It covers two changes. The first is [[BPR-0622]]'s
`c94e02b1`, which enables the workspace registry and recovery for
`Q_OS_WIN` and appends the neighbourhood leaf natively; the dsp-002 critical
review found no blocker. The second is this card's `49d7df695ff7115dd1020b3dce330ec00a657c48`, reviewed by the
integrator above. New values:

- whole file `9e5a612127cfcec0e4cc47bb0549195e2118c2ea79e39cbdc1e4c04194813736`;
- manifest `cabf96c66cc646cc24767f6cebb898a2b6452fcb4ac03a40414a35a3fb71402f`;
- mutation branch unchanged.

**Still open: Windows and Linux confirmation.** The gate and the seam
function are portable. Windows should report 24 findings instead of 25, since
the raw `add_test` ownership finding is gone, with no other change. Linux
should pass its manifest and structure contract. Both go with the next station
envelopes. The `application_composition_capabilities_*` rows stop at
`PROFILE_ALREADY_OPEN` while a person's app holds the default profile:
[[BPR-0820]].

## Windows and Linux confirmation; finished (dsp-003)

- **Windows** (`bpr-win`, `task-20260917-win-hardening` step 1, merged in
  main `902cda35`; [the Windows confirmation](../../verification/BPR-0802-windows-confirmation.md)).
  The manifest reports 24 findings, the previous 25 minus the raw `add_test`
  ownership finding, as predicted; one finding's wording now names its command.
  The structure contract passes (58 tests, 10 skipped).
  `ENVIRONMENT_MODIFICATION` is identical before and after for all 354 rows,
  and `application_device_authorization_browser_handoff` passes.
  `application_poller_liveness` stops at startup on Windows before its boundary
  line, on an owner-private refusal of its data root. That is [[BPR-0832]],
  not this card's reader. `application_composition_capabilities_inside_boundary`
  fails only contained there: [[BPR-0833]].
- **Linux** (`bpr-linux`, `task-20260917-lnx-confirm`, merged in main
  `48b6422b`; "BPR-0802" in
  [the dsp-002 Linux confirmation](../../verification/dsp2-linux-confirmation.md)).
  5/5 pass. `test_network_isolation_manifest` prints `OK:configured=362` with
  the raw `add_test` finding gone, and `application_poller_liveness` prints
  `github-application-boundary-tests:pass` before its tick subtest passes.
- **macOS at main `8a03a5be`** after both merges: `test_network_isolation_manifest`,
  `architecture_boundaries` and the round-trip contract pass
  (`build/r2-logs/dsp3-int1-ctest.log`).
