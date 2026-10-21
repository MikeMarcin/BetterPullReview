---
schema: "implement-rfc.card.v1"
id: "BPR-1694"
title: "Prepare and qualify the coordinated v0.3.2 release"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"release-v032-owner-request-20261021","baseline":"111235987772e808f4fae26383925759985b3041","execution_strategy":"direct","milestone_plan":{"id":"release-v032","path":"docs/rfcs/plans/release-v032/milestone-plan.json"},"external_state":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032"}
---

# Prepare and qualify v0.3.2

The owner requested `$publish-release v0.3.2 if there are fixes to be made make us an implementer`.
This card owns source preparation, reproduced blocker repairs and exact-candidate
qualification for the existing full macOS universal, Windows x64 and Ubuntu
26.04 WSL producer route. Broad changes since v0.3.1 require comprehensive
native Release and Debug. Release preparation supplies no inferred legal,
security, native installation or trust acceptance.

## Acceptance

- AC1: Prepare grounded user-facing notes and version 0.3.2; commit one clean common candidate and preserve unrelated work.
- AC2: Reconcile native jobs before dispatch; qualify the exact committed candidate with comprehensive native Release and Debug, retaining failures and configured skips. Repair reproduced blockers in explicit committed slices without weakening gates.
- AC3: Prepare and audit complete binary, corresponding-source, SPDX, checksum, provenance and native-signature sets through the existing release tools and approved producer configuration. Keep required human/security/legal decisions explicit and use short attended review or wizard handoffs when needed.
- AC4: Publish only a qualified, authorized exact candidate through the existing publisher; verify remote state and downloaded bytes/native trust. Preserve unfinished obligations and external continuation state if prerequisites prevent publication.

## Initial reconciliation

Clean main at the baseline above. CMake still selects 0.3.1 and no v0.3.2
notes exist. Early composition inventory and documentation checks passed in
the assessing chat; neither is native/artifact qualification. BPR-1653 is Done
under its separately approved 240 MiB RSS and 216 MiB live-heap criteria;
v0.3.1's exception is not extended. BPR-1633's foreground native-menu course,
BPR-1371's resource investigation and older BPR-1551/BPR-1404 qualification
frontiers require current evidence reconciliation, not an assumption of a
current product failure. External phase ledger preserves request timing,
producer observations and subsequent results.

## First rejected candidate and repair

Candidate `b15be685a51d01053cf48a1d550bff0da2c7baa8` fails the macOS
Debug build: `gh_cli_transport_tests.cpp` omits the new
`list_home_pull_requests` operation from its exhaustive semantic-request
helper. Warnings-as-errors stop compilation. The same exact-invocation test
also needs the operation's repository authority parent rather than a
pull-request parent. The repair supplies the two requested PR numbers and
repository parent through the existing all-operation course; no product
policy, warning, assertion or limit changes. Raw failure is retained externally.
The remote candidate rejection sentinel blocks all unstarted downstream
stages while its already-running Release build retains its own result.

Target-only milestone discovery initially encountered an invalid unrelated
legacy plan (missing boundary fields). This bounded release plan links the
card directly, preserving the complete owner mandate and all four criteria;
no historical plan is rewritten.

The repaired Debug CLI target builds and `gh_cli_transport` passes in 0.70 s.
The existing closed-invocation catalog now exercises the batched Home read
with both server profiles, exact child argv/environment/body/bounds, and the
validated repository parent. Focused clang-format 22 and clang-tidy 22
warnings-as-errors pass, as do the documentation and whitespace gates. This
fix changes only the contract fixture. Full native gates remain pending.

## Windows fixture portability blocker

The original candidate's native Windows Release build completed with exit 1
in 1004.75 s. Its sole collected compiler error is C4996 at
`commit_filter_view_tests.cpp:355`: `std::getenv` is deprecated under MSVC's
warnings-as-errors policy, which the test instructions explicitly prohibit.
No Windows suite ran. The macOS producer for `ef5939561` was then stopped by
its exact owned process IDs during Release compilation, before tests, signing
or packaging. Its interruption and child identities remain in external evidence;
neither candidate is qualified.

The fixture now uses Qt's environment and path APIs for the optional generated
row render directory, preserving unset/set handling and Unicode paths. No
product code, warning policy, test assertion or budget changes. Both Mac
100%/200% commit-filter view courses pass and write their requested PNGs into
a temporary Unicode directory; the directory is removed after observation.

## Native Home fixture lifetime failure

Candidate `6bc681a005b1b894242a0d303205597f7a965266` builds successfully
in Windows Release, then its eight affected public courses return seven
passes and one failure: Home's open-feedback course loses its expected queue
selection when returning from Recently viewed. Unchanged native observations
also crash. The simulated #4001 network error printed by the course is expected.
A passing instrumented run and a passing debugger run are retained as diagnostic
observations, not accepted retries.

A process-local exception trace preserves the failing original gesture sequence
and captures access violation `c0000005` in `QSortFilterProxyModel::parent`,
through `QListView::rectForIndex` and `QListView::visualRect`. The fixture retains
plain proxy indexes across mouse helpers that process events and across renders.
Its later projection adoption can replace the proxy mapping. Temporary diagnostic
sources and binaries are restored after every native observation.

The repair retains only row identity across those boundaries: mouse helpers
resolve the target immediately before dispatch and release the index before
processing events; colour sampling resolves the row after grabbing the image;
selection checks reacquire current indexes. All original gestures, four themes,
selection/colour/accessibility assertions, network waits, deadlines and Home
preemption checks remain. No product code changes.

## Historical snapshot contamination

The Mac comprehensive Release attempt also fails two network-isolation source
checks because ignored historical scan snapshots under `.mask/sessions` are
counted as extra CMake registrations. After stopping only the owned producer
process tree, both snapshots are moved outside the checkout, every file's size
and SHA-256 is verified, and their original paths remain directory symlinks for
historical receipts. The checker is unchanged and does not follow directory
links. Both focused source checks pass in 1.58 s. The failing/incomplete suite
is retained; final comprehensive qualification is still required.

The repaired exact fixture passes the full native Windows Release course in
64.26 s and Mac Release in 64.24 s. Focused clang-format 22 and clang-tidy 22
warnings-as-errors pass; documentation and whitespace checks pass. The native
engineering observation is built with only the repaired fixture substituted;
its original source and binary are restored afterward. Final committed-source
comprehensive gates remain due.

## Expanded candidate and pack regeneration

The owner explicitly included the features that landed before publication. The
isolated release branch now includes RFC-BPR-0089 M3 and BPR-1693; the release
notes describe split per-commit diffs, force-push rematch offers and opening a
set-aside generated file. The original development checkout remains untouched.

Windows Git encodes the regenerated fixture packs differently from the committed
packs. The contract now imports each with Git's strict pack validator and
compares the complete typed object contents in object-ID order. Every other
fixture file remains byte-exact. All three variants, including M3's split
fixture, pass on Mac and native Windows Release. Controls reject a truncated
pack and a valid pack containing different objects. This follows the existing
recorded-pack builder's cross-Git-version contract; no fixture data or product
assertion is weakened. Expanded-source comprehensive qualification remains due.
