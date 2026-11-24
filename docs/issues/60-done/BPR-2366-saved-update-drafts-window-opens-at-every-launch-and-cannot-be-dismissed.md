---
schema: "implement-rfc.card.v1"
id: "BPR-2366"
title: "The Saved update drafts window opens at every launch forever: the update recovery store is append-only, Close removes nothing, no reset clears it, and the only way to stop it is to delete a file by hand"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-23, build 92 on macOS: after an in-app update at 08:08 saved one inline draft, the Saved update drafts window opened at every launch for the rest of the day, and nothing in it or in Settings made it stop","related_cards":["BPR-1602"],"related_rfcs":["RFC-BPR-0014"],"reservation":"docs/issues/_reservations/BPR-2366--BPR-2366--fba97f9e01e9.json","acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1aee1358b6df69260aa0e82c1af18b3be0d19a1b","evidence":{"kind":"command","detail":"release_update_recovery discards exact versions, checks byte-for-byte surviving row fields, and rolls back a batch containing an unknown row. application_release_update_public_path exercises cancellation, one confirmed discard, confirmed Discard all, and the next empty launch."}},{"criterion":"AC2","outcome":"pass","revision":"1aee1358b6df69260aa0e82c1af18b3be0d19a1b","evidence":{"kind":"command","detail":"release_update_recovery inserts with the v1 writer query and confirms old schema readability after persisting shown flags. application_release_update_public_path copies selected text, closes without discarding and restarts without a forced dialog. Help and Storage & privacy Troubleshooting reopen the current count."}},{"criterion":"AC3","outcome":"pass","revision":"1aee1358b6df69260aa0e82c1af18b3be0d19a1b","evidence":{"kind":"command","detail":"application_release_update_public_path exercises the updater save worker against a synthetic draft committed to the real private review ledger. Exact active text is omitted and raw editor and indeterminate text are kept. Startup removes an exact redundant copy but retains different body, legacy identity and uncertain copies. The original review binding and draft remain equal after recovery discard."}},{"criterion":"AC4","outcome":"pass","revision":"1aee1358b6df69260aa0e82c1af18b3be0d19a1b","evidence":{"kind":"command","detail":"macOS arm64, Qt 6.12.1 and pinned clang-format 22. The release update, SQLite recovery and private-store, reset, Qt Troubleshooting, architecture and composition source courses passed. verify_documentation.py returned DOCUMENTATION_OK, with 82 documentation tests passing. The tests-disabled app linked and its offscreen self-test returned PUBLIC_PATH_OK."}}]
---

# BPR-2366: The Saved update drafts window opens at every launch forever

Before an in-app update, BetterPullReview saves every profile's unsent
text to `state/v1/update-draft-recovery.sqlite3`, so that the update can't
lose it. At the next launch, a non-empty store opens **Saved update drafts**.

That is the right first launch. Every launch after it is the same, because
nothing ever removes a saved draft.

## Observed

The owner, 2026-11-23, build 92 on macOS. An update at 08:08 saved one
inline draft: a review suggestion on a pull request, with its host, account,
repository, pull request, head and line as the description.

From then on, every launch opened:

```text
Saved update drafts
Private text saved before an update. Copy what you need. Nothing here is
submitted automatically; check GitHub before retrying an uncertain
publication.

<host> · account <n> · repository <n> · pull request <n> · head <oid> · right line <n>
<the draft's text>
                                                                [Close]
```

**Close** shut it until the next launch. The window has no other control,
and the owner found nothing in Settings that would stop it.

## Cause

- **The store only grows.** `release_update_recovery_port`
  (`src/application/include/bpr/application/release_update_recovery.hpp`)
  has `save` and `load` and nothing else. Its comment calls it
  "Append-only recovery storage". The SQLite adapter only `INSERT OR IGNORE`s
  (`src/adapters/sqlite/src/release_update_recovery_store.cpp`, around line
  45).
- **Any saved draft opens the window.** `runtime_ready`
  (`src/app/src/profile_desktop.cpp`, around line 1261) makes
  `release_update_recovery_dialog` whenever `load()` returns a non-empty list.
- **Close removes nothing.** The dialog
  (`src/adapters/qt/src/release_update_recovery_dialog.cpp`) has only a Close
  button wired to `reject`. Nothing records that a draft was seen.
- **No reset clears it.** [[BPR-1602]]'s reset table puts
  `update-draft-recovery` under **Never**, Reset everything included
  (`src/core/src/profile_data_reset.cpp`, around line 36).
- **Every update adds to it.** `desktop_update_drafts`
  (`src/app/src/github_application/application_desktop.cpp`, around line 96)
  saves every inline draft each draft owner holds, and both editors' text.
  If those inline drafts are also kept in the review store and come back
  by themselves after the update (to be confirmed: this card did not trace
  `recovery_snapshot` to its storage), a reviewer with drafts in progress
  gets more saved entries with each update, and sees again text they never
  lost.

The only way out is to quit and delete or rename the file by hand.

## Change

### 1. Let the reviewer say a draft is handled

- Each row in **Saved update drafts** gets **Discard**, which asks once
  ("Discard this saved text? It can't be brought back.") and removes that
  row from the store.
- **Discard all** does the same for every row, with one question.
- **Copy** copies the selected row's text, so nobody has to select it in
  the read-only box.
- **Close** keeps the rows, as today, and says so: "Saved drafts stay until
  you discard them."

### 2. Don't reopen for text that is not lost

- When saving before an update, skip an inline draft that the review store
  will restore by itself after the update. Keep only text with no other
  home (the two editors' unsaved text) and drafts whose publication was
  uncertain (`Indeterminate`).
- At launch, drop a saved inline draft whose identity is again in the review
  store with the same body: the reviewer has it where they left it.

### 3. Offer it without forcing it

- After the first launch that shows a row, later launches don't open the
  window. They show a "Saved update drafts (n)" item in the Help or
  Troubleshooting menu and in Settings › Troubleshooting, which opens it.

### 4. The port

- `release_update_recovery_port` gains `discard(identities)`. It removes
  exactly those rows in one transaction and leaves every other row
  byte-identical.
- The store stays out of [[BPR-1602]]'s resets. Discarding is the
  reviewer's own act, row by row.

### 5. Tests

- SQLite adapter: `discard` removes the named rows, keeps the rest, and
  refuses an unknown identity without removing anything.
- Profile desktop: a store whose rows are all discarded opens no window at
  the next launch; one with rows already shown opens no window and lists
  them in the menu.
- Update save: an inline draft the review store holds is not saved; an
  `Indeterminate` one and editor text are.
- Qt: Discard and Discard all ask, then remove; Close removes nothing.

## Acceptance

- AC1. A reviewer can discard saved update drafts from the window, one at a
  time or all at once, and a store with no rows opens nothing.
- AC2. A saved draft forces the window open at most once; afterwards it is
  reachable from the menu and Settings.
- AC3. An update saves only text that would otherwise be lost, or whose
  publication was uncertain.
- AC4. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step

With a store holding the 2026-11-23 draft: launch, Discard it, quit, launch
again. The window does not open.

## Out of scope

- Whether a whole-profile reset should clear this store. It stays out of
  [[BPR-1602]]'s resets.
- Restoring a saved draft back into the review as a draft.

## Implementation and verification

The recovery store retains its original schema for updater rollback.
Presentation is tracked by exact recovery row in a separate private store.
Discard acts on recovery copies and preserves the original review ledger.
The upgrade omits an active inline copy when its private ledger incarnation,
draft identity, bound head and body match. Legacy inline records without
that identity and records of uncertain publication remain available.

The synthetic macOS arm64 course exercises the update worker and starts the
composed desktop on a disposable profile. The behavior and layout were
reviewed with the frontend-design skill and an offscreen Qt 6.12.1 render.
The owner's original 2026-11-23 draft remains an unexercised owner observation.
