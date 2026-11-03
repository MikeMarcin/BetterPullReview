---
schema: "implement-rfc.card.v1"
id: "BPR-1843"
title: "Home's Active review card, and Recently viewed rows without Reviewing or Resume"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","owner_direction":"2026-11-03","reservation":"docs/issues/_reservations/BPR-1842--BPR-1844--749b2d50ac34.json","related_cards":["BPR-0733","BPR-0776","BPR-1338","BPR-1365","BPR-1381","BPR-1573"],"frontend_design":"frontend-design:frontend-design loaded before the design; card hierarchy, ink and spacing chosen with it","renders":"build/dev/renders/BPR-1843 in the implementing worktree (not committed)"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 Over an open review Home shows an Active review card under the bar and above every list: caption, identity, literal title, where the reading was left, Back to the review and an × that is Close review","outcome":"pass","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"command","command":"ctest --preset dev -R 'qt_home_list_scale|qt_composed_github_scale|application_close_review|application_home_open_feedback_public_path'","detail":"home_list_tests test_active_review_card_reads_in_every_theme (light, dark, high-contrast light and dark, at 1.0x, 1.25x and 2.0x); composed_github_tests places the card under the bar, as wide as it and before the lists; application_close_review drives the real composition: the × is named Close review, Tab reaches it from Back to the review, Space closes the review through the existing close_pull_request_intent and the bar takes focus."}},{"criterion":"AC2 The × uses the existing Close review behaviour: the core decides, the host performs, Qt presents","outcome":"pass","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"command","detail":"The × is workbench_landing::close_button(), wired to workbench_window::close_review() and set_close_review_dispatch, which dispatches core::close_pull_request_intent ([[BPR-1573]]); application_close_review proves the read lane, snapshot, reading, tabs and drafts ledger are released and the drafts survive a reopen."}},{"criterion":"AC3 Recently viewed rows carry no Reviewing tag and no Resume chip","outcome":"pass","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"command","detail":"recent_review_item sets neither home_row_role::reviewing nor row_action; home_list_tests test_recent_rows_offer_open_on_github_and_no_resume, composed_github_tests test_home_lists_recently_viewed_pull_requests and the open-review course, and application_home_open_feedback_public_path assert it on the composed window. Resume stays in the row's menu and on Shift and Return."}},{"criterion":"AC4 Recently viewed rows carry the queue row's Open on GitHub chip, the same action and component","outcome":"pass","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"command","detail":"home_list::adopt_recent builds the same home_row_action (icon_action role, objectName homeRowOpenOnGithubButton, the window action's icon and tooltip); its press dispatches workbench_window::open_pull_request_page(recent_review_row), the path the row menu's Open on GitHub takes. The delegate keeps the icon its own place beside the viewed time so it never covers it. composed_github_tests asserts the press names the row's page and resumes nothing."}},{"criterion":"AC5 Accessibility, themes and display scaling: names and descriptions for the card, its × and the row chip; keyboard order; WCAG AA ink on the card's ground in every theme; one card height whatever the title","outcome":"pass","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"command","detail":"test_active_review_card_reads_in_every_theme: card accessible name Active review with its description, × named Close review and described, Back to the review default, caption and position ink at 4.5:1 or more on the card's ground (stepped toward the text where a theme needs it), rail and ground pixels, constant height with an elided long title whose whole text is its tooltip and description."}},{"criterion":"AC6 The owner accepts the look of the Active review card and the Recently viewed rows in light and dark","outcome":"pending","revision":"worktree-agent-a81c97e20a82bee1c","evidence":{"kind":"observation","detail":"Look-and-judge; offscreen renders prepared under build/dev/renders/BPR-1843 for the coordinator's attended review. No session started by the implementer."}}]
---

# Home's Active review card, and Recently viewed rows without Reviewing or Resume

The owner, 2026-11-03, on Home:

1. The pull request being reviewed gets its own always-on-top treatment: a
   dedicated, visually distinct Active review card above the lists, with an
   × that closes the review, using the existing Close review behaviour.
2. Recently viewed rows lose the Reviewing chip; the owner finds it
   confusing.
3. Recently viewed rows lose the Resume chip; the row itself opens the pull
   request.
4. Recently viewed rows get the Open on GitHub chip the queue rows have, the
   same action and component.

## Design

`frontend-design:frontend-design` was loaded before designing.

The card is the landing's `back_to_review` presentation, moved from the
bar's right ([[BPR-1381]]) to under the bar, as wide as it, before every
list. It is the one place on Home tinted by the accent: the theme's
Highlight washed into Base (7 %), a 45 % accent edge, and a 4-point accent
rail down its left, clipped to its 8-point radius. Its text is three short
lines: "Active review" in a small semibold caption in the accent pulled
toward the text, then the identity (`owner/name#123`, semibold) beside the
literal title, then where the reading was left ("Left off at file 3 of 12",
or "Open in this window") in the quiet ink. Back to the review stays the
page's one filled control and Return's default; the × follows it, a quiet
round mark that lifts under the pointer and rings in the accent on focus.
The caption and position inks are stepped toward the text until they read
at 4.5:1 on the card's ground, so high-contrast themes keep AA.

The title is one line, ellipsised where it must be, with the whole title as
its tooltip and in the card's accessible description, so the card has one
height whichever review is open. When another pull request is opened from
Home, the read lets the open review go before the new one arrives; the
card's place is held, empty, until that open is over, so the row under the
pointer does not move ([[BPR-1381]]). Close review holds nothing.

Recently viewed rows are ordinary rows now: no AlternateBase tint, no
Reviewing tag or words, no Resume chip. Their hover and focus chip is the
queue's Open on GitHub. Because these rows also show the viewed time, the
delegate gives an icon action on such a row its own place at the right
instead of the time column's, so it never covers the time.

The queue's own row for the open pull request keeps its Reviewing tag
([[BPR-1365]]); the request named Recently viewed only.

## Evidence

See the acceptance entries. Renders (not committed) from the implementing
worktree's build tree: `renders/BPR-1843/home-over-review-{light,dark}.png`
(application_close_review), `renders/BPR-1843/home-open-feedback/*-reviewing*.png`
(application_home_open_feedback_public_path, light, dark and both high-contrast
themes), and `tests/adapters_qt/home-list-{100,125,200}-active-review-*.png`.

## Pending

AC6, the owner's look-and-judge, reaches the owner through the coordinator's
attended review.
