# BPR-0971 classifier handoff — 2026-09-25

Role: rfc-implementer | Model: gpt-6-astra | Effort: xhigh

Assignment `classifier-20260925`, baseline `daaa2023`, branch
`codex/bpr0971-classifier`; source/tracker root
`/Users/jaeger/.codex/worktrees/bpr0971-classifier/BetterPullReview`.
The prior audit remains integrated. Codex only, no delegation, primary checkout
edits or push. Concurrent BPR0905/main.cpp work and protected RFC0052 attendance
are untouched.

## Result

Unaccepted partial milestone. No product source or sanitizer policy change.
Test candidate `161b988b26e0a4299059a4f570164b3acd91a67b` adds:

- `tests/support/capability_error_content_canaries.hpp`: eight synthetic cases,
  including four identifier-shaped variants, with distinct per-field markers.
- `tests/github_contract/github_capability_probe_tests.cpp`: opt-in sanitizer
  acceptance probe; existing exact useful unfamiliar error control preserved.
- `tests/github_contract/github_provider_tests.cpp`: the same eight cases
  through the real provider facade, credential absence and failure terminal
  checks, and stronger exact field/code usefulness control.
- `tests/logging/quill_logging_tests.cpp`: the same cases through real private
  file storage and export, with per-case boolean evidence.

All three opt-in audits exit 1: all eight markers are retained before/file
storage, no markers survive export, and the active credential is absent.
The [investigation](../../../verification/bpr0971-capability-classifier.md)
records the exact policy conflict and why filtering names by shape cannot fix
it while guaranteeing exact arbitrary unknown-value retention.

## Verification

Configure and three target builds pass. Baseline focused CTest 3/3, 16.98 s;
candidate focused CTest 3/3, 15.61 s. Scoped format and diff hygiene pass.
Strict tidy provider/logging units pass; the capability-test unit reports ten
existing findings, reproduced on the unchanged baseline with matching source
SHA-256. Aggregate strict tidy remains failed, with no suppressions or unrelated
helper edits. Exact commands and dispositions are in the investigation.

The documentation gate passes with the installed MASK scripts through
`PYTHONPATH` because this isolated worktree lacks the ignored local skill
deployment. Board validation passes (1,182 cards). The final checkpoint helper
result is retained beside this handoff.
No native HTTPS, live account, remote mutation or release ran. Windows/Linux
verification remains owner-deferred; existing native crash limits remain open.

## Review handoff

There is no repaired product candidate for attended acceptance and no review
server/session was started. One bounded contract interpretation is prepared:
may useful unknown errors omit unclassified code/symbol bytes, while retaining
safe explanations, known public query symbols, correlation and an explicit
content-policy omission reason? Resolve that before changing exact historical
usefulness tests. It does not waive privacy or close BPR0971.

## Ownership and continuation

All task-owned configure/build/test/tidy processes have completed. The previous
audit worktree was read only for one baseline tidy run; it remains unchanged.
No fixture, app, review server, watch or repository writer is handed off. The
writer is released; the new isolated worktree and ignored build may remain for
an explicit continuation. No temporary source substitution or product policy
experiment remains. Logs contain synthetic fixed evidence only.

The card remains In Progress: AC1 partial, AC2 failed, AC3 partial from the
original audit. No card is promoted and no RFC closes. The completion helper
rebinds after the card checkpoint changes; it records continuation with the
remaining obligations, not acceptance.

Next independent slice: give direct recovery/Git diagnostic stderr an explicit
bounded, classified output contract and exercise blocked-output/shutdown with
synthetic paths/source canaries. Start from the producer inventory in the
original audit; preserve causes and request/durable authority. Coordinate the
main.cpp owner before editing it. Diff/syntax/persistence/queue/agent hostile
all-output courses remain separate pending evidence. The classifier decision
does not block those investigations.

Ordered commits begin `303c7cdb` (assignment binding) and `161b988b` (canaries).
The final checkpoint commits contain evidence, card state and consistency
records. Integrate this branch only after inspecting these intentionally
failing acceptance probes and the inherited tidy failure; none is a new passing
privacy qualification.
