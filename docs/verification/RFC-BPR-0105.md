# RFC-BPR-0105 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `dependable-session`
(BPR-2010, BPR-2011). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.
Frontend-design was loaded before the Qt surface work. Debug timings are
nongating; nothing below is a timing claim.

The public-path course is `application_dependable_session_public_path`
(`tests/end_to_end/dependable_session_public_path_tests.cpp`):
the composed desktop, a profile window, two pull requests opened from Home,
the real review page and its buttons, the fixture GitHub server and the
normal diagnostic log. Each step prints what the review page shows
(`dependable-session:observe`).

## AC01: the stall reproduced

The course was built and run against the baseline with only the test and two
fixture holds added (a detached checkout of `dc5453afa`). Excerpt of its
observations, unedited apart from line cuts:

```text
step=first-ready         draft=drafting   approve_enabled=1
  (the shared publication lane is now held by an earlier effect)
step=second-opened       draft=none       approve_enabled=0 status="Checking for an unfinished review…"
step=second-after-wait   at_ms=10000      draft=none approve_enabled=0 status="Checking for an unfinished review…"
step=second-after-release draft=drafting  approve_enabled=1
  (#18 ready; the lane is taken again, then Approve)
step=held-approve-pressed draft=submitting waiting="Waiting for GitHub to confirm this review…"
step=held-after-wait     at_ms=10003      draft=submitting waiting="Waiting for GitHub to confirm this review…"
step=held-after-release  draft=drafting   approve_enabled=1
```

Both waits last exactly as long as the earlier effect and end the moment it
does; no log line names them. The RFC's cause section was amended with this
record ([Found cause](../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md#found-cause-bpr-2010-2026-11-10)).
The owner's own session was not logged, so the effect that held the lane then
is unknown. A diagnosis run also found that, in this desktop fixture, an
approval followed by "Finishing a review" returning Home closes the review
and invalidates its authority; that is the configured behaviour, identical at
the baseline, not a defect.

## Results at the candidate

| Criterion | Check | Result |
|---|---|---|
| AC01 | Baseline course above; RFC cause amended | Reproduced and recorded |
| AC02 | Course steps 1 and 2: the shared publication lane held, and #17's own submission held on its lane while the reviewer leaves it; #18's Approve is offered at once and publishes in both | Pass |
| AC03 | Course step 3 (bound shortened to 1.5 s by the course): Waiting with "Waiting for …", then at the bound "Not published. Still waiting for … Your review is kept." with Retry, summary and verdict intact, nothing sent; Retry publishes once released. The 20-second constant is `core::review_submission_wait_bound` | Pass |
| AC04 | Course step 4: the window's authority changes while a submission waits; its answer from the retired host ends the page as not published ("Publication cancelled. Your draft is kept."), never left submitting | Pass |
| AC05 | Course step 5 reads the normal diagnostic log: `review_submit_request`, `review_submit_wait` with `submit_subject` and `duration_ms`, `review_submit_send`, `review_submit_drop submit_subject=retired`, `review_submit_end` with `published` and `wait_expired` | Pass |
| AC06 | Course step 6: #17 left on the Tour tab with the tour side chat open; #18 opens on Comments with no side chat and no tour page; the same again when #18 is reopened from Home | Pass from Home (side chat opened through its own widget entry point, not an agent run). Links and restored reviews take the same signalled path; a notification's open was not exercised |
| AC07 | Course step 6: `view_filters()` and the Comments drawer width are equal before and after | Pass |
| AC08 | Approval on BPR-Test through the window's own Approve after five or more pull requests with agent and tour use (amended 2026-11-10: agent-run, not the owner's run) | Pass: the agent-run live course approved #151 after six pull requests; see [the run](#agent-run-live-course-2026-11-10) |

Focused unit evidence: `core_operation_diagnosis` (new submission lines and
their refusals), `application_review_submission_application_host` (abandon
before start, start before terminal), `qt_review_submission_editor_scale_*`
(waiting, sending and not-published states, offscreen renders in light and
dark). Existing whole-review and tour publication courses were rerun; two
waits in `review_submission_composition_public_path` now wait for the start
and the terminal, since a held decision lane now also holds the start.

Integration run at the candidate `31f900910`: full `ctest --preset dev -j10`,
950 rows, 940 pass, 4073 s. Failures, each reproduced unchanged on a separate
baseline `dc5453afa` checkout (none caused by this milestone):

- `application_agent_chat_custom_agent_contract` (`custom-agent-empty-refusal-said`);
- `application_agent_finding_codex_found_contract` (`cleanups-is-one`);
- `application_review_tour_cold_import_public_path` ("real preview never
  treats loaded code as pending-operation support");
- `qt_profile_management_window` (28 View actions expected; RFC-BPR-0097
  added Go to line after the test's last change).

`application_large_review_*` (five rows; `cache_bytes` skipped) exceeded
Debug budgets: Debug timings are nongating, and these count as neither pass
nor functional coverage. `application_source_pr_find` failed once under load
and passed alone. Earlier passes found and fixed two regressions of this
milestone's own first designs (see BPR-2011).

`application_composition_source_contract` was resealed for the reviewed
production app sources, and `test_network_isolation_manifest` received the new
course's unmeasured Windows pin.

Owner's answer, 2026-11-10: Accept on `RFC-BPR-0105-AC08-approve-151`
(session `26d89bac…`, decision `e5e3d58e`, no note), but BPR-Test #151 holds
no review (read with `gh`, read only). AC08 is not marked passed and BPR-2010
is not finished; the discrepancy is recorded under RFC-BPR-0099 AC08 and the
coordinator is asking the owner.

Prepared 2026-11-10, awaiting the owner: the tests-off frozen build
`/Users/jaeger/bpr-live-v001.wpvoqim9` (`freeze_attended_build.py freeze
--live`, from `42eca211e`, SHA-256 `39983ab2229e…`, self-test and launch
checks passed) and attended session `243efdc0c300f68944464fc350c097ed`,
candidate `RFC-BPR-0105-AC08-approve-151`. The owner's review was requested on
#151 so it appears in Home's review requests. After the decision, #151's
reviews are read with `gh`.

The owner rejected it ("build closes instantly"): the frozen build could not
start, for the freeze-order cause recorded under RFC-BPR-0099 AC08, not a
submission fault. Refrozen as `/Users/jaeger/bpr-live-v002.nnhzt2pb` from
`abba53926` (SHA-256 `cb02049fbf2f…`) in session
`26d89bac1f89acd7841ee451d1e5ffd1`, same candidate ID; #151 is still open
with no reviews and the owner's review requested.

Before this round, no agent live course was run for AC08: there is no composed-application live
submission harness, and the provider path that writes to GitHub is unchanged.
The test account opened
[BPR-Test #151](https://github.com/MikeMarcin/BPR-Test/pull/151) so the
owner, its reviewer, can approve a pull request they did not author.

### Live look v002 abandoned (2026-11-10)

Session `26d89bac1f89acd7841ee451d1e5ffd1` (root `bpr-live-v002.nnhzt2pb`,
frozen from `abba53926`) is abandoned. The build predated the tour-stops
redesign, so it showed the old stop card: its stops could not be accepted and
Publish stayed disabled. The owner: "this is way too many manual steps ... a
freaking mess I cant even accept these comments or try to publish ... Things
are moving around as I click them, buttons packed in like a clown car." The
Accept records `ec9e4113` and `e5e3d58e` from that session are not evidence:
RFC-BPR-0099 AC08 and RFC-BPR-0105 AC08 are **not passed**. Both move to an
agent-run live course on BPR-Test in a later round. BPR-Test is left as is
(the owner pushed `929b028f7` to #152's branch). Host 36810/36892 stopped and
the root removed with `freeze_attended_build.py remove` after the owner's app
(PID 46168) had exited.

## Agent-run live course (2026-11-10)

The owner moved AC08 to an agent-run course on the real public path; the
course and its composition seam are described under
[RFC-BPR-0099](RFC-BPR-0099.md#agent-run-live-course-2026-11-10) (lane commit
`33c282ccd`). The approval is the course's last step, in the same window and
session: #1, #2, #3 and #43 opened (Tour tab shown on each, one question to
the installed Claude Code from #1's Agent tab), the disposable #177 opened,
its tour written, accepted and published, then Back to the code, Home, and
`MikeMarcin/BPR-Test#151` opened from Home's bar: six pull requests. On #151
(authored by MikeMarcinTest; the course signed in as MikeMarcin through `gh`)
it opened the review page and pressed Approve.

What the page said, at each change (`live-session.jsonl`, 2026-11-11 UTC):

| Draft | Review page | Window |
|---|---|---|
| drafting | Approve offered | |
| submitting | Waiting panel: "Sending your review to GitHub…" | |
| (next review) drafting | page closed | "Review submitted. Closed the review of MikeMarcin/BPR-Test#151." |

The editor counted one confirmed submission. GitHub's answer, read with
`gh`: #151 (open, head `e5a0b157…`) has one review, id 5854726479, by
MikeMarcin, **APPROVED**, at `e5a0b157…`, submitted 2026-11-11T06:27:43Z.

Rate limit: 4,694 remaining before the run, 2,860 after reading the evidence
(the account's limit is shared with other work).

Observed, not changed: the first open from Home, made while the window still
read "Refreshing the pull request." after the tour's publication, was dropped
without a word; the course opened again and it took. The Agent tab never
reported idle before the question (the course waited 120 s, then sent; the
question was answered).

## Closeout (2026-11-11)

[[BPR-2012]] judged every criterion against the evidence above; all pass. AC06 was exercised from Home; a notification's open was not. BPR-2088 and BPR-2090 (Backlog) are later observations outside the criteria. Evidence reused unchanged; no product change in the closeout round. Not run: Windows and Linux, the whole dev suite. RFC-BPR-0105 is Auto-Verified.
