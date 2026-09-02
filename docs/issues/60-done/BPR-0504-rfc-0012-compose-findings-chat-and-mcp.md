---
id: BPR-0504
title: "Composition: wire findings, chat, workshop, and MCP into the shipped workbench"
status: Done
rfc: RFC-BPR-0012
depends_on: [BPR-0500, BPR-0501, BPR-0502, BPR-0503]
---

# Composition: wire findings, chat, workshop, and MCP into the shipped workbench

`src/app/src/github_application.cpp` constructs the new finding host, the MCP
helper lifecycle, the side-chat panel, and the finding-queue and workshop
surfaces beside the existing `AgentHost` construction near line 2493 and the
review-mutation host, and `scripts/verify_composed_hosts.py` gains the new
constructors in `SHARED_REQUIRED`. An end-to-end contract modeled on
`tests/end_to_end/agent_invocation_composition_public_path.cpp` proves a real
gesture reaches a finding and a local draft with zero GitHub mutation, and that
a full agent's GitHub changes return only through RFC-BPR-0008 synchronization.

## Required final state

- The new hosts and panels are constructed in the same place and under the
  same fixture and production split as `AgentHost` and the review-mutation host.
- `SHARED_REQUIRED` names each new constructor, so `composed_application_hosts`
  fails on either binary if one is dropped.
- One real workbench gesture reaches one delivered finding, and converting it
  produces one local draft with zero GitHub mutation calls.
- A full-agent launch's GitHub changes appear only through the RFC-BPR-0008
  fixture synchronization path. There is no second edge from the agent host to
  the review-mutation coordinator.
- Each new host has a construction cut: removing it keeps the gesture
  observable, leaves every downstream effect at zero, and makes the positive
  predicate false.

## Non-goals

- The components themselves — BPR-0497 through BPR-0503.
- Documentation and the matrix rows — BPR-0505.
- The simplification pass and the ledger's closeout sections — BPR-0506.
- Any live GitHub call; the RFC-BPR-0008 fixture path is the surface.

## Required direct evidence

- A new `application_agent_finding_composition_contract` registered like
  `application_agent_invocation_composition_contract` in
  `tests/end_to_end/CMakeLists.txt`; run
  `ctest --preset dev -R
  '^(application_agent_finding_composition_contract|composed_application_hosts|application_review_round_trip_composition_contract)$'
  --output-on-failure`.
- The positive course drives a real gesture through the shipped window to one
  delivered finding and one local draft, with zero GitHub mutation calls.
- The full-agent course proves the resulting GitHub state arrives only through
  RFC-BPR-0008 synchronization; a backchannel would appear as a mutation call
  outside it.
- One construction cut per new host: the gesture still fires, effects stay at
  zero, and the shared positive marker is false.
- `composed_application_hosts` fails when a new anchor is removed by deleting
  its construction, not by editing the verifier.
- `ctest --preset dev -R '^qt_composed_github_scale_100$' --output-on-failure`
  stays green — new drawer content is downstream of the composed window.

## What this slice delivered — 2026-09-02

Five composed constructions, all of them in `src/app/src/github_application.cpp`
beside the existing `AgentHost` and review-mutation host, and all of them under
the same `BPR_BUILD_TESTING` fixture and production split: the new
`application::agent_finding_host`, the new
`application::agent_comment_workshop_host`, the new
`application::mcp_context_helper_supervisor` with its own process launcher, the
`qt::side_chat_panel`, and the `qt::finding_queue_view`. `SHARED_REQUIRED` names
all five constructors, so `composed_application_hosts` fails on either binary if
one is dropped.

One reading was made explicit rather than assumed. The card names the workshop
as a *surface*, and the only workshop surface in the tree is the one BPR-0503
built inside `review_draft_editor`, which the window already constructs. Rather
than give that surface a second constructor nobody needs, the workshop is
composed through a host of its own: `agent_comment_workshop_host` turns a
promoted draft command into the before/after proposal the shipped editor
presents, refuses a revision meant for another draft, and has its own anchor and
its own construction cut. That is what makes "the workshop is composed here"
checkable instead of asserted.

`application_review_round_trip_composition_contract` pins digests of
`src/app` sources and editing `github_application.cpp` drifted both. Repinned in
this same change: `bpr_expected_full_application_digest`
`1be908db8c0aedfc00584c8660e81dfeb963e3c3f4f92ed42c23c194bdbfda84` →
`5254e4a6ca7f421a3f8cc76537483151756b8d3c72bbbf7e9200d38cbfcdde14`, and
`bpr_expected_production_app_manifest_digest`
`ae1a89c1a8bd5384bc0faefda27ec07934708a72d842078cf7e803745fac6e49` →
`bf09fecb398eff212893f2df0fdbf2b16a2532ca8200910d5841e0675649a5a6`. BPR-0510
recorded the previous drift; this is the next one.

| Claim | Command | Result | Commit |
|---|---|---|---|
| One real workbench gesture reaches one delivered finding, and converting it produces one local draft with zero GitHub mutation calls | `ctest --preset dev -R '^application_agent_finding_composition_contract$' --output-on-failure` | Passed, 14.36 s. Connect, open PR 17, select a file, navigate to a change, mark it read, choose Codex JSONL, start: one process start, one delivery, one queued finding shown `anchored (none)`, one frozen snapshot file, one snapshot write. `convert_to_draft_comment` produces one `review_draft_command` and changes no draft — the reviewer's body is still byte-identical, tab and trailing space included. Accepting on the shipped editor makes the local draft equal the agent's suggested comment byte for byte. `review_mutation_evidence` is 0/0/0 throughout | this commit |
| A full-agent launch's GitHub changes appear only through RFC-BPR-0008 synchronization, with no second edge to the review-mutation coordinator | `ctest --preset dev -R '^application_agent_finding_full_agent_contract$' --output-on-failure` | Passed, 14.18 s. Zero GitHub mutation calls of every kind across a complete full-agent launch, and the composition source names `review_mutation_coordinator` zero times, carries exactly one `ReviewMutationHost->publish(` call site, one `->submit(`, no `.submit(`, and no `observe_target` | this commit |
| `SHARED_REQUIRED` names each new constructor, and the gate fails when one is dropped | `ctest --preset dev -R '^composed_application_hosts$' --output-on-failure` | Passed, 1.84 s, 25 required anchors. Negative: removing the composed `qt::side_chat_panel` from `github_application.cpp` — the construction, the member, the presentation, and the gesture wire, not the verifier — rebuilt clean and failed in 1.60 s on `COMPOSED_HOSTS_FAILED: required exact demangled anchor is absent: Qt side chat panel constructor`. Restored byte-exact (md5 `e8c231953763e7b15a2871358aa3f5b4` before and after) | this commit |
| Editing `github_application.cpp` drifts the pinned digests, and both are repinned in the same change | `ctest --preset dev -R '^application_review_round_trip_composition_contract$' --output-on-failure` | Passed, 32.91 s after repinning both values above | this commit |
| Each new host has a construction cut that keeps the gesture, leaves every downstream effect at zero, and makes the positive marker false | `ctest --preset dev -R '^application_agent_finding_.*_construction_cut_contract$' --output-on-failure` | Five courses, all passed: finding host 14.39 s, workshop 14.30 s, MCP 14.37 s, chat panel 14.26 s, queue 14.62 s. Every course keeps `StartIntents == 1`, one launcher invocation, and one process start; each takes its own effect to zero and turns the shared positive marker false | this commit |
| Promotion refusals reach the reviewer rather than being swallowed | `ctest --preset dev -R '^application_agent_finding_workshop_construction_cut_contract$' --output-on-failure` | Passed, 14.30 s. With the workshop host cut, the same conversion gesture produces zero proposals, leaves the draft body byte-identical, and presents exactly one refusal on the shipped agent panel. `agent_finding_host::act` presents every outcome unconditionally, refusal included | this commit |
| New drawer content stays downstream of the composed window | `ctest --preset dev -R '^qt_composed_github_scale_100$' --output-on-failure` | Passed, 1.66 s | this commit |
| The composed selector set is green together | `ctest --preset dev -R '^(application_agent_finding_composition_contract\|composed_application_hosts\|application_review_round_trip_composition_contract\|qt_composed_github_scale_100)$' --output-on-failure` | 4/4 passed | this commit |
| Formatting, documentation, and whitespace gates | `clang-format --dry-run --Werror` on each touched file; `python3 scripts/verify_documentation.py --root .`; `git diff --check` | Every new and touched file clean except `github_application.cpp`, which reports the same two violations at HEAD (lines 3069 and 3103, both in untouched pre-existing code); `DOCUMENTATION_OK`; `git diff --check` clean | this commit |

### Not delivered, and why

- **The panels are composed and driven, but not placed in the window's
  drawer.** `side_chat_panel` and `finding_queue_view` are owned by the
  application and reached through the composed gesture path; laying them into
  `workbench_window`'s drawer is an edit to `src/adapters/qt/`, which is
  BPR-0502 and BPR-0503's file surface and outside this slice's. Recording it
  here so BPR-0505 or BPR-0506 can card it rather than rediscover it.
- **The side chat has no composed session host.** RFC-BPR-0012 cards a core
  chat scope (BPR-0499) and a Qt panel (BPR-0502) but no chat *session* host,
  so a send is refused by name on the agent panel rather than quietly doing
  nothing. The panel's presentation is composed and proven; the turn is not.
- **The frozen snapshot carries alignment facts, not file bytes.** The
  helper's `binding` and `alignment` records are written from the same values
  the finding validator uses, and `files/` is created empty, so
  `list_snapshot_files` answers with an empty list rather than a wrong one.
  Freezing file content is a larger effect than this composition slice owns.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran the seven new end-to-end contracts with
`composed_application_hosts`, `application_composition_contract`,
`application_review_round_trip_composition_contract`,
`qt_composed_github_scale_100`, `application_mcp_context_helper`, and
`live_mutation_isolation`: 13/13 — the composition contract 124.59 s, the
round-trip contract 32.49 s on the repinned digests, each construction cut
about 14.4 s. `clang-format --dry-run --Werror` is clean on
`github_application.cpp` at HEAD and in this tree. The two gaps recorded
under "Not delivered" are carded as [[BPR-0513]] (drawer placement) and
[[BPR-0514]] (a composed side-chat session host), both dependencies of the
RFC's closeout.
