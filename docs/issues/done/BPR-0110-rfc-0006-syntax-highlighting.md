---
id: BPR-0110
title: Implement complete-blob syntax classification
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0109]
---

# Implement complete-blob syntax classification

Implement the custom low-level highlighter that produces semantic spans and line-end state.

Highlight complete old and new blobs independently so a hunk beginning inside a multiline comment or string receives the same classification as the complete blob — this is the acceptance criterion that a hunk-local highlighter would silently fail. Cover C++, CMake, Markdown, JSON, Python, and plain-text fixtures selecting their expected definitions.

Switching theme or color must not re-run lexical parsing; prove it by counting parses across a theme change.

## Implementation

`bpr::adapters::syntax` implements the classification port BPR-0105 already
declared, through KSyntaxHighlighting's low-level `AbstractHighlighter`.

The whole blob is classified in one lexical pass, and a hunk is a *projection*
of that pass rather than a separate highlight. The code says why at the site:
highlighting a range on its own would lose the state that opened a multiline
comment or string on an earlier line. That is the entire mechanism behind the
acceptance criterion, and it is the reason the derivation must not be
"optimised" into a per-hunk call later.

Definitions and revisions come from packaged records; a name the build does not
package resolves to nothing rather than falling back, and a key naming an
unpackaged revision is refused. Roles come from the closed core vocabulary — the
adapter produces roles, never colours.

## Direct verification

- The syntax contract passes; the full `dev` build is clean; both architecture
  boundary suites pass; the format gate and strict `clang-tidy` with
  `--warnings-as-errors` pass.
- All six required languages are exercised from fixtures: C++, CMake, Markdown,
  JSON, Python, and plain text, each asserted to select its expected
  definition, with a JSON Lines fixture alongside JSON.
- **The Qt-free `core-only` preset does not acquire this adapter.** Its
  `CPM_PACKAGES` is `BprSQLite` alone, and it passes without Qt. The single
  `syntax` string in its cache is CMake's own boilerplate comment, checked
  rather than assumed.
- Non-vacuity, the criterion that matters: dropping spans that opened *before*
  the hunk's first line — precisely what a per-hunk highlight loses — fails with
  "a hunk beginning inside a multiline comment classifies exactly as the same
  region of the complete blob". A weaker break would not have shown the test
  can detect the defect the criterion exists for.
- The suite also pins one classification to one lexical pass, refuses an absent
  blob before any span bound applies, refuses rather than truncates a
  classification above the bound, and reports cancellation.

## A delivery note

This slice took two passes. The first spent its entire budget investigating and
produced a single unwired `CMakeLists.txt`, leaving the tree unaffected but the
work undone. The recovery was to remove decisions rather than restate
requirements: name the port to implement, state that an unmapped class maps to
the nearest role and is reported rather than added to the closed vocabulary,
and say plainly that a partial slice with tests beats a complete one without.

## Retained risks

- Class-to-role mapping covers the categories the core vocabulary names. A
  KSyntaxHighlighting class with no close role maps to the nearest and is
  reported rather than silently extending a vocabulary that BPR-0108's exchange
  format treats as a compatibility contract.
- No Qt surface consumes the spans yet. Applying roles to rendered text is
  presentation work that no slice owns.

## Result

Blobs classify completely and hunks project from that pass, so a hunk opening
inside a multiline construct carries the same classification as the whole file.
Six languages select their definitions from packaged records with no standard
path consulted, and the Qt-free preset is provably untouched. BPR-0110 is
complete.
