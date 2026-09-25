# Review Cycles wire contract v1

The JSON schemas define field shape; the domain/specs also impose semantic and
resource constraints. Valid JSON or a matching digest is not trusted provenance,
write permission, source applicability or a complete instruction set by itself.
Schema documents use JSON Schema 2020-12. The artifact's standard-library reference
checks selected semantics and exact examples; they are not a general schema engine.

## Round envelope and payload

`schema = bpr.cycle.round-envelope.v1` wraps `payload`, its SHA-256 and the explicitly
named canonicalization profile. The payload has schema `bpr.cycle.round.v1`. Only the
payload is hashed. A new transport does not alter it. The outer serialized file may
have whitespace; changing even one authored payload character changes the digest.
Parse with duplicate-key refusal before canonicalization.

Cycle/round/source/item identifiers are opaque strings. Examples use RC-17/F1/R0 for
readability; no client may derive authority, storage paths or numeric range from them.
An item's positive decimal-string `revision` selects its exact text/suggestions.
Assignments contain an action, goal, constraints and advisory `write_scope`: that
field describes requested work, **not** a capability to edit files or execute code.
A recipient's own environment still controls those actions.

Items are ordered; `body` is authoritative Markdown, including an optional conventional
label in the text. No second public label field can disagree with it. `action_override`
is null unless the reviewer intentionally overrides the round action for that item.
`related_item_ids` must name included items. Separate `anchors` can reference multiple
source locations; their selected source context must be present or explicitly identified
as unavailable, never replaced with a guessed path in a different checkout.

A range anchor uses a project-relative path, source revision, full original file digest,
and half-open raw byte offsets. A file anchor has path and optional observed full file
digest with null offsets. Module anchors have a scope path with null digest/offsets;
cycle anchors have null path/digest/offsets. Suggestions require range anchors and exact
original bytes. Full-file context has start=0, end=length and blob digest equal to byte
digest. Excerpts have offsets into the original file; their byte digest is not the
whole-file digest. A snippet alone cannot establish exact patch applicability.

`bytes` objects carry strict base64, exact decoded `length` and SHA-256. Decode under
budgets before admitting data. Suggestion replacements are limited to 256 KiB; larger
source attachments use the separate source limits. Check range validity, original-slice
match when full material is present, same-boundary conflicts, valid text boundaries when
editing decoded Unicode, and `observed_already_applied` result references. Do not apply
an illustrative example or an already-applied change as a proposed exact replacement.

`selection` counts must equal the actual arrays. `instruction_set_complete` means the
selected assignment is complete, not that the entire project was reviewed. Intentional
exclusions are disclosed without leaking their private prose. A false/unknown completion
state cannot authorize the consumer to start work on a partial instruction set.

## Recipient responses

`bpr.cycle.response.v1` binds a unique response ID to one cycle, round and canonical
round digest. Entries name the exact included item revision. Dispositions are claims,
never `verified`, `approved`, `published` or capability grants. `claimed_author` values
remain self-reported unless separate runtime evidence corroborates them. A response
cannot supply its own trust rank. Optional `result_source_revision_id` is merely a
reference until BPR observes/resolves that revision; an external harness need not mint
local source revision IDs to complete work.

Evidence values are inert text. Opening a path/URL or reading a commit is a separate
validated action. Duplicate response ID plus identical canonical content is idempotent;
the same ID with different content is a conflict. A fresh reply can supersede a claim
by reference/history, not by altering an older imported response. Manual prose remains
supported and importing a response is never required to capture the next revision.

## CLI reads and paging

`bpr.cycle.result.v1` provides operation identity, typed outcome/code, exact target,
payload digest, completeness, cursor and operation-specific data. The schema deliberately
leaves `data` operation-specific; the admitted verb/version defines that nested shape.
A successful complete `round read --json` returns the whole validated round envelope
as data. Paged retrieval binds every cursor to the same round digest and scope grant,
reports total counts and verifies reconstructed canonical content before acting.
An opaque cursor is neither a grant nor a substitute for authorization.

Human default output is readable text. Machine stdout is one JSON result; diagnostics
belong on stderr. Command-line arguments do not carry review prose or credentials.
The skill discovers capabilities/schema support and does not depend on mutable active
tabs or an undocumented generic RPC. Proposed verbs are in CLI-CONTRACT.md; names must
be reconciled with the actual implementation and all consumer docs together.

## Portability and compatibility

v1 rejects unsupported schema versions and unrecognized executable/capability fields;
future versions need explicit migration or a safe read-only unsupported result. Unknown
metadata must not be silently treated as trusted instructions. Documents generated from
a round are projections; editing exported Markdown does not mutate the sender's frozen
round. Importing an edited document creates new local authored material with provenance.
The local store's richer identity and authorization state is deliberately not serialized
as reusable authority in these portable files.

## Reference-check coverage

The Python oracle covers canonical JSON, example counts and source bytes, representative
exact-edit conflicts, inert response correlation and a conservative in-memory bundle
validator. It is not the production parser and does not implement every JSON Schema
constraint, Unicode editor rule, provenance field or native filesystem defense. The
product must implement the complete schema and semantic contracts with the acceptance
courses. Never ship the reference code as an alternate BPR application.
