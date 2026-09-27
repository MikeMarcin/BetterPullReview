# Review tour v1: live import

Import-only record for `betterpullreview-review-tour-rfc-pack-v1` (pack id `bpr-review-tour-v1`, authored 2026-09-23), imported on 2026-09-23. The pack (83 files) is retained byte-for-byte in [bpr-review-tour-v1](bpr-review-tour-v1/); `SHA256SUMS` and `scripts/verify_pack.py` define its integrity.

## Source, live inventory, and destinations

- Pre-import `main` HEAD `3e12138ae8c603315b3d55c34872d7a4fc777a71`. The pack's [source baseline](bpr-review-tour-v1/references/SOURCE-BASELINE.json) was generated from a 2026-09-22 snapshot containing RFCs through 0038; the source-drift reconciliation of its named seams against live HEAD is the work of the intake card BPR-0991, not of this record. Source `milestone-plan.json` SHA-256 `9ce484ada9fa72ddd662900ef968c8399aeee3c22c45ef02ab24fde6ea10f834`.
- No live RFC-BPR-0039 to 0043 or destination plan path existed at intake, so the RFC IDs are unchanged. The five [live RFCs](../RFC-BPR-0039-manual-first-review-tour-authoring.md) equal the overlay copies (which already carry the `milestone_plan` metadata) except for two mechanical changes the documentation gate requires: `## Acceptance` is headed `## Acceptance criteria`, and pack-relative `../../../` links point into `assets/bpr-review-tour-v1/`. All five are Proposed. No existing RFC's status or text changed apart from the index rows in `docs/rfcs/README.md`.
- Allocation: `BPR-0991` to `BPR-1016` from [the reservation](../../issues/_reservations/BPR-0991--BPR-1016--72a28a61d8e9.json); `NEXT_ID` is `BPR-1017`. All 26 cards are in `docs/issues/10-backlog/`.
- Live [plan](../plans/bpr-review-tour-v1/milestone-plan.json), [generated view](../plans/bpr-review-tour-v1/MILESTONES.md) and [alias map](../plans/bpr-review-tour-v1/id-map.json) are adopted with all eight authored milestones, outcomes, work order, dependencies, exits and boundaries.
- The pack authors one closeout card for all five RFCs; [BPR-1016](../../issues/20-ready/BPR-1016-close-rfc-0043.md) is titled "Verify and close RFC-BPR-0043" and named `close-rfc-0043` to satisfy the documentation gate's closeout naming, carries `closeout_for` all five and a checklist of the 43 canonical AC rows, each **not run**, with the pre-import HEAD as RFC base.
- The in-flight BPR-0990 working-tree changes (Qt, application, app and tests) were not touched by this import.

## Card transformation

Each card keeps the pack body with aliases replaced by live IDs, gains `metadata` (pack, pack alias, milestone, acceptance requirement IDs from `CARD-INDEX.json`, plan link) and a live-ID title prefix; the "allocate at intake" sentence names its live ID instead. BPR-1016 additionally gains `closeout_for`, the RFC base and the checklist.

## Complete alias mapping

| Pack alias | Live card |
|---|---|
| `PACK-TOUR-01` | `BPR-0991` |
| `PACK-TOUR-02` | `BPR-0992` |
| `PACK-TOUR-03` | `BPR-0993` |
| `PACK-TOUR-04` | `BPR-0994` |
| `PACK-TOUR-05` | `BPR-0995` |
| `PACK-TOUR-06` | `BPR-0996` |
| `PACK-TOUR-07` | `BPR-0997` |
| `PACK-TOUR-08` | `BPR-0998` |
| `PACK-TOUR-09` | `BPR-0999` |
| `PACK-TOUR-10` | `BPR-1000` |
| `PACK-TOUR-11` | `BPR-1001` |
| `PACK-TOUR-12` | `BPR-1002` |
| `PACK-TOUR-13` | `BPR-1003` |
| `PACK-TOUR-14` | `BPR-1004` |
| `PACK-TOUR-15` | `BPR-1005` |
| `PACK-TOUR-16` | `BPR-1006` |
| `PACK-TOUR-17` | `BPR-1007` |
| `PACK-TOUR-18` | `BPR-1008` |
| `PACK-TOUR-19` | `BPR-1009` |
| `PACK-TOUR-20` | `BPR-1010` |
| `PACK-TOUR-21` | `BPR-1011` |
| `PACK-TOUR-22` | `BPR-1012` |
| `PACK-TOUR-23` | `BPR-1013` |
| `PACK-TOUR-24` | `BPR-1014` |
| `PACK-TOUR-25` | `BPR-1015` |
| `PACK-TOUR-26` | `BPR-1016` |

## Plan representation

As in the shell-surfaces import, the plan helper needs live issue IDs, so [import-proposal.json](../plans/bpr-review-tour-v1/import-proposal.json) replaces only the 26 `PACK-TOUR-*` work references (SHA-256 `76f8551c41428c9cce0b3d1fff3e6e84db83d619670f726cd935b9e3c20e7a28`) and was adopted with an empty helper map; `source.json` therefore records `mapping: {}`. The immutable source plan is unchanged.

## Validation and exclusions

The source's `python3 -B scripts/verify_pack.py` passed (5 RFCs, 26 cards, 8 milestones, 43 requirements, 82 checksums, 25 local links, 7 text transactions) and `shasum -a 256 -c SHA256SUMS` reported every file OK. The plan helper checked and adopted the derived plan (coverage complete; semantic coverage unknown); `issue_board.py validate` reports valid, and `scripts/verify_documentation.py` reports `DOCUMENTATION_OK`. `__pycache__` and `.DS_Store` were excluded from the retained copy. No application code was changed, nothing was built or tested, and no GitHub service was contacted. All 43 acceptance rows remain not run.
