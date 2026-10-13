---
schema: "implement-rfc.card.v1"
id: "BPR-1464"
title: "Verify and close RFC-BPR-0083"
status: "Backlog"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454", "BPR-1455", "BPR-1456", "BPR-1457", "BPR-1458", "BPR-1459", "BPR-1460", "BPR-1461", "BPR-1462", "BPR-1463"]
closeout_for: ["RFC-BPR-0083"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1464: Verify and close RFC-BPR-0083

RFC base: `9a4f61976bba23727acd77d10c23c041f87208f0`.

Own all seven criteria of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) against the original
baseline. Preserve RFC-BPR-0054's acceptance and every recorded owner answer.

## Acceptance criteria

- **AC01:** The differential harness runs in the workspace contract suite over the corpus with an isolated oracle. It reports agreement for each component and case, reports a missing Git feature as skipped with the version found, and turns red under the negative control.
- **AC02:** Every divergence of each evaluated implementation is classified under H1, and no component's product implementation keeps a silent divergence.
- **AC03:** The extended canary course records containment, no-execution and no-write results for K and for every L component evaluated, on macOS, Linux and Windows.
- **AC04:** Every parser that stays or is adopted has a fuzz target in the existing harness with the oracle defined for it. The BPR-0970 input and the adversarial corpus meet H6, and performance is reported against rule 3.
- **AC05:** Each evaluated candidate has a cost record that lists its license and SPDX expression, the trial packaging result, the binary-size and build-time deltas for each platform, its SHA-256 and reftable status, and the integration size. G's desk check records the reasons it was rejected.
- **AC06:** The committed decision record gives each component's verdict, the gate or rule applied, links to the evidence, and every question answer it relies on. An adopted library sits behind the workspace-adapter port with no type crossing inward. The `core-only` build, the architecture-boundary checks and the documentation gate pass.
- **AC07:** After the decision, a reftable checkout produces Git's HEAD commit, or the refusal or marker chosen under Q2. It never produces an empty `HeadCommit` that looks like an unborn branch.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** The differential harness runs in the workspace contract suite over the corpus with an isolated oracle. It reports agreement for each component and case, reports a missing Git feature as skipped with the version found, and turns red under the negative control. | Pending | pending | — |
| **AC02:** Every divergence of each evaluated implementation is classified under H1, and no component's product implementation keeps a silent divergence. | Pending | pending | — |
| **AC03:** The extended canary course records containment, no-execution and no-write results for K and for every L component evaluated, on macOS, Linux and Windows. | Pending | pending | — |
| **AC04:** Every parser that stays or is adopted has a fuzz target in the existing harness with the oracle defined for it. The BPR-0970 input and the adversarial corpus meet H6, and performance is reported against rule 3. | Pending | pending | — |
| **AC05:** Each evaluated candidate has a cost record that lists its license and SPDX expression, the trial packaging result, the binary-size and build-time deltas for each platform, its SHA-256 and reftable status, and the integration size. G's desk check records the reasons it was rejected. | Pending | pending | — |
| **AC06:** The committed decision record gives each component's verdict, the gate or rule applied, links to the evidence, and every question answer it relies on. An adopted library sits behind the workspace-adapter port with no type crossing inward. The `core-only` build, the architecture-boundary checks and the documentation gate pass. | Pending | pending | — |
| **AC07:** After the decision, a reftable checkout produces Git's HEAD commit, or the refusal or marker chosen under Q2. It never produces an empty `HeadCommit` that looks like an unborn branch. | Pending | pending | — |

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
