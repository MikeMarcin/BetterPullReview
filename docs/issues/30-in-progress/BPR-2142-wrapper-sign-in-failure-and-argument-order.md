---
schema: "implement-rfc.card.v1"
id: "BPR-2142"
title: "A wrapper's sign-in failure ends the run as one; options with several values come first"
status: "In Progress"
rfcs: ["RFC-BPR-0103", "RFC-BPR-0104"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["RFC-BPR-0103 AC10", "RFC-BPR-0104 AC11"]}
---

# A wrapper's sign-in failure ends the run as one; options with several values come first

## Scope and acceptance

From the corporate wrapper tests (2026-11-11 and correction): report a
401 / `authentication_failed`, including plain-text output instead of stream
JSON, as a sign-in problem, and stop at the first `authentication_failed`
retry instead of waiting minutes. Place Claude's multi-value options
(`--add-dir`, `--allowedTools`, `--disallowedTools`, `--tools`,
`--mcp-config`) before `-p`, each followed by another option, so none can
swallow the prompt.

Acceptance: RFC-BPR-0103 AC10, RFC-BPR-0104 AC11 as amended on 2026-11-12.
