---
schema: "implement-rfc.card.v1"
id: "BPR-1890"
title: "Refresh conversations before the file list"
status: "Done"
rfcs: []
depends_on: ["BPR-1889"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1890--BPR-1890--32071319c6e9.json","milestone_plan":{"id":"responsiveness-latency-v7","path":"docs/rfcs/plans/responsiveness-latency-v7/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; retain the existing partial loading plate and conversation layout."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"fda0636eccb4a0e5c3587357942cdcf9ed237393","evidence":{"kind":"observation","detail":"Observed authenticated refresh publishes reviews and complete threads before any changed-file page, leaving Files absent. Provider tests retain final records and request counts, first-open and unobserved behavior, catalogue record/count validation, shared page limits and cancellation. The full github_paged_read course passes."}},{"criterion":"AC2","outcome":"pass","revision":"fda0636eccb4a0e5c3587357942cdcf9ed237393","evidence":{"kind":"observation","detail":"Core and host tests pass with files/conversation in either order after metadata, at most once each. Exact request, accepted PR, head, base, mode, cancellation and terminal checks remain; the early conversation grants no new snapshot or epoch authority. Core-only and both architecture gates pass."}},{"criterion":"AC3","outcome":"pass","revision":"fda0636eccb4a0e5c3587357942cdcf9ed237393","evidence":{"kind":"observation","detail":"The composed offscreen application passes the held-catalogue course: exact comparison context and scrollbar are retained before an actual thread click opens the new reply body while the file read is still blocked. Refreshing remaining details remains visible. Its screenshot was inspected. The old-order negative control fails the intended reply-before-files assertion."}},{"criterion":"AC4","outcome":"pass","revision":"fda0636eccb4a0e5c3587357942cdcf9ed237393","evidence":{"kind":"observation","detail":"Optimized three-run local measurements show composed median reply visibility 1670 to 1336 ms with 18 reads, and provider observation 1009.18 to 2.92 ms with 20 reads. Full changed-TU tidy, format, documentation and focused functional checks pass. The card, evidence and one-line journal entry landed on main as fda0636eccb4a0e5c3587357942cdcf9ed237393. No total-duration or live-network timing claim is made."}}]
---

# BPR-1890: Refresh conversations before the file list

The owner requested continued direct responsiveness work, particularly independent
loads with clear loading states, preserving other behavior and landing each round
on main with a card and one-line journal entry.

A refresh still reads every changed-file page before its reviews and threads.
When the accepted head and base are unchanged, the host already has the files
needed to validate and display that conversation. This independent catalogue
read should not hold up new replies on the file the reviewer is reading.

## Acceptance

- AC1: An observed authenticated refresh can publish its complete reviews and
  threads before changed-file pagination; first-open, signed-out and unobserved
  reads retain their order. The final snapshot, read count, shared limits,
  validation, error and cancellation obligations remain intact.
- AC2: Core and host admit metadata followed by files and conversation in either
  order, once each. Only an exact current refresh with the same accepted head,
  base and mode can update the conversation; no new snapshot or epoch authority
  is granted, and stale, duplicate or terminal observations remain excluded.
- AC3: Through the composed offscreen application, a new reply appears while
  the file-list read is held, the comparison and reading position remain, and
  the existing "Refreshing remaining details…" plate stays visible until the
  refresh completes. The dependency-breaking control fails on the old order.
- AC4: Record optimized before/after latency and focused checks, add one journal
  line, complete the card and land the verified round on main.

## Plan

Move the refresh's file read behind its complete conversation and ahead of
viewed-file validation. Keep a single paged-read execution and its bounds. Let
the core admit the two independent observations after metadata; bound host
callback posts to one of each. Reuse the existing refresh partial plate without
changing its layout. Test file failures/cancellation after the early reply as
well as the final successful records and the composed held-file path.

## Implementation

An observed authenticated refresh now reads and publishes complete reviews and
threads before its changed-file catalogue. The catalogue still precedes
viewed-file validation, within the same execution, page/byte limits and terminal
snapshot validation. First-open, signed-out and unobserved reads retain their
existing order. The conversation observation leaves `Files` absent, so an
unfinished catalogue cannot masquerade as a completed empty list.

Core admission now requires metadata before the conversation, independently of
the file list. The host bounds callback posts with one bit for each of the three
observations. Existing current-request, accepted-PR, head, base and mode checks
remain authoritative; early content grants neither a new snapshot nor an epoch
handoff. The later file observation cannot replace that conversation.

The existing partial loading plate and comparison projection are reused without
widget or layout changes. The frontend-design skill and its desktop guidance
were applied when inspecting the composed offscreen window. A real click opens
the newly arrived reply while the catalogue is held, and its rendered body is
readable beside the retained comparison and remaining-loading plate.

## Measurement

Native macOS arm64 (M4 Pro), AppleClang 21, Qt 6.12, optimized
`qualification-release`; three serial runs before and three after, with no
task build, lint or competing test during measurement. The provider fixture has
201 files and a 330 ms delay on each of its three catalogue pages. The composed
application has a 330 ms delay on each PR read. These are local, synthetic,
nongating measurements, not live GitHub or universal timing claims.

| Median | Before | After | Requests before / after |
|---|---:|---:|---:|
| Provider conversation observation | 1,009.18 ms | 2.92 ms | 20 / 20 |
| Provider terminal completion | 1,010.98 ms | 1,014.82 ms | 20 / 20 |
| Composed reply visibility | 1,670 ms | 1,336 ms | 18 / 18 |
| Composed refresh completion | 5,997 ms | 6,014 ms | 18 / 18 |

The improvement is earlier usable conversation; total refresh duration is
approximately unchanged. Every composed measured run retained the comparison,
recorded zero blank samples and showed no washed-out comparison after the reply
arrived. The final strict interaction run additionally preserved the exact
comparison context and scrollbar position before opening the reply.

## Verification

External evidence root:
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`.

- Release build passed for all five focused targets. The focused courses
  `core_github_window_read`, `application_github_window_read_host`,
  `github_paged_read`, `application_progressive_open_public_path` and
  `application_refresh_current_file_public_path` passed (5/5;
  `files-final-build.log`, `files-host-final-build.log`, `files-functional.log`).
- Core and host tests exercise both independent observation orders, one-time
  publication, exact current identity, first-open/moved-head refusal and no new
  epoch authority. The provider tests compare final records and request counts
  to an ordinary read, reject invalid catalogue records/counts, preserve shared
  budget exhaustion before/during catalogue loading, and stop continuation on
  cancellation after the early conversation.
- The old product order, built with the held-catalogue public-path test, failed
  the intended assertion that the reply and loading plate appear before the
  catalogue is released (`files-order-negative-control.log`). The final strict
  course passed, including an actual thread click and visible rendered reply
  body while held (`files-readable-public.log`,
  `files-readable-command.json`,
  `files-final-renders/refresh-reply-readable.png`). The rendered reply,
  comparison and "Refreshing remaining details…" plate were visually inspected
  offscreen. The owner's desktop was not driven or captured.
- Optimized measurements are retained in `files-before-provider.log`,
  `files-after-provider.log`, `files-before-public.log` and
  `files-after-public.log`; `files-timing-summary.json` records all six samples
  for each fixture and their medians. Measurement invocations and exit statuses
  are in `files-baseline-commands.json` and
  `files-final-measurement-commands.json`.
- Core-only configure, focused build and `core_github_window_read` passed.
  Both `architecture_boundaries` and `architecture_boundary_contract` passed,
  including all 163 contract tests (`files-verification-commands.json`,
  `files-core-only.log`, `files-architecture.log`). Dedicated Debug timing
  courses were not run or counted as evidence.
- Full clang-tidy 22 with warnings as errors passed for all seven changed C++
  translation units (`files-tidy.log`); the final interaction-test enhancement
  was rebuilt and independently passed tidy (`files-readable-build.log`,
  `files-readable-tidy.log`). No user-code warning remains in this scope.
- The documentation gate, clang-format 22 check on all nine changed C++ files
  and `git diff --check` passed. The explicit selected milestone binding is
  valid (`round-seven-verified-binding.json`); unrelated historical global
  checkpoint discovery still encounters an unsupported `amendments` field.

Direct implementation and self-review found no additional mechanism necessary:
the existing paged read, observation channel and loading presentation suffice.
No extra requests, worker, provider dependency or product layout was introduced.
No live GitHub or other-platform performance claim is made.

## Landing

Source and one-line journal entry landed on local `main` as
`fda0636eccb4a0e5c3587357942cdcf9ed237393`. All four acceptance criteria have
direct execution or observation evidence; no owner action remains for this
round. No remote push or release was performed.
