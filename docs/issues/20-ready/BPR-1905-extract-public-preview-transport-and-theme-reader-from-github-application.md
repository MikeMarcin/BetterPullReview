---
schema: "implement-rfc.card.v1"
id: "BPR-1905"
title: "Extract the public-preview transport and theme reader from github_application.cpp"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"f430ebd907772b95576230d0858ae600e2f220c4","mandate":"Owner requested on 2026-11-06 a direct, behavior-preserving refactoring of the largest C++ files into smaller cohesive files, starting with github_application.cpp: one bounded extraction round, landed on main, with a card, coherent commits and a one-line refactoring journal entry. Files aim for about 500 physical lines; up to 1,500 when one responsibility clearly reads better together. No new public API, framework or dependency; no formatting or header games."}
---

# Extract the public-preview transport and theme reader from github_application.cpp

`src/app/src/github_application.cpp` is 42,564 lines. Its first 3,000 lines
are an anonymous namespace of small composition adapters, followed by the
profile runtime (about 2,800 lines), `github_application::implementation`
(about 19,300 lines, members defined in the class), the startup and public
`github_application` members, and 14,400 lines of tests-only
`github_application_fixture_access` members.

This round moves the adapters whose collaborators are narrow and whose
behavior is already covered, without exposing the profile runtime or the
implementation class to another translation unit.

## Acceptance

- AC1: Extract the signed-out public-preview wire transport (pull-request
  read counter, wire operation log, single-attempt counting transport,
  shipped anonymous transport factory, anonymous Git smart transport
  factory) into its own source and private header.
- AC2: Move the tests-only `public_preview_single_attempt_self_check` body
  out of the composition root into its own source, keeping its behavior in
  both tests-on and tests-off builds.
- AC3: Extract the bounded theme document file reader, including its
  Windows and POSIX file-handle code, into its own source and private header.
- AC4: Preserve behavior: the composition library builds and its focused
  public-path, appearance/theme, public-preview and composition contract
  courses pass; architecture boundaries, clang-format 22 and documentation
  checks pass. The composition contract's source pins and inventories are
  resealed for the moved code only.
- AC5: Land the change, the completed card and a one-line refactoring
  journal entry on main.
