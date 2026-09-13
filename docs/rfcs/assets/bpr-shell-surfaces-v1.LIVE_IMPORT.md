# Shell surfaces v1: live import

Import-only record for the pack authored in this checkout on 2026-09-13 from the owner's brief and four screenshots ("this settings menu, home screen, and profile selector [are] ugly as heck and so dang cluttered. The profile select screen coming up every time you close the main window is terrible."). The original 29-file pack is retained byte-for-byte in [bpr-shell-surfaces-v1](bpr-shell-surfaces-v1/); `SHA256SUMS` and `scripts/verify_pack.py` define its integrity. Its design reference is an illustration of palette roles and layout, not runtime evidence.

## Source, live inventory, and destinations

- Source: authored against the live checkout, pre-import `main` HEAD `38f32226351f5cc8e4657d55c3e151d8450c6ce5`. The pack's [source audit](bpr-shell-surfaces-v1/supporting/SOURCE-AUDIT.md) names the composition seams by file and function at that revision; there is no earlier snapshot to reconcile. Original `milestone-plan.json` SHA-256 `ff207f11b6add3b620a8147a6e15ae291e420e08adcb639ab4df5fff3092f442`.
- No live RFC-0032/0033 or destination pack/plan path existed at intake. Their IDs remain `RFC-BPR-0032` and `RFC-BPR-0033`; both [live RFCs](../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) are Proposed and carry only the adoption's `milestone_plan` metadata beyond the overlay text. RFC-BPR-0018, 0024 and 0028 and their historical records are intact. The 13 new cards are in `docs/issues/10-backlog/`, not claimed or finished.
- Allocation: `BPR-0770` to `BPR-0782`, next ID `BPR-0783`, from [the reservation](../../issues/_reservations/BPR-0770--BPR-0782--c28923131dc6.json) committed with this import. No ID was recycled.
- Live [plan](../plans/bpr-shell-surfaces-v1/milestone-plan.json), [generated view](../plans/bpr-shell-surfaces-v1/MILESTONES.md) and [machine-readable alias map](../plans/bpr-shell-surfaces-v1/id-map.json) are adopted with two RFCs, all thirteen work refs, original card dependencies, five original milestone IDs, grouping, outcomes, dependencies, exits and boundaries.
- The two closeout cards, [BPR-0781](../../issues/10-backlog/BPR-0781-close-rfc-0032.md) and [BPR-0782](../../issues/10-backlog/BPR-0782-close-rfc-0033.md), are independent: the RFCs share no dependency on each other. Each checklist lists its RFC's canonical criteria with every row **not run**; their immutable RFC base is the pre-import HEAD above.
- The already-dirty `.gitignore`, `.mask.json` and the unrelated in-flight GitHub read and logging changes in the working tree were not touched by this import.

## Complete alias mapping

| Pack alias | Live card |
|---|---|
| `PACK-SHELL-01` | `BPR-0770` |
| `PACK-SHELL-02` | `BPR-0771` |
| `PACK-SHELL-03` | `BPR-0772` |
| `PACK-SHELL-04` | `BPR-0773` |
| `PACK-SHELL-05` | `BPR-0774` |
| `PACK-SHELL-06` | `BPR-0775` |
| `PACK-SHELL-07` | `BPR-0776` |
| `PACK-SHELL-08` | `BPR-0777` |
| `PACK-SHELL-09` | `BPR-0778` |
| `PACK-SHELL-10` | `BPR-0779` |
| `PACK-SHELL-11` | `BPR-0780` |
| `PACK-SHELL-12` | `BPR-0781` |
| `PACK-SHELL-13` | `BPR-0782` |

## Plan representation adjustment

The installed plan helper's schema requires numeric issue IDs before `adopt` can run, so the pack's thirteen `PACK-SHELL-*` work references cannot be adopted directly. The immutable source plan is unchanged. [import-proposal.json](../plans/bpr-shell-surfaces-v1/import-proposal.json) replaces only those thirteen typed `issue` values with the allocated IDs; its SHA-256 is `6dc5e27a56a76616be335a7d3c059ca61cc266f694861241838ab05806db2f50`. `adopt` used this validated derived source with an empty helper-level map because the mapping was already applied; its [source receipt](../plans/bpr-shell-surfaces-v1/source.json) therefore records `mapping: {}`. The complete mapping is above and in `id-map.json`. The derived plan preserves the authored five milestones and every outcome, work order, dependency, exit and boundary.

## Validation and exclusions

The retained source's `python3 scripts/verify_pack.py` passed: two RFCs, thirteen cards, five milestones, thirteen criteria covered, closeout checklists equal to the RFC criteria; its six negative tests passed; `shasum -a 256 -c SHA256SUMS` covers every archived file. The installed plan helper checked the derived plan as structurally valid, adopted it, and inspected `intake-baseline` with its one obligation and both exits resolved; semantic acceptance remains unknown. The repository documentation gate reports `DOCUMENTATION_OK` after this record was written. No application code was changed, no build or test of the application ran, and no attended verdict exists. All future obligations in the five-milestone plan remain open.
