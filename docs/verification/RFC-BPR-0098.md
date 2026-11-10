# RFC-BPR-0098 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `home-next-move`
([[BPR-1989]]). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`;
milestone base `f1f6cd22c`. Frontend-design was loaded before the Qt work; the
design round's Home frame was the starting point, and the shipped Home tones
and colour roles were kept. Debug timings are nongating; nothing below is a
timing claim.

## What was built

- **Facts.** `queue_waiting_facts` carries GitHub's review decision, the
  merge state, the running-check count, every other reviewer's standing
  verdict (at most eight, by login, decided per reviewer as
  `my_review_for` decides mine) and the conversations awaiting my reply
  (count, more-than-read flag) with their own staleness. They are stored with
  the row as queue facts version 4; versions 1 to 3 still read, and a row
  without them keeps its version 2 or 3 bytes.
- **Read.** Home's batched query (`ListHomePullRequests`) adds the
  `HomeConversations` fragment, `reviewThreads(last:50)` with each thread's
  resolved flag and latest comment's author, to the aliases of pull requests
  the viewer authored (`AuthoredPullRequestNumbers`). A batch of one is sent
  for a lone pull request of mine, and a return to Home re-reads one of mine
  the same way. The fragment's shape was checked against github.com with
  `gh api graphql` on BPR-Test #151 (read only).
- **Decision.** `core::next_move_for` is the decision table;
  `home_section_for` puts an authored row in Waiting on others exactly when
  its move is Waiting; `queue_precedes` ranks My pull requests by move after
  every Needs me bucket; `home_row_quiet` is what Hide quiet hides.
- **Presentation.** `home_row_word`, the move's sentence in
  `home_why_sentence`, the `home_row_role::tone` role the delegate colours the
  word (and the mark without headers) by, and My pull requests' summary.
- **Fixture.** `home_next_move_fixture_rows` (ten pull requests of mine, one
  per move a fixture can show) for the composed course and the attended look
  (`--attended-home-look --attended-home-moves`).

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `core_home_next_move`: one fixture per row of the table (Returned by decision and by conversations, Failing, Blocked by a refused merge and by a conflict, Ready, Unread, Unassigned, Draft, Waiting, Checking, Quiet, and Unknown for a row never read or whose reviews are unread); a pull request matching Returned, Failing, Blocked, Unread and Draft at once is Returned, and each row removed in turn yields the next; Ready outranks Unread and Waiting; a reviewer's change request decides when GitHub gives no decision | Pass |
| AC02 | `core_home_next_move` (`test_placement_and_order`): Needs me, then My pull requests in move order, then Waiting on others, then Following; no authored row in Needs me; Waiting on others holds exactly the Waiting moves; the relation stays a strict weak order. `application_home_next_move_public_path`: through the composed application, Needs me holds only #17 (another author's request), Waiting on others only #4037, and My pull requests reads Returned, Returned, Failing, Blocked, Ready, Unassigned, Draft, Checking, Quiet | Pass |
| AC03 | `application_home_next_move_public_path`: #4030 "alice requested changes; 3 conversations await my reply." (Needs me tone), #4032 "Approved by alice and bo; checks passed. Ready to merge." (confirmed tone), #4033 "build-linux failed; approved by alice." (error tone); no sentence says "My pull request"; header "2 returned, 1 ready to merge". `core_home_next_move`: an approved, mergeable pull request is never Quiet and Hide quiet keeps it | Pass |
| AC04 | `core_home_next_move` (`test_absent_facts_claim_nothing`): conversations not read, a change request is still Returned and conversations alone never are; reviews not read are never Unassigned or Quiet, requests not read never Unassigned. `application_home_next_move_public_path`: as a search first lists them, no row of mine reads Quiet; checks or merge state not read are never Ready. `application_review_queue_enrollment` (`test_next_move_facts_codec_version_four`): conversations not read read back as not read, never zero; a read without them keeps an earlier count only as last known. `qt_review_landing_model`: a row with no count states none | Pass |
| AC05 | `application_home_next_move_public_path`: a refresh of eleven pull requests (ten mine) is its two searches and two batched queries, exactly as before; no pull request is read alone and no review-thread read is added. `github_paged_read` (`test_retained_home_batch_reads_conversations`): only authored aliases ask for the fragment; the conversations cost no request; a batch of one of mine is one query; more than 50 threads is flagged; an authored number outside the batch, or a batch of one not mine, is refused before any request. The fragment reads at most `maximum_queue_review_threads` (50) threads and one comment each | Pass |
| AC06 | The owner's look at the rows on a frozen build | Pending: see [AC06](#ac06-the-owners-look) |

## AC06: the owner's look

Pending; filed for the owner and not waited on (owner, 2026-11-10: "Continue
working unattended, you can file reviews but dont block on their results").
Session `bb8c24c5f3cb7ed97777f4e3f1a70a78`, state
`/Users/jaeger/.attended-review-state/home-next-move-v001-2026-11-10/state`:

- `BPR-1989-home-moves`: the rows, on the frozen composed course of
  `2f772b6f0` (`/Users/jaeger/home-next-move-v003.ozia7rab`, course binary
  SHA-256 `55c9296fb8f8…`), which passes every assertion above and then
  stays open on Home (`--attended`). The freeze's own launcher `--check` passed
  (`ATTENDED_HOME_SHOWN:rows=13`, app SHA-256 `9dc6988c3032…`).
- `BPR-1989-word-place`: the move's word stays after the sentence, where Home
  draws every row's word, or leads the line as the design round drew it.
- `BPR-1989-own-pull-requests`: AC06's own pull requests, on a tests-off
  live build whose product sources equal `2f772b6f0`'s. The first,
  `home-next-move-live-v001` of `58db13bb5`, could not start (its freeze
  checked it before sealing; see RFC-BPR-0099 AC08) and was replaced before any
  answer by `/Users/jaeger/home-next-move-live-v002.s9ny7rtj` from
  `abba53926` (SHA-256 `27bf0f4818e9…`; checks passed on the root's own
  home), session event 13.

The attended home look of the app (`--attended-home-look
--attended-home-moves`) lists the rows but cannot refresh them: signed in
through the window's connect action, Home's refresh stays disabled, so the
reviews a search does not read are never read in that look. The course is
the look for that reason.

## Runs

At the product candidate: `core_home_next_move`, `core_review_queue`,
`core_review_queue_attention`, `core_home_ignore`, `github_read_contract`,
`github_paged_read`, `application_review_queue_enrollment`,
`qt_review_landing_model`, `qt_home_list_scale_{100,125,200}`,
`application_home_next_move_public_path`, and the Home regression set listed
in the handoff.
