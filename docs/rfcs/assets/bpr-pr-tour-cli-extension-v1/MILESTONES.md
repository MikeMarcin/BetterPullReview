# Authored extension milestones

Use `milestone-plan.json` as the default execution seed. Do not re-plan the imported base pack or treat these groups as optional hints. It covers only the three extension RFCs.

| Milestone | Extension cards | Exit outcome |
|---|---|---|
| intake | PACK-PRTOUR-01 | Import only the extension with collision-free mappings and an explicit parent/runtime baseline. |
| headless-manual | PACK-PRTOUR-02, PACK-PRTOUR-03, PACK-PRTOUR-04, PACK-PRTOUR-05, PACK-PRTOUR-06, PACK-PRTOUR-07 | A separately runnable non-GUI CLI supports the complete local manual authoring and rehearsal path. |
| publish-and-recover | PACK-PRTOUR-08, PACK-PRTOUR-12 | Terminal publication uses the existing plan/journal, and cold published recovery is machine-independent. |
| same-machine-handoff | PACK-PRTOUR-09, PACK-PRTOUR-10, PACK-PRTOUR-11 | One local WIP draft is usable across desktop, CLI and skill-command clients with the desktop open. |
| pr-tour-skill | PACK-PRTOUR-13, PACK-PRTOUR-14, PACK-PRTOUR-15, PACK-PRTOUR-16 | The thin named pr-tour skill uses the active agent and effective scoped hooks over the CLI. |
| verification-closeout | PACK-PRTOUR-17, PACK-PRTOUR-18, PACK-PRTOUR-19, PACK-PRTOUR-20 | Packages, actual interfaces and claimed hosts are qualified and all extension claims are reconciled. |

## Predecessor integration

The parent tour pack is already imported. Resolve its live RFC and issue mappings once. Shared local editing needs its 0039 model/store; cold import needs 0040; remote terminal operations need 0041/0042; assistance needs 0043. Readiness of each exact seam is the technical dependency. A missing seam stays an explicit predecessor task, not a duplicate extension implementation. Formal canonical closeout remains separate.

Publication/recovery and same-machine handoff can proceed independently after the headless manual milestone. The skill follows both and the parent's manual-before-AI frontier. Native/website/actual-agent verification is concentrated at the final boundary; use focused tests earlier. Do not hardcode model names, effort levels, external orchestrators or a fresh planning exercise per command.

## Import and accounting

Allocate only PACK-PRTOUR aliases and proposed RFCs 0044–0046. Preserve the original archive and full alias mapping. Use the installed MASK-managed implement-rfc validator/importer with its current interfaces, remap issue/RFC/dependency references, and retain this plan's identity/lineage. The offline shape follows the supplied `implement-rfc.milestone-plan.v1`; local pack validation is not a native import receipt.

Keep canonical skill source ownership separate from BPR runtime ownership. The skill-distribution card records actual upstream access and revision; no silent repository-local fork. Runtime work can continue with a clearly marked candidate when upstream installation lacks authorization, but distribution acceptance stays open.

## Completion

The archive does not implement the CLI or install the skill. Passing offline examples is not a substitute for target-native binary/IPC/editor evidence, actual agent behavior or a live designated GitHub/GHES round trip. Keep those gates honest and preserve available work without inventing a pass for unavailable targets.
