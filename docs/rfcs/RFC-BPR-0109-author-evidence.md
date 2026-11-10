---
id: "RFC-BPR-0109"
title: "Build evidence while preparing a local PR draft"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0003","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: ["RFC-BPR-0108"]
---

# Build evidence while preparing a local PR draft

## Problem and grounded current behavior

Review Cycles distinguishes recipient claims from reviewer verification of
feedback. The AI finding system already distinguishes claimed evidence and
revision anchoring. Neither is a general record of an author's executed checks,
formal properties or preparation experiments. Reusing Verified from those
lifecycles would imply authority the new records do not have. See [S4 and
S9](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

The owner wants test results and formal verification where useful available
before creating a GitHub draft, with deliberate interaction. Formal verification
is selective evidence for a stated property; this pack does not promise an
automatically proved PR or remove an organization's independent review rules.

## Proposed behavior and boundaries

### Claims and checks

An author can record a claim about the proposed change and attach relevant
checks, observations, artifacts and remaining questions. A claim can concern a
bug fix, preserved behavior, compatibility, performance or a formal property.
The author confirms intended behavior; an agent's inferred motivation remains
a proposal. A proposed claim or planned check is a record the author accepts or
rejects, whatever produced it: the author, an import, or later an agent
conversation. Nothing requires a generic adversarial-review loop or a minimum
number of findings.

Distinguish at least planned checks, reported results and executions whose
process completion/output BPR actually observed. This RFC has no producer of
observed executions: it adds no runner, and a handoff to the author's full agent
or terminal runs where BPR does not watch it. Every result it records is planned
or reported. The observed class is reserved for a later BPR-supervised action
([extension RFC](RFC-BPR-0112-optional-workflow-extensions.md#effects-and-authorization));
the owner chose interactive handoff for ACTIONS-01, so version-one results
remain reported.
The record keeps the distinction so that a later producer cannot be confused
with an imported report. Origin identifies the author,
external tool or agent and the available producer/version. Reported success
never becomes an observed execution solely because an agent emits structured
JSON. Observing exit zero does not itself establish that a claim was tested.
Record pass, fail, inconclusive, not-run and interrupted outcomes truthfully,
including limitations. A failed or unresolved check is visible in preparation;
it is not silently removed to produce a clean summary.

Each result names its actual source/candidate basis, command or method, relevant
environment/inputs, assertions or expected observations, actual outcome and
artifact references. A commit SHA alone is insufficient for a dirty candidate.
An unbound imported report remains unbound until its material identity is
established; matching a filename is insufficient. The author can still retain
such a report as context without claiming it checked the current candidate.

An import is one versioned BPR-defined JSON document of claims, checks, results
and artifact references, chosen in the workbench or handed over by the `bpr`
command. BPR does not parse third-party report formats in this version; the
author's tool or companion skill translates them. An unrecognized version or
record kind is refused whole with its reason, never applied in part.

Summaries and explanations use the existing bounded Markdown/body handling;
external record imports are bounded to 16 MiB and refuse oversized/malformed
input before applying records. Large raw logs can remain referenced artifacts,
with missing/unavailable state, rather than becoming unbounded core state.
Selected local artifacts copied into the private store have content identities;
remote links are references, not downloaded or executed on import.

### Currency and meaning

Bind evidence to the actual material and conditions checked. Candidate-changing
edits, selected-file changes, transformations and relevant base/environment
changes require rechecking the affected claim. Reuse is allowed only with a
recorded unchanged input scope; absent sufficient dependency information, mark
code evidence as needing recheck after a candidate change. Do not assume that
an unchanged diff against a newly merged base preserves runtime evidence.
Description-only edits do not invalidate an unchanged code result. Preserve old
results with their original scope; do not overwrite history.

A regression-fix check can demonstrate failure on the previous code and success
on the candidate. A behavior-preserving change may instead compare outputs or
invariants across both. These are useful methods, not mandatory rituals for
every edit. Tests, fuzzing, property tests, inspection and model/proof results
retain their method names; they are not interchangeable assurance grades.

Formal evidence records the exact property, model or implementation analyzed,
tool/version, assumptions, bounds, result and artifact identity. A successful
bounded model check applies to its admitted bounds; a model result does not
establish correspondence with production code. Record any independently checked
correspondence separately. No aggregate Formally verified PR badge is created.

### Running work and deliberate interaction

The first implementation supports manual records and explicit import from an
author's existing tools/companion skill. This RFC starts no agent run: the
workbench's agent conversation arrives with the
[local-tour RFC](RFC-BPR-0110-local-tours-and-description.md#resumable-interaction),
whose candidate-bound chats can then propose claims and planned checks into this
record. Until then an author's own agent or terminal investigates outside BPR
and its results are imported as reported. Writing and read-only tour modes do
not gain test-running or mutation capabilities from a prompt reference. Bringing
code changes back requires capture and review as a new candidate.

Adding a general native test runner, build-system discovery or a formal prover
is explicitly outside this RFC. Configurable execution comes through the
optional action contract in the extensions RFC, not by treating a Markdown
command as executable. Imported declarations are not process isolation or
execution attestations.

The author can accept or reject proposed claims and record dispositions on
concerns, ask a focused follow-up, and carry open questions into the intended
reviewer-facing output. No check automatically approves, resolves, publishes,
or marks the future PR ready for review. An enterprise validator may require
additional evidence; it owns that rule. A separate verification skill is a
consumer/producer of this record, not a mandatory BPR dependency.

### Fitted to this pack's other decisions

This rule comes from the [reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md), row X16.

[RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md) lets an allowed Agent tab agent build and run tests in its disposable
checkout. Such a run is the agent's report, not an execution BPR observed.
If its result reaches a local draft's evidence it arrives by import, as
reported, bound to the material actually tested, which is that pull
request's checkout and not a candidate. The observed class stays reserved as
this RFC says.

## Acceptance criteria

- **AC01:** An author records and revises claims, imports a targeted check result, follows its artifacts and resumes the evidence history without an agent, external service or existing PR.
- **AC02:** Planned, reported, failed, interrupted and unbound results remain distinguishable; no path in this RFC labels a result observed, and an agent's claimed pass or zero exit cannot automatically verify a claim or approve a PR.
- **AC03:** Dirty candidates and selected content bind evidence by actual material; relevant candidate changes require recheck while unchanged text-only edits preserve applicable code results and historical evidence remains inspectable.
- **AC04:** Test, benchmark and formal/model evidence show their distinct scope; a model or bounded proof result cannot be displayed as proof of the whole implementation or PR.
- **AC05:** Imported oversized/malformed records, missing artifacts, cancellation and stale completions produce recoverable outcomes without partial accepted records, secret-store reads, command execution or false current evidence.
- **AC06:** The public preparation flow supports accepting or rejecting proposed claims and checks and retaining unresolved questions, with no agent run started by this RFC; evidence is selectable for later description and tour use without exposing unselected private history.
- **AC07:** A test result produced by an Agent tab agent under its build allowance can enter the evidence record only as an imported, reported result bound to the material it tested; no path labels it observed.

## Proposed delivery

[Evidence records](../issues/10-backlog/BPR-2023-retain-candidate-bound-evidence-and-qualified-provenance.md), [interactive evidence flow](../issues/10-backlog/BPR-2024-curate-evidence-in-local-preparation.md)
and [closeout](../issues/10-backlog/BPR-2025-close-rfc-0109.md) form the third [milestone](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
