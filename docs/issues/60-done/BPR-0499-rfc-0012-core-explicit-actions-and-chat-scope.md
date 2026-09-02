---
id: BPR-0499
title: "Core: explicit AI actions, side-chat scope, and stale continuation"
status: Done
rfc: RFC-BPR-0012
depends_on: [BPR-0496]
---

# Core: explicit AI actions, side-chat scope, and stale continuation

Extends `core::decide_agent_invocation` in
`src/core/include/bpr/core/agent_profile.hpp` to cover the seven entry points
`docs/ux/ai-experience.md` names — ask about selection, analyze current file,
analyze latest upload, evaluate proposed fix, workshop comment, run AI review,
open full agent session — as explicit intents and nothing else. It adds chat
scope as a typed value and reuses `agent_session_staleness`,
`agent_revision_binding`, and `observe_agent_session` from
`src/core/include/bpr/core/agent_session.hpp` unchanged for continuation.

## Required final state

- The seven entry points are a closed enumeration. Legality stays a pure
  function of the profile, the host policy, the current selection, and the
  session, so the suite runs under `core-only`.
- Every legal invocation carries an explicit intent. There is no
  default-constructed or implied intent, so refresh, navigation, comment
  arrival, and revision update are observations with nothing to pass.
- Chat scope is a typed value over selected lines, file, pull request, and
  repository. It starts at the reviewer's selection and widens only through an
  explicit intent that records the scope it came from.
- Private notes and unpublished drafts are opt-in per session and default off.
  The opt-in lives on the session rather than a profile or a setting, so it
  cannot be turned on once and forgotten.
- After `head_moved` or `epoch_pair_changed` a session offers exactly two
  continuations — a new session, or an explicit old-revision continuation —
  and retargets neither. `observe_agent_session` keeps returning a verdict.

## Non-goals

- The finding type and anchor verdicts — BPR-0497; promotion — BPR-0498.
- The drawer, the chips, and the stale banner — BPR-0502.
- Any process, protocol, decode, or bound — BPR-0500.
- The MCP helper's scope, which is its own frozen projection — BPR-0501.

## Required direct evidence

- `ctest --preset core-only -R '^core_agent_(session|profile|chat_scope)$'
  --output-on-failure`. The implementing slice records whether the new cases
  extend `core_agent_session` and `core_agent_profile` or register
  `core_agent_chat_scope`.
- A table-driven case over all seven entry points asserts each is legal only
  with its own explicit intent; an eighth without a case fails the count.
- Refresh, navigation, comment arrival, and revision update are each swept and
  each produces zero invocations, asserted by reading the type.
- A chat turn at file scope with no widening intent stays there; making it
  widen fails by name, and the widening case records the prior scope.
- A session without the notes opt-in carries zero notes and zero drafts even
  when both exist for the review.
- After `head_moved` the session offers exactly the two continuations; making
  it retarget fails the existing binding assertion in `core_agent_session`.

## What this slice delivered — 2026-09-01

`src/core/include/bpr/core/agent_chat_scope.hpp` and `.cpp` (the seven entry
points as `agent_entry_point`, the four observations as `agent_observation`,
`agent_action_intent`, `agent_selection`, `agent_chat_scope`, chat turns, and
`decide_agent_action`), additive extensions to `agent_session.hpp`
(`agent_continuation`, `agent_continuation_offer`, `offer_agent_continuations`,
`valid_agent_session`) and `agent_context_supplement.hpp`
(`agent_private_context_optin`, `admit_private_context`, a two-argument
`build_agent_supplement`), proven by the new `core_agent_chat_scope` plus
extended `core_agent_session` and `core_agent_context_supplement` under both
graphs. Implemented by a bounded `rfc-worker`; the integrator inspected the
complete diff and reproduced the verification below.

### Decisions worth knowing

- **A sibling decision, not a widened one.** `decide_agent_invocation` is
  called by the application host and described by the launch panel; giving
  it defaulted intent, selection, and session parameters would have created
  the implied intent this card forbids. `decide_agent_action` sits beside it
  and calls it for the host-policy half, carrying that verdict whole.
- **No default intent is structural.** Intents are classes with deleted
  default constructors and `explicit` constructors; an observation's only
  product is a one-field record, and `intent_for` returns nothing for every
  observation in four named cases.
- **Scope is an ordered enumeration** (`selected_lines` < `file` <
  `pull_request` < `repository`), so `wider` is a comparison; exactly one
  identity shape is engaged per kind, and the pull-request and repository
  scopes engage nothing because the session's binding already names them.
  `next_agent_chat_turn` is the only place a scope changes and is the identity
  without a widening intent; a widening records `WidenedFrom` and keeps it.
- **Continuation is a two-element `std::array`** carrying `observe_agent_session`'s
  verdict whole, with `ContinueAgainst` always the session's own binding and
  no field the observed binding could occupy; `different_review` offers the
  same two choices with `Required` true.
- **The opt-in is keyed by `agent_session_id`**, separately for notes and
  drafts, and refuses the whole build when the ids disagree; the existing
  one-argument `build_agent_supplement` is untouched so the four downstream
  suites that assert selected prose reaching the child keep passing.
- **Two judgements beyond the card's words**, documented in the header:
  `acts_on_selection` is true for ask-about-selection, analyze-current-file,
  and evaluate-proposed-fix only; and an action naming a profile the session
  was not started with is `session_profile_mismatch`.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The core suites and boundaries pass under `core-only` | `cmake --build --preset core-only` then `ctest --preset core-only -R '^(core_agent_\|core_dependency_boundary\|core_boundary_contract)' --output-on-failure` | build exit 0; 8/8 passed | this commit |
| The dev graph, its host and Qt consumers, and the composition contract are undisturbed | `cmake --build --preset dev` then `ctest --test-dir build/dev -R '^(core_agent_\|application_agent_launch$\|application_agent_invocation_host$\|qt_agent_launch_panel$\|application_agent_invocation_composition_contract$\|architecture_boundaries$)' --output-on-failure` | build exit 0; 11/11 passed (worker's run additionally passed `architecture_boundary_contract`, 12/12) | this commit |
| Formatting | `clang-format --dry-run --Werror` on the nine touched files; `git diff --check` | exit 0 | this commit |
| Seven entry points legal only with their own intent; an eighth fails the count | table-driven case plus a `static_assert` against `agent_entry_point_count` | pass; the count mutation fails to build | this commit |
| Refresh, navigation, comment arrival, and revision update produce zero invocations | the observation sweep, asserted by reading `agent_observation_record` | pass | this commit |
| A file-scope turn without a widening intent stays; widening records the prior scope | the turn cases | pass | this commit |
| A session without the opt-in carries zero notes and zero drafts | the admission cases | pass | this commit |
| After `head_moved` exactly two continuations, neither retargeting | the continuation cases | pass | this commit |
| Non-vacuity | worker mutations, each restored and md5-verified: twelve behavioural (each failed by name, including the overload-never-calls-admission seam and the offer-retargets case), seven type-level and four count-guard (each refused at compile time) | as stated | this commit |

Handed to the next slices: the host still calls the one-argument
`build_agent_supplement`; moving it onto the opt-in overload is
[[BPR-0500]]/[[BPR-0502]]'s work, and until then the ungated path remains
reachable from the host.
