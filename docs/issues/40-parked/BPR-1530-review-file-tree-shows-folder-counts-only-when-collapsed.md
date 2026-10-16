---
schema: "implement-rfc.card.v1"
id: "BPR-1530"
title: "Show a Review file-tree folder's file count only while it is collapsed"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"I think the file count on folders in the file tree explorer is a bit too noisy. Lets hide that and only show it when the folder is collapsed.","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0658","BPR-1363","BPR-1529"],"reservation":"docs/issues/_reservations/BPR-1530--BPR-1530--82f5f2096afd.json","requested_skill":"frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"command","detail":"qt_files_tree test_folder_counts_show_only_while_collapsed: count slot inked only on collapsed folders across new-folder open, collapse-all, expand-all, mouse branch clicks, Left, Right and Space. Control with always-drawn counts fails."}},{"criterion":"AC2","outcome":"pass","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"command","detail":"Same test keeps 'name, N files' accessible text open and closed; BPR-1529's AC2 suites pass 13/13."}},{"criterion":"AC3","outcome":"pending","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 9b4f7e98ea5592bf28d39c59d06f3949, candidate BPR-1530-folder-counts-v001, fingerprint b78c04404adb2a72df5184f69b8d8c8f5b0f0943aa5e808608fba49d1b3487cf, cursor 6; frozen root bpr1529-folder-look-v001.r0_305or."}}]
---

# BPR-1530: Show a Review file-tree folder's file count only while it is collapsed

The owner asked on 2026-10-15: "I think the file count on folders in the file
tree explorer is a bit too noisy. Lets hide that and only show it when the
folder is collapsed."

The file tree explorer is Review's files drawer tree ([[BPR-0658]],
`files_tree_view`). Every folder row draws its file count at the row's right
edge, open or closed. An open folder already shows its files beneath it, so
the count repeats what is visible; a closed folder hides them, so the count is
the only sign of what it holds.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
files drawer ("file counts on folders"), amended by the owner for this one
choice and recorded in the
[File drawer specification](../../ux/workbench-specification.md#file-drawer).
This card changes only the Review drawer. Overview's folder rows keep their
count slot ([[BPR-1363]] v002): the owner named the file tree explorer, and
Overview's counts are a column the owner asked for separately; if the owner
wants Overview changed too, that is a separate request.

The model and the accessible text are unchanged: a folder's accessible text
still says "name, N files" whether it is open or closed, because assistive
technology reads the row, not the painted digits, and the count is not
redundant there.

## Acceptance

- AC1: A collapsed folder row draws its file count at the right edge as
  before; an expanded folder row draws no count. The count appears and
  disappears as the folder is collapsed and expanded by mouse, by keyboard,
  and by expand-all and collapse-all; a check against always-drawn counts
  fails.
- AC2: The folder's name, its ink ([[BPR-1529]]), row geometry, file rows,
  the folder's accessible text with its file count, selection, focus and
  keyboard behaviour are unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Review file
  tree, shown with folders expanded and collapsed in offscreen Light and Dark
  renders.

## Implementation

Load frontend-design:frontend-design before design work. Qt presents: the
change is `files_tree_delegate::paint`'s folder detail, read from the view's
expansion state; no core, host, provider or storage change. Shares one frozen
build with [[BPR-1529]] and asks its own question in the same session. Never
drive or capture the owner's desktop; renders are offscreen.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `0e4d1f89385ca1cbcd64113908d8717b3eda4324` on branch
`bpr-1529-folder-colour`, shared with [[BPR-1529]] (one commit, one frozen
build, separate questions). Not merged to main until the owner accepts.
Exact paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1530/handoff.json).

Design (frontend-design loaded first): `files_tree_delegate::paint` draws a
folder's count only when its `QTreeView` does not have it expanded; with no
view it draws the count as before. The label keeps its width up to the counts
slot, so nothing moves. The model's `AccessibleTextRole` keeps "name, N files"
for every folder. Overview's folder count slot ([[BPR-1363]] v002) is
unchanged, and the owner's question offers that as a note.

| Acceptance | Evidence at `0e4d1f893` | Result |
|---|---|---|
| AC1 | `qt_files_tree`: new `test_folder_counts_show_only_while_collapsed` reads the count slot's pixels after new folders open, collapse-all, expand-all, mouse branch clicks, and Left, Right and Space; control with always-drawn counts fails "an open folder draws no count" | Pass |
| AC2 | The same test checks the accessible text with the count on open and closed folders; the AC2 suites of [[BPR-1529]] pass (13/13) | Pass |
| AC3 human | Session `9b4f7e98ea5592bf28d39c59d06f3949`, candidate `BPR-1530-folder-counts-v001`, fingerprint `b78c0440…`, ready and protected at cursor 6, Light and Dark renders before, after with folders closed, and after with all open | Pending |

Gates, frozen build and server as recorded on [[BPR-1529]].

Next: on hand-back after cursor 6, an Accept (with BPR-1529's verdict)
merges the branch and finishes the card; a Reject's note becomes v002.

## Closeout — 2026-10-16

**Owner decision, 2026-10-16T01:34:37Z: Accept**, no note (decision
`87c3af7b-3072-45c0-81f5-d631912996f6`, event 8 of session
`9b4f7e98ea5592bf28d39c59d06f3949`), on candidate `BPR-1530-folder-counts-v001` at
`0e4d1f893`, fingerprint `b78c0440…`. The session was handed back at event 9
with nothing pending; event cursor consumed: 9. BPR-1529 and BPR-1530 share
the one product commit and were both accepted, so it lands whole.

`main` merged into the branch at `74e25c1d3` (through `0b37d5438`) and
`a7506ceb9` (through `550da2cd0`, with BPR-1528 and BPR-1534) without
conflicts. On the merged tree (`dev`, Debug): `qt_files_tree`,
`qt_files_tree_folder_rows_{125,200}`, `qt_pull_request_overview_page`,
`qt_pull_request_overview_folder_rows_{125,200}`,
`qt_workbench_window_scale_{100,125,200}`,
`qt_composed_github_scale_{100,125,200}` and
`application_github_public_path_nested_folders_v1` pass 13/13 at both
merges; clang-format 22.1.8 `--dry-run -Werror` over the three touched C++
files is clean; the documentation gate prints `DOCUMENTATION_OK` on main.
The review server (pid 48572) was stopped, both candidates' protection
released and the frozen root removed.
