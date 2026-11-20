---
schema: "implement-rfc.card.v1"
id: "BPR-2314"
title: "Status's link-styled lines, People's faces and the conflicts row are clickable"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2314--BPR-2314--d6cba9f193be.json","origin":"owner request, 2026-11-20 (Overview Status drawer)","related_cards":["BPR-1589","BPR-1601"],"related_rfcs":["RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2314: Status's link-styled lines, People's faces and the conflicts row are clickable

## Asked

The owner, 2026-11-20, on the pull request Overview's Status drawer: "These
look like links and I want to click them but I have to go find the go to them
button all the way off to the side. Can we let you just click them? The same
with avatars to expand people and conflicts with Master to expand needs
attention." Their screenshot showed My review's "4 threads with new posts" and
"1 new comment in the conversation" lines in the unread (link) colour, each
with a separate Go to them button at the far right of the drawer.

## Change

- `drawer_link_row` (`drawer_parts`): a drawer line that is itself a button.
  Its words wrap and take no pointer of their own, so a press anywhere on them
  reaches the row. It shows the pointing hand, underlines its words under the
  pointer, is one keyboard stop that Space or Return presses, and draws the
  window's focus ring. It emits `clicked` only.
- My review: "N threads with new posts" and "N new comments in the
  conversation" are `drawer_link_row`s in the unread ink. Each keeps the old
  button's object name, accessible name ("Go to the …") and tooltip, and calls
  the same dispatch (Threads with Only new; Discuss). **Both Go to them buttons
  are removed**: the line now does exactly what they did, so they added
  nothing.
- People: `overview_faces` is now a button. Pressing the faces (pointer,
  Space or Return) clicks People's header, so it opens the roster or closes
  it as the header does. It is a keyboard stop only when there are faces,
  shows the drawer's quiet hover tint and the focus ring, and its accessible
  description follows the header's ("Collapsed; press to show the details").
- Needs attention: the "Conflicts with <base>" row is a `drawer_link_row`. A
  press clicks the section's header, which opens it to Merge's conditions.
  The title is underlined under the pointer. The other attention rows (failed
  checks, waiting on, replies to me) stay plain words with no hand.
- Sweep: the only link-ink text in Status was those two lines and their Go to
  them buttons. `set_value`'s unread highlight is never set. Nothing else in
  the drawer is drawn as a link, and a course now fails if any visible label
  in the link ink is not inside a link row.
- Widgets emit intents only: the rows call the existing dispatch callbacks or
  the section header's `click`. No effect moved into Qt.
- RFC-BPR-0085: an owner amendment dated 2026-11-20 records this, and
  section 3 and AC03 no longer name Go to them. The workbench specification's
  Discuss line says the same.

## Acceptance

- AC1: pressing either new-posts line's words, or pressing Return or Space on
  it, opens Threads with Only new or Discuss, exactly as Go to them did. The
  pointing hand shows, the words are underlined under the pointer, and it is a
  button whose accessible name is the old button's.
- AC2: no Go to them button remains in Status.
- AC3: pressing People's faces, or Return or Space on them, opens and closes
  People's roster as its header does.
- AC4: pressing the conflicts row, or Return or Space on it, opens and closes
  Needs attention to Merge's conditions. Focus stays on the row, and the other
  attention rows show no hand.
- AC5: every visible label in Status drawn in the link ink is inside a link
  row.

## Evidence

Candidate: `56abd9ad7` on `main` (built and run at `c74f96a19`, the same tree before rebasing onto `d0a17c890`, which changed no file this card touches).

Disk had about 1 GB free (a full checkout did not fit), so no new build tree
was made. The changed translation units were compiled with the existing
`build/dev` tree's exact compile commands (that tree was built at
`6dd628566`; main's later changes touch only agent headers). The Qt course was
linked against that tree's archives with the changed objects first:

- `qt_pull_request_overview_page` (offscreen, `QT_SCALE_FACTOR=1.0`):
  `pull-request-overview-page-tests:pass`. This includes the new
  `test_status_lines_are_their_own_actions` (AC1 to AC5) and the updated
  `test_status_leads_with_my_role_and_what_needs_attention`.
- Negative control: with Return handling removed from `drawer_link_row`, the
  course fails at "4 threads with new posts: Return goes there too".
- Every translation unit that includes `pull_request_overview_page.hpp` or
  `drawer_parts.hpp`, directly or through `workbench_window.hpp`,
  `window_composition.hpp`, `author_description_editor.hpp` or
  `pull_request_overview_support.hpp` (84 in all, including the end-to-end
  courses that press these buttons), compiles with `-Werror`.
- `setup_destination_public_path_tests` now names the line's class
  (`QAbstractButton overviewGoToNewConversation`) in its thread-vocabulary
  exception. It compiles, but the course was not run (it needs the full app
  build).
- clang-format 22 (`_coding-standard/scripts/format.py`) leaves the changed
  files unchanged. clang-tidy on the five changed translation units reports
  only an existing `modernize-use-emplace` at
  `setup_destination_public_path_tests.cpp:831`, which this change does not
  touch.
- `python3 scripts/verify_documentation.py --root .` passes on main.
