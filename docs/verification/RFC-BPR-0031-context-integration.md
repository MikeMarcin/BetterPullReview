# RFC-BPR-0031 context integration evidence

This record binds the authored `context-integration` milestone
(BPR-0763/0764/0765) to run `dif-takeover-20260914`, dispatch `d13`, producer
`rfc-integrator-d13`, and mandate revision 7. The selected plan is
`bpr-dif-integration-v1`; its definition, obligation, and prerequisite
fingerprints are respectively
`sha256:927aa4b96049c4277a154f8f11a9562d1685563ffa2d06bffe5ddb1fe7a1833a`,
`sha256:b47a75c70498097dededb496474f6c8bd8e4de9c1b0f74f5d861d5638ad1f519`,
and
`sha256:cd68c05cc64c591abb1760c0cb8f995289ec2a352b3b36d839b7e1cfa889838e`.
The accepted predecessors are intake `1c0d0c65`, runtime foundation
`3036364c`, production art `d7fc946c`, and Home integration `9c9f6cde`.

## Contextual projection and placement

Revision `3a00929154dc913249c48b1aa866eb8b9651947a` adds a bounded Qt-local
cue router. The application offers typed facts only after the corresponding
reply, finding, workshop proposal, draft result, or continuation has reached
its existing presentation host. Start maps to `consider`; a presented reply
maps to `comment_ready`; presented findings map to `attention`; a real failure
or required continuation maps to `blocked`; a presented workshop proposal maps
to `comment_ready`; and the matching accepted local draft maps to
`acknowledge`. Merely exiting successfully cannot produce a cue. Terminal
content coalesces with `blocked` above `attention` above `comment_ready`.

The router owns no result, effect, timer, provider, navigation action, queue, or
history. Its bounded state is one current invocation plus current workshop,
draft, and continuation identities. Authoritative text and result surfaces
remain unchanged when a cue is hidden, cooled down, stale, or rejected.

Revision `ad0bb67de13e18a984903718e041b6920ea8c144` composes the existing Dif
widget into a fixed 64-pixel slot under one checkable `AI tools and Findings`
selector in the Conversations drawer. The selector never opens its drawer.
One window owns one Dif widget and moves it between Home and this slot; Home
wins when visible, a narrow drawer yields the slot, and no avatar, message,
card, code, or shared-comment overlay is created. Contextual rest is static,
and reduced motion has no timer.

## Identity, lifecycle, and composed public path

Revisions `8353a4fe47a22151a1c1306581bd35c59c195b8c` and
`8512a3d81599a5a5d715d0522e8cbb0d4833ca75` bind admission to the
exact application session, host, repository, pull request, agent session,
revision binding, and invocation generation. A changed window, profile,
review, or same-review revision clears prior invocation and cooldown state and
cancels a playing contextual one-shot to rest. Admission happens before
workspace materialization, so a matching pre-start failure can cue `blocked`
without inventing a process start. Duplicate delivery/terminal, late or
superseded operation, mismatched delivery, rejected draft, ordinary edit,
older or conflicting same-revision workshop replay, and local draft conflict
cannot replay or acknowledge the wrong operation. Hidden terminal results are
consumed without later replay. An old-revision
continuation is allowed only when the visible `ContinueAgainst` value exactly
labels that stale session binding.

Cooldown affects presentation only. The router continues to accept newer
authoritative state and retains no animation queue. During the two-second
semantic cooldown, only a higher-priority `blocked` or `attention` cue may
interrupt playback. Context clear and widget shutdown cancel the single-shot
timer synchronously.

The `application_agent_finding_dif_context_contract` course runs the real app
composition with the network-isolated fixture review. It expands the existing
Conversations drawer, selects the explicit AI tools surface, observes
`compare_glance`, starts the existing agent control, observes `consider` and
terminal `attention`, and verifies that the accepted finding remains in the
queue. The shipped conversion action presents the workshop proposal and
`comment_ready`; accepting that proposal creates the expected local draft and
`acknowledge`. The course reports `agent-finding-dif-context:pass` and requires
zero GitHub submissions, target observations, and stored comments.

Together with the accepted Home integration, every catalog clip has its
specified product route: `rest`, `greet`, `calm_idle`, and `blink` on Home;
`compare_glance`, `consider`, `comment_ready`, `attention`, `acknowledge`, and
`blocked` in the explicit AI area.

## Focused verification

After the reviewed correction, this network-isolated CTest selection passed:

```text
qt_dif_runtime
qt_review_draft_editor
qt_workbench_window_scale_100
qt_workbench_window_scale_125
qt_workbench_window_scale_200
application_agent_finding_dif_context_contract
application_agent_invocation_workspace_diagnostic_contract
```

All seven tests passed in 84.51 seconds. The runtime course covers wrong session,
pull request, revision and delivery; duplicate delivery and terminal;
superseding invocation; profile/window/review/revision switches; hidden result;
pre-start failure; coalescing, cooldown and permitted interruption; exactly
labeled old-revision continuation; workshop replay, ordinary/rejected edits,
draft conflict; context reset; and timer cancellation. The editor course proves
only the explicit Accept revision dispatch can begin Dif acknowledgement. The
workspace diagnostic course proves the real pre-process materialization failure
produces `blocked` with one workspace start, zero process starts, the exact
visible error, and quiescence. The workbench courses
exercise one movable widget, drawer/selector behavior, static reduced motion,
narrow-layout yield, Home precedence, focus and accessibility at 1.0, 1.25,
and 2.0 scale factors. `coding-format-check` and `git diff --check` passed after
the correction.

The pre-existing `application_agent_finding_composition_contract` and
`application_agent_invocation_composition_contract` failures reproduce before
this milestone: the former does not satisfy its exact helper-launch marker, and
the latter reports the context canary identity absent despite its other exact
context fields. `application_agent_chat_composition_contract` passes. The new
context course provides the direct Dif public-path evidence without treating
those unrelated baselines as milestone success.

## Acceptance ledger

| Obligation | Result | Evidence |
|---|---|---|
| BPR-0763 / B7 and B8 | Accepted | Source `3a009291`; bounded typed routing, terminal coalescing, no process-only success, and focused identity/dedup checks. Card acceptance `290ac785`. |
| BPR-0764 / B6, B7 and B11 | Accepted | Source `ad0bb67d`; explicit single AI slot and all six contextual routes, with three-scale workbench coverage. Card acceptance `857f17b0`. |
| BPR-0765 / B7, B8 and B9 | Accepted | Sources `8353a4fe`, `8512a3d8`; corrected admission, workshop fencing, semantic interruption policy, adversarial router/editor/lifecycle coverage, and composed application courses. Card acceptance is rebound by the final milestone record. |
| `context-integration:exit:1` | Passed | Every contextual clip has an actual application fanout and one explicit UI slot. Dif consumes accepted presentation facts and remains non-authoritative. |
| `context-integration:exit:2` | Passed | Exact identity/dedup, terminal coalescing, hidden-result suppression, preemption and interruption cases pass without a queue. |

## Limits retained for verification closeout

No GitHub or GHES mutation, credential access, model run, art generation, or
new human visual verdict occurred. This milestone changes no accepted pixels.
Actual operating-system motion-setting delivery, native backend memory,
supported-appearance/native rendered qualification, packaging claims, and
formal RFC closeout remain assigned to `verification-closeout`.
