---
schema: "implement-rfc.card.v1"
id: "BPR-1311"
title: "Verify and close RFC-BPR-0062"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1310"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
closeout_for: ["RFC-BPR-0062"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"observation","detail":"First-run 'Add a repository' opens Settings > Repositories ready for entry (amended AC01); public course first save then ordinary row; populated/filtered rows in workbench tests; owner Accept c46e3ec4 (BPR-1310-home-v003)."}},{"criterion":"AC02","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"command","detail":"application_personal_discovery_public_path at frozen clean export 873bd4b0b: exact account/host destination, invalid input, cancelled server setup returning to draft, stale destination refused."}},{"criterion":"AC03","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"command","detail":"Same course: real SQLite save, duplicate, storage refusal and retry, sibling window, restart, removal."}},{"criterion":"AC04","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"command","detail":"Same course: searches limited to author:/review-requested: viewer, offline and secondary-limit saves kept, review mutation submissions 0."}},{"criterion":"AC05","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"command","detail":"All Add repository entries open the one Repositories page (amended AC05); Pull Request menu Save Repository intent asserted in public course and qt_workbench_window x3."}},{"criterion":"AC06","outcome":"pass","revision":"873bd4b0b1705e98f3f80a94b679017e8e77bcc9","evidence":{"kind":"observation","detail":"qt_settings_window x4, qt_workbench_window x3, qt_home_list x3: Return/Cancel/Escape, polite announcements, long identities, narrow layouts; 126 renders; owner native macOS Accepts c46e3ec4 and 2cda22d2 in session 6226f237. VoiceOver and Windows/Linux not claimed."}}]
---

# Verify and close RFC-BPR-0062

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

RFC base: `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`.

## Closeout checklist

The RFC's 2026-10-08 owner amendment applies to AC01, AC05 and AC06. Home's
first-run button opens Settings › Repositories ready for entry, and Home and
Settings share that page in place of a dialog.

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** In the composed application, a newly signed-in profile adds its first repository from Home without visiting Settings or opening a PR; a returning reviewer can find the same action with populated or filtered rows. (Amended: one Home button opens Settings › Repositories ready for entry.) | Personal-discovery public course: first-run button to page, save, then the ordinary row; workbench rows for populated and filtered queues | Pass as amended; owner native Accept of `BPR-1310-home-v003` | `873bd4b0b` |
| **AC02:** The selected repository-entry method reaches an exact displayed account/host destination; invalid input, cancelled setup and stale sign-in completions cannot create a record under another authority. | Public course: exact destination line, invalid input, cancelled server setup returning to the draft, stale destination | Pass | `873bd4b0b` |
| **AC03:** Saving, duplicate entry and storage refusal produce the defined result states through the real persistence path, with successful records surviving restart and appearing in other windows of the same profile. | Public course: real SQLite save, duplicate, storage refusal and retry, sibling window, restart, removal | Pass | `873bd4b0b` |
| **AC04:** Discovery after a save shows personal results or an accurate empty/offline/limited/unavailable state without undoing the save, opening a review, expanding discovery beyond saved repositories or mutating GitHub. | Public course: searches limited to `author:`/`review-requested:` the viewer, offline and secondary-limit saves kept, zero review mutations | Pass | `873bd4b0b` |
| **AC05:** Home and Settings invoke the same addition flow, and the existing open-PR shortcut retains its direct save behavior without a competing store or contradictory account prerequisite. (Amended: the shared flow is the Repositories page.) | Every Add repository entry opens the same page; public course and workbench rows check the Pull Request menu's Save Repository intent | Pass as amended | `873bd4b0b` |
| **AC06:** The real Home and dialog support keyboard completion and cancel, announced progress/refusal, long identities and narrow layouts at 100–200% scale in the supported appearances; the owner judges each changed visual surface through a fresh short attended session with a frozen build. (Amended: Home and the Repositories page.) | Settings window at four scales, workbench and Home list at three; Return, Cancel and Escape; polite announcements; 126 renders; owner Accepts of both v003 surfaces | Pass on macOS; VoiceOver not observed | `873bd4b0b` |

## Closeout (2026-10-08)

Closed by dispatch `parallel-integrator-r5-001` against RFC base
`f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Product history:

- BPR-1306 to 1309 built the input normalization, durable results, shared
  flow and Home composition (implementation candidate `8e2f855e9`).
- The owner rejected v001 and v002.
- `2f6fe7f1a` added the Repositories page.
- `873bd4b0b` replaced the dialog with the in-place, Accounts-style page and
  reduced first-run Home to one button.

The owner accepted both v003 surfaces in session
`6226f23752342fb0ad75662926641f35` (decisions `c46e3ec4…` and `2cda22d2…`,
recorded on [[BPR-1310]]). Its self-contained frozen build passed 18/18 in a
clean export of `873bd4b0b`, and no source or test file has changed since. The
operator documentation (`docs/development/settings.md`,
`docs/ux/workbench-specification.md`) describes the page and the first-run
button. Its dependency RFC-BPR-0026 is Auto-Verified.

Limits: VoiceOver and Windows/Linux native behaviour are not claimed.
Offscreen, the Settings window renders light in explicit dark modes, which
predates this RFC. RFC-BPR-0062 and its index row are set to Auto-Verified,
and the [ledger](../../verification/RFC-BPR-0062-0065.md) records the
closeout.
