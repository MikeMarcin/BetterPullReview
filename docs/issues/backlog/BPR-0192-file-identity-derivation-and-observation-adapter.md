---
id: BPR-0192
title: File identity derivation and the workbench observation adapter
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0188]
---

# File identity derivation and the workbench observation adapter

Split out of BPR-0189, which was carded as "two adapters" before either port
was read closely. Observation turned out to need a decision the codebase has
never made.

## There is no way to name a file

`core::file_identity` is `fixed_identity_256<file_identity_tag>` — a 256-bit
identity. It is the key of RFC-BPR-0007's receipts, the subject of
`file_material_observation`, and the identity a workbench selection carries.

**Nothing in the codebase derives one.** The only two constructions in `src/`
are inside `github_application_fixture_access`, and both fill an array with a
constant:

```cpp
file.fill(0xA7U);
return core::file_receipt{ .File = core::file_identity{ file }, ... };
```

So every test that exercises receipts, selection, or actionability names files
with a number somebody chose. That works perfectly and says nothing about
whether two real files can be told apart.

## The decision this card must make

`file_identity` names the **file**; `file_material_id` names its **content**.
A receipt pairs them, so that a content change makes a receipt stale without
making it meaningless. That division is already in the types and is not open.

What is open is what a file's identity is derived *from*, and the honest answer
has a consequence that must be chosen deliberately rather than discovered:

- Derived from the **path**, a rename produces a new identity. A reviewer who
  marked a file read loses that receipt when a later push renames the file,
  even though not one byte of content changed.
- Carrying identity **across** a rename means consuming
  `github_file_record::PreviousPath`, which exists precisely because GitHub
  reports renames — and means identity is no longer a pure function of one
  path, so two callers can disagree about the same file.

Decide it here, record the reasoning, and make the losing consequence visible
in the card. Do not pick the one that is easier to hash.

Whatever is chosen, the derivation is a pure function of stated inputs, lives
in the functional core beside `file_identity` itself, and follows the existing
domain-separated canonical encoding — `diff_model.cpp`'s private
`canonical_encoder` appends a domain string before hashing, so identities of
different kinds cannot collide even when their inputs coincide. `sha256` is
already a core facility.

It must be **one** function. RFC-BPR-0007 keys receipts by this identity, so a
second derivation anywhere means a reviewer's receipts silently stop matching
their files.

## The observation adapter

`workbench_observation_port::assemble` answers a request with the actionable
file order. The source is `github_pull_request_snapshot::Files`, which the
application already holds.

Three obligations the port states outright:

- `Context` and `Authority` must repeat the request **exactly**, so a misrouted
  answer cannot be adopted.
- A port that cannot answer within `MaximumFiles` reports `limit_exceeded`
  rather than truncating, because a truncated order gives the core false
  neighbour facts.
- The port owns uniqueness within `Actionable`; the host resolves the first
  matching entry.

## What "actionable" can mean before RFC-BPR-0007 is composed

Actionability is decided from receipts — `core::actionable_item` carries an
`actionability_reason` — and the review ledger host is not composed yet.

So this adapter initially supplies the review's files in presentation order
with nothing filtered out, and the filter arrives when RFC-BPR-0007 is
composed. That is a staged delivery and must be **stated in the code and the
ledger**, not left for a reader to infer from an unfiltered list. An
"actionable" set that silently means "all files" is exactly the kind of quiet
half-truth this pack keeps finding.

## Required direct evidence

- Two different paths produce different identities; the same path produces the
  same identity across processes, proven by deriving in one run and comparing
  against a value recorded in the test rather than recomputed beside it.
- The domain separation is load-bearing: an identity derived for a file does
  not equal one derived from the same bytes for a different kind.
- The chosen rename behaviour is asserted directly, in whichever direction was
  decided, with the test named for the consequence rather than the mechanism.
- `Context` and `Authority` are repeated exactly; a mismatched answer is shown
  to be rejected by the host rather than adopted.
- A request whose file count exceeds `MaximumFiles` reports `limit_exceeded`
  and returns no partial order. Asserting the status alone is not enough — a
  truncating implementation could report it and still hand back rows.
- Cancellation through the stop token is honoured and reported as `cancelled`.
- The staged actionability is stated where a reader will meet it, and the
  ledger records it as unfinished rather than verified.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
