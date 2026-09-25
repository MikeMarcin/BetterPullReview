# BetterPullReview — Review Cycles

**RFC pack v1 · 2026-09-25 · implementation proposal, not implemented software**

**Review the code. Share one complete round. Iterate locally. Publish when ready.**

Review Cycles is BPR's source-first, recipient-independent review workflow. Open a local
project, module or file without a PR or initial diff. Capture what you read, leave
anchored feedback and small suggested replacements, and share a whole review round
with a human or an agent. Capture subsequent revisions, compare iterations and
verify outcomes. Optionally publish the finished result as a clean, squashed GitHub
PR while retaining the private local review history.

A **cycle** is the long-lived review; a **revision** is captured source; a **round** is
one immutable feedback assignment. “Synthetic PR” explains the idea, but is not a
fake GitHub identity, a draft PR on a server, or the product's UI name.

## Start here

Read [IMPLEMENTATION-PROMPT.md](IMPLEMENTATION-PROMPT.md),
[the source audit](supporting/SOURCE-AUDIT.md), and
[the decisions](supporting/DECISIONS.md). Import the authored
[milestone-plan.json](milestone-plan.json) as the default execution seed;
[MILESTONES.md](MILESTONES.md) explains its grouping. [RFC-INDEX.md](RFC-INDEX.md)
links the nine additive RFCs. Their proposed numbers are **RFC-BPR-0053–0061**,
after the supplied snapshot's RFC-BPR-0052. Resolve collisions and canonical order
at intake; this archive reserves no live issue numbers.

## Non-negotiable outcomes

- A complete two-round workflow works with agent-specific components **not installed**.
- Clipboard and readable files carry the same selected feedback as direct delivery.
  An optional provider-neutral CLI and thin `bpr-cycle` skill support direct retrieval.
- The whole assignment reaches the recipient before implementation; exact manual
  suggestions and your wording survive. No required AI summarization or second prompt.
- Source revisions, feedback rounds, delivery observations and verification are distinct.
  No agent completion callback is needed to capture the next revision.
- PR publication is explicit, optional and recoverable. Squash outgoing code history,
  never the private history that explains it; never reset the user's checkout.

## Included

Nine RFCs, 52 implementation cards, an eleven-milestone plan, source/provenance audit,
architecture and lifecycle specifications, CLI and wire contracts, JSON schemas,
portable examples with exact suggestion bytes, a thin skill candidate, acceptance
and security matrices, attended/manual courses, and offline artifact validation.
These are proposals for the live C++/Qt product, not a parallel Python application.
The Python reference code is a small **test oracle**, never the proposed runtime.

## Validate this archive

Python 3.10 or newer; no third-party packages are required.

```sh
python3 -B scripts/verify_pack.py
python3 -B -m unittest discover -s tests -p 'test_*.py' -v
```

The standard-library checks validate archive integrity, links, IDs, dependencies,
plan coverage, requirement traceability and example/reference semantics. They do
not compile BPR, perform its native import, launch an agent, contact GitHub, install
a skill, or certify security. Product acceptance starts **Not run**; see
[verification/ACCEPTANCE-MATRIX.md](verification/ACCEPTANCE-MATRIX.md).

## Scope and rollout

Build the manual source/feedback/file round trip first; CLI retrieval second;
clean publication next; direct agent delivery last. UI work integrates with the
owner-selected RFC-BPR-0052 scaffolding. [The native workflow contract](specs/UX.md)
sets semantics and a compact layout direction, not invented user approval of art.
No new mascot, brand, IDE, hosted service, agent orchestrator or cloud sync is required.
