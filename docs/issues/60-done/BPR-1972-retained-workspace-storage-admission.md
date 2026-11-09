---
schema: "implement-rfc.card.v1"
id: "BPR-1972"
title: "Allow native continuation on extremely large projects"
status: "Done"
rfcs: []
depends_on: ["BPR-1969"]
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"mandate_revision":"retained-large-projects-r2","reservation":"docs/issues/_reservations/BPR-1972--BPR-1972--c187b3b0f7ca.json","followup_of":"BPR-1969","origin":"Owner reports corrected build still refuses at 0:03 with retained storage budget or verification sentence; selected workspace earlier measured100800entries/8.4GiB.","milestone_plan":{"id":"bpr-1972-retained-budget","path":"docs/rfcs/plans/bpr-1972-retained-budget/milestone-plan.json"},"original_mandate":"retained-workspace-continuation-r1","owner_amendment":"I mean the workspace is an extremely large project. There's no reason we shouldn't be able to operate on extremely large projects though."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3f233964dfe59461b4e347c757fa6e8fd72c7dd4","evidence":{"kind":"observation","detail":"Above16GiB logical sparse-file and263169real zero-byte entry fixtures permit first/reuse/new-scope host dispatch; previous production-object negative controls fail both corresponding assertions. No native project-size inventory gate remains.","ledger":"docs/verification/BPR-1972-large-native-workspaces.md","external_evidence":"/Users/jaeger/.codex/tmp/bpr-1972-retained-budget"}},{"criterion":"AC2","outcome":"pass","revision":"3f233964dfe59461b4e347c757fa6e8fd72c7dd4","evidence":{"kind":"observation","detail":"Selected ownership/journal/HEAD/noalternates and pinned object checks unchanged; host failure rows and realGit retained/sparse contract pass. Native catalog/storage fault classification remains private_storage_failure.","ledger":"docs/verification/BPR-1972-large-native-workspaces.md","external_evidence":"/Users/jaeger/.codex/tmp/bpr-1972-retained-budget"}},{"criterion":"AC3","outcome":"pass","revision":"3f233964dfe59461b4e347c757fa6e8fd72c7dd4","evidence":{"kind":"observation","detail":"Materialization, actual workspace failure, cancellation and protected cleanup tests pass; production diff only removes36line budget helper/call. No larger cap, deletion or automatic replacement introduced.","ledger":"docs/verification/BPR-1972-large-native-workspaces.md","external_evidence":"/Users/jaeger/.codex/tmp/bpr-1972-retained-budget"}},{"criterion":"AC4","outcome":"pass","revision":"3f233964dfe59461b4e347c757fa6e8fd72c7dd4","evidence":{"kind":"observation","detail":"Exact product host/public2of2 and native6course pass; full entrycourse, integration4of4, isolation manifest, format22, scoped tidy22 and documentation67tests pass. Owner r2 amendment documented; corporate cause/native platform limits explicit.","ledger":"docs/verification/BPR-1972-large-native-workspaces.md","external_evidence":"/Users/jaeger/.codex/tmp/bpr-1972-retained-budget"}}]
---

# Allow native continuation on extremely large projects

## Owner amendment retained-large-projects-r2, 2026-11-09

The owner reported that the corrected build now refuses after0:03 with the generic
retained-storage16GiB/262144-entry-or-unverifiable sentence. The earlier screenshot
measured one workspace at100800entries/8.4GiB; no current aggregate measurement
exists. The actual corporate Enterprise machine is unavailable from this harness.

The owner then instructed: "I mean the workspace is an extremely large project.
There's no reason we shouldn't be able to operate on extremely large projects
though." This supersedes the initial follow-up's requirement to preserve the
fixed16GiB/262144 aggregate gate. Remove that gate for both retained continuation
and fresh native-session dispatch, without replacing it with a larger hidden cap
or a whole-tree scan. Preserve ownership and required-object checks, catalog and
journal admission bounds, actual filesystem failures and safe cleanup guarantees.
This amends the original mandate; it does not rewrite BPR-1969's historical evidence.

## Acceptance

- AC1. A valid native session can start and continue when the workspace neighbourhood
  exceeds the old16GiB or262144-entry project-size thresholds; neither those fixed
  limits nor whole-neighbourhood inventories gate dispatch. Regression evidence
  distinguishes the old refusal from the new behavior.
- AC2. Reused workspaces retain exact ownership/journal/HEAD/no-alternates and pinned
  commit/tree validation. Unusable retained state still refuses before an agent
  starts. Native catalog/storage failures retain their existing classification.
- AC3. New-session materialization retains actual disk/Git/cancellation failures and
  protected cleanup semantics. The change grants no automatic deletion and adds no
  silent replacement or new fixed project-size cap.
- AC4. Focused host and composed native/tour/replay courses pass. The live contract,
  owner amendment, exact candidate and actual evidence limits are documented. The
  inaccessible corporate-machine cause is explicitly unverified; fixture evidence
  is not claimed as a new measurement there.

## Out of scope

Automatic reclamation of protected work, unlimited catalog/journal counts,
full-history materialization optimization, installed-agent or cross-platform
native qualification, and another agent's BPR-1971 replay diagnosis.
