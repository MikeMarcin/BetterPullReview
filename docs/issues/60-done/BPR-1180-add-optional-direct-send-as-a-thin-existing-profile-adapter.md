---
schema: "implement-rfc.card.v1"
id: "BPR-1180"
title: "Add optional direct Send as a thin existing-profile adapter"
status: "Done"
rfcs: ["RFC-BPR-0060"]
depends_on: ["BPR-1179", "BPR-1169", "BPR-1164"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-07", "RC60-08", "RC60-09"], "pack_alias": "PACK-CYCLE-38", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC60-07","outcome":"pass","revision":"2dad1dbbc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1180's share, macOS arm64 Debug. application_review_cycle_send_public_path (real window, composed desktop, real agent_process_launcher and supervisor, stand-in harness copies in the course's root): the Claude harness's one user message and the external terminal's plain stdin equal the preview, the platform clipboard after Copy and the file Save wrote; review_cycle_send: the adapter's document equals Copy and export_round_selected(terminal_output), what bpr cycle export prints; one assignment at a time; the harness's structured response is checked against the round that was sent and appended through the response model with the agent_adapter channel. Negative control: one byte added to Send's document fails both courses."}},{"criterion":"RC60-08","outcome":"pass","revision":"2dad1dbbc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1180's share, macOS arm64. Existing profiles under the existing agent host policy: with an account on ghe.example.com Codex is listed not allowed and a send to it is refused by name (profile_forbidden_on_host) with nothing frozen or started; the agent_adapter channel decides first (refused until configured; denied stays denied). Sessions: a new one is said as new; Claude continues only a session it reported to this cycle, bound as --resume NAME in the launched arguments; other harnesses and unknown or option-like names are refused (core, host, launch and real-process courses). The launch passes only allowed environment names. Negative control: ignoring the corporate host fails core, host and real-process courses."}},{"criterion":"RC60-09","outcome":"pass","revision":"2dad1dbbc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1180's share, macOS arm64. Late output is recorded against the round it was sent for (lasting record_send); a reply naming another round is refused and nothing appended; stopped, timed-out and Stop-cancelled sends record accepted/acknowledged then uncertain, a missing harness records failed, each with no second send and no copy, save or other agent in its place; nothing is captured or verified. Removal: a host with no adapter, set_send_profiles({}) in the real window, and an agent-free build reopening a full build's sent cycle (--profile-root / --reopen) all read every observation and response and continue by Copy and a hand-entered reply. Negative controls: a reply checked against the next round fails the host course; --reopen on an empty profile fails."}},{"criterion":"BPR-1180:boundary","outcome":"pass","revision":"2dad1dbbc","evidence":{"kind":"command","detail":"Full graph before the rebase: 592 of 595 (composed_host_verifier_contract lacked the new anchor's fixture, fixed; live_mutation_isolation timed out at 300 s under -j6 and passes alone in 281 s; BPR-1236). After the rebase onto 3d414df18: the focused selection 15 of 15 including architecture_boundaries, the round-trip composition contract (pins unchanged; github_application.cpp untouched) and coding_format_check. Agent-free graph (BPR_BUILD_AGENT_INTEGRATIONS=OFF): 5 of 5; composed_application_hosts forbids the round sender anchor; nm finds 95 agent_round_sender/build_round_launch symbols in the full image and none in the agent-free one; each image fails the other family's audit naming the round sender. clang-format 22 and 23 agree; clang-tidy: no finding on a changed line; DOCUMENTATION_OK. Linux and Windows owed to stations."}}]
---

Pack alias: **PACK-CYCLE-38** (live `BPR-1180`, allocated at import).

# BPR-1180: Add optional direct Send as a thin existing-profile adapter

## Outcome and bounded work

Deliver one exact frozen assignment using an explicitly selected admitted profile/session. Reuse provider process/credential/host policy and shared response correlation. Differentiate accepted, acknowledged, completed, failed and uncertain without making any state a capture gate.

## Acceptance and direct evidence

Payload parity with Copy/Save/CLI, existing-session versus explicit new-session disclosure, denied GHES profile, late wrong-round output, uncertain send, remove adapter mid-cycle and manual continuation; no automatic fallback/resend.

Criteria: RC60-07, RC60-08, RC60-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Result (2026-10-04)

Promoted at `6acb5b25a`, claimed at `b1d95b24c`, implemented at `e72b7f972`
with its launch test at `70606b8c3` (verified as `2dad1dbbc` and `afe26732a`;
the rebase changed no owned path). Owned paths, decisions, every command and
its result are in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1180-send-to-agent-a-thin-client-of-the-frozen-round-2026-10-04).

- Share notes' Send to Agent… opens a box: which installed agent, which
  session (a new one, said as new, or one this cycle's earlier sends were
  told, continued by exactly that name), where it runs and what it is given.
  Send Round N freezes the round and hands its readable document, the bytes
  Copy and Save give, to that one profile through the existing supervised
  agent launch. Nothing else is sent.
- The round's `agent_adapter` channel and the profile's own agent host policy
  decide before anything is frozen: a local folder needs the channel allowed
  (`bpr cycle policy … --channel agent_adapter --permission allowed`), and a
  corporate account's host refuses Codex by name. A refusal sends nothing and
  uses no other agent or route.
- What was seen is recorded against the round that was sent, whenever it
  ends: failed, accepted by the adapter, acknowledged with the harness's
  session, uncertain when the turn did not finish. A reply that is a
  structured response for that round is appended through the response model;
  one about another round is refused; prose waits for Add as Reply. Nothing is
  sent again, captured or verified by itself.
- The adapter (`agent_round_sender`) is part of the agent integrations:
  `BPR_BUILD_AGENT_INTEGRATIONS=OFF` compiles it out, the audit forbids its
  anchor, and an agent-free build reopens a cycle that used it and carries on
  by hand.

macOS arm64 evidence: core, host and real-process public-path courses in a
full graph, the agent-free graph's courses, a full build's sent cycle reopened
by an agent-free build, the image audits both ways, three negative controls
and the integration suite. Owed to stations, not run here: the Linux and
Windows runs of the same selections; the commands are in the ledger.

**Note: Linux station run at `a1f7a9fa7` (station `bpr-main-linux`, WSL2,
GCC 15.2.0, offscreen; stand-in agent only, no remote mutation).**

1. Full `dev` build: the 13-row selection passed 13/13, including
   `application_review_cycle_send_public_path` (1.3 s),
   `application_review_cycle_round_launch`, `composed_application_hosts`,
   `composed_host_verifier_contract`,
   `application_review_round_trip_composition_contract` and
   `architecture_boundaries`.
2. `build/bpr1180-off` (`BPR_BUILD_AGENT_INTEGRATIONS=OFF`) built its
   listed targets with no warnings. Run exactly as written, all 5 rows were
   "Not Run": the listed targets do not include
   `tool_better_pull_review_network_sandbox`, the runner every
   network-isolated row needs, so ctest could not find
   `build/bpr1180-off/better_pull_review_network_sandbox`. With that one
   target added, 5/5 passed: `core_review_cycle_send`, `review_cycle_send`,
   `composed_application_hosts`,
   `application_review_cycle_desktop_public_path` and
   `application_review_cycle_send_public_path`. The Linux and Windows station
   request should add the sandbox target, as [[BPR-1184]]'s does.
3. Removal across builds: in an owner-private `mktemp -d` directory (mode
   700), the full build's course with `--profile-root D` printed "NATIVE: real
   adapter, real processes: parity, sessions, corporate refusal, uncertain,
   missing harness, removal" and `review-cycle-send-public-path:pass`. The
   agent-free build's course with `--reopen D` printed "NATIVE: agent-free
   graph reopened a full build's sent cycle" and
   `review-cycle-send-public-path:pass`. `D` and `build/bpr1180-off` were
   removed.

Done is not reopened.

### Windows at `f198d4e09` (bpr-main-win, 2026-10-04)

Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2, Debug. The stand-in
fixture `cycle_send_agent_main.cpp` did not compile under MSVC (`std::fopen`,
C4996). It was fixed at `d146264da` (`_wfopen_s`, the same exclusive `"wbx"`
mode).

- **`windows-dev`.** The build printed no `warning C` or `warning LNK` line.
  The station selection passes **13 of 13**: `core_review_cycle_send`, `_share`
  and `_response`, `review_cycle_send`, `review_cycle_workbench`,
  `application_review_cycle_round_launch`, the send and desktop courses,
  `sqlite_review_cycle_storage`, `composed_application_hosts`,
  `composed_host_verifier_contract`, `architecture_boundaries` and the
  round-trip contract.
  - `review_cycle_send` prints its four `NATIVE` lines, among them "late
    wrong-round reply refused, uncertain kept uncertain, no resend or fallback".
  - The send course prints "real adapter, real processes: parity, sessions,
    corporate refusal, uncertain, missing harness, removal". It asserts the
    stopped send reads "stopped before finishing its turn" and is never sent
    again.
- **How the stand-in is launched on Windows.** A Toolhelp watcher sampling
  every 5 ms during a lone run saw the stand-in `claude.exe` started by the
  course's own process (`review_cycle_send_public_path.exe`). It was not
  started by `better_pull_review_agent_supervisor.exe`, which did not run.
  That is the design: `run_windows_agent_process` takes the supervisor path
  only for a `WorkspaceLifetimeGuard`, which only `agent_invocation_host` sets
  for an agent in a materialized workspace. Every other launch, Send included,
  is the documented suspended native launch in a kill-on-close job object,
  assigned before its first resume (`agent_process_launcher.hpp`).
- **Pins measured.** Contained, through the sandbox launcher,
  `review_cycle_send` exits 3 at its first store step ("FAILED: two notes"). The
  send course exits `0xC0000409` on a Debug "operator*() called on empty
  optional", a refused contained step whose result the course dereferences.
  Both pass uncontained, and both are now pinned `dos-volume-name` with these
  lines. `test_network_isolation_manifest` passes.
- **Agent integrations off** (`build/windows-bpr1180-off`, with
  `tool_better_pull_review_network_sandbox` added to the target list, as Linux
  found). It builds clean and passes **5 of 5**. The send course prints
  "agent-free graph: no Send to Agent route", and `review_cycle_send` prints
  its four lines.

Seen, not changed: the send course dereferences an empty optional when a step
is refused. It only happens contained, and an expectation there would name
the step.
