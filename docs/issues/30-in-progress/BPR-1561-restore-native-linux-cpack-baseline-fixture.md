---
schema: "implement-rfc.card.v1"
id: "BPR-1561"
title: "Restore the native Linux CPack baseline fixture"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
platforms: ["linux"]
metadata: {"mandate_revision":"ubuntu-readiness-20261016-unversioned-r1","reservation":"docs/issues/_reservations/BPR-1561--BPR-1561--32d31ecb043c.json","milestone_plan":{"id":"bpr1561-linux-cpack-fixture","path":"docs/rfcs/plans/bpr1561-linux-cpack-fixture/milestone-plan.json"}}
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
