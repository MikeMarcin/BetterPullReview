---
schema: "implement-rfc.card.v1"
id: "BPR-0192"
title: "File identity derivation and the workbench observation adapter"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0188"]
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

## The decision: identity is a pure function of the path

**A rename produces a new identity.** A reviewer who marked `old/name.cpp` read
sees it unread again once it moves, even though not one byte changed.

That cost was chosen, not overlooked. The alternative carried identity across a
rename by consuming `github_file_record::PreviousPath`, and it fails on
something worse than inconvenience: identity would depend on the review
*snapshot* rather than on the file. Two callers holding different snapshots
would derive different identities for the same file, and the receipt key would
stop being well defined — which is the one thing it cannot be, because
RFC-BPR-0007 stores receipts under it.

Re-reading a moved file is a small cost paid visibly. Receipts that silently
stop matching are neither small nor visible.

`core::file_identity_of` is the single derivation, and the doc comment says so
in those terms. A second one anywhere re-keys every stored receipt.

## What was built

- `core::file_identity_of(std::string_view)` in `diff_model.hpp`/`.cpp`, using
  the existing domain-separated canonical encoding: `BPR-file-identity-v1`,
  then the length, then the path bytes. Empty or beyond
  `core::maximum_identified_path_bytes` derives an invalid identity.
- `application::snapshot_workbench_observation` — a
  `workbench_observation_port` over `github_pull_request_snapshot::Files`. No
  repository, no process, no clock. The snapshot source is a plain callable so
  composition decides where it comes from.
- Tests: two cases in `tests/core/diff_model_tests.cpp`, seven in
  `tests/application/workbench_snapshot_observation_tests.cpp`.

### The encoding is pinned to a recorded digest

`sha256("BPR-file-identity-v1" || u32be(16) || "src/app/main.cpp")` is written
into the test as a literal rather than recomputed beside the assertion.
Recomputing would agree with any derivation including a changed one, and this
value is the key of durable receipts: the day the encoding moves silently is
the day every reviewer's stored progress stops matching their files.

### Absent is not empty

The supplier returns `std::optional<std::vector<...>>`. A disengaged optional
means the snapshot is not held; an engaged empty vector means the review has no
files. Both cases are constructed in the tests, because collapsing them would
tell a reviewer that a busy pull request was empty.

### Staged actionability

The adapter reports every file, unfiltered. `Actionable` is named for a filter
that cannot exist yet — actionability is decided from receipts and
RFC-BPR-0007's ledger host is not composed. That is stated in the adapter and
in the RFC-BPR-0005 ledger rather than left to be inferred from an unfiltered
list.

## A false claim in my own test comment, caught by a break

Two breaks landed on the same assertion, which exposed it. The test compares
`"a/bc"` against `"ab/c"` and the comment claimed this proved the length prefix
load-bearing. It does not: both are four bytes, so they differ with or without
the prefix. With a fixed domain and the path as the final field, no two
distinct paths can collide whatever the prefix says — the prefix is defensive
against a future appended field, and the recorded digest is what actually pins
the encoding. The comment now says that.

## Evidence

### Six breaks, each with 0 compile errors, each on its own assertion

| Break | Assertion that failed |
|---|---|
| Domain string changed to `-v2` | the recorded-encoding assertion |
| Length prefix dropped from the encoding | the recorded-encoding assertion |
| Path bound no longer refused | "a path beyond the published bound is refused, never truncated" |
| `limit_exceeded` reports *and* truncates | "and hands back no partial order at all" |
| Duplicate paths no longer collapsed | "a snapshot repeating a path yields one entry for it" |
| Unnameable path silently skipped | "a path this build cannot name makes the whole answer unavailable rather than silently dropping the file" |

The truncation break is the one that justifies its own test. It reported
`limit_exceeded` **and** handed back four rows; the status assertion passed and
only the empty-order assertion caught it. A test checking the status alone
would have certified an implementation that gives the core false neighbour
facts — the last file it could see looking like the last file there is.

### Gates

- `ctest --preset dev` — **121/121**.
- `ctest --preset core-only` — **56/56**, up from 55.
- `coding-format-check` — 283 files, passed.
- `verify_documentation.py` — `DOCUMENTATION_OK`. `git diff --check` clean.

The core-only count rising is the useful part. The observation adapter's suite
runs under a configuration that links neither Qt nor SQLite, so the adapter is
proven free of both by linkage rather than by reading it.
