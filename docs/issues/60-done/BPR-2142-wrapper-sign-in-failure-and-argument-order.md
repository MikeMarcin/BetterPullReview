---
schema: "implement-rfc.card.v1"
id: "BPR-2142"
title: "A wrapper's sign-in failure ends the run as one; options with several values come first"
status: "Done"
rfcs: ["RFC-BPR-0103", "RFC-BPR-0104"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["RFC-BPR-0103 AC10", "RFC-BPR-0104 AC11"]}
acceptance: [{"criterion":"RFC-BPR-0103 AC10","outcome":"pass","revision":"1257a42c7e096039235e1647cf5e44e0c092248c","evidence":{"kind":"command","detail":"api_retry authentication_failed (or error_status 401) ends the run at that frame as not_signed_in; plain-text 401, a 401 result and Not logged in too; other retries and banners do not; the run's reason says it is a sign-in problem","command":"ctest --preset dev -R '^application_agent_launch_policy$'"}},{"criterion":"RFC-BPR-0104 AC11","outcome":"pass","revision":"1257a42c7e096039235e1647cf5e44e0c092248c","evidence":{"kind":"command","detail":"Every Claude launch (chat, writing, read-only, granted, with instructions, round) has its multi-value options before -p, each followed by another option","command":"ctest --preset dev -R '^(application_agent_launch_policy|application_agent_launch|application_enhance_comment_note_public_path|application_review_tour_writing)$'"}}]
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
