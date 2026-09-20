---
schema: "implement-rfc.card.v1"
id: "BPR-0948"
title: "Implement updater install transaction and recovery"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0947"]
milestone: "RFC-0035 milestone 3"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Implement updater install transaction and recovery

Third authored milestone of [[BPR-0911]]. Add the bundled signed helper,
installation-scoped cooperative ownership, shutdown barrier, durable journal,
recovery and exact healthy-start receipt. Qualify disposable signed installations
before enabling production packages; never replace the developer's running app.

## Acceptance

- AC-01: Exact verified-candidate gesture, refreshed release authority and immediate
  staged revalidation precede replacement; failed flush/quit/quiescence aborts.
- AC-02: All windows and installation processes preserve drafts and indeterminate
  publication reconciliation; timeout never kills processes or retries a write.
- AC-03: Fault injection at every rename/journal/relaunch boundary preserves a
  launchable old copy, prevents concurrent versions and gates success/cleanup on
  transaction/executable/version-bound healthy startup.
- AC-04: Native disposable signed qualification records actual artifacts and
  outcomes; cleanup is restricted to journal-proven ownership and stays truthful.

Record RFC AC-04 through AC-07 evidence and remaining native limits on [[BPR-0912]].
