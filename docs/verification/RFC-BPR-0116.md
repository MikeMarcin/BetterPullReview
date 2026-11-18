# RFC-BPR-0116 verification

Pack `agent-assistance-v1`, milestone `returning-pull-request` ([[BPR-2073]],
[[BPR-2074]]; closeout [[BPR-2075]]). Added to lane
`claude/v0-5-0-prerelease-targets-52265d`'s queue by the owner ("add all the
work we planned to its current queue"). RFC base
`a9663a706fd72061f60b615bd8e749ee0ddb66b5`; milestone base `8ad26328e`.
Frontend-design was loaded before the Qt work. Debug timings are nongating;
nothing below is a timing claim.

## What was built

- **Core** (`bpr/core/returning_review.hpp`). `returning_point_of` takes the
  later of my last submitted review and my Seen (by the pull request's commit
  order; a Seen when neither is listed) and none on my own pull request.
  `decide_latest_upload` offers What changed since my review only with commits
  newer than that point: `reading`, `offered`, or `unavailable` when the
  earlier revision's material cannot be read. `proposed_fix_offered` offers
  Was this addressed? on a review thread I commented in once the head differs
  from my last comment's commit (never on a note, never when the read does
  not name that commit). `read_addressed_claim` reads the four lead words.
  `render_returning_change` turns a material comparison into unified text
  (three lines of context, touching hunks merged, non-UTF-8 said as binary);
  `bound_returning_changes` keeps it within 96 KiB and 64 files and counts
  the rest. `supplement_returning` carries a run's material, with its bounds.
- **Subject rule.** `acts_on_selection(evaluate_proposed_fix)` is now false:
  it acts on a conversation (`docs/ux/ai-experience.md` says so).
  `decide_agent_question` sends What changed since my review as a pull
  request question through `analyze_latest_upload`, and Was this addressed? as
  a conversation question through `evaluate_proposed_fix`, refused without a
  conversation.
- **Prompts.** Two built-in prompts, `review.latest-upload`
  (`review-latest-upload-v1`) and `review.proposed-fix`
  (`review-proposed-fix-v1`), in Settings › Agents & context › Built-in
  prompts under Agent tab, with the editor, default, version and Reset the
  others have; profile schema 30 (two columns), the prompt provenance and
  import journal tables widened to hooks 0 to 12; `bpr tour hooks` names
  them `review-latest-upload` and `review-proposed-fix`.
- **Material** (`window_returning_review.cpp`). On each read of the pull
  request the window works out what each question offers. For What changed,
  it asks the review's own objects which files differ between the earlier
  revision and the head, reads each at both commits (at most 64, a side over
  256 KiB said as too large) and compares them with the review's diff engine;
  the cache not yet serving the head is asked again shortly, a bounded
  number of times. For each offered thread it reads the lines my comment was
  on at its commit (`returning.then`) and the thread's lines at the head
  (`returning.now`). Nothing here launches an agent or touches GitHub.
- **Launch.** A returning turn's first answer runs on
  `assistance_first_answer` (RFC-BPR-0101 as amended 2026-11-12): fresh (no
  native session, `--no-session-persistence`), no grant, no scratch folder,
  no MCP server, its own prompt as the system prompt. With Claude it is one
  tool-free call (`WritingOnly`) whose request carries both revisions and the
  change or the lines then and now; the review skill and earlier Agent tab
  turns are not sent. **Named launch (AC13):** an agent with no tool-free
  mode (Codex) runs the same request fresh with the Agent tab's ordinary tools and no scratch folder.
- **Surfaces.** Pull Request › What Changed Since My Review beside Mark New
  Commits Seen, enabled while offered; while unavailable its words say why
  ("(code at 0badc0d can't be read here)"). The Agent tab's first suggestion,
  in the Agent accent, while offered; while unavailable a quiet line stands
  where it would. A thread offered Was this addressed? shows it as a text
  control in its header (Agent accent, at the header's tool height), in its
  menu, and first among its conversation prompts. Its answer shows
  "Agent's claim: Partly" above the reply; every returning answer offers
  Ask more, which seeds the next Agent tab question with the one-off
  question and answer (that turn resumes the Agent tab's own session).
- **Fixture.** `returning-review-v1`
  (`scripts/build_returning_review_fixture.py`): conventional-comments-v1
  read by `jiayin`, who reviewed its first head and commented in the
  line-12 thread there; served under the reuse phase at the next push,
  which clamps that resize.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `core_returning_review` (offered only with newer commits; names the earlier revision; review against Seen). `application_returning_review_latest_public_path`: offered in the menu and the Agent tab; the press is one call whose request carries `returning.earlier_head`, `returning.current_head` and the change to `src/engine/renderer.cpp` (echoed `changes=1`); its code references are links. `application_agent_launch_policy` (`test_a_returning_question_is_one_fresh_tool_free_call`). | pass |
| AC02 | Superseded in part by the [owner amendment of 2026-11-17](../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md#owner-amendment-2026-11-17-the-past-is-read-at-the-press): the press is refused saying why, and the item is no longer disabled in advance ([[BPR-2251]]). As first verified: `core_returning_review` (`unavailable` names the revision). `application_returning_review_gone_public_path`: with my review taken to be on a commit the review's objects lack, the menu item is disabled and says the code can't be read, the Agent tab says so where the suggestion would be, no suggestion stands in its place, and a press launches nothing. | pass |
| AC03 | `core_returning_review`, `core_agent_chat_scope` (two of eight entry points act on the selection). `application_returning_review_addressed_public_path`: exactly the line-12 thread offers it; the press, with no selection, asks about that conversation (`conversation=1`) with its lines then and now (`then=1 now=1`). | pass |
| AC04 | `core_returning_review` (the four words, longest first, start only). The addressed course: "Agent's claim: Partly" above the answer; review mutation evidence (submissions, target observations, stored comments) unchanged. No defect disposition is touched by the path. | pass |
| AC05 | `application_agent_launch_policy`: two prompts, purposes and versions; an edit reaches only its own prompt; Reset restores the built-in; the latest and addressed courses' stub reads its own prompt in the system prompt (`system=1`, `system=2`). Storage rows: `sqlite_workbench_settings_storage`, `sqlite_agent_configuration_storage`. | pass |
| AC06 | The latest course: no launch from opening or reading the returning pull request (`AgentProcessLauncherInvocations` 0 before the press), one launch per press, no GitHub mutation. Nothing in the path writes Seen or attention state. | pass |
| AC07 | Owner's look on a frozen build. | pending |

## Live check (BPR-Test, read facts only)

2026-11-12, `X-RateLimit-Remaining` 4995 at the read. MikeMarcinTest opened
disposable BPR-Test #180 from its fork (`returning/resize.cpp`, head
`0a60216e`); MikeMarcin submitted a COMMENTED review with a line comment on
line 3 ("Clamp the width to at least one pixel."); MikeMarcinTest pushed
`14376934` clamping it. GitHub's GraphQL, read as MikeMarcin: the review's
`commit` is `0a60216e` and the head `14376934` (What changed is offered); the
comment's `originalCommit` is `0a60216e` (Was this addressed? is offered); the
thread is `isOutdated` with `line` null and `originalLine` 3, because the fix
changed the commented line. That last fact changed the build: a thread GitHub
no longer places at the head now reads its lines at the head by the same
numbers, with context, instead of carrying none. #180 was closed and its fork
branch deleted. No agent call was made live.

## Decisions for the owner

- P03 (an answer is a claim and changes no state) is a proposal this
  milestone depends on; taken as written, the conservative reading.
- Where a fresh first answer shows: the Agent tab, as RFC-BPR-0116 says ("an
  ordinary Agent tab reply"), marked by Ask more; it is not part of the
  Agent tab's native session, so the next question resumes that session
  without it unless Ask more seeds it.
- What changed compares only the pull request's files (and their earlier
  paths); a file the commits changed and then changed back is not read.
- An outdated thread's lines "as they are" are read at the head by the same
  line numbers, with six lines of context each side; GitHub gives no place
  for it there.
- AC02's composed course names the unreadable revision through a testing
  seam (`force_returning_review_head`): the fixture GitHub refuses to load a
  review on a commit it cannot serve, so the read cannot carry one.

## Owner verdicts (attended session ff82755e1b9fba2463ef8b257a6de0f5, handed back 2026-11-13T03:34:12Z)

Owner words verbatim. Recorded only: another round reworks both; the returning
review code is unchanged here. Host 48480 and root
`/Users/jaeger/returning-review-v001.34b39a8g` were removed after hand-back.

- `BPR-2073-what-changed`: **Reject** (decision `40b79eb4`, 2026-11-13T03:27:25Z)
> I don't like that the drawer is panning, seems layout isn't quite right.
>
> I don't like that the top menu is the only way to access this feature, that stuff is going to be mostly ignored by users. Oh I guess it's also a suggested prompt? Okay I guess it's fine to stay if it's like that but it needs an icon in the menu bar then that make it clearly an AI feature. I dunno if we can apply our ai color up there.
- `BPR-2074-was-addressed`: **Reject** (decision `e789321a`, 2026-11-13T03:34:11Z, screenshot attached in the session)
> I dunno it doesn't read well with a narrow drawer thats for sure. I don't know why 12 would get it but 6 doesnt. 
> Just doesn't seem like a great place for this.
>
> I think we can do better.

## Closeout (2026-11-16)

Owner, 2026-11-16, on the hygiene audit: close; delivered through
RFC-BPR-0128 or superseded, the implementer's call. Closed as **delivered
through RFC-BPR-0128** by [[BPR-2075]]. The verdicts above were answered by
the Ask model the owner then chose: What changed since my review and Was this
addressed? are built-in rows of the review bar's, a file's and a thread's Ask,
answered as report cards and reports, with the claim a chip in the thread's
header. The owner accepted those controls and answers in the RFC-BPR-0128
looks ask-v002, ask-v008, report-templates-v003 and report-templates-v004
([ledger](RFC-BPR-0128.md)). The returning-review courses that carry AC01 to
AC06 stayed green through RFC-BPR-0128's integration pass at `7b3fe6eaa`.
[[BPR-2074]] closes as superseded.
