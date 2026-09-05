# BetterPullReview RFC-BPR-0022 Implementation Pack

**Private Review Threads, Provenance, and Comment Composition**

This pack turns the product discussion recorded on 2026-09-04 into an
implementation-ready post-pack RFC and provisional issue decomposition.

The feature adds a durable private deliberation layer to BetterPullReview:
reviewer notes and local conversations can remain local, AI contributions carry
honest runtime provenance, selected material can be non-destructively squashed
into a local review draft, and publication still occurs only through the
ordinary GitHub round-trip owned by RFC-BPR-0008.

## Governing product decision

The user-facing workflow is deliberately two-step:

```text
Private thread / selected turns
        -> Squash to Draft
        -> inspect and edit the local draft
        -> Publish through ordinary GitHub controls
```

`Squash to Draft` never publishes. The source thread is retained. GitHub
receives no hidden BetterPullReview marker, provenance blob, or mandatory AI
footer.

## Source baseline

- Project source generated: `2026-09-04T11:47:19-07:00`
- Source dump: `better_pull_review_project_source.md`
- Source SHA-256: `0c73fc6d9b0c19e89ef43d728ef5d7a3bdc86ddf8cc6e2828ae7abf6a0d257d2`
- Expected repository: `BetterPullReview`
- Highest RFC present in the source baseline: `RFC-BPR-0021`
- Review-ledger schema in the source baseline: `5`

The importer must reconcile this baseline with the live tree. Live repository
state outranks this snapshot. In particular, schema versions and live issue IDs
must never be reused if the repository has advanced.

## Pack contents

| Path | Purpose |
|---|---|
| `PACK_MANIFEST.json` | Machine-readable inventory and source baseline |
| `IMPLEMENTATION_ORDER.md` | Dependency graph, phases, and parallelization guidance |
| `IMPLEMENTATION_PROMPT.md` | Paste-ready steering prompt for the repository's `implement-rfc` skill |
| `repository-overlay/docs/rfcs/future/RFC-BPR-0022-*.md` | Normative RFC |
| `repository-overlay/docs/rfcs/plans/RFC-BPR-0022-issue-plan.md` | Immutable imported planning source |
| `issue-templates/*.md` | Provisional cards to allocate into the live board |
| `supporting/CURRENT_STATE_SOURCE_AUDIT.md` | Source-grounded gap analysis |
| `supporting/ARCHITECTURE_AND_DATA_MODEL.md` | Non-normative implementation shape |
| `supporting/ACCEPTANCE_TRACEABILITY.md` | Acceptance-criterion-to-evidence map |
| `supporting/INDEX_INSERTION.md` | RFC-index insertion and frontier language |
| `MANIFEST.sha256` | SHA-256 for every pack file except itself |

## Import instructions

Use the repository's `.agents/skills/implement-rfc` workflow. The project
integrator must:

1. inventory this pack and compare its baseline to the live tree;
2. retain an immutable copy under `docs/rfcs/assets/<pack>/` and write the
   corresponding `LIVE_IMPORT` record;
3. copy the RFC and issue plan from `repository-overlay/` to their live paths;
4. update `docs/rfcs/README.md` using `supporting/INDEX_INSERTION.md` after
   reconciling status and dependencies;
5. allocate collision-free `BPR-NNNN` IDs from `docs/issues/NEXT_ID` for the
   provisional cards, rewriting filenames, front matter, and dependencies
   together;
6. encode sequencing in live card dependencies rather than copying
   `IMPLEMENTATION_ORDER.md` as a second mutable tracker;
7. run the repository structure and documentation gates before opening a lane.

The issue templates are not live board state. Do not copy them verbatim into a
status directory without first allocating live IDs.

## Non-negotiable boundaries

- **The core decides. The host performs. Qt presents.**
- Private review content never reaches GitHub without an explicit publish
  gesture after a local draft exists.
- Private content reaches an agent only through explicit selection for that
  invocation or composition.
- Requested model/effort and protocol-reported model/effort are separate facts.
  Unknown remains unknown; no inference from executable, provider, profile, or
  output style is permitted.
- Editing AI text creates a reviewer-authored successor revision while retaining
  the derivation edge; it does not rewrite history.
- Semantic source identities are authoritative. No Qt index, visual row,
  y-coordinate, ribbon path, nearest-line guess, or silent reanchor may be
  persisted.
- Local source material is retained after composition. Deletion requires an
  explicit private-state deletion operation and never changes GitHub.

## Intended result

After implementation, a reviewer can create or continue a private thread on a
PR or exact diff subject, see whether each contribution came from them or an
agent, inspect the best available model/effort provenance and its basis, select
one or more exact revisions, squash them into a local draft, edit it, and then
publish an ordinary GitHub comment under the authenticated reviewer identity.
