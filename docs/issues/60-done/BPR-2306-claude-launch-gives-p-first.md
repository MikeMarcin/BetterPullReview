---
schema: "implement-rfc.card.v1"
id: "BPR-2306"
title: "Every Claude launch gives -p first; RFC-BPR-0104 AC11 amended"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"amendment","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2306--BPR-2306--36d7747cf794.json","origin":"owner decision, 2026-11-19: \"Amend AC11\" (after BPR-2304 found AC11 kept -p after the multi-value options)","related_cards":["BPR-2142","BPR-2178","BPR-2304"],"related_rfcs":["RFC-BPR-0104"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2306: Every Claude launch gives `-p` first; RFC-BPR-0104 AC11 amended

## Asked

[[BPR-2304]] found that RFC-BPR-0104 AC11 required every Claude option that
takes several values (`--add-dir`, `--allowedTools`, `--disallowedTools` and
the rest of `core::claude_variadic_options`) to come before `-p` and be
followed by another option. The owner's harness audit of 2026-11-19 found
that argument order makes no difference to Claude, and suggested
`<fixed arguments> -p --append-system-prompt … --setting-sources user
--mcp-config <empty> --strict-mcp-config --output-format stream-json
--verbose <prompt>`. The owner, 2026-11-19: "Amend AC11".

## Change

- RFC-BPR-0104: a dated amendment records the decision; AC11 now reads "each
  option that takes several values is followed by another option".
- Core: `inspect_claude_arguments` refuses `open_variadic` only when the last
  option given takes several values, so one directly followed by the prompt
  or ending the arguments still refuses. Its position relative to `-p` no
  longer matters.
- Application: `build_agent_launch` and `build_round_launch` give `-p`
  first, so an added agent's fixed arguments are followed directly by `-p`.
  The rest keeps its order; the prompt still rides stdin.
- The finding [[BPR-2178]] filed (a launcher that needed `-p` first) is done
  there; that card's other findings stay open.

## Acceptance

- AC1: RFC-BPR-0104 AC11 is amended as the owner decided, with the decision
  dated and quoted.
- AC2: the core refuses a several-valued option left open (directly followed
  by the prompt or the end of the arguments) and accepts one closed by
  another option before or after `-p`.
- AC3: every Claude launch kind (Agent tab, writing, read-only, granted,
  with instructions, both round kinds) gives `-p` first, and each option
  that takes several values is followed by another option.
- AC4: a custom agent's fixed arguments are followed directly by `-p`.

## Evidence

Candidate `a257583e1` (core-only Debug tree at that commit, macOS):

- `core_claude_launch_arguments`: closed before and after `-p` passes; left
  open, empty, `=`-spelled open, or last after other options refuses (AC2).
- `application_agent_launch_policy`:
  `test_options_with_several_values_are_followed_by_another_option` (`-p`
  first, each closed) and `test_every_claude_launch_keeps_the_audited_rules`
  over every launch kind (AC3).
- `application_agent_launch`, `application_review_cycle_round_launch`: the
  exact argument lists with `-p` first (AC3).
- `application_agent_program_resolution`:
  `test_a_custom_agents_fixed_arguments_come_first` checks `-p` right after
  the fixed arguments (AC4).
- `application_claude_process_wrapper` passes unchanged.
- `tests/end_to_end/scoped_writing_agent.cpp`, the stub `claude` the tour and
  writing courses run, expects the new order. Its dev-tree courses were not
  run (the disk had under 4 GB free); the stub was compiled on its own and
  recognised a writing launch in the new order (and not in the old).
- `python3 scripts/verify_documentation.py --root .` passes; clang-format 22
  leaves the changed files unchanged; clang-tidy on the changed translation
  units reports only findings that were already there.
