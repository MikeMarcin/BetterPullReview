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
