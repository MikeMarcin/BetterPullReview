# BPR-0966 Mac diagnosis and partial correction

Assignment: standalone BPR-0966, existing r4 follow-up plus the coordinator's
unattended Mac assignment of 2026-09-25 (isolated segment r1).
Base: `f530e08b8296034a237c2c21ff2722a7a3caceb0`.
Test candidate: `12ea8646b9175a32abfa396eb502a2c09007c088`.
Branch: `codex/bpr0966-dif-cues`.
Source root: `/Users/jaeger/.codex/worktrees/bpr0966-dif-cues/BetterPullReview`.

The milestone remains **blocked on an owner contract decision**. This slice
corrects the course's consider observation and strengthens its diagnostics.
Product source, placement, routing, resources, publication behavior and historical
acceptance are unchanged. Windows verification is explicitly deferred by the
coordinator's user direction and is not claimed.

## Contract reconciliation

RFC-BPR-0031's Explicit AI tools area requires the single sprite in the actually
visible AI tools, hides it elsewhere, cancels cues on navigation/hiding, and
forbids retrospective animation. B7 also requires all ten ordinary product paths,
including acknowledgment of accepted workshop edits. The owner-accepted
RFC-BPR-0037 design retains the editor on a dedicated Compose review page.

The live composition presents a workshop proposal, opens that page, then calls
`present_dif_workshop`. Opening Compose hides the conversations drawer, so
`showing_dif_ai` becomes false and `update_dif_presentation` disables the router
and hides the single widget. Acceptance occurs on that same editor page. The
diagnostic below observes both states. This is source tracing and current
reproduction, not a first-bad bisect.

Deleting the two assertions would leave B7's ordinary acknowledgment path
unfulfilled. Moving Dif or replaying cues after returning changes placement or
lifecycle contracts. BPR-0800 explicitly requires the corner versus AI-slot
decision before implementation and an honest RFC-BPR-0031 amendment. Its
accepted Home verdict supplies no Compose decision. Neither RFC nor historical
review packets are amended here.

**Pending decision:** While a workshop proposal is displayed and accepted on
Compose, which surface should own the existing single Dif widget: a visible
workshop slot, the corner coordinated with BPR-0800, or no sprite with an explicit
amendment to B7's workshop acknowledgment requirement? The answer must establish
whether the two workshop cues remain required there. The accepted Compose layout
does not answer that question.

## Independent correction

Previously the course checked `isVisible()` and sampled `current_frame()` after
each event-loop pass. A sprite can satisfy that flag while fully clipped by the
Findings scroll viewport, and start/terminal projections can land between samples.

A passive Qt Paint-event observer, before adding exposure, recorded no consider
paint on Mac and failed consider plus the two original assertions. A disposable
probe measured `visibleRegion()` empty before
`findings_tab_scroll()->ensureWidgetVisible(dif)` and nonempty afterward; the
observer then recorded consider. The committed course exposes that existing
viewport, waits for a visible region, and observes Paint events during invocation.
It does not delay the agent or add a product observer hook. A cue preempted before
painting still cannot be claimed as observed. The disposable probe was removed.

The course also verifies that proposal presentation preserves the complete
original draft, acceptance retains its ID and emits one draft intent, and the
original single sprite remains. Exact proposed/accepted bytes and all three
zero-publication counters remain asserted. Passing router/editor/workshop checks
retain session, action and revision refusal/dedup evidence. No rule changes.

## Verification

macOS 26.6.2 (25G83), arm64, AppleClang 21.0.0, Qt 6.11.2, Debug;
network-isolated fixture courses, offscreen Qt, serial. No live PR or paid model.
The fixture canary speaks Codex JSONL. No Claude process was used.

Commands run from the source root above. Local logs are under its ignored
`build/` directory and are not committed.

| Check | Result |
| --- | --- |
| `cmake --preset dev` | Pass, own dependency cache/build. Third-party uninitialized-variable warnings. |
| Focused target build | Pass. Initial name `test_better_pull_review_dif_runtime_tests` was rejected by Ninja before building; corrected target has `qt_` before `dif_runtime_tests`. |
| Baseline Dif course, source unchanged from base | Fail, 20.72 s; original `1:1:1:0:0:1:0`, only ready/acknowledge fail. |
| Paint observer before exposure | 4/5 focused checks pass; Dif fails consider plus ready/acknowledge, 17.82 s. Retained failure evidence. |
| Disposable exposure probe | Dif fails only ready/acknowledge, 18.61 s. Region changes 0 to 1; painted consider is 1. |
| Exact candidate focused batch below | 4/5 pass, 45.71 s total. Dif fails only ready/acknowledge in 18.07 s. New draft/sprite checks pass; painted consider is 1. |
| Touched-file clang-format and `git diff --check` | Pass. Self-review checked passive observation, lifetime and retained assertions. |
| Documentation gate | Pass using installed skill scripts on `PYTHONPATH`; plain invocation initially failed because this fresh worktree lacks the ignored local skill deployment. Shared deployments unchanged. |
| Documentation regression suite | 64/64 pass, 1.653 s; negative-fixture ERROR output is expected. |

```sh
cmake --build --preset dev --parallel 6 --target test_better_pull_review_agent_finding_composition_public_path test_better_pull_review_agent_finding_canary test_better_pull_review_qt_dif_runtime_tests test_better_pull_review_qt_review_draft_editor_tests tool_better_pull_review_mcp_context_helper
ctest --test-dir build/dev --output-on-failure --no-tests=error -j 1 -R '^(application_agent_finding_(dif_context|composition|workshop_press)_contract|qt_dif_runtime|qt_review_draft_editor)$'
PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 scripts/verify_documentation.py --root .
PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 tests/scripts/verify_documentation_tests.py
```

The baseline and disposable probes use only the Dif alternative of that selector.
Final exact candidate evidence:

```text
agent-finding-composition-failure:dif-context-evidence failed=dif-comment-ready-after-conversion dif-acknowledge-after-acceptance
agent-finding-dif-context-evidence:1:1:1:0:0:1:0
agent-finding-dif-workshop-visibility:proposal-editor=1 proposal-ai-slot=0 proposal-dif=0 accepted-editor=1 accepted-ai-slot=0 accepted-dif=0 painted-consider=1 painted-ready=0 painted-acknowledge=0
```

Executable SHA-256 at the candidate:

- Finding course: `86de5254fc1a2859fba471bae1ef132def5d12a8d4420a0bc7518bc75f7b85be`.
- Fixture canary: `109315a6ee7d4f519e3b49b33577d62b6794be4d7a796b59869b07c46df24ccc`.

No new native/attended verdict or Windows result was obtained. No visual candidate
is ready for attended acceptance until the contract is resolved. Product,
architecture and user-flow documentation has no semantic change; this evidence
and the card retain the unresolved product obligation.

## Continuation

AC1 awaits the owner decision above. AC2 has a verified partial test correction;
workshop reconciliation remains open. AC3 retains these exact Mac results,
including the failed Dif course, with Windows deferred. Consume the owner's
decision, amend only authorized contracts, implement the chosen behavior, and
rerun the public course with exact identity, no replay, single-sprite and
zero-publication checks. The candidate is not accepted as complete.
