---
schema: "implement-rfc.card.v1"
id: "BPR-0802"
title: "Bring the two compositions the test-boundary audit cannot cover inside the seam"
status: "Ready"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"BPR-0797 named both gaps in its own AC4 evidence and its frontier rather than glossing them; d13 asked for one card between them; allocated on main in d14","acceptance":["BPR-0797 AC4 follow-up"]}
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
