# RFC-BPR-0101 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `agent-tab-asks`
([[BPR-1998]], [[BPR-1999]], [[BPR-2000]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt surface work. Debug timings are nongating.

## Which surfaces

The "selected-lines side chat" is `side_chat_panel`. In the shipped window its
panel sits on the hidden Agent tools page, and the Agent tab presents its
composer, Send and replies: the Agent tab's composer *is* that chat's composer,
and its replies are Agent tab turns. The rules below therefore reach it through
the Agent tab; its own composer, shown alone, also sends on Return. The tour
side chat is `tour_side_chat`.

## References (AC01, AC02)

One core recogniser ([agent_reference.hpp](../../src/core/include/bpr/core/agent_reference.hpp))
replaces the Agent tab's Qt regular expressions. It is a closed scanner for
the RFC's forms, not a general pattern engine, so no library question arose;
RE2 stays approved for source matching only. It finds `path:12`, `path:12-34`,
`path:12:5`, `path#L12`, `path#L12-L34`, `path line 12`, `path lines 12–34`,
`path (lines 12–34)`, each with "before the change", a hyphen, en dash or "to";
and a bare `line 12` naming the question's or the stop's one file. In Markdown
it links prose, emphasis and a code span whose whole content is a reference,
and leaves fenced code, other spans and existing links as written.

Resolution: a changed file by exact path, then unique suffix of whole
segments; a name several changed files share is a link that offers them
(`a|b#L7`, a menu at the pointer); a path that matches nothing is plain. This
application has no view of an unchanged file at a line, so a reference only
to one is not drawn as a link (the RFC's "or is not a link"); the resolver
takes head files when such a view exists. A range opens at its first line with
the range selected (`#L12-L34`).

| Criterion | Evidence | Result |
|---|---|---|
| AC01 Agent tab | `qt_workbench_window_scale_100`: question and answer links, a whole code span, fenced code left alone, `#L12-L34` opening 12 to 34; the long-file click course opens and reveals the line | Pass |
| AC01 selected-lines side chat | The same Agent tab turns (see [Which surfaces](#which-surfaces)) | Pass |
| AC01 tour side chat | `qt_tour_side_chat`: prose, emphasis, a whole span and a range link; fenced code does not; Copy keeps the reply | Pass |
| AC02 | `core_agent_reference` and `qt_workbench_window_scale_100`: a shared name offers both files and opens the chosen one; a missing path is plain; an unchanged file is not a link while no unchanged-file view exists | Pass (no unchanged-file view) |

## Send key (AC03)

The Agent tab's composer and the side chat panel's send on Return and on
Ctrl or Cmd with Return; Shift+Return starts a line; Return while an input
method composes is the input method's; plain Return in an empty field does
nothing (Ctrl or Cmd with Return still says why). Send's and the composer's
tooltips say so. Comment composers keep Ctrl or Cmd with Return.

Evidence: `qt_workbench_agent_send_refusal`
(`test_the_agent_chat_sends_on_return_and_says_its_cost`) and
`qt_conversation_cards_view` (Return alone in a comment composer neither sends
nor keeps it). Pass.

## Cost

### Cost: how Claude's figure accrues

Established against installed Claude Code 2.1.319 on 2026-11-10, in a scratch
folder, with `claude -p … --output-format json --model haiku` and then
`--resume <session>` twice:

| Turn | `total_cost_usd` | `modelUsage` input / output tokens | frame `usage` cache reads |
|---|---|---|---|
| 1 | 0.00279231 | 2 / 3 | 24141 |
| 2 (resumed) | 0.00549827 | 4 / 6 | 26506 |
| 3 (resumed) | 0.00631917 | 6 / 9 | 36900 |

`modelUsage` counts are running sums over the session (`cacheReadInputTokens`
24141, 50647 = 24141 + 26506, 87547 = 50647 + 36900) and `total_cost_usd` is
their cost, while `usage` is the turn's own. A per-token fit to turns one and
two predicts turn three at 0.00082, which is 0.00631917 − 0.00549827. So the
figure on a resumed session is the session's running total, and a turn costs
the difference of consecutive figures.

The core holds both numbers (`account_agent_turn_cost`,
[agent_cost.hpp](../../src/core/include/bpr/core/agent_cost.hpp)): a run that
resumed the runtime's session cost the difference from the conversation's
last figure; with no earlier figure the turn shows none rather than the whole
session's spend; a lower figure means the runtime counted again; a run that
started a session cost its figure. The native conversation keeps the last
figure and the total with its history (`native_agent_costs`), and New session
starts at none. Costs are billionths of a dollar, so turns sum exactly.

| Criterion | Evidence | Result |
|---|---|---|
| AC04 | `core_agent_cost`: 0.4123567 → `$0.41`, 0.004 → `< $0.01` (amended from `under $0.01`, [[BPR-2038]]), 12 → `$12.00`; no figure, no cost. `qt_workbench_agent_send_refusal`: the footers and the session line | Pass (re-run at the BPR-2038 candidate) |
| AC05 | [Installed Claude course](#installed-claude-course): three native turns, each footer a cost, the session line their sum, the interpretation recorded above | Pass |
| AC06 | The tour side chat's ending line gives the run's cost after its time and the open chat's session line its total (`tour_thread_cost`); tour runs take the same accounted figures | Pass (unit and composed path; no installed tour run) |

## Colour (AC07, AC09)

Agent accent is a palette role (`agent_accent`, appended to the 64 roles;
Appearance › Colours lists it under Surfaces and text) with built-in `#4A3FD6`
(Light), `#A3A0FF` (Dark), `#3A2FC4` (High Contrast Light, 8.8:1 on white) and
`#B8B5FF` (High Contrast Dark, 11.1:1 on black), held to the text floor on the
application surface. `agent_accent()` reads the theme on screen's role, which
the application sets from the appearance profile and announces to the
window's widgets; no surface keeps a fixed value. The rule and the inventory
are in [shared-action-buttons.md](../ux/shared-action-buttons.md#the-agent-variant).

| Criterion | Evidence | Result |
|---|---|---|
| AC07 | `test_the_agent_accent_is_on_exactly_the_run_controls` walks every button of the composed window: the marked run controls (`agentAsk`, each `agentSuggestion`, `tourSideChatSend`, `tourSideChatUpdate`, `tourDraftTour`, `tourStopMagic`, `tourProblemMagic`, `tourSolutionMagic`, `tourSuggestOutline`) wear the accent and no other button does. Resend (Agent tab and tour chat) and the Review Cycles Send are marked where they are made | Pass |
| AC09 | `qt_appearance_settings_panel`, `qt_colour_role_list`, `qt_appearance_role_display` (listed, 63 offered); the inventory test with a changed accent and after Reset; `core_appearance_contrast` (built-in themes pass); `application_appearance_older_theme_contract` (a theme without the role imports with the built-in value) | Pass |
| AC08 | The owner's look on a frozen build | Accent: pass (session `fd2f7fdb4d5b89ca67e9793084a68d04`, decision `e8d273c6`, frozen build of `c1d445720`; the Colours filter-box note is [[BPR-2039]]). Footers: rejected there (decision `3dd7f36f`: "one line for both turn and session status lines at this default width"); reworked by [[BPR-2038]] at `b1d66829d`, whose window test asserts both lines fit one line at the default width; the owner accepted the rework (session `9b8a51a3e23f4f46260bdb9d4832f5bf`, decision `f55208c9`, event 5, frozen build of `b1d66829d`). Pass |

## Installed Claude course

`live-claude`, an opt-in course of
[agent_chat_composition_public_path.cpp](../../tests/end_to_end/agent_chat_composition_public_path.cpp)
that ctest never runs, because it spends the owner's Claude account:

```sh
QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/better_pull_review_agent_chat_composition_public_path live-claude /opt/homebrew/bin/claude
```

The composed application on the `nested_folders_v1` fixture
(`bpr-fixtures/nested-folders#21`, 17 changed files), Claude Code 2.1.319 added
in the agent setup and set to Haiku with the Agent tab's own `/model haiku`,
README.md open in Review, then three turns from the Agent tab: the Review this
pull request suggestion and two questions typed and sent with Return. Run on
2026-11-10, unedited apart from the long answer cut:

```text
live-claude review: outcome=answered footer="Claude Code (Haiku 5.5) answered in 1:01 · $0.01. 5 findings, above." reported=0.012785600000000001 session="$0.01 this session"
live-claude open=README.md other-files-discussed=13 (docs/api/reference.md, docs/api/v2/endpoints.md, docs/guide.md, src/engine/renderer.cpp, src/engine/renderer.h, src/engine/shaders/basic.frag, src/engine/shaders/basic.vert, src/engine/shaders/post/bloom.frag, src/engine/shaders/post/tonemap.frag, src/main.cpp, src/ui/widgets/slider.cpp, tests/engine/renderer_tests.cpp, tools/build/scripts/package.py)
live-claude second: outcome=answered footer="Claude Code (Haiku 5.5) answered in 0:05 · under $0.01" reported=0.015335010000000001 session="$0.02 this session"
live-claude third: outcome=answered footer="Claude Code (Haiku 5.5) answered in 0:03 · under $0.01" reported=0.01702213 session="$0.02 this session"
live-claude native: process-starts=3 workspace-starts=1
live-claude turn-costs=12785600 2549410 1687120 sum=17022130 session-total=17022130 session-line="$0.02 this session"
agent-chat-live-claude:pass
```

Three processes in one native workspace, the second and third resuming the
first's session: the reported figure grows as a running total, each footer
shows the difference, and the session line is their sum, which equals the
runtime's last figure. AC05: Pass.

Two earlier runs are kept as diagnosis. Without the agent in the setup the
turns were not native (each process its own session); the figures fell
(0.0099, 0.0072, 0.0032) and each was the turn's own, which the ledger
reads the same way. A run with the agent in the setup showed no cost on the
first turn: the host read whether a run resumed after the launch had saved
the new session's handle. It now records that at launch
(`ResumedNative`), and the run above is after the fix.

## Closeout (2026-11-11)

[[BPR-2001]] judged every criterion against the evidence above; all pass. AC06 rests on unit and composed courses; no installed-agent tour run was made. Evidence reused unchanged; no product change in the closeout round. Not run: Windows and Linux, the whole dev suite. RFC-BPR-0101 is Auto-Verified.

## Agent launch policy (owner, 2026-11-12, [[BPR-2138]], [[BPR-2141]])

The owner's classification of fresh and resumed runs and the record of the
whole round (MCP servers, system-prompt instructions, argument order, sign-in
failures; their own ledgers point here). Frontend-design was loaded before
the Qt work.

What was built:

- `core::agent_run_path` names sixteen launch paths and `session_mode_of`
  gives the owner's classification; `ask_more_path` names where a fresh
  answer's Ask more continues. `application::agent_launch_options::Path`
  carries it to the launch: a fresh path gets Claude
  `--no-session-persistence` (and keeps MCP off), Codex `exec --ephemeral`;
  `prepare_native_agent_turn` refuses a native turn on it, and the
  invocation host opens no native session for it.
- The app names the path at each launch: Agent tab (`agent_tab`), tour side
  chat and whole-tour conversation, Draft Tour (`draft_tour_first_draft`,
  now fresh: no native scope), a field draft or outline (`tour_field_draft`),
  a rewrite and a part's Enhance (`tour_enhance`), Enhance on a comment,
  reply or note (`enhance_writing`), a local draft's discussion and its commit
  message, Review Cycle rounds (`build_round_launch`, resumed by session
  name as before), agent checks (`agent_check`).
- A tour stop's or part's Enhance (AC11) is a fresh run: `tour_side_chat::enhance`
  neither opens the side chat nor sends it a turn; it dispatches a fresh
  update ask (`tour_ask::Fresh`), whose answer becomes the existing tour
  proposal on the part (marked grey, Accept or Discard). The run is marked
  `tour_agent_run::Fresh` so no conversation replays it.
- Ask more for Draft Tour (AC12, partial): the fresh first draft's answer
  (Problem, Solution, stop count) seeds the next whole-tour conversation
  turn, which then resumes as before.

Runs at `5286b69c2`: `application_agent_launch_policy` (new: AC10's table
and per-path launches, MCP, argument order, instructions, sign-in failures),
`application_agent_launch`, `application_review_cycle_round_launch`,
`qt_tour_side_chat` (AC11), `application_enhance_comment_note_public_path`,
`application_review_cycle_send_public_path`, `core_agent_configuration`,
`sqlite_agent_setup_storage`, `architecture_boundaries`,
`test_network_isolation_manifest`, `application_composition_source_contract`
(pin resealed), `coding-format-check`; integration pass below.

Not done then (the cards stayed In Progress): the local draft's discussion
as a native session, its first description as a fresh path, an explicit Ask
more, and Draft Tour, field drafts and the outline as one call. The second
pass below finished them.

### Second pass (2026-11-12): resumed local discussion, Ask more, one call per fresh click

- **Local draft discussion (AC10, AC12).** `local_draft_workbench::ask`
  names the path: the discussion (`local_draft_discussion`, resumed) or,
  for a question about the title or description while the description is
  empty and no session is open, the first description
  (`local_description_first_draft`, fresh); the commit message stays
  `local_commit_message` (fresh). A resumed tool-free Claude launch keeps
  its session (the writing restrictions without `--no-session-persistence`);
  `prepare_native_agent_turn` now takes a tool-free turn on a resumed path
  (Claude only, never connected) and adds `--resume <handle>`. The runner
  runs the discussion in an owner-private empty folder kept per draft
  (`local-draft-sessions-v1/<draft>`), so the next turn finds its session;
  the launcher keeps a host folder only for a resumed tool-free path. The
  handle the agent reports is recorded on the conversation
  (`core::record_handle`); `compose_local_request` then sends only the
  question (no replay, no files: `local_request::Resumed`). A conversation
  with no handle starts its session with its earlier turns, so a fresh
  first description's question and answer seed it (Ask more). A session that
  cannot be resumed is let go and the next question starts one from the
  conversation.
- **Ask more (AC12).** An explicit control, flat beside Discard: on a part
  of Draft Tour's proposal (`tourProposalAskMore`) and on a fresh Enhance's
  suggestion in the part's side chat (`tourSideChatAskMore`). It opens that
  thread's chat (the whole tour's for Draft Tour, the part's for Enhance;
  `ask_more_path(tour_enhance)` is now `tour_side_chat`) and arms a seed the
  next turn carries ahead of its message; that turn resumes as every chat
  turn does. Nothing is seeded without the press. A commit message and an
  Enhance on a comment, reply or note offer none. Frontend-design was loaded
  first; render `side-chat-ask-more` from `qt_tour_side_chat`.
- **One call per fresh click (AC13, [[BPR-2141]]).** The snapshot keeps each
  file's patch (`github_file_record::Patch`, up to 64 KiB a file). Draft
  Tour, Draft from PR context and Suggest an outline choose a route in the
  core (`choose_tour_context`): with a tool-free agent and a diff of at most
  `maximum_up_front_diff_bytes` (96 KiB) they are one tool-free call (no
  tools, no MCP, no saved session) whose request carries every patch
  (Draft Tour's evidence JSON; a field's `render_up_front_diff`); the prompt
  rides the tour prompt's carrier (`agent_context_run::tool_free_tour`).
  Past the budget, or with an agent with no tool-free mode, they read the
  repository as before and the notice says which and why ("This pull
  request's diff (N KiB) is more than one request carries (96 KiB), so the
  agent reads the repository instead, in several requests"). An agent that
  can do neither is refused by name.
- **Instructions in the system prompt ([[BPR-2140]]).** `split_tour_prompt`
  splits a tour prompt at its evidence: Draft Tour's and every tour
  conversation turn's instructions go to the system prompt with the rest of
  the round's built-in prompts; the local draft's description, discussion
  and commit message prompts (`local_request::Instructions`, the commit
  runner's `Instructions`) and the review skill (out of the brief, which
  says `brief.skill.location system_prompt`) do too. The per-agent switch
  and the 24 KiB bound paste them as before.

Runs (focused, at the second pass's candidate): `application_agent_launch_policy`
(new: the resumed local discussion and its fresh siblings, the review skill,
up-front drafts and the route, the tour conversation's split within the
bound), `core_local_draft_tour` (resumed and seeding requests),
`core_local_changes`, `local_changes`, `local_draft`,
`application_github_contract`, `application_tour_agent_conversation`,
`qt_tour_agent_session_view` and `qt_tour_side_chat` (Ask more),
`qt_tour_chat_in_tab`, `application_local_draft_tour_desktop_public_path`
(the follow-up resumes `session-1`: no replay, no files),
`application_local_draft_author_desktop_public_path`,
`application_agent_invocation_composition_contract`,
`application_review_tour_composition_public_path`, `tour_cli_composition`
and `application_profile_tour_handoff_public_path` (eleven writing hooks).

Live, 2026-11-12, installed Claude Code (haiku, the person's own sign-in):
`python3 tests/scripts/claude_resume_canary.py --launch-printer build/dev/tests/application/better_pull_review_agent_launch_policy_tests`
ran a local draft discussion's two turns exactly as BPR builds them in one
empty folder: the first told a word, the second resumed the first by the
session it reported (`--resume`, still tool-free) and answered
`CANARY_PASS: 'KUMQUAT' same_session=True` (two calls, $0.0013).
