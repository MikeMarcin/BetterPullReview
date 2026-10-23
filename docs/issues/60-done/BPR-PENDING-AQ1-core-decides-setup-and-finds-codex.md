---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ1"
title: "Core decides the agent setup, and a shipped build finds and starts Codex and the plain-text harness"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AN"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner answers Q1-Q9)","related_cards":["BPR-PENDING-AN","BPR-1301"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0092's design frames","milestone":"RFC-BPR-0092 M1"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "c3a561778", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(core_agent_setup|core_agent_wrapper|core_agent_profile)$'; ctest --preset core-only -R '^(core_agent_setup|core_agent_wrapper|core_agent_profile|architecture_boundaries|architecture_boundary_contract)$'", "detail": "core_agent_setup covers plain names, one Codex agent over both protocols, rows in order with each row's policy verdict (GHES keeps Codex refused and visible, an unmapped host refuses all), the plain-text harness only by its saved program, the choice without asking (one found, several wait, a saved choice wins, a forbidden one never), the status check's effect (signed in with version, not signed in, unreadable never signed in), every failure's own name and the four steps. No credential field exists in any setup type. All pass in build/rel and core-only."}}, {"criterion": "AC2", "outcome": "pass", "revision": "c3a561778", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(core_agent_wrapper|application_agent_program_resolution|qt_claude_process_wrapper|application_agent_finding_codex_found_contract|application_agent_finding_composition_contract)$'", "detail": "core_agent_wrapper pins Codex's search order and Windows codex.exe only. application_agent_program_resolution covers the saved program first and inspected at launch, a refused one never falling back, the found codex otherwise, codex_not_found_message when none, the plain-text harness only by its saved program, and pass-through of Claude and of named programs. qt_claude_process_wrapper's new case shows, through the real launcher, the shipped bare codex failing to start alone and the codex on a scratch PATH running through located_agent_process with Codex's exact arguments, a removed codex ending not found, and the unset harness starting nothing. The composed codex-found course is the positive finding course with no executable override and passes; the positive course is unchanged."}}, {"criterion": "AC3", "outcome": "pass", "revision": "c3a561778", "evidence": {"kind": "command", "command": "cmake --build --preset core-only; the focused rows above; _coding-standard/scripts/format.py (clang-format 22) and tidy.py --warnings-as-errors on every changed translation unit including github_application.cpp; python3 scripts/verify_documentation.py --root .", "detail": "core-only builds and its focused rows pass. Format and tidy are clean. The documentation gate fails only on provisional card IDs, as at RFC-BPR-0090's adoption. Also passing on the slice: application_review_round_trip_composition_contract (resealed), command_correctness_registry_contract, application_review_cycle_send_public_path, the eight application_agent_invocation_*_contract rows and application_agent_chat_composition_contract."}}]
---

# BPR-PENDING-AQ1: Core decides the agent setup, and a shipped build finds and starts Codex and the plain-text harness

RFC-BPR-0092 M1 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)).

## Scope

- The core's agent setup model: the steps, the agent rows (plain names, the
  found path and how it was found, host policy), the choice rule (one found
  is chosen, several wait for the reviewer), the decoded result of the status
  check, and the failure words for each setup reason.
- Codex is found as Claude is (Q5): a saved Codex program path, then `PATH`,
  then the usual folders; on Windows only `codex.exe`. The plain-text harness
  runs only a program path the reviewer chose and is never searched for.
- Every Codex and plain-text-harness launch resolves to an absolute program
  at launch, so a shipped build no longer hands a bare name to a launcher
  that refuses it.

## Acceptance criteria

- AC1: Core tests cover the rows, order, policy refusal, the choice rule,
  the check decoding and every failure's words (RFC AC02, core share; AC13,
  core share).
- AC2: The Codex search order and refusals are tested, and a composed launch
  with no fixture executable override starts the found absolute `codex`;
  none found ends with words naming where it looked (RFC AC03).
- AC3: `core-only` builds and its tests pass; format, tidy on the changed
  files and the documentation gate pass.
