---
schema: "implement-rfc.card.v1"
id: "BPR-0295"
title: "Anchor RFC-BPR-0019 ordinary verification"
status: "Blocked"
rfcs: ["RFC-BPR-0019"]
depends_on: ["BPR-0115","BPR-0287","BPR-0288","BPR-0289","BPR-0290","BPR-0291","BPR-0292","BPR-0293","BPR-0294"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0019-closeout","path":"docs/rfcs/plans/rfc-bpr-0019-closeout/milestone-plan.json"}}
blocked_by: "independent-closeout-review"
---

# Anchor RFC-BPR-0019 ordinary verification

RFC base: `58b5116f8706ac336664cc3fc39dabc66745a70d`.

## Current frontier — 2026-09-25, mandate r1

Run `rfc0019-20260925-parallel`, milestone `ordinary-closeout`, on isolated
branch `codex/rfc0019-closeout`. Product candidate is
`fc4b39e8c3a54ecf931d74071d7793e5e4112496`; the claim revision is
`07f41e25d4bd0793839f15c084e5d2d1e53ee251`. This audit changes documentation
and tracker state only. The [bounded plan](../../rfcs/plans/rfc-bpr-0019-closeout/milestone-plan.json)
selects this final authored stage and preserves every original criterion.

**Ordinary closeout remains unaccepted.** Both predecessor RFCs and all
BPR-0287–0294 implementation cards are now terminal. Current assets are the
accepted RFC-BPR-0030 schema-2 ten-clip revision, and RFC-BPR-0031 already
composes them. Those successor RFCs deliberately supersede the old clip,
resource, regeneration and no-runtime claims; they do not supply this card's
required independent ordinary review. The [current ledger](../../verification/RFC-BPR-0019.md#ordinary-closeout-audit--2026-09-25)
records the exact mapping, commands, candidate hashes and remaining gates.
The original criteria below are retained without an inferred acceptance amendment.

Historical results remain bound to their recorded candidates; current replay and
authorized successor changes are distinguished below.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [ ] | The checked-in neutral master and all source frames are original Dif assets with a complete ownership/license and tool-provenance record plus a digest-bound rights decision or underlying license/assignment evidence; no Nib, Mel, AllTheThings, private-repository, or credential material is present. | Exact reference, neutral-master, source-frame, manifest, provenance, tool, and generated-product digest set plus scoped metadata and foreign-material audit | Replay the completed [BPR-0287](../60-done/BPR-0287-record-dif-asset-authority.md#recorded-verdict) decision and the final-rights portion of [BPR-0291](../60-done/BPR-0291-record-dif-rights-and-visual-verdicts.md#recorded-verdict), then validate both immutable decisions against the final source pack. | The validated authority names the creator or rightsholder basis, exact permitted derivative and distribution scopes, tools, notices, and final source hashes, while the scoped audit finds no prohibited identity, private material, or credential. | Owned-original-source row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “The checked-in neutral master and all source frames are original Dif assets with a complete ownership/license and tool-provenance record plus a digest-bound rights decision or underlying license/assignment evidence; no Nib, Mel, AllTheThings, private-repository, or credential material is present.” | external-authority | Current 72 source files and manifest match the accepted v008 intake; its twelve accepts and derivative authority remain bound to session sequence 123. BPR-0291 describes different historical bytes. Unknown external generator metadata remains explicit; ordinary review pending. |
| [ ] | A clean offline regeneration reproduces every committed strip, generated catalog, and digest byte for byte from the checked-in source frames. | Two fresh empty output trees and the complete committed generated tree, including relative paths, contents, digest inventory, and absence of extras | `python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif` after deleting no committed file and allowing the verifier to create both isolated temporary outputs. | Both independent regenerations have identical tree hashes and every relative file byte equals the committed generated product; any stale, hand-edited, missing, extra, or environment-dependent output fails. | Byte-identical-regeneration row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “A clean offline regeneration reproduces every committed strip, generated catalog, and digest byte for byte from the checked-in source frames.” | automated | Current validate/verify and a strict generated-product byte comparison pass on Python 3.14.7/zlib 1.2.12. BPR-0378/0408 and RFC-0030 preserve portable canonical-pixel comparison across compressors; ordinary review pending. |
| [ ] | The versioned manifest defines exactly `calm_idle`, `greet`, and `rest`, with valid timing, representative frames, priority, return, geometry, accessibility, and bounded-resource fields. | Strict manifest/schema validator, generated descriptor compile consumer, and hostile missing, unknown, cyclic, stale-digest, and over-limit cases | `python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif && ctest --preset dev -R '^dif_assets_contract$' --output-on-failure` at the final closeout head. | Exactly three declared clips share one schema and digest with the compiled descriptor; every invalid identity, field, return graph, geometry, timing, accessibility, or resource bound is rejected non-vacuously. | Exact-clip-contract row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “The versioned manifest defines exactly `calm_idle`, `greet`, and `rest`, with valid timing, representative frames, priority, return, geometry, accessibility, and bounded-resource fields.” | automated | Literal three-clip claim is superseded by RFC-0030: current schema 2 has ten clips. Current 63-case asset contract passes; no three-clip assertion is marked passing. |
| [ ] | Every runtime cell is transparent 256 x 256 RGBA, all gutters and body-bound rules pass, and the admitted catalog stays within 48 content frames, 12 frames per second, 12 MiB content pixels, and 13.5 MiB complete decoded strips including the six fixed gutters. | Every decoded strip cell, transparent gutter, alpha edge, declared body/center/baseline position, clip timing, and aggregate decoded-pixel budget | `python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif && ctest --preset dev -R '^dif_assets_contract$' --output-on-failure` with the hostile dimension, alpha, gutter, edge, travel, rate, frame, and decoded-byte corpus enabled. | Every admitted cell is RGBA at the exact size and stays inside declared geometry; all six gutters are empty and every per-clip and aggregate limit passes while each forced violation fails. | Transparent-bounded-cells row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “Every runtime cell is transparent 256 x 256 RGBA, all gutters and body-bound rules pass, and the admitted catalog stays within 48 content frames, 12 frames per second, 12 MiB content pixels, and 13.5 MiB complete decoded strips including the six fixed gutters.” | automated | Literal original frame/gutter/byte limits are superseded by RFC-0030: 71 content cells, 20 gutters, 17.75 MiB content and 22.75 MiB complete; current geometry/rate validation passes within successor limits. |
| [ ] | Mechanical loop and handoff checks pass, and the digest-bound attended review approves Dif's identity, motion, registration, appearance contexts, shipping scales, calm-loop seam, greet-to-idle handoff, and reduced-motion `rest` frame. | Mechanical seam/handoff metrics plus immutable five-subject offline review package containing every source/generated frame, declared-speed motion, overlays, contexts, scales, and rest comparisons | Replay the completed attended visual portion of [BPR-0291](../60-done/BPR-0291-record-dif-rights-and-visual-verdicts.md#recorded-verdict) against the final package manifest after all mechanical audits pass. | The exact returned verdict approves all five independent subjects and binds the final source, manifest, generator, generated strips, preview renderer, and capture bytes; any rejection or digest drift leaves the row open. | Mechanical-and-attended-motion row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “Mechanical loop and handoff checks pass, and the digest-bound attended review approves Dif's identity, motion, registration, appearance contexts, shipping scales, calm-loop seam, greet-to-idle handoff, and reduced-motion `rest` frame.” | owner-attended | Historical BPR-0291 receipt identities remain intact but every one of its fifteen bound source/generated files differs now. The current v008 manifest and all 72 sources match its separate accepted intake; prior visual decisions are not copied forward. |
| [ ] | The ordinary developer, core-only, and tests-disabled application graphs do not run art generation or require its tool dependencies; core-only remains free of mascot assets and Qt. | Clean developer, core-only, and tests-disabled selected build/resource/dependency graphs plus executable and source scans and cut selection negatives | `cmake --preset dev && cmake --build --preset dev && cmake --preset core-only && cmake --build --preset core-only && ctest --preset core-only --output-on-failure` followed by the completed BPR-0292 tests-disabled graph and scoped dependency/resource audit. | All selected graphs build without invoking or depending on art generation; the offline contract runs where registered, while core-only contains neither Qt nor Dif resources and the application contains no RFC-BPR-0020 runtime composition. | Build-graph-isolation row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “The ordinary developer, core-only, and tests-disabled application graphs do not run art generation or require its tool dependencies; core-only remains free of mascot assets and Qt.” | automated | Current offline-tool and source/resource boundary checks pass. Current actual dev/core-only/tests-disabled graph and runtime replay not run in this bounded audit; historical BPR-0292 and successor graph evidence remain candidate-specific. |
| [ ] | Documentation names the canonical source, generated products, regeneration command, review procedure, limits, provenance, and RFC-BPR-0020 consumption boundary without claiming runtime composition. | RFC-base changed-path ledger, asset-local maintainer guide, owning architecture/UX/security/build/package documentation, and stable agent guidance disposition | `python3 scripts/verify_documentation.py --root . && git diff --check` after completing the BPR-0293 immutable-base documentation review and verifying all relative links and final newlines. | Every changed path has an owning update or exact no-impact disposition, all commands and limits match the final implementation, and no document claims a runtime mascot, package certification, or unattended approval. | Honest-documentation row in the [RFC-BPR-0019 direct claim matrix](../../verification/RFC-BPR-0019.md#direct-claim-matrix); final replay record keyed by “Documentation names the canonical source, generated products, regeneration command, review procedure, limits, provenance, and RFC-BPR-0020 consumption boundary without claiming runtime composition.” | automated | Current documentation gate and 64 documentation tests pass. Present docs correctly describe RFC-0030/0031 runtime composition; this original no-runtime description is historical. Required ordinary independent review and terminal reconciliation remain pending. |

## Closeout checklist

- [ ] BPR-0287 through BPR-0294 are terminal and every exact final-head replay
      record is filled with command, result, proving commit, and artifact digest.
- [x] BPR-0115 closes RFC-BPR-0006; BPR-0617 closes RFC-BPR-0013. Both
      RFCs are Auto-Verified at the current candidate.
- [ ] Fresh developer, core-only, tests-disabled, focused asset, documentation,
      dependency, metadata/security, runtime, secret-scan, and diff gates pass.
- [ ] An independent `rfc-reviewer` falsifies the complete asset, provenance,
      generator, review, isolation, documentation, and non-runtime claims with
      no unresolved blocker.
- [ ] RFC status, verification ledger, issue board, decisions, artifacts, and
      proving commits agree before the RFC is marked `Auto-Verified`.

## Remaining acceptance and continuation

The coordinator-implementer recipe forbids a nested reviewer, and this assignment
supplies no separately authorized independent report for BPR-0295. The historical
RFC-0030/0031 reviewer covered its own changed surfaces; this audit does not
reinterpret that as complete independent ordinary RFC-0019 acceptance.

Resume this milestone with the current/successor criterion mapping and a separately
authorized independent review. Select the final graph/runtime checks against that
reconciled closeout candidate, then accept only proven criteria. Keep the RFC
Deferred until every non-superseded obligation and required review is satisfied.
No new art verdict is requested: existing v008 human decisions remain accepted
for their exact unchanged assets. No review server or watch was started.

The [handoff](../../rfcs/plans/rfc-bpr-0019-closeout/handoff-r1.json) records
candidate identities, verification, writers, command state and integration scope.
