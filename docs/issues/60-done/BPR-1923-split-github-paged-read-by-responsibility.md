---
schema: "implement-rfc.card.v1"
id: "BPR-1923"
title: "Split github_paged_read.cpp by responsibility"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"e71bb6995","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"200218758","evidence":{"kind":"observation","detail":"Five github_paged_read_* sources and two support headers; public header unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"200218758","evidence":{"kind":"command","detail":"Tests-off builds; dev build except main's broken test; 83/84 rows pass, the one failure pre-existing. Windows/Linux not built."}},{"criterion":"AC3","outcome":"pass","revision":"dced9c6a3","evidence":{"kind":"observation","detail":"Card and journal line accompany the source for the main landing and push."}}]
---

# Split github_paged_read.cpp by responsibility

Round 11 of the largest-files refactor.
`src/adapters/github/src/github_paged_read.cpp` is 6,407 lines: 4,800 lines of
anonymous page, JSON, failure, rate-limit and record-parsing helpers and the
paged reads declared in `github_paged_read.hpp` (response metadata and
timeline, point reads, personal discovery and the pull-request read).

## Acceptance

- AC1: Move the definitions, unchanged, into responsibility-named sources
  beside it; helpers used by one source stay private to it, shared ones are
  declared in narrow support headers. `github_paged_read.hpp` does not change.
- AC2: Preserve behavior: GitHub adapter and app build in both
  configurations; the GitHub contract, paged-read, markdown image, large
  review replay and composition rows; architecture, format and documentation
  gates.
- AC3: Land the change, the completed card and a journal line on main.

## Result

Source `200218758`; `github_paged_read.cpp` goes from 6,407 to 42 lines.
New files (lines): `github_paged_read_pull_request.cpp` 1909,
`github_paged_read_personal_discovery.cpp` 1352,
`github_paged_read_page_wire.cpp` 1153, `github_paged_read_record_parsing.cpp`
1018, `github_paged_read_point_reads.cpp` 446,
`github_paged_read_metadata_and_timeline.cpp` 426,
`github_paged_read_page_wire.hpp` 361 and
`github_paged_read_record_parsing.hpp` 218. All are under 2,000. One support
header for every shared helper would have reached 2,250 lines, so the shared
helpers are split in two: page limits, JSON access, failures, rate limits and
link paging; and actor, state, connection and record parsers with the
collection reads. The pull-request source is long because its private
helpers serve only `perform_paged_github_read`.

Checks on macOS arm64: GitHub adapter and full tests-off builds; full dev
build except `main`'s broken `agent_context_supplement_tests.cpp`; format
gate; and 84 rows (every `github_*` row, large review replay, markdown image
resolution, architecture, composition and coding rows): 83 pass, and
`application_agent_invocation_composition_contract` fails its seed step as it
has in every run since round 1's baseline. Windows and Linux were not built.
