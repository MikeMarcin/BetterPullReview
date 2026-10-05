# Conventional Comments pack v1: live import

Import-only record for `betterpullreview-conventional-comments-rfc-pack-v1` (authored 2026-09-23), imported on 2026-09-23. The pack (62 files) is retained byte-for-byte in [betterpullreview-conventional-comments-rfc-pack-v1](betterpullreview-conventional-comments-rfc-pack-v1/); `SHA256SUMS` and `scripts/verify_pack.py` define its integrity. It is additive to the review-tour pack and its pr-tour CLI extension and does not touch any of their RFCs, cards or plans.

## Source, predecessors and destinations

- Pre-import `main` HEAD `b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`. Source `milestone-plan.json` SHA-256 `c7ea0fc205efacd7206be21d2b6f183128a5c0dbdec173d66d406d25f9e45c9b`. The extracted download matched the delivered `betterpullreview-conventional-comments-rfc-pack-v1.zip` file for file.
- Predecessors: the pack's [PREDECESSOR-PACKS.json](betterpullreview-conventional-comments-rfc-pack-v1/references/PREDECESSOR-PACKS.json) could not verify its predecessors' bytes when authored. At import both archives were present in the downloads folder, and their SHA-256 equal the expected digests: `betterpullreview-review-tour-rfc-pack-v1.zip` `b4f45e7e…78d6` and `betterpullreview-pr-tour-cli-extension-rfc-pack-v1.zip` `3cdc6a07…6782`. Their live import records ([review tour](bpr-review-tour-v1.LIVE_IMPORT.md), [pr-tour CLI extension](bpr-pr-tour-cli-extension-v1.LIVE_IMPORT.md)) confirm RFC-BPR-0039 to 0043 and 0044 to 0046 were not remapped, so this pack's references to them need no rewriting.
- Predecessor seams observed at import: all 46 predecessor cards (BPR-0991 to BPR-1016, BPR-1052 to BPR-1071) are in Backlog and `src/` contains no tour, headless tour CLI or Conventional Comments implementation. The inherited seams this pack relies on (tour body segments, shared tour draft and owner, headless runtime, tour hooks) do not exist yet. Intake card [BPR-1075](../../issues/60-done/BPR-1075-import-after-both-tour-packs-and-reconcile-the-live-contracts.md) owns recording each exact seam, and the pack's own guidance applies: independent semantics and design work may proceed, and dependent integration waits for the real services rather than building a second engine.
- Intake reconciliation, 2026-10-05: the seams above have since been built. BPR-1075 verified all three delivered archives byte for byte against their retained copies and the predecessor sidecar digests, and checked the map, plan, cards and source drift again. It recorded the exact tour body-segment, shared-draft, headless-owner, hook and promotion/placement owners in the [execution ledger](../../verification/RFC-BPR-0047-0051.md) and its [audit](../../verification/RFC-BPR-0047-0051.intake-audit.json). The observation above describes the import date and is kept as written.
- No live RFC-BPR-0047 to 0051 or destination plan path existed at intake, so the RFC IDs are unchanged. The five [live RFCs](../RFC-BPR-0047-conventional-comment-semantics.md) equal the overlay copies except that `## Acceptance` is headed `## Acceptance criteria` for the documentation gate. They contain no pack-relative links. Their `proposed_id_requires_live_reconciliation` metadata is left as authored; this record is that reconciliation. All five are Proposed. No existing RFC changed apart from the index rows and paragraph in `docs/rfcs/README.md`.
- Allocation: `BPR-1075` to `BPR-1101` from [the reservation](../../issues/_reservations/BPR-1075--BPR-1101--243d1ef0e225.json); `NEXT_ID` is `BPR-1102`. `BPR-1072` to `BPR-1074` were already reserved for the Windows station and were skipped. All 27 cards are in `docs/issues/10-backlog/`.
- Live [plan](../plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json), [generated view](../plans/betterpullreview-conventional-comments-rfc-pack-v1/MILESTONES.md) and [alias map](../plans/betterpullreview-conventional-comments-rfc-pack-v1/id-map.json) are adopted with all eight authored milestones, outcomes, work order, dependencies, exits and boundaries. The plan keeps its authored id `betterpullreview-conventional-comments-rfc-pack-v1`, which the RFC and card metadata already name.

## Card transformation

Each card keeps the pack body with aliases replaced by live IDs in `id`, `depends_on`, `closeout_for`, title and text. Its `metadata` keeps pack, milestone, acceptance requirement IDs and plan link, drops `alias_requires_live_allocation`, and gains `pack_alias`; a pack-alias line follows the title. The pack-relative `verification/PRODUCT-VERIFICATION.md` reference becomes a link into the retained pack. Filenames are the live ID plus the slugged pack title.

The five closeout cards (one per RFC, as authored) are retitled "Verify and close RFC-BPR-00NN" and named `BPR-NNNN-close-rfc-00NN.md` for the documentation gate's closeout naming, keeping the pack title in the body. Each gains the pre-import HEAD as RFC base and a closeout checklist: the RFC's canonical acceptance rows plus the card's cross-cutting requirements (R43 to R48 from `verification/REQUIREMENTS.json`), each **not run**. R42, R44 and R45 are owned by verification/closeout cards as authored and appear where the pack lists them.

## Complete alias mapping

| Pack alias | Live card |
|---|---|
| `PACK-CC-01` | [BPR-1075](../../issues/60-done/BPR-1075-import-after-both-tour-packs-and-reconcile-the-live-contracts.md) |
| `PACK-CC-02` | [BPR-1076](../../issues/60-done/BPR-1076-implement-the-complete-catalog-and-bounded-lossless-prefix-recognizer.md) |
| `PACK-CC-03` | [BPR-1077](../../issues/60-done/BPR-1077-implement-revisioned-header-edits-and-root-post-classification.md) |
| `PACK-CC-04` | [BPR-1078](../../issues/60-done/BPR-1078-implement-conversation-partition-and-explicit-resolution-coverage.md) |
| `PACK-CC-05` | [BPR-1079](../../issues/60-done/BPR-1079-develop-compact-authoring-counter-and-resolution-ux-candidates.md) |
| `PACK-CC-06` | [BPR-1080](../../issues/60-done/BPR-1080-develop-all-twelve-reusable-label-icon-candidates.md) |
| `PACK-CC-07` | [BPR-1081](../../issues/60-done/BPR-1081-record-the-selected-ux-and-icon-delivery-contract.md) |
| `PACK-CC-08` | [BPR-1082](../../issues/60-done/BPR-1082-compose-convention-edits-through-shared-durable-draft-services.md) |
| `PACK-CC-09` | [BPR-1083](../../issues/10-backlog/BPR-1083-implement-selected-authoring-controls-and-shared-label-visuals.md) |
| `PACK-CC-10` | [BPR-1084](../../issues/10-backlog/BPR-1084-preserve-and-compose-native-suggested-changes.md) |
| `PACK-CC-11` | [BPR-1085](../../issues/10-backlog/BPR-1085-integrate-reviewer-scoped-preflight-with-native-review-decisions.md) |
| `PACK-CC-12` | [BPR-1086](../../issues/10-backlog/BPR-1086-integrate-private-tracking-promotion-and-ordinary-tour-comments.md) |
| `PACK-CC-13` | [BPR-1087](../../issues/10-backlog/BPR-1087-project-accepted-label-buckets-and-bounded-counter-deltas.md) |
| `PACK-CC-14` | [BPR-1088](../../issues/10-backlog/BPR-1088-persist-scoped-label-column-and-authoring-preferences.md) |
| `PACK-CC-15` | [BPR-1089](../../issues/10-backlog/BPR-1089-deliver-resolution-aware-overview-columns-and-exact-drill-down.md) |
| `PACK-CC-16` | [BPR-1090](../../issues/10-backlog/BPR-1090-author-canonical-default-convention-guidance-and-hook-ownership-map.md) |
| `PACK-CC-17` | [BPR-1091](../../issues/10-backlog/BPR-1091-wire-optional-convention-aware-hooks-through-the-existing-workshop.md) |
| `PACK-CC-18` | [BPR-1092](../../issues/10-backlog/BPR-1092-expose-deterministic-convention-operations-through-the-shared-headless-cli.md) |
| `PACK-CC-19` | [BPR-1093](../../issues/10-backlog/BPR-1093-verify-syntax-ownership-migration-and-mutation-adversaries.md) |
| `PACK-CC-20` | [BPR-1094](../../issues/10-backlog/BPR-1094-measure-counter-costs-and-verify-the-actual-compact-native-ui.md) |
| `PACK-CC-21` | [BPR-1095](../../issues/10-backlog/BPR-1095-verify-native-github-interoperability-on-an-authorized-safe-target.md) |
| `PACK-CC-22` | [BPR-1096](../../issues/10-backlog/BPR-1096-reconcile-documentation-simplify-and-independently-review-the-feature.md) |
| `PACK-CC-23` | [BPR-1097](../../issues/10-backlog/BPR-1097-close-rfc-0047.md) |
| `PACK-CC-24` | [BPR-1098](../../issues/10-backlog/BPR-1098-close-rfc-0048.md) |
| `PACK-CC-25` | [BPR-1099](../../issues/10-backlog/BPR-1099-close-rfc-0049.md) |
| `PACK-CC-26` | [BPR-1100](../../issues/10-backlog/BPR-1100-close-rfc-0050.md) |
| `PACK-CC-27` | [BPR-1101](../../issues/10-backlog/BPR-1101-close-rfc-0051.md) |

## Plan representation

As in the earlier tour imports, the plan helper needs live issue IDs, so [import-proposal.json](../plans/betterpullreview-conventional-comments-rfc-pack-v1/import-proposal.json) replaces only the 27 `PACK-CC-*` work references (SHA-256 `c357e4a53fd4d557c25ab1ab01b7add2dd4c6914b19bee71212333d58db1764f`) and was adopted with an empty helper map; `source.json` therefore records `mapping: {}`. Adoption added only `unplanned_work: []`. The immutable source plan is unchanged.

## Validation and exclusions

The source's `python3 -B scripts/verify_pack.py` passed (5 RFCs, 27 cards, 8 milestones, 48 requirements, 12 labels; 32,768 normalized counter projections) on both the download and the retained copy, its 19 unit tests passed, and `shasum -a 256 -c SHA256SUMS` reported every file OK. The plan helper checked and adopted the derived plan (coverage complete; semantic coverage unknown). No `.DS_Store` or `__pycache__` was present. No application code was changed, nothing was built or tested, no hook or skill content was installed, and no GitHub service was contacted. All acceptance rows remain not run.
