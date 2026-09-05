---
schema: "implement-rfc.card.v1"
id: "BPR-0514"
title: "Host: compose a side-chat session host so a send reaches the agent"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0504"]
---

# Host: compose a side-chat session host so a send reaches the agent

## Why

RFC-BPR-0012's card set ([[BPR-0496]]) allocated the core chat scope
([[BPR-0499]]) and the Qt side-chat panel ([[BPR-0502]]) but no chat
*session* host. [[BPR-0504]] composed the panel's presentation and proved it,
and a send on the panel is therefore refused by name rather than quietly
ignored. The turn itself — carrying a send, a scope change, a note opt-in, or
a continuation choice from the panel to a running agent under the
session-keyed opt-in and the ordered scope the core decides — is not
composed anywhere.

## Required final state

- One application host owns the side-chat session: it takes the panel's
  intents, asks the core for the legal scope and continuation
  ([[BPR-0499]]), and drives the agent through the existing launch and
  protocol path with the frozen snapshot [[BPR-0500]] delivers findings from.
  No second launch path exists.
- A send outside the session's opt-in, or against a stale binding without a
  chosen continuation, is refused by the core and the refusal is presented on
  the panel.
- The host is constructed under the same fixture/production split as the
  other agent hosts, named in `SHARED_REQUIRED`, and has a construction cut.
- The replies the agent returns arrive through the same finding delivery
  bounds [[BPR-0500]] enforces; nothing bypasses them.

## Non-goals

- Drawer placement — [[BPR-0513]].
- Any change to the protocol framing or bounds — [[BPR-0500]].

## Required direct evidence

- A registered end-to-end contract beside
  `application_agent_finding_composition_contract`: one send from the panel
  reaches one agent turn and one delivered reply with zero GitHub mutation
  calls; a stale send without a continuation is refused and presented; a
  continuation choice starts exactly one session.
- `composed_application_hosts` fails when the host's construction is deleted.
- The sibling RFC-0012 selectors stay green.

## What this slice delivered — 2026-09-02

One application host, `application::agent_chat_session_host`, owns the
side-chat session. It holds the session, the revision it has observed, the
scope, and the reviewer's per-session private-prose opt-in — and decides none
of it. Every answer is the core's, asked in a fixed order that *is* the policy:
`offer_agent_continuations` first, then `decide_agent_action`,
`next_agent_chat_turn`, and `admit_private_context`. Staleness is asked first
deliberately, because a reviewer whose head moved is owed "which revision are
you asking about" before "is that action legal" — asking the second first
refuses a send for a missing selection when the reason the selection is missing
is the revision that moved under it.

**No second launch path.** The host has no process port, no launcher, and no
workspace; a legal turn goes to a `turn_dispatcher` the composition points at
the one `agent_invocation_host` that already existed. `dispatch_agent_intent`
was split so the launch panel's start gesture and the chat's send both reach a
single `dispatch_agent_invocation`, and `AgentHost->dispatch(` now has exactly
one call site in `github_application.cpp`. The reply comes back at the same
seam the finding host reads, so a chat reply is a `deliver_agent_findings`
product and nothing about it bypasses BPR-0500's bounds.

**Head movement never retargets.** `observe` records the revision and starts
nothing; the session changes only when the reviewer chooses
`start_new_session` or when the launch path admits the session a turn runs
under. `agent_chat_projection` has no field the observed revision could occupy.
`Required` is the one field the host answers, and only after the reviewer has —
`Status` is untouched, so a stale session still reads `head_moved`.

`github_application.cpp` is digest-pinned; both values were repinned in two
passes, the second drift being hidden until the first was fixed.

**Not delivered, named rather than left to silence.** The reviewer's typed
message rides the turn as a value but does not reach the protocol frame: that
needs a field on `core::agent_context_supplement` and a change to
`frame_agent_input`, which is BPR-0500's framing and this card's non-goal.
Choosing `continue_against_old_revision` records the choice and re-enables Send
through the host's `Required` answer, but the panel keeps showing the stale
state, which is correct; no course exercises that branch end to end.

| Claim | Command | Result | Commit |
|---|---|---|---|
| One send from the shipped panel reaches one agent turn and one delivered reply, with zero GitHub mutation calls | `ctest --preset dev -R '^application_agent_chat_composition_contract$' --output-on-failure` | Passed, 13.75 s. Connect, open PR 17, select a file, navigate to a change, type into the composer, trigger Send: one send intent, one turn, one process start, one workspace cleanup, one validated delivery, one queued finding, zero refusals, `PresentedOutcomes == Intents`. The composition source carries exactly one `AgentHost->dispatch(`, one `agent_chat_session_host>(` construction, one `dispatch_agent_invocation(` declaration with two call sites, and one `ChatHost->deliver(`. `review_mutation_evidence` 0/0/0 | this commit |
| A stale send without a continuation is refused by the core and the refusal is presented; a continuation choice starts exactly one session | `ctest --preset dev -R '^application_agent_chat_stale_contract$' --output-on-failure` | Passed, 14.58 s. After `reaccept_fixture_revision(true)` the projection reports `head_moved` with `Required` true, the panel disables Send and offers exactly two continuations. A send on the panel's own composed dispatch edge adds one refusal and no turn, and the agent panel reads *side chat refused: session_stale (head_moved)*. Triggering New session raises Continuations and SessionsStarted by one each, leaves SessionsAdopted unchanged, binds the new session to the revision now under review while the old one keeps its own, starts no process, and re-enables Send | this commit |
| A construction cut keeps the gesture observable, effects at zero, and the positive marker false | `ctest --preset dev -R '^application_agent_chat_construction_cut_contract$' --output-on-failure` | Passed, 12.61 s. With `DisableChatSessionHostComposition`, the same Send gesture still fires: one panel send intent, one composed intent, the panel still presented. Turns 0, statistics default-equal, no session, no observation, zero process starts, zero launcher invocations, and the agent panel reads *side chat refused: no composed chat session host*. Positive marker false; `review_mutation_evidence` 0/0/0 | this commit |
| `SHARED_REQUIRED` names the new constructor and the gate fails when the composed object is deleted | `ctest --preset dev -R '^composed_application_hosts$' --output-on-failure` | Passed, 1.58 s. Negative: deleting the composed object from `github_application.cpp` — the construction, the projection, the member, the presentation wire, the clear path, the delivery and admission sinks, and the gesture translation, not the verifier — rebuilt clean and failed in 1.61 s on `COMPOSED_HOSTS_FAILED: required exact demangled anchor is absent: agent chat session application host constructor`. Restored byte-exact, md5 `8c28848005f8adabe3673fdf02b68c50` before and after | this commit |
| Editing `github_application.cpp` drifts both pinned digests, repinned in the same change | `ctest --preset dev -R '^application_review_round_trip_composition_contract$' --output-on-failure` | Passed, 31.68 s. `bpr_expected_full_application_digest` repinned to `c481f63ed1c8d8aa989b25659b2570606fe23d2b3f6f3c8205d7bd4d3fd8e5ab`; the second drift was hidden until the first was fixed, and `bpr_expected_production_app_manifest_digest` repinned to `059713cb3560248216f037577498cb57c579b305cca8a3d6aa255333fbc11112` | this commit |
| The sibling RFC-0012 selectors stay green | `ctest --preset dev -R '^(application_agent_finding_composition_contract\|application_agent_finding_full_agent_contract\|application_agent_finding_[a-z_]*_construction_cut_contract\|composed_application_hosts\|qt_side_chat_panel\|qt_composed_github_scale_100\|application_review_round_trip_composition_contract)$' --output-on-failure` | 11 tests, all passed, 131.18 s total. The presented chat projection is value-identical to what BPR-0504 presented — same session, same offer, same `pull_request` scope, same entry point — so the drawer crops did not move | this commit |

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran the three chat contracts, the seven
finding contracts, `composed_application_hosts`,
`application_review_round_trip_composition_contract`,
`application_composition_contract` (111.31 s), `qt_composed_github_scale_100`,
`qt_side_chat_panel`, and `live_mutation_isolation`: 16/16. `clang-format`
clean by exit code on the five touched C++ files. The undelivered message
framing is [[BPR-0517]], a dependency of [[BPR-0506]] and [[BPR-0507]].
