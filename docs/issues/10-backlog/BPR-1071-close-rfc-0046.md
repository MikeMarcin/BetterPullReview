---
schema: "implement-rfc.card.v1"
id: "BPR-1071"
title: "Verify and close RFC-BPR-0046"
status: "Backlog"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["BPR-1069", "BPR-1070"]
closeout_for: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-20","milestone":"verification-closeout","acceptance":[],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1071: Verify and close RFC-BPR-0046

Pack title: "Independently review extension boundaries and close out truthfully". The repository names a closeout card after one RFC; this single authored card closes RFC-BPR-0044 to RFC-BPR-0046 in canonical order and is named after the last.

Pack alias: **PACK-PRTOUR-20** (live `BPR-1071`, allocated at import).

RFC base: `2928e90ea38cb58dce57f7ee32aa7b817ed4e876`.

Execution run `bpr-pr-tour-cli-20260927` starts at immutable
`89f23eeece835ecafb9f8752b524644b22fbf281` under mandate
`r1-20260927-implement-and-land-each-milestone`. Preserve the historical import
base above for complete pack closeout. The [execution ledger](../../verification/RFC-BPR-0044-0046.md)
and [intake handoff](../../verification/RFC-BPR-0044-0046.intake-handoff.json)
record the accepted intake, current parent contracts and next milestone.
All product acceptance rows below remain unrun by intake.

## Closeout checklist

Reconciled 2026-10-27 under bundled-cli-r1. Implementation records below are
reused at their original producing revisions and scope; they are not fresh
native, human, independent-review or final RFC acceptance. BPR-1069–1071
remain open, as do the amended BPR-1068 obligations.


| Criterion | Result | Revision |
|---|---|---|
| **AC44-01:** Tests-disabled `bpr` installs and runs without desktop/GUI/display dependencies; desktop and core-only builds remain valid. | Implementation evidence accepted in [[BPR-1054]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `d62902a869100847dcc92d1376329c1aa9986a88` |
| **AC44-02:** Desktop and CLI exercise the same tour services, parser, publisher, journal, and normalized result semantics; no duplicate publisher or GUI child process exists. | Implementation evidence accepted in [[BPR-1053]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `434b34af8` |
| **AC44-03:** A human can open, write Problem/Solution, add/reorder/walk/complete stops, preview, and publish without an AI provider or GUI. | Implementation evidence accepted in [[BPR-1058]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `9c1c9447463d838e7306e3911316c28b8802dd76` |
| **AC44-04:** Human/JSON outputs, typed errors, paging, exact identifiers, cancellation, and noninteractive refusal obey CLI-CONTRACT. | Implementation evidence accepted in [[BPR-1056]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `048a4a91e919c987bbbab280e267cf4c55fb39d2` |
| **AC44-05:** Every command binds an explicit or unambiguous profile/account/PR/draft/revision; concurrent agent sessions cannot redirect each other through current selection. | Implementation evidence accepted in [[BPR-1055]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `699c619d79c25f9f61eaa3c38462561d2b40d9ec` |
| **AC44-06:** Source destinations are pinned and distinguish prose byte ranges from source line ranges, including historical/left/unchanged fallback targets. | Implementation evidence accepted in [[BPR-1058]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `9c1c9447463d838e7306e3911316c28b8802dd76` |
| **AC44-07:** Editor/stdin/file operations preserve text, use exact revision checks, release ownership while awaiting an editor, and do not execute repository instructions. | Implementation evidence accepted in [[BPR-1057]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `3c82ef7248240f11751a1a130b9070805cb52cc3` |
| **AC44-08:** Preview/recover/readiness/local editing make zero GitHub writes; publication uses the exact finite existing plan with explicit confirmation and no unscoped noninteractive bypass. | Implementation evidence accepted in [[BPR-1059]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `5341b12e8a2f0f250d7251501473e81197664430` |
| **AC44-09:** Native and admitted CLI transports retain host/platform qualification and credential boundaries; unsupported paths refuse honestly. | Implementation evidence accepted in [[BPR-1059]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `5341b12e8a2f0f250d7251501473e81197664430` |
| **AC44-10:** Killed/interrupted terminal publication and maintenance reconcile known identities without duplicate sends or automatic resume. | Implementation evidence accepted in [[BPR-1059]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `5341b12e8a2f0f250d7251501473e81197664430` |
| **AC44-11:** Terminal rendering neutralizes untrusted control/escape sequences, reports truncation, and remains usable with no color, narrow width, and Unicode paths. | Implementation evidence accepted in [[BPR-1058]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `9c1c9447463d838e7306e3911316c28b8802dd76` |
| **AC44-12:** A macOS desktop release with local automation includes `Contents/MacOS/bpr` and its runtime closure. Inspect the installed app and run `bpr tour capabilities --json` from that final path; qualify dependencies, signing/notarization, installed notices, SPDX, corresponding source and first-run profile/authentication there. Document stable absolute-path and optional persistent PATH setup, and verify normal and relocated bundles remain headless. The `pr-tour` skill locates PATH first, then the standard macOS app, and retains one absolute executable path. Preserve separately recorded Windows/Linux safety and qualification obligations; macOS evidence does not qualify them. | Bundled CLI, final/relocated public path and complete package/source/security mechanics pass; native sign-in, release trust/legal and other-platform obligations pending ([[BPR-1068]]). | `e0d07290a9d10b56d8c44a2a6e0e0dcc6792f71b` |
| **AC45-01:** Cold open from only GitHub reconstructs all available shared content/order/bindings in desktop, CLI and skill without local metadata or AI. | Implementation evidence accepted in [[BPR-1063]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `3a9a7fcc1dcfe053d1144292e2cfe7dc887c58c3` |
| **AC45-02:** Missing private context is reported as unknown; complete/partial/ambiguous/deleted/stale observations never collapse to empty or fabricated content. | Implementation evidence accepted in [[BPR-1063]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `3a9a7fcc1dcfe053d1144292e2cfe7dc887c58c3` |
| **AC45-03:** Local WIP saves and cross-interface switches produce zero GitHub writes; staged and submitted material have visibly distinct status and recovery rules. | Implementation evidence accepted in [[BPR-1060]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `012edc856ddae35249721f6e995ce8ad73e1c545` |
| **AC45-04:** The same unfinished draft moves CLI → desktop → skill → CLI, retaining text, order, IDs, source/readiness state, baselines and reversible committed edits without copy/import. | Implementation evidence accepted in [[BPR-1062]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `637cd41df1b87748fedbd1003270bebec8c7a45f` |
| **AC45-05:** CLI commands work against a running desktop through an authenticated, bounded, versioned owner channel, with no second store writer and no mandatory daemon. | Implementation evidence accepted in [[BPR-1061]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `27dbeef51cee810c143467fb6d38adbd578ba0ae` |
| **AC45-06:** Owner/peer replacement, protocol mismatch, duplicate request, uncertain delivery, crash, and stale endpoint cases refuse/reconcile without duplicate local or remote effects. | Implementation evidence accepted in [[BPR-1061]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `27dbeef51cee810c143467fb6d38adbd578ba0ae` |
| **AC45-07:** Dirty buffers, IME, external-editor saves, concurrent fields/structure and stale AI replies preserve authors' text and report conflicts instead of last-writer-wins. | Implementation evidence accepted in [[BPR-1062]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `637cd41df1b87748fedbd1003270bebec8c7a45f` |
| **AC45-08:** Cross-interface undo/redo is one coherent local transaction history with monotonically advancing revisions, no duplicate editor application, and no network/AI replay. | Implementation evidence accepted in [[BPR-1060]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `012edc856ddae35249721f6e995ce8ad73e1c545` |
| **AC45-09:** Cold pending-review discovery, local purge and missing journals never auto-submit/adopt/delete or claim recovery of absent private WIP/authorization. | Implementation evidence accepted in [[BPR-1063]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `3a9a7fcc1dcfe053d1144292e2cfe7dc887c58c3` |
| **AC45-10:** Website edits, multiple batches/roots, historical anchors, second-machine recovery and profile/account isolation pass common production-path fixtures plus separately recorded native/website evidence. | Required attended native/website/agent and second-machine acceptance remains pending in [[BPR-1069]]. | No producing attended revision |
| **AC46-01:** Skill/package/frontmatter identity is exactly `pr-tour`; canonical distribution uses the existing shared-skill workflow with no divergent implementation copies. | Implementation evidence accepted in [[BPR-1067]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `8924d9f1a` |
| **AC46-02:** The main skill is a thin, progressively loaded wrapper; GitHub access, draft state, parsing, anchors, publication and recovery all use `bpr tour`. | Implementation evidence accepted in [[BPR-1065]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `b6717e02ead025fd9c151d18c0f7c722bb5587f4` |
| **AC46-03:** Activation/capability/version checks and every subsequent call bind explicit returned identities; a missing/incompatible CLI produces no fallback publisher. | Implementation evidence accepted in [[BPR-1065]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `b6717e02ead025fd9c151d18c0f7c722bb5587f4` |
| **AC46-04:** The conversation follows manual-first Problem/Solution/individual stops, reuses known answers, and persists accepted user text without bulk unsolicited generation. | Implementation evidence accepted in [[BPR-1065]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `b6717e02ead025fd9c151d18c0f7c722bb5587f4` |
| **AC46-05:** The active agent uses the effective four hooks and scoped context/results without nested agents, hardcoded model routes or direct model APIs. | Implementation evidence accepted in [[BPR-1064]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `eca094604bddbd0aafb4821f03d2b090dca05950` |
| **AC46-06:** Exact-range rewrite, ambiguity, stale/ABA response, no-op, failed result and undo/redo behavior match desktop/CLI semantics with no mandatory acceptance panel. | Implementation evidence accepted in [[BPR-1064]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `eca094604bddbd0aafb4821f03d2b090dca05950` |
| **AC46-07:** Outline-only and individual-stop user interaction are preserved; tool reads are not human visits and acknowledgement provenance is not overstated. | Implementation evidence accepted in [[BPR-1066]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `0799fa1660d2f824eca2a93a039199d3ff174d3a` |
| **AC46-08:** No direct API/DB mutation or fabricated approval exists; publication requires a current explicit user command and the CLI's real authorization path. | Implementation evidence accepted in [[BPR-1066]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `0799fa1660d2f824eca2a93a039199d3ff174d3a` |
| **AC46-09:** Prompt injection, oversized context, forbidden host/provider, private-note exclusion, claimed identity and interrupted-session cases are handled without policy bypass. | Implementation evidence accepted in [[BPR-1066]]; original scope/limits retained in the execution ledger. Final pack acceptance pending. | `0799fa1660d2f824eca2a93a039199d3ff174d3a` |
| **AC46-10:** Packaged skill smoke tests in supported interactive agent hosts, shared-draft round trips, and human-attended transcript checks are recorded separately from offline fixtures. | Required attended native/website/agent and second-machine acceptance remains pending in [[BPR-1069]]. | No producing attended revision |

## Outcome

Reconcile all 32 extension acceptance rows against actual production, native, website and skill evidence. Confirm no parent RFC/card duplication, public-format fork, local-writer bypass or unattended publication path. Perform focused independent review and canonical closeout in dependency order.

## Existing ownership seams

Live RFC/issue/plan indexes; extension acceptance matrix; unchanged baseline/predecessor lineage.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

All claims carry direct producing revisions and remaining evidence limits. Missing predecessor/native/human evidence remains open; do not fake Done/Auto-Verified or rerun unrelated exhaustive courses to manufacture confidence.

Acceptance ownership: Integration/lineage/closeout slice; no new product criterion is implied.

## Execution boundary

Milestone: **verification-closeout**. Pack alias PACK-PRTOUR-20 was allocated as live `BPR-1071` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
