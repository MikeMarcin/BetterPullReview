---
id: RFC-BPR-0004
title: Diff Computation, Line Mapping, and Change Grouping
status: Proposed
area: BPR
milestone: M1 Native Read-Only Workbench
depends_on: [RFC-BPR-0003]
pack_version: 2
---

# RFC-BPR-0004: Diff Computation, Line Mapping, and Change Grouping

## Summary

Build the deterministic diff model that powers side-by-side comparison, stable source-line identity, intraline changes, grouped center connectors, navigation, and the selected-change line-over-line detail pane.


## Motivation

The distinctive BetterPullReview experience depends on trustworthy line identity and visual grouping. A pretty renderer built on an underspecified diff model would fail on comments, navigation, epochs, and verification.

## Goals

- Compute deterministic file and line changes between any retained epoch pair.
- Represent additions, deletions, replacements, renames, binary files, and unsupported/oversized content.
- Preserve stable old/new source-line identity and hunk/change-group identity.
- Compute intraline spans.
- Build clean ordered change groups for the center connector band.
- Drive previous/next change navigation.
- Produce the selected-change line-over-line detail model.

## Non-goals

- Full semantic AST differencing.
- General moved-code detection in the first implementation.
- Rendering widgets or comments.

## Architectural allocation

- **Deterministic core/planner:** owns the Qt-free algorithms for normalized line identity, hunks, intraline spans, ordered change groups, selected line-over-line mapping, and previous/next change semantics. Given the same blobs, options, and scratch storage, it produces the same result.
- **Application host:** owns blob loading, Git-process output, background execution, cancellation, request generations, immutable diff/syntax caches, and bounded prefetch. It may execute the deterministic planner on a worker, but the worker cannot mutate core interaction state or Qt.
- **Functional core state:** owns which file/change is active and whether a computed result still belongs to the current PR/epoch selection.
- **Qt adapter:** paints the supplied model and connector geometry, translates selection/navigation input, and never invents line correspondence or regrouping.

## Diff model

The public diff result is immutable and contains file changes, hunks, old/new lines, display alignment rows, intraline spans, change groups, and anchor mappings. Comparison policy includes whitespace mode and rename/copy thresholds and participates in cache identity.

Change grouping preserves source order. A connector may taper around unequal group heights but cannot cross another group unless the underlying mapping itself is non-monotonic; the initial model avoids non-monotonic semantic mappings.

The lower detail model is derived from the selected group and contains paired line-over-line rows rather than a generic unified text patch.

## Acceptance criteria

- Golden fixtures validate file classification, paths, line numbers, hunks, intraline spans, and groups.
- Added, deleted, modified, renamed, binary, submodule, and too-large files remain explicit.
- Whitespace-policy changes produce separate cached results and predictable navigation.
- Previous/next change visits each group once in display order.
- The lower detail model exactly matches selected old/new group lines and context.
- Cancellation prevents stale or partial results from entering the current projection.
- Parser and mapping code has focused fuzz targets and allocation/line-length limits.
- The deterministic diff planner is directly replay-tested without Qt/Git I/O, and host/Qt tests prove workers and rendering do not alter or reinterpret its line/group identities.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Line mapping is correct | Public diff model | Compare every row to known blob/line identities |
| Group connectors are truthful | Group model | Validate monotonic extents and no invented correspondence |
| Lower detail is selected-group-specific | Detail model | Select groups with unequal heights and compare expected paired rows |
| Navigation order is stable | Domain command sequence | Traverse forward/backward and match fixture group order |
| Diff planner is deterministic and presentation-neutral | Core replay plus render comparison | Same inputs produce same identities/groups; Qt paints without regrouping |

## Documentation impact

Diff semantics, whitespace policy, rename handling, unsupported content, group construction, line identity, anchor mapping, cancellation, and limits are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
