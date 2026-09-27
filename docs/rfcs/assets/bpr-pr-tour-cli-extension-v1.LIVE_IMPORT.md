# pr-tour CLI extension v1: live import

Import-only record for `betterpullreview-pr-tour-cli-extension-rfc-pack-v1` (pack id `bpr-pr-tour-cli-extension-v1`, authored 2026-09-23), imported on 2026-09-23. The pack (63 files) is retained byte-for-byte in [bpr-pr-tour-cli-extension-v1](bpr-pr-tour-cli-extension-v1/); `SHA256SUMS` and `scripts/verify_pack.py` define its integrity. It is additive to the review-tour pack and does not re-import, clone or reset any of that pack's RFCs, cards or plan.

## Source, parent and destinations

- Pre-import `main` HEAD `2928e90ea38cb58dce57f7ee32aa7b817ed4e876`. The pack's [source baseline](bpr-pr-tour-cli-extension-v1/references/SOURCE-BASELINE.json) was generated from a 2026-09-22 snapshot that predates the parent import (`live_import_mapping_observed: false`). Source `milestone-plan.json` SHA-256 `4ace78bb6b324c99fe7321c26bcacfece377a0ecf0b2bb478240f6bc5d702070`.
- Parent: the baseline's `bpr-review-tour-v1` manifest SHA-256 `acbaa942…fb20` equals the retained [parent pack](bpr-review-tour-v1/PACK-MANIFEST.json), imported by `684858ef` under [its record](bpr-review-tour-v1.LIVE_IMPORT.md). The parent RFC IDs were not remapped, so the extension's references to RFC-BPR-0039 to 0043 need no rewriting. Parent RFCs and plan are unchanged since that import; this import did not touch them.
- Predecessor seams observed at import: all 26 parent cards (BPR-0991 to BPR-1016) are in Backlog and `src/` contains no review-tour implementation. The seams the extension reuses (0039 model/store, 0040 import, 0041/0042 publication and maintenance, 0043 hooks) therefore do not exist yet. They remain the parent cards' work; intake card [BPR-1052](../../issues/30-in-progress/BPR-1052-import-only-the-extension-and-reconcile-live-predecessor-mappings.md) owns reconciling each exact seam before extension implementation, and no extension card duplicates them.
- No live RFC-BPR-0044 to 0046 or destination plan path existed at intake, so the RFC IDs are unchanged. The three [live RFCs](../RFC-BPR-0044-headless-tour-runtime-and-terminal-cli.md) equal the overlay copies (which already carry the `milestone_plan` metadata) except for two mechanical changes: `## Acceptance` is headed `## Acceptance criteria` for the documentation gate, and pack-relative `../../../` links point into `assets/bpr-pr-tour-cli-extension-v1/` as the pack's integration map directs. All three are Proposed. No existing RFC's status or text changed apart from the index rows and paragraph in `docs/rfcs/README.md`.
- Allocation: `BPR-1052` to `BPR-1071` from [the reservation](../../issues/_reservations/BPR-1052--BPR-1071--079ef8f97886.json); `NEXT_ID` is `BPR-1072`. All 20 cards are in `docs/issues/10-backlog/`.
- Live [plan](../plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json), [generated view](../plans/bpr-pr-tour-cli-extension-v1/MILESTONES.md) and [alias map](../plans/bpr-pr-tour-cli-extension-v1/id-map.json) are adopted with all six authored milestones, outcomes, work order, dependencies, exits and boundaries.
- The pack authors one closeout card for all three RFCs; [BPR-1071](../../issues/10-backlog/BPR-1071-close-rfc-0046.md) is titled "Verify and close RFC-BPR-0046" and named `close-rfc-0046` for the documentation gate's closeout naming. It carries `closeout_for` all three, the pre-import HEAD as RFC base and a checklist of the 32 canonical AC rows, each **not run**.
- The candidate [pr-tour skill](bpr-pr-tour-cli-extension-v1/skill-source/pr-tour/SKILL.md) is retained only as pack provenance. It was not installed, and no BPR-local Claude or Codex copy was made; canonical shared-skill distribution through MASK is BPR-1067's work.

## Card transformation

Each card keeps the pack body with aliases replaced by live IDs. It gains `metadata` (pack, pack alias, milestone, acceptance requirement IDs from `CARD-INDEX.json`, plan link), a live-ID title prefix and a pack-alias line. The "pack-local alias, not an allocated live BPR issue" sentence names its live ID instead. BPR-1071 also gains `closeout_for`, the RFC base and the checklist.

## Complete alias mapping

| Pack alias | Live card |
|---|---|
| `PACK-PRTOUR-01` | `BPR-1052` |
| `PACK-PRTOUR-02` | `BPR-1053` |
| `PACK-PRTOUR-03` | `BPR-1054` |
| `PACK-PRTOUR-04` | `BPR-1055` |
| `PACK-PRTOUR-05` | `BPR-1056` |
| `PACK-PRTOUR-06` | `BPR-1057` |
| `PACK-PRTOUR-07` | `BPR-1058` |
| `PACK-PRTOUR-08` | `BPR-1059` |
| `PACK-PRTOUR-09` | `BPR-1060` |
| `PACK-PRTOUR-10` | `BPR-1061` |
| `PACK-PRTOUR-11` | `BPR-1062` |
| `PACK-PRTOUR-12` | `BPR-1063` |
| `PACK-PRTOUR-13` | `BPR-1064` |
| `PACK-PRTOUR-14` | `BPR-1065` |
| `PACK-PRTOUR-15` | `BPR-1066` |
| `PACK-PRTOUR-16` | `BPR-1067` |
| `PACK-PRTOUR-17` | `BPR-1068` |
| `PACK-PRTOUR-18` | `BPR-1069` |
| `PACK-PRTOUR-19` | `BPR-1070` |
| `PACK-PRTOUR-20` | `BPR-1071` |

## Plan representation

As in the review-tour import, the plan helper needs live issue IDs, so [import-proposal.json](../plans/bpr-pr-tour-cli-extension-v1/import-proposal.json) replaces only the 20 `PACK-PRTOUR-*` work references (SHA-256 `31ea5b2fe985811115386d4b2310f930ec35b7bd6cf5ccb77db94bce648068b9`) and was adopted with an empty helper map; `source.json` therefore records `mapping: {}`. The immutable source plan is unchanged.

## Validation and exclusions

The source's `python3 -B scripts/verify_pack.py` passed (3 RFCs, 20 cards, 6 milestones, 32 requirements, 62 checked payload files), both on the download and on the retained copy, and `shasum -a 256 -c SHA256SUMS` reported every file OK. The plan helper checked and adopted the derived plan (coverage complete; semantic coverage unknown). `.DS_Store` and `__pycache__` were excluded from the retained copy (none were present). No application code was changed, nothing was built or tested, no skill was installed, and no GitHub service was contacted. All 32 acceptance rows remain not run.
