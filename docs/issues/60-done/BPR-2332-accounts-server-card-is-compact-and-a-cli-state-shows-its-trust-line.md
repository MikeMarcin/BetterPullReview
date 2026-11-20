---
schema: "implement-rfc.card.v1"
id: "BPR-2332"
title: "The Accounts & hosts server card is compact, and a GitHub CLI connected state shows its trust line"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"reservation":"docs/issues/_reservations/BPR-2332--BPR-2332--7de574540265.json","origin":"owner's attended review bbbd62f881b93ad3315947522fe41e0d (frozen from main 7aeba8bdf), Reject on BPR-2319","related_cards":["BPR-2319","BPR-0772","BPR-1430"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2332: The Accounts & hosts server card is compact, and a GitHub CLI connected state shows its trust line

## The owner's words

Attended review session `bbbd62f881b93ad3315947522fe41e0d`, frozen from main
`7aeba8bdf`, rejected [[BPR-2319]]'s candidate (Accounts trust sentence):

"Feels like a lot of wasted space, also i think the string in question was
CLI which there is no CLI account in this fixture."

The look's build served a GitHub.com fixture signed in with a typed token,
so the card it showed had no GitHub CLI connection and never drew the short
trust sentence the question was about.

## What was wrong

- Each of the card's rows (Access, Rate limit, Details) was 44 pixels tall
  with 12-pixel insets above and below the header and the account, so three
  one-line rows took 132 pixels.
- The label column was a fixed 150 pixels, so "Access" and "Rate limit"
  stood about 166 pixels from their values.
- The Sign out button spanned only the login's two grid rows, so its height
  held the login and the line under it apart: a GitHub CLI card's trust line
  sat a row's gap below the login.
- No test state or render showed a card connected through GitHub CLI.

## What changed

| Where | Before | After |
|---|---|---|
| Row height | 44 px | 32 px, as Settings › Appearance's rows |
| Header and account insets | 12 px above and below | 8 px |
| Label column (Access, Rate limit, Details) | Fixed 150 px | As wide as the widest of the three labels (`server_card::fit_label_column`, refitted on a font or style change); the values start one 16 px gap after it, aligned across the rows |
| Details' own name column | 150 px minimum | 120 px minimum |
| Sign in / Sign out / Connect | Spanned the login and its consequence line | Spans every line of the account (login, consequence, CLI offer, trust line), so the trust line follows the login directly |
| A GitHub CLI connected state | None | `qt_surface_capture_course_scale_{100,125,150,200}` adds an `accounts / cli-connected` state: the GitHub.com card (a GitHub CLI profile) connected through gh, in all four appearances, with its device flow over |

No text changed: the trust sentence, its info glyph and the full
disclosure are [[BPR-2319]]'s.

## Acceptance

- AC1. The server card's Access and Rate limit rows are no taller than
  32 pixels (or two text lines, whichever is more), and their values start
  one gap after the widest label, one label column across the rows.
- AC2. A card connected through GitHub CLI shows "Uses gh's sign-in for this
  account; the token isn't saved." directly under the login (no more than
  4 pixels below it), its info glyph beside it holding the full disclosure,
  and no session-only token claim.
- AC3. A committed Qt test state draws that connected GitHub CLI card for
  looks (`accounts-cli-connected-<appearance>-<scale>.png` under
  `BPR_SURFACE_CAPTURE_DIR`), and its assertions run in every ctest row of
  the course.
- AC4. [[BPR-2319]]'s checks still pass (`qt_composed_github_scale_100`,
  `qt_composed_cli_read_consent`).

## Decisions

- The frontend-design skill was loaded for the layout. The change spends
  nothing on decoration: it removes height and the fixed label column and
  keeps the card's structure, hairlines and text.
- The look fixture is a Qt test state, not a new `--github-fixture`: the
  frozen app's fixture composition has one native profile, and a GitHub CLI
  connection there would need the account setup walked and the gh
  discovery fixture. The surface capture course already composes the window
  with a GitHub CLI profile for GitHub.com, so the connected state is
  published to it the way the shipped presentation update arrives.

## Verification

macOS (arm64), dev preset in a task tree of its own built with
`-gline-tables-only` debug info to stay inside the shared disk (behaviour is
unchanged; only debug tables are smaller). Candidate: BPR-2332 `7c93a2fc7`,
BPR-2334 `bc23441e6`, BPR-2333 `1dd688040` on main `02d9a2f84`. Every changed
C++ file passes `format_check.py` (clang-format 22.1.8) and `tidy.py
--warnings-as-errors` (the changed `.cpp` files); `architecture_boundaries`
and `python3 scripts/verify_documentation.py --root .` (`DOCUMENTATION_OK`)
pass.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2, AC3 | `qt_surface_capture_course_scale_100`, `_125`, `_150`, `_200`: the new `accounts / cli-connected` state in all four appearances (row heights, one label column, the trust line under the login, its glyph holding the disclosure, no token claim, no device code) | pass |
| AC4 | `qt_composed_github_scale_100` (`test_cli_warning_lives_on_its_servers_card` and the Accounts page courses), `qt_composed_cli_read_consent` | pass |
| Regression | `qt_workbench_window_scale_100` | pass |
| Look | Native Cocoa render (`BPR_NATIVE_LOOK=1`, dark, nothing on screen) of `accounts-cli-connected`: the card's three rows sit tight, values start right after the labels, the trust sentence and its glyph follow the login. The first render showed the trust line a row below the login and a stale device code from the signing-in state; both were fixed (the button span and the course clearing the finished flow) | checked by the implementer; render not committed |

Not run: Windows and Linux, `qt_composed_github_scale_125/200`, a full
`ctest`. No owner look was prepared in this round; a new attended session
can show the `accounts-cli-connected` capture or freeze a build.
