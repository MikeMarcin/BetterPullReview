---
schema: "implement-rfc.card.v1"
id: "BPR-1115"
title: "Windows: a Git launch refused for its handle list is started and then killed; add committed coverage of the inheritance restriction"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"independent rfc-reviewer-critical review of BPR-1104 (claude-fable-5-1, read-only, macOS from source), findings 2 and 3","reviewed_candidate":"51784082","origin":"owner-authorized review, attended session cbd48b91000b00f1e72e1ad62c7c384c, decision ae582fb8-d480-456a-8ef5-79011030e131, 2026-09-24","reachable":"the refusal only when the attribute list cannot be created or a standard handle is NULL; the missing coverage always","related_cards":["BPR-1104","BPR-1114","BPR-1040"],"reservation":"docs/issues/_reservations/BPR-1111--BPR-1115--c042082d78b1.json"}
---

# BPR-1115: Windows: a Git launch refused for its handle list is started and then killed; add committed coverage of the inheritance restriction

## Source

[[BPR-1104]]'s independent review, 2026-09-24. The owner asked for it in
attended session `cbd48b91` (decision `ae582fb8`). The reviewer was an
`rfc-reviewer-critical` (`claude-fable-5-1`), working read-only on macOS
from source. The candidate was `51784082`. The review found no blocking
issue. This card takes findings 2 and 3. The review is recorded on BPR-1104
under "Independent review (2026-09-24)".

Line references are the reviewer's, in
`src/adapters/git/src/installed_git_process.cpp`. That file is unchanged
from `51784082` to the revision where this card was written.

## Defect

**1. "Refused" means started and then terminated (review finding 2).**
BPR-1104 says that a start which bypassed the list "inherits nothing and is
refused as `process_start`". In the code:

- When building the list fails, the catch at `:1891-1894` only clears
  `inheritHandles`. The program is still Git.
- The process starts at `:2627`.
- The refusal at `:2652-2654` comes after that start, and before
  `job.attach` (`:2675-2678`). So the child is not yet in the job object.
- `kill_process_group` is a no-op on Windows (`:1334-1349`).

So Git runs, without inherited handles, until it is terminated, and a
grandchild it started can survive. The guarded path already avoids this. When
it cannot redirect, it replaces the program with one that exits at once
(`:2036-2043`). The reviewer suggests the same pattern here, and tighter
wording. The branch is reachable only if the attribute list cannot be
created, or if a standard handle is NULL.

**2. The inheritance restriction has no committed failure-mode coverage
(review finding 3).** BPR-1104's Windows evidence is a station loop that was
not committed. Nothing in the tree fails if the handle list is removed, and
nothing drives the refusal branch.

## Scope

- In the catch at `:1891-1894`, replace the program with one that exits at
  once, as the guarded path does at `:2036-2043`, so that no Git runs on a
  refused launch. Or refuse before `start()`. Either way the result stays
  `process_start`.
- Change the code comments, and the wording on any live document, to say
  what now happens. BPR-1104 is Done and keeps its text; its review section
  already records the correction.
- Add the two rows the reviewer suggests.

## Acceptance

- AC1 A launch refused for its handle list never runs Git, and the result is
  still `process_start`. A committed row drives the refusal branch through a
  test seam that makes the list fail to build, and it shows that no Git
  process started.
- AC2 A committed Windows row holds an `std::ifstream` on a file, runs
  `git cat-file --batch` through an unguarded launch, and while Git runs
  asserts that an exclusive `CreateFileW` on the file succeeds. Removing the
  handle list fails the row (negative control).
- AC3 macOS and Linux keep their paths: every change is inside `Q_OS_WIN`,
  or the change is run there.
- AC4 No regression on Windows: `git_process_contract`, `git_cache_contract`,
  `git_sparse_cache_contract`, `git_protocol_boundary`,
  `native_git_smart_transport_contract`, `application_git_protocol_v2`,
  `architecture_boundaries`, `architecture_boundary_contract` and
  `msvc_secure_crt_contract`.

[[BPR-1114]] should use the refusal shape this card settles for the gh CLI
runner.
