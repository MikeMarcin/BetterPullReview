---
schema: "implement-rfc.card.v1"
id: "BPR-1697"
title: "Offer the commit carrying a gone hidden commit's change after a force push"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1695","BPR-1686"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1695--BPR-1697--03615e35bed9.json","milestone":"RFC-BPR-0089 M3","frontend_design":"frontend-design:frontend-design loaded before the rematch offers in the chip and the Commits tab were drawn","related_cards":["BPR-1695","BPR-1696"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|application_commit_filter_model|qt_commit_filter_view_scale_(100|200))$'","detail":"Fixture, shipped composition, offscreen: commit-filter-split-v1 in the seed phase, whose refresh serves C3r, C3 reworded on C2 with C3's own tree. gen/item_000.txt (only C2 changes it) is marked read; Hide C3; Hide files these commits changed on C1. Refresh: the chip names the missing commit ('Hidden commit 968f892 (Follow-ups) is no longer in the pull request'), and once the new head's trees are read offers 'cfaecb6 (Follow-ups) makes the same change as 968f892.' with Hide cfaecb6; the Commits tab offers it too. Nothing is applied by itself: C3r is shown. The file filter persists and gen/item_000.txt is still read under the new head. Choosing the chip's offer hides C3r in C3's place (2 of 3 commits shown) and the gone commit is forgotten. No GitHub mutation (AC05). Before the trees are read only a headline and author match is possible, offered and labelled a guess. Negative control: without recording the hidden commit's changes the course fails waiting for the same-change offer. Model and Qt: application_commit_filter_model (C4 amended with a new headline: nothing offered until the trees are read, then the same change; a commit not offered is refused) and qt_commit_filter_view (the chip offers the first match and points at the rest, the tab lists every offer, a guess is labelled, nothing dispatches until chosen). macOS."}},{"criterion":"AC2","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix rfc0089-m3-push1 --force-push --cleanup","detail":"Live, MikeMarcin/BPR-Test #115 (closed, both branches deleted), signed in through gh as MikeMarcin, sparse review, the owner's shape: the course marked a file only C2 changes read and hid C3, then waited while the script force-pushed C3 reworded on C2 with C3's tree. After Refresh the Commits tab listed C1, C2, C3r; the chip named C3 as gone and offered exactly C3r as the same change; C3r stayed shown until the course chose the chip's offer; the receipt was still read; after the choice C3r was hidden and nothing was gone. GitHub's Viewed state was unchanged for all 325 files (AC05). X-RateLimit-Remaining 4868 before, 4699 after. Run on this tree before two clang-tidy renames and one string build in the course binary, none of which changes behavior."}}]
---

# BPR-1697: Offer the commit carrying a gone hidden commit's change after a force push

RFC-BPR-0089 M3, "Force push and rebase" and RFC AC06: after a force push the
chip names the hidden commits that are gone and offers, without applying, the
commits in the new history that carry the same change; a match on headline and
author alone is offered and labelled a guess. M1 named the gone commit. The
`frontend-design:frontend-design` skill was loaded before the offers were
drawn.

## Change

- **Host.** The chain's tree diffs keep each side's blob
  (`commit_filter_touched_path::LeftBlob`, `RightBlob`), from the review's
  cache or a sparse review's chain store.
- **Model.** While the trees are read the model records what each hidden
  commit changes, and its author; after a push its projection carries the
  core's `rematch_offers` (`commit_filter_rematch_row`), and `rematch`
  applies only an offer the core makes now.
- **Qt.** The chip names the gone commit, says which commit makes the same
  change ("cfaecb6 (Follow-ups) makes the same change as 968f892.") or only
  shares its headline and author ("…, so it may be the same commit."), and
  offers the first as "Hide cfaecb6"; with more, it says the Commits tab lists
  them, and the tab offers each. Nothing changes until the reviewer chooses
  one.
- **Composition.** The window's rematch gesture reaches the model; file
  filters are paths and persist; identical-material receipts persist.
- **Fixture.** `commit-filter-split-v1` serves C3 reworded as its second head.
- **Live course.** `--force-push` and `--course force-push`.

## Acceptance

- **AC1.** RFC AC06 on the fixture through the shipped composition.
- **AC2.** RFC AC06 live on BPR-Test.
