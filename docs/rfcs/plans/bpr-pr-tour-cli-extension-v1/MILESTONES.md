# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr-pr-tour-cli-extension-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## intake

Import only the extension with collision-free mappings and an explicit parent/runtime baseline.

**Work:** `BPR-1052`

**Prerequisites:** None

**Completion:**

- Parent archive/import mappings and current HEAD are recorded; no parent cards are cloned or reset.
- New RFC/card aliases and support links are remapped; the installed implement-rfc imports this offline grouping.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.

## headless-manual

A separately runnable non-GUI CLI supports the complete local manual authoring and rehearsal path.

**Work:** `BPR-1053`, `BPR-1054`, `BPR-1055`, `BPR-1056`, `BPR-1057`, `BPR-1058`

**Prerequisites:** intake

**Completion:**

- Desktop/core-only builds retain their boundaries; tests-disabled CLI has no GUI/display requirement.
- Precise open/edit/outline/source/undo and human/JSON behavior work through shared services with zero AI and zero remote writes.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.

## publish-and-recover

Terminal publication uses the existing plan/journal, and cold published recovery is machine-independent.

**Work:** `BPR-1059`, `BPR-1063`

**Prerequisites:** headless-manual

**Completion:**

- Preview/confirm/stage/submit/update/recover flow preserves exact authority and no-duplicate semantics in isolated fixtures.
- Clean-profile GitHub reconstruction preserves current published content and reports absent/private/partial data honestly.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.

## same-machine-handoff

One local WIP draft is usable across desktop, CLI and skill-command clients with the desktop open.

**Work:** `BPR-1060`, `BPR-1061`, `BPR-1062`

**Prerequisites:** headless-manual

**Completion:**

- Native ownership/forwarding and shared journal prevent second-writer, duplicate-request and stale-editor loss.
- CLI→desktop→command-client→CLI retains exact text/order/source/readiness/conflicts/undo without copying or uploading; owner crash and IME cases have evidence.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.

## pr-tour-skill

The thin named pr-tour skill uses the active agent and effective scoped hooks over the CLI.

**Work:** `BPR-1064`, `BPR-1065`, `BPR-1066`, `BPR-1067`

**Prerequisites:** publish-and-recover, same-machine-handoff

**Completion:**

- No secondary publisher, draft format, model session or hook defaults are introduced.
- Per-stop author interaction, policy, exact replacement and real publication approval boundaries pass focused tests; canonical distribution status is explicit.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.

## verification-closeout

The macOS desktop bundles bpr with native release qualification; actual interfaces and claimed hosts are qualified and all extension claims are reconciled.

**Work:** `BPR-1068`, `BPR-1069`, `BPR-1070`, `BPR-1071`

**Prerequisites:** pr-tour-skill

**Completion:**

- Actual native/website/agent courses record supported and absent evidence independently; a fresh machine needs only GitHub for published meaning.
- All extension acceptance rows, docs and canonical closure agree; no missing prerequisite is called complete.

**Boundary:** Extension-only work; preserve parent authority and current source/platform/human evidence limits. No automatic remote effects.
