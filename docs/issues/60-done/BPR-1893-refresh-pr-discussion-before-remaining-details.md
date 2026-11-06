---
schema: "implement-rfc.card.v1"
id: "BPR-1893"
title: "Refresh PR discussion before remaining details"
status: "Done"
rfcs: []
depends_on: ["BPR-1890"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1893--BPR-1893--901ac9044cbf.json","milestone_plan":{"id":"responsiveness-latency-v8","path":"docs/rfcs/plans/responsiveness-latency-v8/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; reuse existing discussion layout and remaining-loading plate."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8f869fa6347b7dd087449c7d7b2f76624a68593c","evidence":{"kind":"observation","detail":"Observed authenticated refresh with review-thread support publishes complete PR-wide comments after inline threads and before catalogue pages. Provider tests pass for pagination, invalid data, shared budgets, cancellation, equal terminal records and request counts, unchanged first-open/signed-out/unobserved behavior."}},{"criterion":"AC2","outcome":"pass","revision":"8f869fa6347b7dd087449c7d7b2f76624a68593c","evidence":{"kind":"observation","detail":"Core and host courses pass for both conversation collection orders, exact current request/PR/head/base/mode, one-time independent admission, invalid/stale/terminal rejection and no new snapshot or epoch authority. Core-only and both architecture tests pass."}},{"criterion":"AC3","outcome":"pass","revision":"8f869fa6347b7dd087449c7d7b2f76624a68593c","evidence":{"kind":"observation","detail":"Corrected old-reader control fails the intended held-catalogue assertion. Final composed strict course opens the PR conversation, clicks and reads the new comment with remaining-loading visible while files are held, then returns to the exact comparison and scroll position. Offscreen screenshot inspected."}},{"criterion":"AC4","outcome":"pass","revision":"8f869fa6347b7dd087449c7d7b2f76624a68593c","evidence":{"kind":"observation","detail":"Release medians from three serialized runs per side: comment availability 6006 to 1672 ms, completion 6006 to 5999 ms, same 18 reads and zero blank samples. All six focused courses, separate strict discussion course, core-only, architecture, format, documentation and all eight changed TUs tidy pass; the final guard also passes the composed comment-edit contract. Source and journal landed on local main."}}]
---

# BPR-1893: Refresh PR discussion before remaining details

The owner requested continued direct responsiveness work, especially decoupled
loads with clear loading states, preserving other behavior and landing each
round on main with a card and one-line journal entry.

Inline replies now appear before independent file and detail reads, but PR-wide
comments remain invisible until the whole refresh completes. Give complete
PR-wide comments their own bounded observation after the inline conversation,
before the catalogue, without delaying the existing early inline reply.

## Acceptance

- AC1: An observed authenticated refresh with review-thread support publishes complete PR-wide comments
  before file pagination and remaining details. Inline conversation remains
  earlier; first-open, signed-out and unobserved reads keep their order. Final
  validation, record/request counts, budgets, failure and cancellation remain.
- AC2: Core and host admit each independently observed conversation collection
  at most once after metadata, only for the exact current refresh of the
  accepted PR, head, base and mode. Early content adds no snapshot or epoch
  authority, and stale, duplicate, invalid or terminal observations are refused.
- AC3: In the composed offscreen application, updated PR-wide comment text is
  readable while the catalogue is held, with remaining loading clearly shown;
  the comparison and reading position survive. The old-order control fails.
- AC4: Measure optimized before/after visibility, pass focused functional,
  architecture and coding checks, record one journal line and land on main.

## Plan

Use the existing observation channel and serialized lane. Add one independent
discussion observation containing complete issue comments and their worker-side
body digests. Share exact refresh admission with inline conversations, while
tracking their one-time acceptance separately. Reuse the existing conversation
projection and partial loading plate. Exercise both orders, invalid/stale data,
shared limits and held-catalogue interaction through the real composition.

## Implementation

The same authenticated refresh path that publishes inline conversations early
now publishes complete PR-wide comments next, before catalogue and remaining
detail reads. Inline replies keep their earlier position. Other reads retain
their existing order; all paths still use the same execution and shared bounds,
with mandatory validation before terminal snapshot acceptance.

One optional discussion observation carries the complete collection and its
body digests, computed off the window thread. The serialized host bounds posts
to metadata and one of each independent collection. The core shares the exact
refresh, PR, head, base and access-mode admission rules between the two kinds of
conversation, tracking each acceptance separately. The early projection keeps
the existing snapshot identity and grants no epoch handoff or new authority.

The current conversation view and "Refreshing remaining details…" plate are
reused; no widget/layout, transport, worker or request was added. The
frontend-design skill and desktop guidance apply to the offscreen interaction
and render check. The public-path course changes only its fake server's
comments, opens them through the real Review link while catalogue loading is
held, and returns to the retained comparison and exact reading position.

## Measurements and interaction

Optimized `qualification-release`, macOS arm64, AppleClang 21, Qt 6.12,
three serialized local fixture runs per side with 330 ms per simulated read:

| Median | Old reader | Independent discussion |
|---|---:|---:|
| Comment available to open | 6,006 ms | 1,672 ms |
| Full refresh settled | 6,006 ms | 5,999 ms |
| Requests | 18 | 18 |

No total-duration or live-network improvement is claimed. Each measured run
retained the comparison with zero blank samples. The strict held-catalogue
course opens the PR conversation through Review's link, clicks the new comment,
reads its rendered body with “Refreshing remaining details…” visible, then
returns to the exact comparison context and scroll position. Its offscreen
screenshot was inspected; the owner's desktop was never accessed.

The corrected old-reader control fails the intended before-catalogue assertion.
An earlier test sampled Review's intentionally file-scoped model for a PR-wide
comment and incorrectly reported no early content. It now observes the shared
cards' retained content and proves actual readability by clicking through the
public interface; all timings and the negative control were rerun. The first
pagination test also supplied a one-record nonterminal page where the contract
requires 100; the fixture was corrected. Both original failures remain in the
external logs. No product behavior was changed to accommodate either test.

The legacy strict transport fixture now reorders its exact recorded exchanges
for authenticated refreshes; first-open and signed-out recordings keep their
order. This repairs the conversation read course exposed by the earlier
conversation-first rounds, retaining request validation and complete consumption.

Evidence root:
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`.
Corrected commands, exit codes, three-run samples and medians are in
`discussion-corrected-baseline-commands.json`,
`discussion-corrected-final-commands.json` and `discussion-timing-summary.json`;
logs are `discussion-corrected-{before,control,held,after}.log`, with the rendered
comment in `discussion-corrected-renders/refresh-reply-readable.png`.

## Verification

- All six focused Release courses pass: `core_github_window_read`,
  `application_github_window_read_host`, `github_paged_read`,
  `application_progressive_open_public_path`,
  `application_refresh_current_file_public_path` and
  `application_conversation_read_public_path`
  (`discussion-corrected-functional.log`, 6/6). The separate strict discussion
  course also passes, including the actual comment click while files are held.
- Core and host checks cover independent observation order, duplicate/stale
  refusal, invalid records, cancellation and unchanged snapshot/epoch authority.
  Provider checks retain final records and request counts, first-open and
  unobserved order, complete pagination, shared budgets and cancellation.
- Core-only configure, build and `core_github_window_read` pass; both
  `architecture_boundaries` and `architecture_boundary_contract` pass
  (`discussion-verification-commands.json`). No dedicated Debug timing course
  was run or counted.
- clang-format 22 passes for all ten changed C++ files. The documentation gate
  passes (`discussion-docs-final.log`); its earlier failure was the branch's
  issue counter falling behind concurrent reservations, resolved by merging
  main. `git diff --check` passes.
- Full clang-tidy 22 ran on all eight changed translation units. Seven passed;
  the fixture exposed an existing analyzer warning at a comment-body pointer
  whose non-null condition is guaranteed by an out-of-line core decision. A
  local guard after that decision preserves its outcomes, including an
  already-saved response without write permission. The correction and focused
  recheck are recorded below.

Direct implementation and a focused simplification review reuse the existing
read execution, observation channel, projection and loading presentation. No
new worker or product layout was necessary. All evidence is local; no live
GitHub or other-platform latency claim is made.

The corrected fixture passes clang-tidy 22 with warnings as errors and the
rebuilt application's existing `application_comment_edit_contract` on both
GitHub.com and GHES fixtures (`discussion-guard-tidy.log`,
`discussion-guard-build.log`, `discussion-guard-functional.log`). Together with
the seven unchanged passing translation units, all touched C++ is lint-clean.
The final documentation gate passes (`discussion-closeout-docs.log`).

## Landing

Source, verified evidence and the one-line journal entry landed on local `main`
as `8f869fa6347b7dd087449c7d7b2f76624a68593c`. All four acceptance criteria are satisfied; no
owner action remains for this round. The owner authorized this landing before
the coordinated history rewrite. No remote push or release was performed.
