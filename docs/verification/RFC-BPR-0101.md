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

One core recogniser (`src/core/include/bpr/core/agent_reference.hpp`)
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
`src/core/include/bpr/core/agent_cost.hpp`): a run that
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
