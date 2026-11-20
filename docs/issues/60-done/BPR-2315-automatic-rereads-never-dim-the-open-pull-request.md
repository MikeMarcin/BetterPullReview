---
schema: "implement-rfc.card.v1"
id: "BPR-2315"
title: "A reread the reviewer did not ask for never dims the open pull request"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2315--BPR-2316--9d803824eace.json","origin":"owner, 2026-11-20: \"I just get the feeling like my PRs dim while I'm reading them quite often.\"","related_cards":["BPR-1575","BPR-1576","BPR-1850","BPR-1851","BPR-1867","BPR-2216","BPR-2316"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2315: A reread the reviewer did not ask for never dims the open pull request

## Asked

The owner, 2026-11-20: "I just get the feeling like my PRs dim while I'm
reading them quite often." Then: "i think its the 5 minute one I'm
noticing.. the thing is after 5 minutes usually nothing has changed so its
just disruptive for no value for it to dim like that", and: "if something did
change after the cheap anything new then im fine with dimming".

## What dimmed, and what started it

Every reread of the open pull request ran the reviewer's Refresh
presentation, whoever asked for it:

- **The Review veil** ([[BPR-1576]]). `update_pending_panes` laid
  `reviewPendingVeil` over the drawers and the centre (`CentralSplitter`)
  whenever a read was under way on a pull request already shown. Past
  `pending_reveal_milliseconds` (400 ms) its look is `refreshing`: a wash of
  the pane's ground over everything (the dim) and a sweeping rule. A whole
  read is several paged GraphQL reads, seconds on a real pull request, so
  nearly every reread showed it. Only [[BPR-1851]]'s case (the file on screen
  already current) took the partial plate instead.
- **The status line** said "Loading pull request details… N pages received."
  with its turning mark, and the Updates line dropped what it said.
- **The Refresh button** turned and read as engaged, the status bar's
  freshness said "Updating…", and a read that found nothing still left the
  settled tick.

Nothing set widgets disabled or changed opacity; the Overview's section veils
and the comparison veil wait only on a first load or a file's own comparison,
not on a reread.

The rereads the reviewer did not ask for, all through
`workbench_window::run_automatic_refresh`:

| Cause | Where | How often |
|---|---|---|
| `return` ([[BPR-1867]]) | `changeEvent(ActivationChange)` → `refresh_due_on_return` | Every activation of the window once its update is 300 s old: coming back from another application, and also closing Settings, a dialog or any other top-level window of the application. Reading one pull request for five minutes and then opening and closing anything is enough. The owner's case. |
| `activity` ([[BPR-1850]], [[BPR-1867]]) | the profile's notification poll → `ActivityArrived` | At most once a poll (60 s at the fastest, backing off to 600 s), and only when the notification list carries a new thread update on the shown pull request: the owner's own pull requests and ones they take part in. |
| `publication` | after a review, reply or resolution is published | Once per publication the reviewer makes. |
| `edit` | after a description or comment edit is saved | Once per saved edit. |

No other caller dispatches `refresh_pull_request_intent` or triggers the
Refresh action outside fixture courses; agent runs, the linked clone and an
epoch's preparation do not reread the pull request. Not changed here: when a
reread brings a push, the comparison the new head needs is prepared under the
diff's own comparison veil, as any new comparison is.

## What changed

- **Core decides** (`refresh_feedback.hpp`). `core::refresh_quiet(state)` is
  true while a read the reviewer did not ask for runs (automatic or handed
  over); `core::refresh_settled_cue(before, after)` says whether an ended read
  leaves the tick and the note: the reviewer's always, a quiet one only when
  it brought something. `core::refresh_cause` names what asked for a reread.
- **Qt presents.** While `refresh_quiet` holds, the Review veil stays
  `ready`, the status line and the Updates line keep their words, the button
  neither turns nor reads as engaged, and the freshness keeps saying how old
  the shown update is. The content is replaced in place when the read lands;
  [[BPR-1575]]'s quiet "Updated automatically: …" note follows only when it
  brought something. A failure is still said (note, failure dot, Retry), with
  nothing dimmed. A first load keeps its own loading look.
- **Refresh during a quiet read** makes it the reviewer's: the core keeps the
  read under way (it refuses a second one), and the window promotes it, so it
  shows its wait from that gesture on.
- **The diagnostic log** names each reread's cause on its
  `github.pull_request.started` line as `refresh_cause=reviewer|activity|return|publication|edit`
  (`github_window_read_host::dispatch`'s new cause; the export admits exactly
  these). An opening names none.
- **Rule for the owner's "fine with dimming".** One rule for every reread the
  reviewer did not ask for, the return's included: it never dims, whether or
  not it finds something. A reread that brings news says so with the quiet
  note instead. Making the return reread cheap (ask GitHub whether anything
  changed before reading it whole) is [[BPR-2316]]; it needs a new provider
  read through the read contract and the fixture packs.

## Acceptance

- AC1. The core's decision table: quiet only while a read the reviewer did
  not start runs; a reviewer gesture during it makes it shown; the settled cue
  only for the reviewer's read or one that brought news; never for a failure.
- AC2. An activity-triggered and a return-triggered reread, in Review and in
  Discuss, leave the Review veil and every Overview section unveiled, nothing
  disabled, the status line, freshness and button as they were, and the
  window's central area pixel-identical while the read runs.
- AC3. Such a reread that finds nothing changes nothing on screen: the same
  central pixels, the same button glyph, no note, no announcement, the same
  status line; its update is recorded so the next return is five minutes on.
- AC4. An automatic reread that brings news lands in place with the quiet
  note; one that fails says so without dimming.
- AC5. The reviewer's Refresh still veils, engages the button and words the
  status line; Refresh during a quiet read does the same from then on.
- AC6. Each reread's started line names its cause, and the export admits
  exactly the five names.

## Evidence

Dev preset, macOS, offscreen, branch `claude/bpr-2315-quiet-automatic-rereads`
rebased on main `eebc0609f`:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `core_refresh_feedback` `test_a_read_the_reviewer_did_not_ask_for_stays_out_of_the_way` (every phase and trigger; promotion; cue for nothing, news and failure; a handed-over answer) | Passed |
| AC2, AC3 | `qt_refresh_feedback_window` `test_an_automatic_reread_never_dims_what_is_read`: `activity` and `return` causes, each in Review and Discuss, past the reveal; the window's central area and the Refresh glyph compared pixel for pixel before, during and after a reread that found nothing | Passed |
| AC2 | `application_open_pull_request_activity_public_path`: the composed application's activity reread never veils the Review pane or disables the centre while it runs, and the file stays chosen and scrolled | Passed |
| AC4 | `qt_refresh_feedback_window` `test_an_automatic_reread_that_brings_news_lands_in_place`, `test_an_automatic_update_is_quiet_unless_it_brings_news` | Passed |
| AC5 | `qt_refresh_feedback_window` `test_the_reviewers_refresh_still_shows_its_wait`, `test_the_reviewers_refresh_is_seen_from_its_first_turn`; `application_refresh_feedback_public_path` (the reviewer's refresh is veiled with the rule, then lifts) | Passed |
| AC6 | `application_github_window_read_host` (open names none; `activity`, `return`, `reviewer` named on each refresh's started line); `application_release_maintenance` `test_pull_request_refresh_cause_export` (known causes kept, anything else omitted, either policy) | Passed |
| All | `application_refresh_current_file_public_path`, `application_home_return_public_path`, `architecture_boundaries`; `format_check.py` (clang-format 22.1.8) on every changed C++ file; `verify_documentation.py`, `verify_documentation_tests.py` | Passed |
| All | `tidy.py` on every changed translation unit | No finding on a changed line; three predate this card (`workbench_window_defects_and_home_rows.cpp` branch clone at the Home list and an analyser null-call at line 579, `come_back`'s name in the activity course) |

Not run: the whole suite, Windows and Linux. No live GitHub course: the
change is presentation and a log field; the fixture courses drive the real
read path.
