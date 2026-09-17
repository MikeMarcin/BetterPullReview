# Shell surfaces v1: live import

Import-only record for the pack authored in this checkout on 2026-09-13 from the owner's brief and four screenshots ("this settings menu, home screen, and profile selector [are] ugly as heck and so dang cluttered. The profile select screen coming up every time you close the main window is terrible."). The pack (36 files at revision 3) is retained byte-for-byte in [bpr-shell-surfaces-v1](bpr-shell-surfaces-v1/); `SHA256SUMS` and `scripts/verify_pack.py` define its integrity. Its design reference is an illustration of palette roles and layout, not runtime evidence.

## Source, live inventory, and destinations

- Source: authored against the live checkout, pre-import `main` HEAD `38f32226351f5cc8e4657d55c3e151d8450c6ce5`. The pack's [source audit](bpr-shell-surfaces-v1/supporting/SOURCE-AUDIT.md) names the composition seams by file and function at that revision; there is no earlier snapshot to reconcile. Revision 3 `milestone-plan.json` SHA-256 `5306f2d5d2c3096148851e43c309c7a9244f0ed520b82d9f34178d74a822315b`.
- No live RFC-0032/0033/0034 or destination pack/plan path existed at intake. Their IDs remain `RFC-BPR-0032`, `RFC-BPR-0033` and `RFC-BPR-0034`; all three [live RFCs](../RFC-BPR-0032-calm-home-and-accounts-surfaces.md) are Proposed and carry only the adoption's `milestone_plan` metadata beyond the overlay text. RFC-BPR-0018, 0024 and 0028 and their historical records are intact. The 17 cards are in `docs/issues/10-backlog/`, not claimed or finished.
- Allocation: `BPR-0770` to `BPR-0782` from [the first reservation](../../issues/_reservations/BPR-0770--BPR-0782--c28923131dc6.json) `BPR-0783` from [the revision-2 reservation](../../issues/_reservations/BPR-0783--BPR-0783--8eb2b8816cfa.json), and `BPR-0789` to `BPR-0791` from [the revision-3 reservation](../../issues/_reservations/BPR-0789--BPR-0791--9065278f98c2.json); `BPR-0784` to `BPR-0788` were allocated by other work between the two and are not this pack's. No ID was recycled.
- Live [plan](../plans/bpr-shell-surfaces-v1/milestone-plan.json), [generated view](../plans/bpr-shell-surfaces-v1/MILESTONES.md) and [machine-readable alias map](../plans/bpr-shell-surfaces-v1/id-map.json) are adopted with three RFCs, all seventeen work refs, original card dependencies, six original milestone IDs, grouping, outcomes, dependencies, exits and boundaries.
- The three closeout cards, [BPR-0781](../../issues/60-done/BPR-0781-close-rfc-0032.md), [BPR-0782](../../issues/60-done/BPR-0782-close-rfc-0033.md) and [BPR-0791](../../issues/60-done/BPR-0791-close-rfc-0034.md), are independent: the RFCs share no dependency on each other. Each checklist lists its RFC's canonical criteria with every row **not run**; their immutable RFC base is the pre-import HEAD above.
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
| `PACK-SHELL-14` | `BPR-0783` |
| `PACK-SHELL-15` | `BPR-0789` |
| `PACK-SHELL-16` | `BPR-0790` |
| `PACK-SHELL-17` | `BPR-0791` |

## Plan representation adjustment

The installed plan helper's schema requires numeric issue IDs before `adopt` can run, so the pack's seventeen `PACK-SHELL-*` work references cannot be adopted directly. The immutable source plan is unchanged. [import-proposal.json](../plans/bpr-shell-surfaces-v1/import-proposal.json) replaces only those seventeen typed `issue` values with the allocated IDs; its SHA-256 is `5da7a1ef2ebe5d4defb36d9d55e9c89ec87e2b9b0388fa14c50405ed562d5ffd`. `adopt` used this validated derived source with an empty helper-level map because the mapping was already applied; its [source receipt](../plans/bpr-shell-surfaces-v1/source.json) therefore records `mapping: {}`. The complete mapping is above and in `id-map.json`. The derived plan preserves the authored six milestones and every outcome, work order, dependency, exit and boundary.

## Validation and exclusions

The retained source's `python3 scripts/verify_pack.py` passed: three RFCs, seventeen cards, six milestones, nineteen criteria covered, closeout checklists equal to the RFC criteria; its six negative tests passed; `shasum -a 256 -c SHA256SUMS` covers every archived file. The installed plan helper checked the derived plan as structurally valid, adopted it, and inspected `intake-baseline` with its one obligation and both exits resolved; semantic acceptance remains unknown. The repository documentation gate reports `DOCUMENTATION_OK` after this record was written. No application code was changed, no build or test of the application ran, and no attended verdict exists. All future obligations in the five-milestone plan remain open.

## Revision 2, 2026-09-13: Home replaced after owner review

The owner accepted the Settings and Profiles designs and rejected the first Home
("the home redesign is not good"; the grouping "all runs together"). Home was
redesigned through nine rendered boards, now `bpr-shell-surfaces-v1/design/home-v2.html`,
and the owner accepted the set. In this revision: RFC-BPR-0032's Home section, its
architecture note and criteria A5 to A8 were rewritten (overlay and live copy alike;
A-count unchanged at eight); cards `PACK-SHELL-06` and `PACK-SHELL-07` were rewritten
and `PACK-SHELL-14` (Group, State headers, account chips and Filter) was added to the
`home-page` milestone with `BPR-0783` allocated for it; cards 10 and 11 now depend on
it; the live plan, `import-proposal.json`, `id-map.json`, `source.json` and
`MILESTONES.md` were regenerated from the same source, the plan re-checked as
structurally valid with references resolved, and the two closeout checklists
regenerated from the live RFC bullets. The owner's direction to place Group, State
headers, Filter and refresh in the title strip while Home is signed in is recorded in
the RFC and the source audit as an amendment to RFC-BPR-0028 AC-06; the rule holds
while signed out. RFC-BPR-0033 and the Settings design are unchanged. No application
code was changed.

## Revision 3, 2026-09-13: Appearance added after owner review

The owner asked for a better Appearance page, reviewed it through several rounds
(six tiles for six modes, real palettes in the tiles, info-glyph rows, a sideways theme
strip, a labelled preview covering every control) and, on learning that the store keeps
one customised palette that moves between themes, asked for that behaviour to change.
This revision adds `RFC-BPR-0034` (Appearance page and per-theme colour customisation),
cards `PACK-SHELL-15` to `PACK-SHELL-17` (`BPR-0789` to `BPR-0791`) and an `appearance`
milestone; cards 01, 03, 10 and 11 gained its references and criteria; RFC-BPR-0032's
shared row language records the info-glyph amendment (overlay and live copy alike);
`design/appearance-v2.html` was added; the live plan, proposal, ID map, receipt and
`MILESTONES.md` were regenerated and re-checked as structurally valid with references
resolved; the three closeout checklists were regenerated from the live RFC bullets. The
Home and Profiles designs are unchanged. No application code was changed.
