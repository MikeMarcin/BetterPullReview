---
id: "RFC-BPR-0058"
title: "Headless Cycle CLI and Portable Consumer Skill"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0056", "RFC-BPR-0057"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}, "owner_dependency_amendment": {"date": "2026-10-06", "removed_closure_dependencies": ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"], "uses": ["BPR-1054", "BPR-1057", "BPR-1061"], "card": "BPR-1191"}}
---

# RFC-BPR-0058: Headless Cycle CLI and Portable Consumer Skill

## Outcome and decisions

Extend the shared AI-free bpr runtime with bpr cycle commands and a thin portable bpr-cycle consumer skill. The desktop and CLI invoke the same C++ services. A harness with local command reachability can read a specific prepared round directly, without copy/paste, file export, another model or an embedded BPR agent session.

Use human-readable output by default, explicit JSON, bounded results and exact target/revision identities. Resolve CWD only when unambiguous; no mutable global selected-cycle target. Same-machine desktop/CLI access uses one runtime owner and finite authenticated operations. A platform lacking qualified transport must fail honestly until delivered, never open a second writer.

Scope external automation to prepared-round reads and optional response append. It cannot read arbitrary private drafts, verify feedback, publish PRs or scrape the database. A skill conveys procedure, not authority or reachability. The active harness keeps its own authentication, tools, policy and orchestration.

Ship the candidate via the existing shared AgentSkills/MASK source once separately authorized. Do not create hand-maintained provider-specific forks, alter implement-rfc roles or promise universal harness support. The plain Markdown/file path remains complete where native skill support, local commands or automation permission are absent.

## Owner amendment, 2026-10-06: dependencies are what this RFC uses

As imported, this RFC listed RFC-BPR-0044, 0045 and 0046 among its
dependencies. Those three close together on
[BPR-1071](../issues/10-backlog/BPR-1071-close-rfc-0046.md) and are Proposed.
The owner was asked in chat whether to amend the dependency and close this RFC
on its own evidence ("RFC-0058 … formally depends on RFC-0044/0045/0046 …
amend its dependency and close on its own evidence?"), and answered:

> Amend and close (Recommended)

RFC-BPR-0058 depends on what it uses from those RFCs, all of it landed, not
on their closure:

- from RFC-BPR-0044, the non-GUI runtime and `bpr` executable with its
  `TourCLI` install component,
  [BPR-1054](../issues/60-done/BPR-1054-build-and-install-a-non-gui-tour-runtime-and-bpr-executable.md)
  (Done), of which `bpr cycle` is a second command family, and the trusted
  external editor configuration,
  [BPR-1057](../issues/60-done/BPR-1057-integrate-trusted-external-editors-and-local-undo-redo.md)
  (Done), which `bpr cycle note` opens;
- from RFC-BPR-0045, the bounded, authenticated profile-owner channel,
  [BPR-1061](../issues/60-done/BPR-1061-add-bounded-authenticated-owner-forwarding-for-tour-commands.md)
  (Done), which RC58-05 extends with one `cycle` operation;
- from RFC-BPR-0046, nothing at run time. The `bpr-cycle` package follows the
  `pr-tour` package's layout and reached AgentSkills and MASK under its own
  owner authorization (BPR-1170).

The front matter's `depends_on` keeps RFC-BPR-0053, 0056 and 0057, and
`owner_dependency_amendment` names the three removed closure dependencies and
what is used from them. This amendment changes no RC58 criterion and closes
none of RFC-BPR-0044, 0045 or 0046: they stay open on BPR-1071. The question
and its answer are recorded on
[BPR-1191](../issues/30-in-progress/BPR-1191-close-rfc-0058.md) and in the
[execution ledger](../verification/RFC-BPR-0053-0061.md).

## Normative contracts

The detailed contracts [CLI-CONTRACT.md](assets/bpr-review-cycles-v1/specs/CLI-CONTRACT.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC58-01:** Agent-free headless cycle commands run without desktop/GUI/display/provider installation and share the desktop application services.
- **RC58-02:** Normal terminal output is human-readable; explicit JSON, stderr separation, noninteractive refusal and typed errors meet the CLI contract.
- **RC58-03:** Commands bind exact profile/workspace/cycle/round identities; ambiguous targets and active-window changes cannot redirect work.
- **RC58-04:** External reads expose only admitted prepared payload/context and never require database scraping or a generic file/HTTP server.
- **RC58-05:** Desktop-open and CLI-alone paths use one owner with qualified native IPC, bounded messages, peer checks and no competing writer.
- **RC58-06:** Cross-interface edits/freeze/undo preserve dirty text/IME and revision conflicts without lost updates or duplicate application.
- **RC58-07:** Consumers can obtain and verify every instruction page before implementing; counts/digests and limit outcomes are explicit.
- **RC58-08:** The thin bpr-cycle skill uses the active harness, no second model/orchestrator, no direct provider publisher, and no silent skill installation.
- **RC58-09:** Response-only automation appends reports but cannot verify, alter original instructions, export other history or publish.
- **RC58-10:** Manual terminal/Markdown and observed supported harness paths remain useful; missing skill support/reachability does not block the cycle.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
