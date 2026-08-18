---
id: BPR-0333
title: Prove the agent output ceilings instead of asserting them
status: In Progress
rfc: RFC-BPR-0011
depends_on: []
---

# Prove the agent output ceilings instead of asserting them

[[BPR-0240]] requires proof of "bounded output/backpressure". Backpressure is
genuinely proven — a throwing sink drives the publish path in
`agent_launch_canary_tests.cpp`. The bounds are not. `output_too_large` and
`frame_too_large` appear four times under `src/` and **zero times under
`tests/`**.

Three declared ceilings, and what actually covers each:

| Ceiling | Where | Covered? |
|---|---|---|
| Total stdout, `maximum_agent_stdout_bytes` (1 MiB) | `src/application/src/agent_launch.cpp:1355` | No test reaches it |
| Retained stderr, `maximum_agent_stderr_bytes` (1 MiB) | `src/adapters/agent/src/agent_process_launcher.cpp:226` | No test reaches it |
| Per-frame, `frame_too_large` | reached twice | Reached, never asserted by value |

The stderr case is the one that looks covered and is not. The canary writes
exactly 256 KiB and asserts `result.StandardError.size() == 256U * 1024U` —
a quarter of the ceiling. That is a real proof of draining without deadlock, and
it is not a proof of the bound.

The per-frame case is reached but the outcome is never distinguished.
`agent_launch_tests.cpp:756` asserts only `!oversized.push(...)`, and
`agent_launch_canary_tests.cpp:462` asserts only `Outcome == protocol_failure`.
A regression that collapsed `frame_too_large` into `malformed_frame` would pass
both.

`docs/verification/RFC-BPR-0011.md:115` currently lists "bounds" among what the
component canaries cover. That is the overclaim this card removes.

## Required final state

- A canary switch emits more than `maximum_agent_stdout_bytes` of stdout, and a
  test asserts the run ends in the exact typed outcome the ceiling declares.
- A canary switch emits more than `maximum_agent_stderr_bytes` of stderr, with
  the same exactness, and the existing 256 KiB drain case is kept rather than
  replaced — it proves a different property.
- The two existing oversize-frame cases assert `frame_too_large` by value, so
  collapsing it into another protocol failure fails the suite.
- `RFC-BPR-0011.md:115` claims bounds only to the extent the tests now prove.

## Non-goals

- Do not raise, lower, or make configurable any ceiling. This card proves the
  values that exist.
- No Windows work. Every ceiling here is portable, and the POSIX path is the one
  under test.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_agent_launch|qt_agent_launch_canary)$'
python3 scripts/verify_documentation.py --root .
```

Each new assertion must fail if the corresponding ceiling is removed from the
source.
