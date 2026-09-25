---
id: "RFC-BPR-0058"
title: "Headless Cycle CLI and Portable Consumer Skill"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0058: Headless Cycle CLI and Portable Consumer Skill

## Outcome and decisions

Extend the shared AI-free bpr runtime with bpr cycle commands and a thin portable bpr-cycle consumer skill. The desktop and CLI invoke the same C++ services. A harness with local command reachability can read a specific prepared round directly, without copy/paste, file export, another model or an embedded BPR agent session.

Use human-readable output by default, explicit JSON, bounded results and exact target/revision identities. Resolve CWD only when unambiguous; no mutable global selected-cycle target. Same-machine desktop/CLI access uses one runtime owner and finite authenticated operations. A platform lacking qualified transport must fail honestly until delivered, never open a second writer.

Scope external automation to prepared-round reads and optional response append. It cannot read arbitrary private drafts, verify feedback, publish PRs or scrape the database. A skill conveys procedure, not authority or reachability. The active harness keeps its own authentication, tools, policy and orchestration.

Ship the candidate via the existing shared AgentSkills/MASK source once separately authorized. Do not create hand-maintained provider-specific forks, alter implement-rfc roles or promise universal harness support. The plain Markdown/file path remains complete where native skill support, local commands or automation permission are absent.

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
