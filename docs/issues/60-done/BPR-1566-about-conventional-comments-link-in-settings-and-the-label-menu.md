---
schema: "implement-rfc.card.v1"
id: "BPR-1566"
title: "About Conventional Comments: a link in Settings and the comment label menu"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-request","found_by":"Owner request via coordinator, 2026-10-16","governing_rfc":"RFC-BPR-0048","related_cards":["BPR-1083","BPR-1088","BPR-1338","BPR-1565","BPR-1568"],"reservation":"docs/issues/_reservations/BPR-1566--BPR-1566--47437684c550.json","acceptance":["BPR-1566:menu","BPR-1566:settings","BPR-1566:path","BPR-1566:gates","BPR-1566:owner"]}
acceptance: [{"criterion": "BPR-1566:menu", "outcome": "pass", "revision": "3058e07f3", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_feedback_label_controls|qt_conversation_cards_view)$'", "detail": "test_about_conventional_comments_is_a_way_out_not_a_label: unbound, no item and Down from the grid's last row stays (as before); bound, the item is last under a rule, named 'About Conventional Comments' with description 'Where these labels come from and how to use them. Opens conventionalcomments.org in your browser.', not checkable, in the readable link colour, Tab-focusable; on a plain comment Down from typo reaches it, Down stays, Up returns to typo; Return names conventional_comments once, the menu closes, text/label/draft commands/edits unchanged, focus back in the body; on a labelled comment Down from the custom field reaches it and Up returns; Command+0 with it focused is Plain and asks nothing; a click asks again; unbinding hides it and its rule. Every existing keyboard test in the file passes unchanged. qt_conversation_cards_view: a reply's menu ends with it, a click names the page once and leaves the reply. Negative control: About made to choose a label fails at 'the menu closes and the comment, its label and its draft are untouched'."}}, {"criterion": "BPR-1566:settings", "outcome": "pass", "revision": "3058e07f3", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_feedback_label_settings$'", "detail": "test_about_conventional_comments_names_the_page: unbound, hidden in Settings and in the composer's menu; bound, the caption 'These labels follow a widely used format for review comments.' and the link sit under the Comment labels heading, above the rows; accessible name and description, Tab focus; Space names conventional_comments once and dispatches no preference; opened is said in the status bar only; failed, unavailable and refused are said under the link; Reset leaves it; the composer's request reaches the window's dispatch; in the built-in Dark palette both section links read at 4.5:1 or better."}}, {"criterion": "BPR-1566:path", "outcome": "pass", "revision": "3058e07f3", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_markdown_navigation|application_github_window_read_host|application_personal_discovery_public_path|application_personal_discovery_cli_public_path|qt_composed_github_scale_100|qt_composed_inline_drawer_scale_(100|125|200))$'", "detail": "Core: admit_reference_page gives conventionalcomments.org and '/', unavailable without a browser, refused for an unnamed value. Application: open_reference_page opens exactly https://conventionalcomments.org/ once; null browser unavailable; unnamed refused; declining or throwing browser failed. Composed application with the recording browser: Settings' link and the composer menu's request each open exactly https://conventionalcomments.org/. Composed drawer: the window's dispatch reaches the open reply's menu. Negative control: an http:// address fails at 'the address is HTTPS, the host and the path'."}}, {"criterion": "BPR-1566:gates", "outcome": "pass", "revision": "3058e07f3", "evidence": {"kind": "command", "command": "full dev ctest, composition contracts, clang-format 22, clang-tidy, verify_documentation.py", "detail": "ctest --preset dev -j 6 on the combined tree (content of c95dea752, lean -g0 build): 761/765. qt_composed_inline_drawer_scale_100/125/200 failed at 'Tab from the notes enters the open thread's first post' because this card's composed-course block opened a real popup; 3058e07f3 drives the bound request instead and all three pass. qt_hide_drawers_native_menu_key failed only under the parallel run's load and passes alone on the same binaries. Composition and boundary rows (17) pass; application_review_round_trip_composition_contract resealed over main at ce74f8f7c. clang-format 22.1.8 on every changed C++ file. clang-tidy --warnings-as-errors over every changed .cpp: the one finding in this work (reference_page.cpp, analyzer null path) fixed in 82fe27a81; the remaining findings are on lines from earlier commits (main.cpp 2191/9716/10195/10203, conversation_cards_view_tests 1719/1731, feedback_label_controls_tests 232/337, settings_window_tests 644, github_window_read_host_tests 373/377). DOCUMENTATION_OK; verify_documentation_tests 67 OK."}}, {"criterion": "BPR-1566:owner", "outcome": "pass", "revision": "frozen-4951564f8-v001", "evidence": {"kind": "observation", "path": "docs/issues/60-done/BPR-1566-about-conventional-comments-link-in-settings-and-the-label-menu.md", "detail": "Owner Accept in attended-review session 1086a4c4343a1f2477fda538fcf9b519 of BPR-1566-about-conventional-comments-v001 (revision frozen-4951564f8-v001, fingerprint c5da44f79d5c...), decision 2e35abcc-86d0-4b14-8f9b-ea3f95485507, 2026-10-17T04:41:30Z, handed back at event 11. Note: \"I'm really not sure why plan comment gets a hotkey, I think we can remove that.\" That note is a follow-up card (Command+0 in the open label menu), not a condition of this acceptance."}}]
---

# BPR-1566: About Conventional Comments: a link in Settings and the comment label menu

## Owner request

Owner, 2026-10-16, verbatim: "add a small thing to the settings page and
dropdown picker menu for the comment labels that links to
https://conventionalcomments.org/ for people who don't know where those come
from or how to use them."

## Design

- **Label menu.** Under a rule, the menu's last item is "About Conventional
  Comments", in the palette's link colour, with the tooltip and accessible
  description "Where these labels come from and how to use them. Opens
  conventionalcomments.org in your browser." It is not a label choice: no
  edit, no draft command, the menu closes and focus returns to the comment.
  Arrow keys reach it below the custom decoration field; on a plain comment
  Down from the grid's last row passes the disabled decorations and field.
  ⌘0 stays Plain anywhere in the open menu. Shown only where a window bound
  the dispatch.
- **Settings › Reviews & queue › Comment labels.** First line under the
  heading: "These labels follow a widely used format for review comments." in
  the quiet ink, then the same link. Not a preference (Reset leaves it alone).
  A page that did not open says why under the link; an open says so in the
  window's status bar.
- **Path.** Qt names `core::reference_page::conventional_comments`; the
  window's dispatch goes to composition, which calls
  `application::open_reference_page` with the window's browser port. The core
  (`admit_reference_page`) admits the page as host `conventionalcomments.org`
  and path `/`, `unavailable` without a browser and `refused` for an unnamed
  value. The application builds `https://conventionalcomments.org/`, requires
  it to pass `address_matches_verification_authority` (exact host, HTTPS
  only) and hands it to the browser port, whose production opener validates
  it again. No address, `QUrl` or opener exists in Qt.

## Acceptance

- **BPR-1566:menu** The label menu shows the item last, under a rule, with
  its accessible name and description; arrow keys and Tab reach it from a
  plain and a labelled comment; Return or a click names the page exactly
  once, closes the menu and changes neither text, label nor draft; ⌘0 and
  the existing keyboard behaviour are unchanged; unbound, the item is absent
  and Down stops where it did (negative control).
- **BPR-1566:settings** Comment labels shows the sentence and link under its
  heading, above the rows, reachable by keyboard with accessible name and
  description; activation names the page once and dispatches no preference;
  failure, unavailable and refused say so under the link; Reset leaves it.
- **BPR-1566:path** Through the real composition and the recording browser,
  the Settings link and the label menu's request each open exactly
  `https://conventionalcomments.org/`; the core and application refuse an
  unnamed page and report no browser and a failing browser.
- **BPR-1566:gates** Focused rows, composition contracts, architecture
  boundaries, clang-format 22, clang-tidy and the documentation gate pass;
  one full dev ctest before landing.
- **BPR-1566:owner** The owner accepts the look in Light and Dark.

## Evidence

All criteria pass (front matter). Owner Accept in attended-review session 1086a4c4343a1f2477fda538fcf9b519 (decision 2e35abcc-86d0-4b14-8f9b-ea3f95485507), with the note "I'm really not sure why plan comment gets a hotkey, I think we can remove that.", carried by its own card. Landed by merging main at 2bfc9827c, resealing the round-trip composition contract on that tree, and one full dev ctest on it before the merge to main.
