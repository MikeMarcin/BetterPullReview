---
schema: "implement-rfc.card.v1"
id: "BPR-1531"
title: "Accept file's shortcut becomes Option-A (Alt+A), replacing Command-Shift-A"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Replace the Accept file shortcut Ctrl+Shift+A (Command-Shift-A on macOS) with Alt+A (Option-A on macOS). Replace, not add.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0007","RFC-BPR-0074"],"related_cards":["BPR-1507","BPR-1515"],"reservation":"docs/issues/_reservations/BPR-1531--BPR-1531--c72af4c619ef.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a51990fd7985c62d872ade1fb522ef5b93d83d89","evidence":{"kind":"command","detail":"qt_review_state_icon: Option-A delivered as Cocoa does (Key_A, Option, text å, virtual key 0) accepts exactly the current file/material without a menu, also from read-only text; Ctrl+Shift+A dispatches nothing. Control with the old binding fails. application_review_ledger_composition_contract delivers Option-A in the composed app (REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK)."}},{"criterion":"AC2","outcome":"pass","revision":"a51990fd7985c62d872ade1fb522ef5b93d83d89","evidence":{"kind":"command","detail":"Same test: QLineEdit, QPlainTextEdit, QTextEdit, editable QComboBox and suggestion_editor unbind the key and receive å; a reader made editable under focus types å and accepts nothing. Controls: old guard and missing ShortcutOverride claim each fail."}},{"criterion":"AC3","outcome":"pass","revision":"a51990fd7985c62d872ade1fb522ef5b93d83d89","evidence":{"kind":"command","detail":"Same test: menu and tooltip show ⌥A (Alt+A elsewhere); no action, QShortcut, menu-title, button or buddy-label mnemonic uses Alt+A; BPR-1515 eligibility and repeat cases unchanged. 97 related tests pass; format, tidy and documentation gates pass."}},{"criterion":"AC4","outcome":"pass","revision":"a51990fd7985c62d872ade1fb522ef5b93d83d89","evidence":{"kind":"observation","detail":"Owner Accept, no note, in attended session 203673dc69e83ec6df55581a9dc16e5a, decision 1ad7b8d2-df3b-4be1-a1d1-1590ae78a37e (2026-10-16T01:34:45Z, event 5), on candidate BPR-1531-option-a-v001 fingerprint f239492a, frozen root bpr1531-option-a-v001.mcd5e3o5. Merged tree on main through 32890abb4: 14/14 focused and shared window/menu rows pass."}}]
---

# BPR-1531: Accept file's shortcut becomes Option-A (Alt+A), replacing Command-Shift-A

The owner asked on 2026-10-15 to replace Accept file's key, Command-Shift-A on
macOS and Ctrl+Shift+A elsewhere ([[BPR-1515]]), with Option-A on macOS and
Alt+A elsewhere. The owner chose replace, not add: the old key stops
accepting.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
review bar, whose Accept command ([[BPR-1507]]) carries the key, recorded in
the [workbench specification](../../ux/workbench-specification.md) and the
[review ledger architecture](../../architecture/review-ledger.md). The
receipt semantics of [RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)
are unchanged: the key still reaches the one retained Accept action, never
Revoke, and does not repeat. The editor rule follows
[RFC-BPR-0074](../../rfcs/RFC-BPR-0074-header-source-navigation.md)'s Option-O:
editable fields keep their native Option/Alt text input.

On a US macOS layout Option-A types `å`. Command-Shift-A typed nothing, so the
old key could only collide with commands; the new one collides with typing.
The key must therefore step aside wherever the reviewer can type, and still
fire everywhere else.

## Acceptance

- AC1: With an eligible current file and the keyboard outside any text
  input, Option-A (Alt+A) delivered through the window system accepts exactly
  the current file and material without opening a menu, including the macOS
  delivery Cocoa makes (shortcut offer with the key `A`, the Option
  modifier and the text `å`). Command-Shift-A (Ctrl+Shift+A) no longer
  accepts. A check against the old binding fails.
- AC2: Every text input the reviewer can type in inside the window keeps
  Option-A: editable single-line fields, multi-line comment, reply, note and
  composer editors, editable combo boxes and the Review Cycle suggestion
  editor. There the key is unbound, it accepts nothing, and the field
  receives `å`. Read-only text keeps the key. A check with the old
  QLineEdit/QTextEdit-only guard fails on the suggestion editor or combo box.
- AC3: Discoverability and collisions: Accept's menu item and the direct
  button's tooltip name the platform key (`⌥A` on macOS, `Alt+A` elsewhere);
  no other action, shortcut or mnemonic in the assembled window, including
  Windows and Linux menu-bar and button mnemonics, uses Alt+A. Eligibility,
  no-repeat and accept-only behaviour from [[BPR-1515]] are unchanged.
- AC4 (human): The owner accepts the frozen candidate: "⌥A accepts the file
  and ⌥A still types å in comment boxes".

## Implementation

Qt presents: the binding and its editor guard live in
`workbench_window::update_accept_file_shortcut`. No core, host, provider or
storage change. Update the ledger course in the application composition, the
Qt key test, and the live documents that name the key. Historical cards,
checkpoints and release notes keep the key they shipped with. Never drive or
capture the owner's desktop; the owner checks the frozen build themselves.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `a51990fd7985c62d872ade1fb522ef5b93d83d89` on branch
`bpr-alt-a-accept` (worktree
`/Users/jaeger/.codex/worktrees/bpr-alt-a-accept/BetterPullReview`). Not
merged to main until the owner accepts. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1531/handoff.json).

Design. `accept_file_key()` is `Alt+A` (NativeText `⌥A` on macOS). The
binding is removed while a widget that takes typed text has focus:
`takes_typed_text` covers editable line and multi-line editors and any widget
with Qt's `WA_InputMethodEnabled` (editable combo boxes, spin boxes, the
Review Cycle `suggestion_editor`), but not item views, which Qt marks
whenever their current item is editable. A window-scoped `ShortcutOverride`
claim (`typed_text_key_claim`) covers a widget that becomes editable while
focused: the Review Cycle reader turns into the suggestion editor in place
without a focus change. The old guard (QLineEdit, QPlainTextEdit, QTextEdit
only) would have let Option-A accept a file from the suggestion editor or an
editable combo box instead of typing `å`.

macOS Option behaviour, from Qt's Cocoa source (6.10.0
`qnsview_keys.mm` and `qapplekeymapper.mm`; the product's 6.11.2 binary
was not read). For a key press with Option or Control, Cocoa's `KeyEvent`
takes the Qt key from `charactersIgnoringModifiers` ("a", so `Key_A`) and
keeps the typed text ("å") separately. `handleKeyEvent` offers that to the
shortcut map (`sendAsShortcut`) before the input context, and the map's
`possibleKeys` falls back to the event's own key combination for virtual key
0 (`kVK_ANSI_A`), so Option-A matches `Alt+A` without a match on the physical
key code; another key position (AZERTY's A) goes through the layout key map,
whose unmodified layer is also "a". So no `nativeVirtualKey` matching was
needed. Limitations: a non-Latin layout (Russian) gives `charactersIgnoringModifiers`
in its own script, so Option-A is not offered there; a layout where Option-A
is a dead key still fires the shortcut outside text, since the key comes from
`charactersIgnoringModifiers`. Because the map is offered the key before the
text input system, the editor guard is required, not optional, on macOS.
Accept is not in the native menu bar, so there is no AppKit key equivalent.
Windows and Linux: no menu title, button or buddy label in the window has an
`&A` mnemonic (checked by the test), and Alt+A has no system binding there.
AltGr+A (Polish ą) arrives as Ctrl+Alt+A and does not match.

| Acceptance | Evidence at `a51990fd7` | Result |
|---|---|---|
| AC1 | `qt_review_state_icon`: Option-A delivered as Cocoa does (shortcut offer: `Key_A`, Option, "å", virtual key 0) accepts exactly the current file and material without a menu, also from focused read-only text; Ctrl+Shift+A through the window system dispatches nothing. Control: binding restored to Ctrl+Shift+A fails "the Accept menu item and direct button name Option-A". `application_review_ledger_composition_contract` delivers Option-A in the composed app and requires `REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK` | Pass |
| AC2 | Same test: QLineEdit, QPlainTextEdit, QTextEdit, editable QComboBox and `suggestion_editor` unbind the key, the offer is declined and each receives "å"; a read-only suggestion editor made editable under focus types "å" and accepts nothing. Controls: the old guard fails "a widget that takes typed text unbinds Accept's key"; without the claim, "a reader made editable under focus…" fails | Pass |
| AC3 | Same test: menu and tooltip show `⌥A` (`Alt+A` elsewhere), no action, QShortcut or mnemonic collides; no-material, excluded, disabled, unavailable, replacement and repeat-safety cases unchanged. Offscreen render of the progress menu shows ⌥A | Pass |
| AC4 human | Session `203673dc69e83ec6df55581a9dc16e5a`, candidate `BPR-1531-option-a-v001`, fingerprint `f239492a…`, ready and protected at cursor 4; frozen root `bpr1531-option-a-v001.mcd5e3o5` | Pending |

Gates: clang-format 22.1.8 dry run and clang-tidy `--warnings-as-errors` over
the three C++ files pass; documentation gate `DOCUMENTATION_OK` and its 67
tests pass; 97 related tests (`qt_workbench*`, review state, accept,
ledger composition, header/source, zoom, keys, suggestion, Review Cycle)
pass.

Observed while probing (not changed here): in a comment editor inside the
window, Option-R (Compose review) is taken by the window instead of typing
`®`; Option-C, Option-N and Option-O typed their characters in that probe.
That is a separate owner decision if it matters.

Next: on hand-back after cursor 4, an Accept merges `bpr-alt-a-accept` into
main and finishes the card; a Reject's note becomes v002.

## Closeout — 2026-10-16

**Owner decision, 2026-10-16T01:34:45Z: Accept**, no note (decision
`1ad7b8d2-df3b-4be1-a1d1-1590ae78a37e`, event 5 of session
`203673dc69e83ec6df55581a9dc16e5a`), on candidate
`BPR-1531-option-a-v001` at `a51990fd7`, fingerprint `f239492a…`. Event
cursor consumed: 5.

`main` (through `32890abb4`, with BPR-1528, BPR-1529, BPR-1530 and
BPR-1534) merged into `bpr-alt-a-accept` at `ee5129876` without conflicts. On
the merged tree (`dev`, Debug): `qt_review_state_icon`,
`application_review_ledger_composition_contract`,
`qt_workbench_focus_order_scale_100`, `architecture_boundaries`,
`application_accepted_file_in_overview`,
`application_diff_zoom_public_path`,
`application_header_source_navigation`,
`qt_workbench_window_scale_{100,125,200}`, `qt_profile_management_window`,
`qt_files_tree`, `qt_home_list_scale_100` and `qt_appearance_profile` pass
14/14; clang-format 22.1.8 `--dry-run -Werror` over the three touched C++
files is clean; the documentation gate prints `DOCUMENTATION_OK` on main.
The review server (pid 57648) was stopped, the candidate's protection
released and the frozen root removed. BPR-1536 builds on this branch and lands
after it.
