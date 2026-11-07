---
schema: "implement-rfc.card.v1"
id: "BPR-1928"
title: "Split github_application_host.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"8608b6ab7","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split github_application_host.cpp by responsibility

Round 14 of the largest-files refactor.
`src/application/src/github_application_host.cpp` is 5,119 lines. Its
`github_application_host::implementation` struct is 4,272 of them, with
about 110 members defined in the class body: dispatch and enrollment, public
preview and projection, profile sessions, viewer and capability reads,
credential refresh, store and retrieval, retained-refresh deletion, and
failure, cancellation, shutdown and launch renewal.

The round does not touch `native_https_transport.cpp`, `main.cpp`,
`installed_git_process.cpp` or `installed_git_cache.cpp`. Architecture rules
name each of those files as a designated effect owner or read its text, so
splitting them is a separate decision.

## Acceptance

- AC1: Move the struct into a private `github_host_implementation.hpp` and
  its member definitions, unchanged, into responsibility-named sources, as
  BPR-1911 did for the epoch host. Helpers used by one source stay private to
  it, and shared ones go in one support header.
  `bpr/application/github_application_host.hpp` does not change.
- AC2: Preserve behavior: application and app build in both configurations;
  the application, device-authorization and public-preview rows; the
  architecture and format gates.
- AC3: Land the change, the completed card and a journal line on main.

## Result

Source `74d867a99`; `github_application_host.cpp` goes from 5,119 to 654 lines
and keeps the public members and the credential binding catalogue; its
anonymous helpers moved to the sources that use them. The struct is now in
`github_host_implementation.hpp` (782 lines). New sources (lines):
`github_host_failure_and_shutdown.cpp` 614 (failures, cancellation,
shutdown, launch renewal), `github_host_viewer_and_capabilities.cpp` 606,
`github_host_credential_refresh_and_store.cpp` 596 (refresh probe, session
acceptance, store and retrieval), `github_host_public_preview_and_projection.cpp`
577, `github_host_retained_refresh_and_delete.cpp` 553,
`github_host_profile_sessions.cpp` 552,
`github_host_dispatch_and_enrollment.cpp` 443, and `github_host_support.hpp`
and `.cpp` (102 and 43). The two epoch host headers from BPR-1911 now spell
their include guards from their paths.

Checks on macOS arm64: full tests-off build; full dev build except `main`'s
broken `agent_context_supplement_tests.cpp`; and 354 rows (every
`application_*` row except the manual ones, the device-authorization and
public-preview rows, architecture and coding): 342 pass. The twelve failures
are the twelve this refactor's round-1 baseline already showed, minus
`application_review_tour_publication_manual`, which this run left out:
- `coding_format_check` flags only BPR-1925's `tour_agent_panel.cpp`.
- Five `application_large_review_*` rows miss Debug timing budgets only.
  Under the owner's standing policy those budgets don't gate.
- Six composed courses fail as before: agent invocation seed, threaded reply,
  thread words, accepted file in overview, profile window restoration, and
  pending states.

Windows and Linux were not built.
