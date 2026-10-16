---
schema: "implement-rfc.card.v1"
id: "BPR-1561"
title: "Restore the native Linux CPack baseline fixture"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
platforms: ["linux"]
metadata: {"mandate_revision":"ubuntu-readiness-20261016-unversioned-r1","reservation":"docs/issues/_reservations/BPR-1561--BPR-1561--32d31ecb043c.json","milestone_plan":{"id":"bpr1561-linux-cpack-fixture","path":"docs/rfcs/plans/bpr1561-linux-cpack-fixture/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"53f391c1617fe6c5866537c960708b7deebaa9a0","evidence":{"kind":"observation","detail":"Exactly one fixture line declares better_pull_review_xdiff SHARED. Byte comparison verifies every existing assertion and production Install.cmake remain unchanged.","path":"docs/issues/_checkpoints/BPR-1561/verification.json"}},{"criterion":"AC2","outcome":"pass","revision":"53f391c1617fe6c5866537c960708b7deebaa9a0","evidence":{"kind":"command","detail":"Exact file ec369e426decff35b03f0f9da60521ebf2048f3dc786086d82fb618953ce3a96 in aa629+patch Ubuntu26.04 WSL snapshot passes all21 package runtime cases,0skips,0.858s through existing network sandbox. Unmodified original fixture fails at missing xdiff target. Relevant inputs byte-identical to main.","path":".mask/sessions/ubuntu-preparation-20261016/ubuntu-readiness-20261016-repair/receipt.json"}},{"criterion":"AC3","outcome":"pass","revision":"53f391c1617fe6c5866537c960708b7deebaa9a0","evidence":{"kind":"command","detail":"Mac package runtime20 checked/1native-only skip;67 documentation tests and DOCUMENTATION_OK pass. Source committed. Helper finishes issue/upkeep with explicit paths; task-owned commands stopped and coordinator confirms remote cleanup. Wider RFC0038/BPR0961/BPR0962 gates remain open.","path":"docs/issues/_checkpoints/BPR-1561/verification.json"}}]
---

# Restore the native Linux CPack baseline fixture

## Mandate and boundary

Continue Ubuntu release preparations, clarified by the owner as "Unversioned
readiness for the next coordinated release". This bounded regression repair is
mandate `ubuntu-readiness-20261016-unversioned-r1`, delegated to
`/root/ubuntu_readiness_fixture` on main at
`bd04bf3df2133ed699931014c28260e7799ce551`.

The coordinator owns all remote commands and raw readiness evidence. This leaf
owns the test fixture, card, small plan and durable checkpoint. Preserve concurrent
work and protected candidates. No version selection/bump, release build,
production package/install, signing/unlock/key enrollment, tag/push/publication
or desktop control. RFC-BPR-0038, BPR-0961, BPR-0962 and unrelated BPR-1404 keep
their existing acceptance and obligations.

## Acceptance

- AC1: Add the required `better_pull_review_xdiff` SHARED target to the tiny native CMake graph in `tests/scripts/package_runtime_tests.py`. Preserve production install guards and all existing baseline/default-generator assertions.
- AC2: Run the unchanged Linux CPack baseline assertion and full package runtime contract on the exact source bytes on Ubuntu 26.04 x86_64. Removing the target must reproduce the configure failure. Record actual skips and limits.
- AC3: Pass focused local verification and the documentation gate; record the exact candidate and native evidence, commit the explicit issue paths, and leave no task-owned command running.

## Initial observation

The coordinator's original producer is the clean
`aa6294c50f95b51d95f7c7861d7430f89cdb2fdc` at
`/home/mmarcin/bpr-v021`, WSL Ubuntu 26.04 x86_64, glibc 2.43. Its package
runtime contract fails because `packaging/Install.cmake` requires the xdiff
target in `set_target_properties` and `install(TARGETS ...)`, while this
configure-only fixture declares only the app and supervisor. Source-assets,
native-release and package-audit contracts passed separately. No product
build or installed-package claim follows from fixture success.

Semantic documentation impact: no-impact. The repair only restores the test
fixture's representation of the existing production graph; it changes no
package behavior, public interface, release policy or product composition.

## Verification and execution record

Source candidate `53f391c1617fe6c5866537c960708b7deebaa9a0` adds one
`add_library(better_pull_review_xdiff SHARED fixture.cpp)` line. Removing that
line reproduces the entire original file byte-for-byte; every baseline and CPack
assertion and the production install definition are unchanged.

The coordinator's exact-patch Ubuntu 26.04 WSL snapshot passes all **21/21**
package runtime tests, **no skips**, in **0.858 s**, through the existing native
network sandbox. The original unmodified producer failed through the same
boundary at the missing xdiff target. The candidate file SHA-256 is
`ec369e426decff35b03f0f9da60521ebf2048f3dc786086d82fb618953ce3a96`.
Relevant test/runtime/install inputs are identical between producer
`aa6294c50f95b51d95f7c7861d7430f89cdb2fdc` and this source candidate.
The coordinator restored/removed its temporary worktree and confirmed the
original producer remained clean. No application compilation or production
package/install was performed.

Local macOS checks pass **20/21** package runtime cases, with the single explicit
native-Linux skip, all **67** documentation tests, `DOCUMENTATION_OK`, and
`git diff --check`. The [verification record](../_checkpoints/BPR-1561/verification.json)
binds commands, receipt and regression evidence. The
[handoff](../_checkpoints/BPR-1561/handoff.json) preserves the plan binding,
source commit, ownership, command state and wider obligations. No attended
criterion remains in this test-only repair. RFC-BPR-0038, BPR-0961 and BPR-0962
retain all native, security, trust, legal and privacy gaps.
