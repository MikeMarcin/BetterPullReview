# RFC-BPR-0100 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `tour-conversation`
([[BPR-1995]], [[BPR-1996]]; closeout [[BPR-1997]] belongs to
`accepted-scope`). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.
Product candidate `e6e84653e`. Frontend-design was loaded before the Qt
surface work. Owner amendments in force: "If you need it you need it go build
it."; "main should be the source of truth for the tracker"; "Continue working
unattended, you can file reviews but dont block on their results"; and the
owner's standing feedback: no explainer text taking space (tooltips
instead), nothing moving on click, consistent button sizes, no crowded button
rows. Debug timings are nongating; nothing below is a timing claim.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 only the composer and the session line under the transcript; a replay or a moved head adds its one notice | `qt_tour_side_chat` (8): with a native session and an unmoved head the visible children under the transcript are exactly the composer field, Send, the Session menu and the cost; the passage is a chip inside the field; a replay shows only `tourContinuationMode`, a moved head only the revision line with Continue old revision, both above the composer, and the notice goes with its cause | Pass (fixture); owner look accepted (AC09) |
| AC02 the link in every thread names the part and sends the update; no other control | `qt_tour_side_chat`: the Solution's and a stop's threads show "Nothing in the Solution changed. Update the Solution from this chat" and "Nothing in this stop changed. Update this stop from this chat"; following it dispatches the update turn; no `tourSideChatUpdate` exists; while the host is busy the words stay without the link. The whole tour's thread is drawn by the same code | Pass |
| AC03 the contracts state it; five fixed change requests each yield a card in the first answer | `application_tour_agent_conversation` (`test_rfc_0100_contracts`): every thread's contract answers a request with its change in the turn and a question in words; the whole tour's says it beside `bpr.tour-revise.v1`. Live, installed Claude Code 2.1.319 (Sonnet) through the composed app on nested-folders#21 after Draft the tour (8 stops): `live-tour-requests` carded 5/5, below | Pass |
| AC04 Accept all, Review each, settling in either place | `qt_tour_agent_session_view`: the card lists "Merge stops 2 and 3 into “Both” / Remove stop 4"; Accept all dispatches both acceptances; Review each shows the run's first waiting part (`reviewed_revision`); presenting the parts accepted in the tour settles the card ("Accepted into the private draft."), one each way says "1 of 2 parts accepted."; Discard discards every waiting part. `ATTENDED_TOUR_REVISION_CARD` in the revision look | Pass (fixture); owner look accepted (AC09) |
| AC05 Enhance sends a turn in the part's session; only Accept changes it; a selection replaces only the selection | `qt_tour_side_chat`: the Problem's Enhance opens its chat and sends its update turn ("Enhance the writing of the Problem."); on a selection the passage rides the turn. `application_review_tour_writing` (composed, scripted Claude): Enhance on a selection sends the Problem's update turn with the passage in its side chat and the field and the saved draft are unchanged after it ends. `core_tour_references`: `confine_to_passage` puts the passage's new text in its place, reads a whole field given back as its passage, proposes nothing when the passage is gone or stands twice. The session is the thread's: `request_tour_ask` runs in the thread's native scope | Pass |
| AC06 a link and a snippet in the app; the permalink on GitHub in the established form | `qt_tour_side_chat` (10): a stop reading "Clears first (`item.cpp:4-6`) … src/item.cpp:4-30" shows a link to `src/item.cpp#L4-L6`, the alone reference as the permalink of lines 4 to 15 (12 lines) followed by Show in Review, and a reference naming no file of the pull request as written; Show in Review opens lines 4 to 30. Snippets are drawn by the BPR-1802 permalink box through the window's snippet host. `application_review_tour_publication_native`: the published stop holds ``([`review.cpp:2-4`](https://github.com/…/blob/<head>/review.cpp#L2-L4))``, the alone reference as the bare permalink, and `elsewhere.cpp:9` as written; a restarted app does not see the published tour as changed. BPR-Test, below: both comment kinds embed the bare permalink. A tour update of an edited published stop ([[BPR-2116]], `f042726a6`): the same course edits the published stop to ``Edited after publishing (`review.cpp:3-4`).`` with `secondary.cpp:2-3` alone, updates, and the edited comment holds ``([`review.cpp:3-4`](https://github.com/…/blob/<head>/review.cpp#L3-L4))`` and the bare `secondary.cpp#L2-L3` permalink; the author's text stays as written and a restarted app counts no unpublished change and writes nothing | Pass |
| AC07 both rules in the drafting prompt, the conversation contract and the stop-comment hook; versions advanced | `application_tour_agent_conversation`: both rules in the drafting prompt and in the whole-tour and part conversations, under `bpr.tour-guide.v2` (was v1); `tour-stop-comment.md` carries both and reports `tour-stop-comment-v2` (was `review-preparation-v1`, which the other tour hooks keep); `application_writing_delivery`, `qt_settings_window` | Pass |
| AC08 the owner accepts that a drafted tour of a flag-move pull request does not describe the retained path | Fixture `flag-move-v1` (`scripts/build_flag_move_fixture.py`): the key=value loader moved byte for byte into `legacy_loader.cpp` behind `--strict-toml`. `live-tour-draft-flag-move` (Sonnet) drafted six stops; stop 3 reads "This is the old loader body moved into its own file and renamed `load_legacy_config`. Compare it with the removed lines … to check that it is unchanged." | Pass: owner accepted (`1180b2e4`, session `c46887c9`) |
| AC09 the owner accepts the lower edge and the revision card on a frozen build | Frozen build of `e6e84653e` with the revision look; native renders | Pass: owner accepted the lower edge (`8736f969`) and the card (`e1ad5848`), session `c46887c9` |

## BPR-Test: how GitHub draws a reference (2026-11-11)

On MikeMarcin/BPR-Test #43 (head `18be4faf…`), one conversation comment and
one review comment (on `prefetch-probe/module_00.cpp` line 24) each carried an
inline Markdown link and the bare permalink
`https://github.com/MikeMarcin/BPR-Test/blob/18be4faf590179f772edaea4beb4deb556e02473/prefetch-probe/module_01.cpp#L2-L5`
alone on its line. Read back with `Accept: application/vnd.github.full+json`,
both `body_html`s were the same: the inline link an ordinary `<a>`, the bare
permalink a `<div class="Box Box--condensed">` headed
"BPR-Test/prefetch-probe/module_01.cpp", "Lines 2 to 5 in 18be4fa", with the
lines in a `blob-wrapper-embedded` table. Both comments were deleted after
reading. So both kinds embed, and the published form is: inline, a Markdown
link to the commit-pinned permalink; alone on its line, the bare permalink.
Rate limit before 4997, after 4968.

## Live courses (installed Claude Code 2.1.319)

`QT_QPA_PLATFORM=offscreen BPR_LIVE_TOUR_MODEL=sonnet build/dev/tests/end_to_end/better_pull_review_agent_chat_composition_public_path live-tour-requests /opt/homebrew/bin/claude`
(opt-in, never a ctest row):

```text
live-tour-requests fixture: stops=8 model=sonnet
"Remove the last stop.": card=1 parts="Remove stop 8"
"Reverse the order of the stops.": card=1 parts="Move the stops into the order 8, 7, 6, 5, 4, 3, 2, 1"
"Rewrite the Problem in one sentence.": card=1 parts="Rewrite the Problem"
"Make the first stop's explanation half as long.": card=1 parts="Rewrite stop 1, “New renderer API”"
"Add a stop on the test that covers this change.": card=1 parts="New stop “Renderer test calls resize” (tests/engine/renderer_tests.cpp, lines 5 to 7)"
live-tour-requests carded=5/5
```

Each revision was discarded from its card, so each request met the same tour.
`live-tour-draft-flag-move` (same binary, `BPR_LIVE_TOUR_DRAFT_OUT` names the
output) drafted the AC08 tour; its text is the session candidate's media.

## Runs

At `e6e84653e` (dev build): `core_tour_references` (new),
`core_agent_reference`, `core_tour_agent_session`, `core_review_tour*`,
`application_tour_agent_conversation`, `application_writing_delivery`,
`application_review_tour_writing` and `_wrapper`,
`application_review_tour_publication_*` (12 courses),
`sqlite_review_submission_recovery_storage`, `qt_tour_side_chat`,
`qt_tour_agent_session_view`, `qt_review_tour_agent_help`,
`qt_scoped_prose_editor`, `qt_settings_window*`, `github_recorded_fixture_contract`,
`architecture_boundaries`, `test_network_isolation_manifest`,
`application_composition_source_contract`,
`application_review_round_trip_composition_contract` (resealed),
`coding-format-check`, clang-tidy on the new and changed core, application,
Qt and app sources, the documentation gate. A run of every `qt_`, `core_` and
`sqlite_` row (431) found two failures outside this change, left as they are:
`qt_profile_management_window` (the window's Pull Request menu has Mark New
Commits Seen and a checkable Compose review that the profile window's list
lacks) and `qt_workbench_conversation_reveal` (Page Down in the notes). Rows
whose end-to-end executables were removed earlier for disk were not run. Not
run: the whole dev suite, the core-only preset, Windows and Linux.

## Decisions made here

- The update link covers "Update the part from this chat" whole rather than
  its last four words: the action's words are the link's target, and a
  six-word target is easier to hit than "tour from this chat".
- Review each takes the tour to the revision's first waiting part; the parts
  stay grey there and are decided one by one as before.
- A selection's Enhance asks the agent for the passage's new text and the
  core puts it back in place, so the proposal can only replace the
  selection; a passage that is no longer in the part, or stands twice,
  proposes nothing.
- The tour's buttons for an empty field (Draft from PR context, Generate
  comment) still use the one-shot hooks; RFC-BPR-0100 moves Enhance only.
- References resolve against the pull request's changed files. A reference to
  an unchanged file stays as written and is said before publishing, since the
  app cannot show an unchanged file and the head's file list is not known.
- A stop text's permalinks use the tour's pinned head; "before the change"
  uses the base.

## Known limits

- Fixed by [[BPR-2116]] (`f042726a6`): an already published stop whose text
  is edited was republished by a tour update with its references as written.
  Two causes: the window never handed a published tour's preview the pull
  request's files (so nothing resolved), and the update rendered the stop
  from the core, which has no permalink maker. The window now observes the
  files for an update too, and the plan writes an edited published stop's
  text in its published form in the frozen update tour.
- The Problem and the Solution link their references on the Overview; in the
  Tour tab they are edited as source, not rendered.
- `qt_tour_agent_session_view` fails under the native-look environment at an
  existing focus-geometry check before any RFC-BPR-0100 step; offscreen it
  passes. Its revision card render was made offscreen; the chat's native
  renders come from `qt_tour_side_chat`.

## Owner look `c46887c9` (2026-11-11)

Handed back at sequence 51 with all three candidates accepted
(`session.json`): `RFC-BPR-0100-chat-lower-edge` (`8736f969`),
`RFC-BPR-0100-revision-card` (`e1ad5848`), `RFC-BPR-0100-flag-move-draft`
(`1180b2e4`); no decision notes. After accepting the lower edge the owner left
one feedback note on it, a suggestion rather than a rejection, recorded
verbatim: "Instead of session number can we get the name/title of the session
from the agent? Maybe that title wouldn't be good I dunno. Being able to name
it like vscode let's you would be cool." (coordinator: a follow-up card if
wanted). Host 19967 (Tailscale child 19990) stopped with
`attended_review.py stop`; root `tour-conversation-v001.l3mto90j` removed with
`freeze_attended_build.py remove`. BPR-1995 (AC09) and BPR-1996 (AC08) Done.

## Closeout review (2026-11-11): left open

[[BPR-1997]] was not closed. AC06 requires the published comment to carry the
commit-pinned permalink; a tour update that republishes an edited, already
published stop writes its references as written (Known limits above), so on
that path the criterion is not met. [[BPR-2116]] (Backlog) fixes it and
[[BPR-1997]] now depends on it. Every other criterion passes as recorded. The
owner's suggestion to name a session is [[BPR-2115]] (Backlog).

## BPR-2116: a tour update republishes references (2026-11-11)

Product commit `f042726a6`. Test first: the extended
`application_review_tour_publication_native` failed on `f35071d0e` with "the
updated comment does not keep the reference as written" (the updated body
read ``Edited after publishing (`review.cpp:3-4`).``). A debug run showed the
plan had no files for a published tour (`files=0`), the first cause above.
With both causes fixed the course passes; with the plan's rewrite disabled
(the window still observing files) it fails at the same check (negative
control). `core_review_tour_update` checks `tour_stop_text_edited`: a stop
published as written, or whose author's text is the published one although
its comment carries permalinks, is not edited; a changed stop is.

Runs at `f042726a6`: `core_review_tour_update`, all twelve
`application_review_tour_publication_*` courses, 62 `tour` ctest rows,
`application_review_round_trip_composition_contract` (pin resealed),
`application_composition_source_contract`, `architecture_boundaries`,
`coding-format-check`, clang-tidy on the three changed sources (clean).

## Closeout (2026-11-11)

[[BPR-1997]] judged every criterion against the evidence above; all pass,
AC06 on both the first publication and a tour update of an edited published
stop. Evidence reused unchanged except BPR-2116's runs. Not run: Windows and
Linux, the whole dev suite. RFC-BPR-0100 is Auto-Verified.
