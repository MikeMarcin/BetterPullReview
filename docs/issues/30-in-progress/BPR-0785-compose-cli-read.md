---
schema: "implement-rfc.card.v1"
id: "BPR-0785"
title: "Compose bounded configured-CLI reads through the public workbench"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# Compose bounded configured-CLI reads through the public workbench

- [ ] The selected official `gh` and exact hostname execute only for an opted-in CLI profile, without token extraction, a BetterPullReview-launched shell, global auth mutation or native-credential fallback. On macOS every `gh` launch has OS-enforced file-write denial with only a bounded owned clone-store exception. A bounded `gh` Git operation may invoke Git and `gh`'s own credential-helper shell; its child-process and account races are disclosed.
- [ ] The CLI process adapter returns actual bounded HTTP status/headers/body and maps failures into normalized provider results while preserving correlation, pagination, cancellation and redaction.
- [ ] Config, executable and observed account changes cause typed refusal. Tests state which races remain uncontained.
- [ ] A tests-disabled Mac composition renders a representative fixture PR from the real CLI profile; removing the CLI process or response adapter makes the public test fail.
- [ ] Focused adapter/provider/composition checks and documentation gate pass. No live PR mutation occurs.

The `EpicGames/UnrealEngine#1773` 250-commit response limit belongs to
RFC-BPR-0013 and is not a CLI read acceptance target.

Initial macOS Git-object qualification is limited to `gh` 2.100.0 and Apple
Git 2.50.1 (Apple Git-155), with a 32 MiB per-file ceiling and a supervised
96 MiB/128-entry temporary-store abort threshold, not a hard aggregate quota.
Refs synthesized for local protocol v2 represent the accepted API snapshot,
not a fresh remote advertisement. The tests-disabled full-diff render and
isolated process-adapter-removal control are recorded on BPR-0788; neither
claims a native HTTPS credential-helper or Keychain-backend qualification.
