---
schema: "implement-rfc.card.v1"
id: "BPR-1850"
title: "Desktop notifications for comments, reviews and mentions on my pull requests"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, accepted design 2026-11-03: desktop notifications when someone comments, reviews or mentions me on my pull requests, from GitHub's Notifications API with conditional polling, 30-60 s latency, low overhead","reservation":"docs/issues/_reservations/BPR-1850--BPR-1850--556ae776bc0e.json","related_cards":["BPR-1250","BPR-1370","BPR-1373","BPR-1831","BPR-1844","BPR-1849"],"frontend_design":"loaded: frontend-design:frontend-design for the Settings switch, its status line and the notification wording; owner judgment pending (AC7)"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_pull_request_activity$'", "detail": "New by identity and updated_at; seen not re-notified across a relaunch's memory; read (own), review_requested, other reasons and the focused pull request suppressed but seen; a burst on one pull request is one notification; more within 120 s held then posted once; held activity dropped when focused; more than three pull requests one summary; click opens the pull request only when one is outstanding; interval floor 60 s and X-Poll-Interval, backoff 120/240/480/600, Retry-After and reset, changed answer with under 100 left waits for the reset, 304 never; unsupported stops; rows round-trip, newer version unwritable, older restarts keeping the setting, damaged seen rows dropped, seen bounded at 512."}}, {"criterion": "AC2", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(github_notifications_read|github_provider|github_read_contract|github_anonymous_read_contract|native_https_anonymous_read_contract|gh_cli_transport|github_paged_read)$'", "detail": "list_notifications compiles GET /notifications?participating=true&per_page=50 (GHES /api/v3), If-Modified-Since only as an IMF-fixdate and only on this read, validated against the semantic request; the session context admits it; the gh 2.103 304 output (captured 2026-11-03) parses with Last-Modified and X-Poll-Interval kept only for this read; 200/304/403 rate limit/403 integration/404/429/5xx/malformed answers; title never read; the provider poll is one job, 200 then conditional 304 through the fixture transport; a native sign-in is answered unsupported with zero requests; anonymous admission refuses it as an identity read."}}, {"criterion": "AC3", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_pull_request_activity_host$'", "detail": "Polls at once on a session, first unconditional, then conditional on the stored date; one timer armed at X-Poll-Interval; a 304 writes and posts nothing; new activity posted, click target is the pull request and comment; repeats and focused activity not posted; failure and rate-limit waits; sign-out cancels the timer and a late answer is stale; relaunch from the store polls conditionally and does not re-notify; off stops, on takes a new baseline; native and refused sign-ins are not polled."}}, {"criterion": "AC4", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(sqlite_pull_request_activity_store|sqlite_profile_data_reset_files|sqlite_storage_diagnostics|sqlite_view_filter_memory_store)$'", "detail": "state/v1/pull-request-activity.sqlite3 round-trips across reopen; a damaged seen row is reported and repaired; version 0 restarts keeping the setting; version 2 reads as nothing, its write is refused and its rows left; a foreign file reads fresh and is not overwritten; Reset settings removes it; storage lines name it."}}, {"criterion": "AC5", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(qt_pull_request_activity_notifications|qt_system_tray_notification_sink|qt_settings_window.*|qt_workbench_window_scale_100|qt_settings_effect_boundary|qt_quiet_buttons|qt_view_filter_memory_window|application_queue_composition_contract|application_profile_multi_window_public_path)$'", "detail": "The host's QTimer is single-shot at 60 000 ms; the timer's poll posts 'octo/widgets #42' / 'New review.' through the tray; the tray's messageClicked reaches the host and the workbench window dispatches open_pull_request_intent for octo/widgets #42; a tray shared with the queue sink routes a click to whichever posted last; the Settings switch is disabled without a dispatch, presenting tells nothing back, a click is dispatched. Existing tray, Settings, window and composed courses pass."}}, {"criterion": "AC6", "outcome": "pass", "revision": "7c4acdc39 (rebased onto e466ed1ea), rerun at e113e760e", "evidence": {"kind": "command", "command": "ctest -R '^(coding_format_check|architecture_boundaries|architecture_boundary_contract)$'; _coding-standard/scripts/tidy.py --warnings-as-errors on changed TUs; python3 scripts/verify_documentation.py --root .", "detail": "Format (clang-format 22) and both architecture gates pass; tidy clean on every new TU; remaining findings in modified large TUs are on lines unchanged from main; DOCUMENTATION_OK."}}, {"criterion": "AC8", "outcome": "pass", "revision": "e113e760e", "evidence": {"kind": "command", "command": "cmake --preset dev -DBPR_BUILD_LIVE_ACTIVITY_COURSE=ON; python3 tests/scripts/activity_live_course.py --tool build/dev/tests/github_contract/activity_live_course --work SCRATCH", "detail": "Live on MikeMarcin/BPR-Test, 2026-11-03, through the app's pinned gh path offscreen with a fresh state root: PR #145 opened as MikeMarcin; baseline poll 200 (Last-Modified present, X-Poll-Interval 60); MikeMarcinTest's comment was posted as 'MikeMarcin/BPR-Test #145 | New comment.' after 1 poll; the click opened that pull request; with it focused the second identity's second comment was suppressed (activity_suppress count=1) and not posted; the two following polls were 304. Rate limit: each /user check spent 1 (4991, 4990); each 304 left X-RateLimit-Remaining unchanged (/user 4991 then 304 4991; 4990 then 4990); 200s spent 1. 5 polls, 10 wire requests. Cleanup: both comments deleted, PR #145 closed, branch deleted; the owner's notification thread for #145 is left unread (nothing is marked read)."}}, {"criterion": "AC7", "outcome": "pass", "revision": "a86306751 (merged as 9d279d1fc)", "evidence": {"kind": "observation", "detail": "Owner, attended review: session 7568bd7baa2ced89570c860e9af4090b accepted the notification wording and on-by-default, rejected the Settings wording ('Less text or an info circle if we want to retain full detail'); session 01f98d7b6ef0b164d3a170325d7204ee accepted the reworked row (switch, info button, one status line) at 158f46713 with no note. The owner instructed 'land on main'; the coordinator merged a86306751 as 9d279d1fc."}}, {"criterion": "AC9", "outcome": "pass", "revision": "fa545a31e (merged as a8b944a9d)", "evidence": {"kind": "observation", "detail": "Owner, wizard bpr-1850-desktop-banner-v004 (session 1f4322ce-6d40-4e44-852d-4c1d0002db48, complete), build frozen from fa545a31e, GitHub CLI sign-in: MikeMarcinTest's issuecomment-6648683560 (03:14:32Z) posted at 03:15:27Z, owner 'Accept: one banner appeared'; under Do Not Disturb issuecomment-6648713706 (03:16:37Z) posted at 03:17:30Z, owner 'Accept: silent, and it is listed in Notification Center'; notes 'none'. History: wizard v003 (session 8585f0ff-f88d-4e24-9574-b0b440b8172d, frozen from 9d279d1fc), owner 2026-11-04: GitHub CLI sign-in, one banner on MikeMarcinTest's first comment on BPR-Test#148, its click logged activity_activate; under Do Not Disturb the second comment was posted (activity_post 02:41:51Z) and silent but not in Notification Center: Qt withdrew it after its 10 s display hint (see Defect). Fixed in fa545a31e. Re-check: wizard bpr-1850-desktop-banner-v004, session 1f4322ce-6d40-4e44-852d-4c1d0002db48, state /Users/jaeger/.codex/wizard/bpr1850-banner-v004-qduDtG/state, over frozen build /Users/jaeger/bpr-attended/BPR-1850-v004.szh_czqj (from fa545a31e), opened with open -n and its own --test-application-data-root; BPR-Test#148 closed and its branch deleted afterwards."}}]
---

# BPR-1850: Desktop notifications for activity on my pull requests

## Request

The owner, 2026-11-03 (accepted design): desktop notifications when someone
comments, reviews or mentions me on my pull requests, from GitHub's
Notifications API with conditional polling; 30–60 seconds of latency is fine,
and it must cost little.

## Decisions

- **Source.** `GET /notifications?participating=true&per_page=50`, one page,
  with `If-Modified-Since` from the last `Last-Modified`. GitHub answers 304
  while nothing changed, and a 304 does not count against the rate limit
  (measured 2026-11-03 through `gh api --include`: `X-RateLimit-Remaining`
  stayed at 4998 across the 304). Nothing is marked read on GitHub.
- **Which sign-ins.** Only a GitHub CLI account polls, on github.com or GHES.
  GitHub refuses its notification list to GitHub App user tokens, which native
  sign-in holds (the device flow grants an empty scope), so a native profile
  sends nothing and Settings says "GitHub does not let this sign-in read
  notifications. Sign in with GitHub CLI to receive them." A 403 or 404 that is
  not a rate limit stops polling for the session the same way.
- **gh path.** Verified: `gh api --include -H "If-Modified-Since: …"` sends
  the header and prints the 304 status line and headers (exit 1, stdout headers
  only). The transport already keeps a completed nonzero `--include` answer;
  for this read only it also keeps `Last-Modified` and `X-Poll-Interval`.
- **Account binding.** Every CLI read is bracketed by `/user` account reads
  ([[BPR-1370]]): three per request, and each spends the rate limit. A poll
  begins one account operation (one pinned credential, [[BPR-1373]]), whose
  account read is the only bracket: that credential cannot answer as another
  account. Per poll: one `gh auth token`, one `/user`, one `/notifications`.
- **Schedule.** One single-shot Qt timer per profile for every window, never
  under 60 seconds or `X-Poll-Interval`; failures back off 120, 240, 480, then
  600 seconds; a rate limit waits `Retry-After` or the reset (at most an hour);
  a changed answer with under 100 requests left waits for the reset. Signed out
  or off: no timer. Offline is a failure and backs off.
- **What is new and said (core).** New by thread identity and `updated_at`
  against at most 512 seen threads. Not announced: a read thread (GitHub marks
  a thread read for whoever acted, so this is my own action), review requests
  (the review queue's own notification covers them), other reasons, and a pull
  request open in the active window. One notification per pull request; more
  within 120 seconds is held and posted once; more than three pull requests in
  one poll is one summary. Wording is a fixed table: title `owner/name #N`,
  body "New comment.", "New review.", "New review comment.", "You were
  mentioned.", "N new updates[, including a mention].", or "New activity on N
  pull requests." No title or comment text.
- **Click.** `QSystemTrayIcon` gives a click no message identity. With exactly
  one notification outstanding a click opens its pull request (focused window
  first, on Overview); otherwise the window only comes forward. A tray shared
  with the queue's sink (the macOS status item) routes a click to whichever
  posted last.
- **Baseline.** The first answer after a first launch, Reset settings, another
  account or turning the setting on only records what is there.
- **Setting.** Settings › Notifications › "Notify me about activity on my
  pull requests" with an ⓘ holding the explanation and one status line,
  **on by default** (accepted): the first poll only takes a baseline,
  posting uses the system surface so Do Not Disturb applies, and it can be
  turned off. Stored with the memory, so Reset settings returns it to on.
- **Store.** `state/v1/pull-request-activity.sqlite3`: one versioned row
  (setting, baseline, `host/account`, `Last-Modified`) and the seen threads.

## Overhead (idle, one GitHub CLI profile)

| Per hour | Count | Rate limit cost |
|---|---|---|
| `GET /notifications` (304) | 60 | 0 |
| `GET /user` account read | 60 | 60 of 5,000 (1.2 %) |
| `gh` launches (`auth token`, `/user`, `/notifications`) | 180 | — |
| Timer wakeups | 60 (coarse timer) | — |

Measured live (AC8): each 304 left `X-RateLimit-Remaining` unchanged and each
`/user` check spent one; a changed answer costs one more point. Native profiles:
zero requests. Making
the account read itself conditional (an `If-None-Match` `/user`) would bring the
rate cost to zero; that changes the shared account bracket and is left open.

## Layering

- Core `core/pull_request_activity.hpp`: decisions, wording, schedule, memory rows.
- GitHub adapter: `list_notifications` in the read contract,
  `github_notifications_read` (answer reader), `github_provider::poll_activity`.
- Application `pull_request_activity_host`: one per profile; the poll, the timer
  port, store writes on the workbench effect lane, posting.
- SQLite `pull_request_activity_store`; Qt `pull_request_activity_notification_sink`,
  `pull_request_activity_timer`, the Settings switch and
  `workbench_window::focused_pull_request` / `open_notified_pull_request`.
- Composition: built after the queue's sinks, sharing their tray; a fixture run
  never polls.

## Defect: a notification left Notification Center after ten seconds

Found by the owner's banner wizard v003 (session 8585f0ff, 2026-11-04, frozen
from 9d279d1fc). Signed in with GitHub CLI, MikeMarcinTest's first comment on
BPR-Test#148 gave one banner and its click logged `activity_activate`. With Do
Not Disturb on, the second comment (02:41:30Z) was posted by the app
(`activity_post` at 02:41:51Z, session c76d21a4) and was silent, but it was
not in Notification Center afterwards.

Cause: Qt's Cocoa tray (6.11 and the pinned 6.12.0) submits
`QSystemTrayIcon::showMessage` as an `NSUserNotification` and, for any nonzero
display hint, schedules `removeDeliveredNotification:` once the hint elapses.
Both sinks passed Qt's default of 10 000 ms, so every notification, the review
queue's included ([[BPR-1250]]), was withdrawn from Notification Center ten
seconds after it was posted; under Do Not Disturb it was gone unseen.

Fix (fa545a31e): both sinks submit through one Qt adapter function,
`bpr::qt::show_desktop_notification`, with `desktop_notification_timeout_hint()`
0 on macOS, so the notification stays until the reviewer clears it, and Qt's
default elsewhere (on Linux the hint is the freedesktop expiry, where 0 would
be a popup that never closes). `qt_pull_request_activity_notifications` pins
the hint; `architecture_boundaries` refuses any other tray `showMessage` in
`src/`. No native notification API was needed.

## Owner verification (AC9)

Wizard `bpr-1850-desktop-banner-v004`, session
1f4322ce-6d40-4e44-852d-4c1d0002db48, state
`/Users/jaeger/.codex/wizard/bpr1850-banner-v004-qduDtG/state` (complete, host
stopped), over the build frozen from fa545a31e (merged as a8b944a9d), signed in
with GitHub CLI. MikeMarcinTest's comment issuecomment-6648683560 (03:14:32Z)
was posted by the app at 03:15:27Z: the owner accepted "one banner appeared".
With Do Not Disturb on, issuecomment-6648713706 (03:16:37Z) was posted at
03:17:30Z: the owner accepted "silent, and it is listed in Notification
Center". Notes: none. BPR-Test#148 is closed and its branch deleted.

## Follow-ups (not acceptance)

- Owner decision open: native (GitHub App) sign-ins send nothing, on GitHub's
  documented refusal of App user tokens, not verified live with a native token.
- Owner decision open: a poll's account is read once as its operation begins
  instead of before and after the request ([[BPR-1370]] bracket narrowed for this
  read only; one pinned credential per operation).
- Opening at the notified comment: the anchor (REST comment or review id) is
  carried to the click, but the window opens the pull request on Overview; it
  has no lookup from a REST id to a thread identity yet.
- Windows station measurement of `sqlite_pull_request_activity_store`
  (registered `WINDOWS_UNCONTAINED`, unmeasured).
