# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **find-and-find-in-files-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## search-session-policy

Search session identity and bounded host lifecycle.

**Work:** `DRAFT-0015`

**Prerequisites:** None

**Completion:**

- The core stores only exact scope, query/request generations, compact result counts and cursor facts; no query text, source documents or result collections enter interaction state.
- The public Qt-free search host validates the 4 KiB query boundary, submits no work for empty input, separates independent pane/PR sessions and reports invalid input without retaining apparently current results.
- Reversed query/scope/authority completions and duplicates cannot publish; supersession, clear, close and shutdown cancel old work, preserve exactly-one terminals for admitted effects, and reach observable quiescence.
- Next/Previous traverses accepted counts in constant work, wraps explicitly and never calls the worker; incomplete coverage cannot become unqualified No results.

**Boundary:** Complete this integrated outcome and upkeep; remaining whole-RFC criteria stay open.

## shared-matching

Shared Unicode source matcher and repository path filters.

**Work:** `DRAFT-0016`

**Prerequisites:** search-session-policy

**Completion:**

- Both commands share deterministic literal and bounded RE2-subset semantics: Unicode simple fold, Unicode word characters, nonoverlap, zero-width progress, multiline LF view and exact original UTF-8 spans.
- Include/exclude glob parsing follows comma/braces/classes/escaping and either-side include/exclude-wins rules; invalid patterns are refused before reads.
- Unified scanning follows actual version-contiguous source ranges, removes duplicate unchanged hits and cannot fabricate cross-version multiline matches; identical-blob same-entry deduplication is exact.
- Compile, scan, preview and result memory have explicit ceilings and cancellable bounded work; source-span tests need no QApplication.

**Boundary:** Complete this integrated outcome and upkeep; remaining whole-RFC criteria stay open.

## exact-pr-corpus

Exact retained-comparison corpus and streamed PR search.

**Work:** `DRAFT-0017`

**Prerequisites:** shared-matching

**Completion:**

- Historical membership derives from exact retained trees when no accepted catalogue proves the pair; missing trees/catalogue are unavailable rather than substituted.
- All changed entries, both exact paths and full documents participate independently of Files filters, receipts or opened state; added/removed/rename/copy and unsupported sides stay honest.
- Exact blob acquisition remains on the epoch actor, coalesces immutable reads across generations, yields behind selected navigation and pane Find, and keeps search inside existing cache limits.
- Results publish in defined path/side/offset order in bounded batches with precise partial/limit/retry coverage; core and host need no QApplication.

**Boundary:** Complete this integrated outcome and upkeep; remaining whole-RFC criteria stay open.

## pane-find

Native pane Find controls, decorations and exact reveal.

**Work:** `DRAFT-0018`

**Prerequisites:** exact-pr-corpus

**Completion:**

- AC01 and AC02 pass through the shipped focused split/unified source reader, including offscreen unchanged source, selection/caret seeding, multiline options, fixed selection, Unicode/line-ending exact selection, invalid regex and wrap/focus.
- View > Find command availability, editor/modality ownership and native layouts use the required frontend-design skill; visible highlights and scroll-track markers do not alter source/diff geometry.
- Match traversal/reveal through the shared controller performs no rescan/recompile/read/diff; palette, accessible counts/status and narrow-width rendering are verified.

**Boundary:** Complete this integrated outcome and upkeep; remaining whole-RFC criteria stay open.

## pr-search

Composed PR Search drawer, result activation and invalidation.

**Work:** `DRAFT-0019`, `DRAFT-0020`, `RFC-DRAFT-0015`

**Prerequisites:** pane-find

**Completion:**

- AC03–AC07 pass through real Search-tab commands and key paths: historical membership, exact result activation/F4, sibling preservation, filters/deduplication, stale completion/reveal, qualified coverage and accessible native layout.
- Selection/reveal binds PR authority, catalogue, tab, comparison, exact path/side/blob/span and query generation; later reading gestures cancel it.
- Pane and PR query state remain independent and runtime-only; authority changes clear sensitive text/results and all newly admitted effects reach their terminal.
- AC01–AC08 retain their complete original meaning and all pass on the exact integrated candidate, with any required attended/native judgment actually supplied.
- Large public-path fixture records warm/cold matching/acquisition, cancellation latency, peak memory, hardware/cache context and committed regression thresholds; navigation remains usable during cold scan and structural no-rescan rules hold.
- Command map, source-search behavior, resource ownership, baseline diff, verification ledger and live tracker agree; architecture and core-only checks plus documentation gates pass.

**Boundary:** Full original AC01–AC08 acceptance and truthful closeout. Human/native judgments remain pending until supplied.
