---
schema: "implement-rfc.card.v1"
id: "BPR-1867"
title: "The open pull request picks up new comments on its own"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04, during the BPR-1850 notification wizard: with MikeMarcin/BPR-Test#148 open, another user commented; the activity poll noticed, but the open pull request did not show the comment \"on its own; I had to manually refresh\"","reservation":"docs/issues/_reservations/BPR-1867--BPR-1867--f4f194cd9757.json","related_cards":["BPR-1475","BPR-1575","BPR-1848","BPR-1850","BPR-1851"],"frontend_design":"not loaded: nothing the owner sees is new; the reread is presented by BPR-1575's accepted automatic-refresh note and freshness label"}
acceptance: [{"criterion":"AC1 New activity a poll finds names its pull requests whatever is posted","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_pull_request_activity|application_pull_request_activity_host|core_operation_diagnosis)$'","detail":"activity_decision::Active lists every pull request with a new thread, ascending and unique: focused, read and of any reason too; none for a baseline, a 304, a repeat or a thread without a pull request; activity_concerns compares host, login, owner and repository without case. The host hands them on on the decision lane before the observer, counts them (statistics Active) and logs activity_refresh with a count only; posting is unchanged."}},{"criterion":"AC2 A window showing that pull request reads it again by itself, keeping the reviewer's place","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^application_open_pull_request_activity_public_path$' -V","detail":"Composed application on the fixture network, Review on a file: activity on another pull request owes and reads nothing; activity on the shown one, spelt 'GitHub.com' / 'Octo-Org/Review-Repository', reads it once as an automatic refresh (Trigger automatic, Changes.NewComments 1), the reply posted meanwhile is on the file's conversation, the comparison never left the screen, the same file is chosen again, the comparison scroll 50000 before and after. 18 reads per reread."}},{"criterion":"AC3 The reread waits while the reviewer writes and leaves the writing alone","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_open_pull_request_activity_public_path|core_refresh_feedback)$' -V","detail":"A new inline draft written in the drawer (threadDraftText): activity arrives, 3 s later the reread is still owed and nothing was read, the text and the focus are untouched; once the focus leaves the editor it runs (the next reply arrives) and the draft is still being written with its text, focus back in it. decide_reread: news waits for a read under way and while composing; a publication's reread does not. The draft editor is read-only for a moment while each edit saves, so composing() counts any focused multi-line editor except a QTextBrowser (the first cut missed exactly that)."}},{"criterion":"AC4 Coming back to an aged pull request reads it again; a current one is not","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_open_pull_request_activity_public_path|core_refresh_feedback)$' -V","detail":"Another top-level window comes in front and the review window comes back: current, nothing is read; with discovery's clock 301 s on, one automatic reread, the file kept; coming back again reads nothing. refresh_due_on_return: the first read ages as a refresh does; none while a read runs, before anything was read, for a clock that went back, or within a rate limit's Retry-After (a minute when none was named); an offline failure over an aged update tries again."}},{"criterion":"AC5 Live on MikeMarcin/BPR-Test through the app's gh path","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"cmake --preset dev -DBPR_BUILD_LIVE_ACTIVITY_COURSE=ON; python3 tests/scripts/activity_live_course.py --tool build/dev/tests/github_contract/activity_live_course --work SCRATCH","detail":"2026-11-05: PR #149 opened as MikeMarcin; baseline 200 (Last-Modified, X-Poll-Interval 60); MikeMarcinTest's comment was posted 'MikeMarcin/BPR-Test #149 | New comment.' and handed on 'refresh:the-pull-request' after 1 poll; with the pull request focused (the owner's case) the second comment was suppressed, not posted, and still handed on after 1 poll; the two following polls were 304 with X-RateLimit-Remaining unchanged (4900, 4898 after each /user). 5 polls, 10 wire requests, active=2. Cleanup: both comments deleted, #149 closed, branch deleted."}},{"criterion":"AC6 Neighbouring courses and gates","outcome":"pass","revision":"5b3feca18","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_refresh_feedback_public_path|application_refresh_current_file_public_path|application_profile_multi_window_public_path|application_profile_window_review_restoration_public_path|qt_pull_request_activity_notifications|application_queue_composition_contract|architecture_boundaries|architecture_boundary_contract|coding_format_check)$'","detail":"All pass (clang-format 22 on every touched file)."}}]
---

# BPR-1867: The open pull request picks up new comments on its own

## Report

The owner, 2026-11-04, during the [[BPR-1850]] notification wizard: with
`MikeMarcin/BPR-Test#148` open in the app, another user commented. The
activity poll noticed it, but the open pull request did not show the comment
"on its own; I had to manually refresh". The theme is responsiveness: less
waiting, less manual refresh.

## Root cause

[[BPR-1850]]'s decision treated a new thread on the pull request open in the
active window as *suppressed*: seen, not posted, and nothing else. Nothing
told the window, so it kept what it had read until Refresh. The window could
already reread itself ([[BPR-1475]] after publishing, presented as automatic by
[[BPR-1575]], current file first by [[BPR-1851]]); the poll simply never asked.

## Built

- **Core.** `activity_decision::Active`: every pull request with a new thread,
  notified or not (never for a baseline). `activity_concerns`: whether a shown
  pull request is among them, names compared without case.
  `refresh_due_on_return` and `refresh_return_seconds` (300, Home's freshness
  window): a window coming back in front on an aged update rereads, never into
  a rate limit's wait. `decide_reread`: an owed reread waits for a read under
  way, and news (not a publication) waits while the reviewer writes.
- **Application.** `pull_request_activity_host::set_refresh`: after each answer
  with new activity, on the decision lane before the observer. Statistic
  `Active`; diagnostic action `activity_refresh` (count only).
- **Qt.** `workbench_window::shown_pull_request` (in front or not; none on Home
  or a public preview), `composing` (focus in a multi-line editor, not a
  browser), and a return dispatch on activation carrying the refresh clock.
- **Composition.** The profile hands `Active` to every window's
  `ActivityArrived`; a window that shows one calls `reread_for_news`, which owes
  the existing one reread with `RereadQuiet`; a publication's reread clears it.
  A wait for writing asks again every 2 s. The return dispatch asks the core.

No tray or notification sink file was touched.

## Native sign-ins

GitHub refuses the notification list to the GitHub App user token native
sign-in holds, so a native profile has no activity signal. A periodic reread of
the open pull request is not a cheap fallback: one reread is 18 reads on the
fixture (pull request, files, threads, reviews, comments, checks, statuses,
timeline, rules...), none conditional, so once a minute would spend over a
thousand requests an hour. The fallback is the return reread: coming back in
front on an update five minutes old reads it again, which costs one reread per
return and nothing while the window sits. It serves CLI sign-ins too for what
the participating notification list does not carry (a push, a thread the
viewer takes no part in). A true cheap poll would be a conditional
`GET /repos/{o}/{r}/pulls/{n}` (`If-None-Match`, 304 free) while the window is
in front; it needs conditional support in the native transport (today only the
gh path keeps `Last-Modified`, for `list_notifications` alone) and is left open.

## Open

- A conditional single-pull-request poll for native sign-ins while the window
  is in front (above); not built.
- The live course exercises the host's hand-off over the real gh path; the
  window's reread is exercised by the composed fixture course. A live run of
  the whole application signed in with GitHub CLI has not been made.
- Latency is the poll's: up to about a minute (`X-Poll-Interval`) after the
  comment.
