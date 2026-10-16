---
schema: "implement-rfc.card.v1"
id: "BPR-1536"
title: "Option-letter window and menu keys step aside in text input on macOS (Option-R types ®)"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"owner request 2026-10-15 (relayed by the coordinator)","owner_note":"Fix: Option-R (Compose review) in a comment box opens Compose review instead of typing ®. Make it step aside in every text-input widget with the BPR-1531 typing-widget rule, shared, and audit every other Option shortcut in the window and menu bar.","governing_rfc":"RFC-BPR-0052","related_rfcs":["RFC-BPR-0074","RFC-BPR-0028"],"related_cards":["BPR-1129","BPR-1295","BPR-1503","BPR-1531"],"reservation":"docs/issues/_reservations/BPR-1536--BPR-1536--f4c3a47480ed.json","branch_base":"bpr-alt-a-accept (BPR-1531 candidate a51990fd7); shares its takes_typed_text rule and key claim","merge_after":"BPR-1531"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c8af0248007a6b66a44da1ab508dfcb57fefaafe","evidence":{"kind":"command","detail":"qt_option_keys_typing: 18 Option+character bindings, each enabled, fire nothing from QLineEdit, QPlainTextEdit, QTextEdit, editable QComboBox or suggestion_editor; each widget receives its US-layout character; a widget-scoped key yields to a field inside its scope; a reader made editable under focus types ®. Pre-change baseline fires ⌥R from every editor kind; without the claim the Files drawer key fires."}},{"criterion":"AC2","outcome":"pass","revision":"c8af0248007a6b66a44da1ab508dfcb57fefaafe","evidence":{"kind":"command","detail":"Same course: menu bar holds no Option+character key while typing and all after; the Comment tooltip keeps ⌥C; Option-R from a button opens Compose review. Controls without lifting and without the ImEnabled refresh fail."}},{"criterion":"AC3","outcome":"pass","revision":"c8af0248007a6b66a44da1ab508dfcb57fefaafe","evidence":{"kind":"command","detail":"Audit of 18 bindings recorded on the card; Option-Down and Command-Option-0 keep their bindings; application_source_find and application_source_pr_find pass (⌥W/⌥L). 98 related tests, format, tidy and documentation gates pass."}},{"criterion":"AC4","outcome":"pending","revision":"c8af0248007a6b66a44da1ab508dfcb57fefaafe","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 6b0c2511ffb1e73c98b891f2543a9094, candidate BPR-1536-option-keys-v001, fingerprint 883a941bfeac305f57f5d2ed7ba4b171c1869ae518ae2c773adbb6e5e1325fd6, cursor 4; frozen root bpr1536-option-keys-v001.ld3sbmrn."}}]
---

# BPR-1536: Option-letter window and menu keys step aside in text input on macOS (Option-R types ®)

While probing [[BPR-1531]], Option-R pressed in a comment editor inside the
window opened Compose review instead of typing `®`. The owner asked on
2026-10-15 to fix it: Option-R steps aside in every widget that takes typed
text, using the same typing-widget rule [[BPR-1531]] built for Option-A, and
every other Option shortcut in the window and the menu bar is audited the
same way and fixed if it steals typing.

Governing design: [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
assigned Option-R and Option-C; [RFC-BPR-0074](../../rfcs/RFC-BPR-0074-header-source-navigation.md)
states the rule this card applies to all of them: "Editable fields and
composers retain their native Option/Alt text input". On macOS Option with a
character key types a character (`®`, `ç`, `å`, `†`, `¡`) or starts a dead
key (`˜` before `n`, `´`, `¨`, `ˆ`); the Cocoa plugin offers such a key to
Qt's shortcut map before the text input system, and AppKit's native menu bar
takes a key equivalent before Qt sees the key at all ([[BPR-1295]]).

Lands after [[BPR-1531]]: the branch starts from its candidate and shares its
`takes_typed_text` rule and key claim, so it can merge only once [[BPR-1531]]
is accepted and merged (recorded as `merge_after`, not `depends_on`, because
the engineering does not wait for that verdict). If [[BPR-1531]] is rejected,
this branch is rebased onto its replacement.

## Acceptance

- AC1: With keyboard focus in any widget that takes typed text in the window
  (editable line, multi-line editor, editable combo box, Review Cycle
  suggestion editor), every window, button and menu-bar Option+character
  shortcut is offered to the editor first on macOS: none fires, and the
  editor receives the character. This holds for each Option+character
  binding the audit lists, with each enabled. A check against the
  pre-change window fails for Option-R.
- AC2: Outside text input the same keys still work: Option-R opens Compose
  review from the code or a button. Menu-bar items keep their keys in the
  menu, and while a text widget has focus the native menu bar does not carry
  an Option+character key equivalent, restored when focus leaves; tooltips
  keep naming the key throughout.
- AC3: The audit lists every Option binding in the window and the menu bar
  with its result; Option with a non-character key (arrows, Page Up/Down,
  Return) and Command-Option keys are unchanged, and the find field's
  Option-W and Option-L still toggle Whole Word and Find in Selection.
- AC4 (human): The owner accepts the frozen candidate: "⌥R types ® in comment
  boxes and still opens Compose review elsewhere".

## Implementation

Qt presents: `workbench_window` only, no core, host, provider or storage
change. Windows and Linux keep Alt+letter as command keys in editors (text
input there is AltGr, which [[BPR-1336]] already yields); the rule is macOS
only. Never drive or capture the owner's desktop; the owner checks the frozen
build themselves.

## Checkpoint — candidate v001, 2026-10-15

Product commit `53f9a260f` on branch `bpr-1536-option-keys`, which starts
from [[BPR-1531]]'s candidate `a51990fd7` and merges main; the frozen and
reviewed candidate is the merge `c8af0248007a6b66a44da1ab508dfcb57fefaafe`
(no source change beyond `53f9a260f`). Worktree
`/Users/jaeger/.codex/worktrees/bpr-alt-a-accept/BetterPullReview`, now on
this branch (`bpr-alt-a-accept` is unchanged). Not merged to main until the
owner accepts this card and [[BPR-1531]]. Paths, commands and identities are
in [the checkpoint](../_checkpoints/BPR-1536/handoff.json).

Design. Shared with [[BPR-1531]], not duplicated: `typed_text_key_claim`
now claims, besides Accept's Option-A, every Option(+Shift)+character key
(`option_character_key`: Option or Option-Shift with a key below
`Qt::Key_Escape`) in `ShortcutOverride` for a widget in the window that
`takes_typed_text`, on macOS (`option_types_text`). That covers window
actions, button shortcuts and widget-scoped shortcuts, whose keys the Cocoa
plugin offers to Qt's shortcut map before the text system. The native menu
bar is different: AppKit fires a menu item's key equivalent before Qt sees
the key, with no `ShortcutOverride` ([[BPR-1295]]'s native course). So
`update_typing_keys` lifts the menu bar's Option+character keys while a text
widget has focus and restores them after, and `presented_key` keeps the
Compose review and Comment tooltips naming the key meanwhile. A widget that
becomes editable under focus (the Review Cycle reader turning into the
suggestion editor) triggers an `ImEnabled` input method query on itself,
which the same filter turns into a re-read. Windows and Linux are unchanged:
Alt is a command key there and Qt can deliver Alt+letter with the plain
letter as text, so yielding it would type "r" instead of composing; AltGr
text is already yielded by [[BPR-1336]].

Audit (offscreen, macOS, Qt 6.11.2; each binding enabled; macOS-style
delivery: shortcut offer with key, Option and the US-layout text, then the
key press), 18 Option+character bindings:

| Binding | Where | Before (pre-change, any of 5 text widget kinds) | After |
|---|---|---|---|
| ⌥R Compose review | window action, Pull Request menu | fired, ® lost | types ®; menu key lifted |
| ⌥T Prepare review tour | window action, Pull Request menu | fired | types † |
| ⌥⇧T Add tour stop | window action, Pull Request menu | fired | types ˇ |
| ⌥⇧1 / ⌥⇧2 / ⌥⇧R Files, Comments, Repositories drawers | window actions | fired | type ⁄, €, ‰ |
| ⌥⇧3 Review details | window action | fired | types ‹ |
| ⌥⇧5 Line numbers | window action, View menu | fired | types ﬁ |
| ⌥⇧4 Private progress button | button shortcut | fired (opened its menu) | types › |
| ⌥C Comment | window action, Pull Request menu | disabled in the probe state, so not shown; no guard | claimed, types ç |
| ⌥N Next new comment | window action | disabled in the probe state; enabled whenever unread threads exist, no guard (⌥N is the ˜ dead key for ñ) | claimed, dead key reaches the editor |
| ⌥O Go to header/source | window action, View menu | already disabled while editing (RFC-BPR-0074) | also claimed and lifted |
| ⌥A Accept file | window action | already unbound by [[BPR-1531]] | unchanged |
| ⌥1 / ⌥2 / ⌥3 comparisons | comparison control, widget-with-children | only from a field inside the control (its revision fields) | claimed there; stand-in scope test |
| ⌥K Label key | Overview page shortcut, widget-with-children | only from a field inside the Overview | claimed there |
| ⌥⇧U Use selected source as replacement | tour outline, widget-only | never from a text widget | unchanged |

Unchanged on purpose: Option with arrows, Page Up/Down and Return
(navigation, tour moves, Option-Return), Command-Option keys (⌘⌥0, ⌘⌥↑/↓,
⌘⌥Return), and the find query's own ⌥W/⌥L toggles, which still work. One
visible change: inside the find query ⌥C and ⌥R now type ç and ® like every
text field; the 2026-10-14 decision kept them as Comment and Compose rather
than Find's Case and Regex toggles, and that assignment stands.

| Acceptance | Evidence at `53f9a260f` sources | Result |
|---|---|---|
| AC1 | `qt_option_keys_typing`: all 18 bindings, each enabled, fire nothing from QLineEdit, QPlainTextEdit, QTextEdit, editable QComboBox and `suggestion_editor`, each of which receives its character; a widget-scoped Option-1 yields to a field inside its scope; a reader made editable under focus types ® and composes nothing. Controls: without the Option claim "toggleWorkbenchFilesDrawerAction Alt+Shift+1" fires; the pre-change window fires ⌥R from every editor kind (baseline table) | Pass |
| AC2 | Same course: while typing the menu bar holds no Option+character key and holds all of them after; the Comment tooltip refreshed while typing still names ⌥C; Option-R from a button opens Compose review. Controls: without lifting, "the native menu bar carries no Option+character key" fails; without the ImEnabled refresh, "a reader made editable under focus lifts the menu bar's keys" fails | Pass |
| AC3 | The audit above; Option-Down and Command-Option-0 keep their bindings while typing; `application_source_find` and `application_source_pr_find` (⌥W/⌥L toggles) pass | Pass |
| AC4 human | Session `6b0c2511ffb1e73c98b891f2543a9094`, candidate `BPR-1536-option-keys-v001`, fingerprint `883a941b…`, ready and protected at cursor 4; frozen root `bpr1536-option-keys-v001.ld3sbmrn` | Pending |

Gates: clang-format 22.1.8 and clang-tidy `--warnings-as-errors` over
`workbench_window.cpp` and the test file pass; documentation gate
`DOCUMENTATION_OK` and its tests pass; 98 related tests pass (Qt workbench,
review state, option keys, header/source, zoom, keys, suggestion, Review
Cycle, document tabs, composition, ledger composition, source find).
`qt_hide_drawers_native_menu_key` was not run: it opens windows on the live
desktop. The native menu-bar path is covered by lifting, which the offscreen
course checks on the actions, and by the owner's look.

Next: on hand-back after cursor 4, an Accept (with [[BPR-1531]] accepted)
merges `bpr-1536-option-keys` after `bpr-alt-a-accept` and finishes the
card; a Reject's note becomes v002 on this branch.

## Closeout — 2026-10-16

**Owner decision, 2026-10-16T01:37:54Z: Accept**, no note (decision
`d0c379b8-f776-4eae-a4c0-966cfacf5761`, event 7 of session
`6b0c2511ffb1e73c98b891f2543a9094`), on candidate
`BPR-1536-option-keys-v001` at `c8af02480`, fingerprint `883a941b…`. The
session was handed back at event 8 with nothing pending; event cursor
consumed: 8.

BPR-1531 landed first. `main` (through `f4c7d3249`) merged into
`bpr-1536-option-keys` at `379a11d4d` without conflicts. On the merged tree
(`dev`, Debug): `qt_option_keys_typing`, `qt_review_state_icon`,
`qt_workbench_focus_order_scale_100`,
`application_review_ledger_composition_contract`,
`application_source_find`, `application_source_pr_find`,
`architecture_boundaries`, `qt_workbench_window_scale_{100,125,200}`,
`application_diff_zoom_public_path`, `qt_profile_management_window`,
`qt_files_tree`, `qt_home_list_scale_100` and `qt_appearance_profile` pass
15/15; clang-format 22.1.8 `--dry-run -Werror` over the three touched C++
files is clean; the documentation gate prints `DOCUMENTATION_OK` on main.
The review server (pid 44711) was stopped, the candidate's protection
released and the frozen root removed.
