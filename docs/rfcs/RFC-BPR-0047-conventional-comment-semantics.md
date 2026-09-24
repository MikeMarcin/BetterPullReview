---
id: "RFC-BPR-0047"
title: "Conventional Comment Semantics and Lossless Recognition"
status: "Proposed"
area: "BPR"
milestone: "Post-tour feedback conventions"
depends_on: ["RFC-BPR-0008", "RFC-BPR-0021"]
pack_version: "conventional-comments-v1"
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# RFC-BPR-0047: Conventional Comment Semantics and Lossless Recognition

## Outcome and scope

Make all twelve Conventional Comments labels first-class, optional review intent in
BetterPullReview. Publish ordinary Markdown that a GitHub-only collaborator can read.
Keep plain comments valid. A label is not a GitHub issue type, a defect state, a
review verdict, or permission to perform an action.

The complete catalog is `praise`, `nitpick`, `suggestion`, `issue`, `todo`,
`question`, `thought`, `chore`, `note`, `typo`, `polish`, and `quibble`.
`specs/LABEL-CATALOG.json` is the pack's vocabulary reference. The twelve stable
IDs participate equally in authoring, recognition, filters, optional counters,
icon roles and convention guidance. Hiding a column does not disable a label.
Custom labels remain readable, but custom-label registration/columns are not
part of this delivery.

The upstream format and meaning are referenced in [CC] in
`references/EXTERNAL-SOURCES.md`. This RFC's parsing tolerances, model fields,
cache policy and diagnostics are BPR design decisions, not additional upstream
requirements. No runtime download of the convention is needed.

## Separate facts and authority

Represent the following independently:

| Fact | Authority |
| --- | --- |
| Public declared label/decorations | Current accepted original comment body |
| Label guidance | Versioned built-in convention catalog |
| Explicit acceptance expectation | Exact recognized decorations, with conflicts retained |
| Shared resolution | Accepted provider observation for the exact native thread |
| Personal defect classification, severity and verification | Existing private defect workflow |
| Review-level disposition | Explicit native review submission and observed GitHub state |

No transition from a parsed annotation to publication, resolution, verification,
approval or Request changes exists. No per-label numeric priority or severity is
inferred. Unspecified expectation stays unspecified. Built-in guidance can
explain why a label normally implies a certain kind of action; it is not a
fabricated explicit `(blocking)` decoration.

Recognize `blocking`, `non-blocking` and `if-minor`, and preserve other decorations
as organization-specific text. `if-minor` is conditional, not shorthand for
unconditional blocking or non-blocking. Conflicting acceptance decorations
produce a diagnostic rather than selecting the last token. Also diagnose an
explicit decoration at odds with built-in guidance, particularly `note (blocking)`.
Never repair another person's body, hide the disagreement, or reject an otherwise
publishable plain comment solely because of convention advice.

## Lossless recognition

`specs/GRAMMAR.md` defines the intentionally bounded recognized subset. The
canonical authoring form is a lowercase label, optional parenthesized comma-
separated decorations, colon, subject and optional discussion. Accept every label
with and without decorations. Support the documented simple strong-emphasis
header variants and ASCII case-insensitive labels as read tolerances; do not
normalize stored text merely to obtain this support.

Recognize only the beginning of an eligible ordinary prose block at the start of
the user body. A quoted example, fenced or indented code, an HTML block, a list,
heading, link, or later discussion paragraph must not classify the whole comment.
Do not strip arbitrary leading text until something matches. Managed tour or
permalink envelopes are handled only by an exact, previously validated body-
segment contract from their existing owner; the parser must not guess a title or
marker away. A missing segment falls back to ordinary text and an explicit
classification-unavailable diagnostic where relevant.

Return source spans and parsed facts without replacing the original Markdown.
Unchanged parse/preview/copy/publication preserves original UTF-8 content and line
endings. An explicit edit may change the requested header span; everything outside
that span, including suggestion fences and replacement text, stays unchanged.
Offsets are byte offsets into the exact UTF-8 body revision, not Qt character
indexes; the presentation adapter must convert them deliberately for editing.

Malformed headers, unknown labels, unsupported presentation forms and budget
refusals remain readable through the original body. A malformed decoration list
is not silently accepted as an empty list. A supported label can be retained as
an observed raw token for diagnosis without receiving a recognized classification.
Unknown decorations do not make a valid supported label unknown.

## Comment versus conversation

Every post may have its own annotation. A logical conversation obtains its
counting label from its exact root post; a differently labelled reply neither
relabels the root nor creates another counted defect. Use normalized identity and
reply linkage, never the first row in an arbitrary page, newest post, nearest line,
or numeric-ID ordering. Missing, deleted or ambiguous roots have unknown
classification/catchall membership; never promote a surviving reply to root.

A later accepted edit of the actual root recalculates its public label and the
corresponding count bucket. Replies retain their own badges and can be found in
post-level search. Thread-count drill-down must use root-label semantics rather
than a broader "any post contains this label" search.

## Functional architecture

The functional core owns the catalog lookup, recognition result, diagnostics,
header-edit decision and semantic classification/projection rules. It uses no
Qt, GitHub SDK, filesystem, database, time or network capability. Prefer a small
bounded scanner for this prefix grammar; do not create a second complete Markdown
renderer or add a new dependency without demonstrated need.

The application supplies exact body revisions and identity, accepts provider
observations, persists only necessary draft/edit data, and invalidates cached
parses. Qt consumes annotations, maps source spans for an explicit edit, and
renders through existing native text surfaces. CLI adapters consume the same core
results. Public classification is a disposable derived cache, not an independently
editable database authority.

Key caches by host/repository/PR, comment identity, body revision/digest and parser
revision within the proper account/profile scope. A label can change without a
commit/head change. Account/window replacement or stale completion must not
restore an annotation from a different scope. Deleted or unavailable provider
content must not remain indistinguishably current in the cache.

## Bounds and compatibility

Use existing admitted body limits; this feature does not lower the raw-text
reading/publishing budget. Choose and record a bounded prefix/decoration scan
budget with direct pathological-input evidence. Exceeding the classifier budget
means unclassified-but-readable, not truncation or automatic non-blocking.
Parsing is linear in the inspected prefix, does not backtrack catastrophically,
and is reused rather than performed inside a paint callback or for every cell.

No hidden HTML metadata is required for convention meaning. Existing GitHub
thread identities, receipts, native Markdown copy/source behavior, author identity,
private provenance and native suggested-change payloads remain intact. Public
edits continue to use existing permissions and explicit publication workflows.

## Acceptance criteria

- R01: All twelve stable labels are supported in every advertised feature; no aliases replace the three additional labels.
- R02: Original body is authoritative; plain and unknown-label bodies remain readable and unchanged.
- R03: The parser accepts the documented start-of-body subset and refuses quoted/code/HTML/later-paragraph false positives.
- R04: Standard and custom decorations survive losslessly; conditional and contradictory expectations remain distinct.
- R05: Recognition and source/preview/copy cycles preserve bytes; explicit header edits leave unrelated body spans intact.
- R06: Public intent, label guidance, explicit expectation, shared resolution, local defect lifecycle and review verdict remain independent.
- R07: Root identity determines a conversation's counting label; reply labels and missing-root cases cannot hijack it.
- R08: Website edits invalidate the right derived state without mutating GitHub or unrelated scopes.
- R40: Bounded untrusted parsing causes no resource access, execution, unsafe Markdown activation or silent truncation.

Implement the corresponding core corpus before relying on label-derived UI or
counts. Passing the pack's fixture checks is not evidence that the product parser
exists. Core, host and mixed-client evidence belong in the live verification ledger.
