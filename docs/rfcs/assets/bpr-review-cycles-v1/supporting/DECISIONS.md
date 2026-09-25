# Decisions and requirement lineage

These decisions derive from the conversation requesting this pack. They authorize
this implementation proposal, not live account mutations, skill installation,
changes to unrelated projects, or certification of any implementation.

| ID | Decision | Reason / boundary |
|---|---|---|
| D01 | Name the workflow **Review Cycles**; singular **Review Cycle**. | Distinguishes local iterative feedback from PR review, private notes and author-prepared Review Tours. |
| D02 | A cycle can begin with no changes, commit, remote, account or PR. | Existing code, not only changed code, is the first review subject. |
| D03 | Source-first browsing remains available after diffs exist. | Do not reinterpret the whole repository as added or hide unchanged files. |
| D04 | One round is one complete, ordered assignment. | Reduce human steering and context reconstruction, not merely Send button presses. |
| D05 | Human judgments and exact manual replacements are primary inputs. | This is not an automatic AI code review product. |
| D06 | Shared review services own iteration; recipients and harnesses are optional adapters. | Removing AI cannot remove the workflow. |
| D07 | Copy, Save, external retrieval and integrated Send render the same frozen selection. | No rich agent-only context or lossy human fallback. |
| D08 | Use an AI-free `bpr cycle` CLI and optional thin `bpr-cycle` skill. | A skill teaches access; it grants neither filesystem reachability nor authority. |
| D09 | Revision capture does not depend on an agent response. | A human or externally prompted agent can deliver changes normally. |
| D10 | Original anchors are immutable; current correspondence is separate and qualified. | Repeated code, renames and edits must not silently change a criticism's subject. |
| D11 | “Addressed” is recipient-reported; “Verified” is reviewer action. | Suggested patches, changed bytes and successful tests do not imply acceptance. |
| D12 | Squash affects the outgoing code presentation, not retained local rounds. | Preserve decisions, rejected approaches, original source and provenance. |
| D13 | Inspection baseline, final local revision and publication base are separate. | Preexisting dirty work may belong in the PR; unrelated work must not be swept in. |
| D14 | Direct native publication is a narrow **new** GitHub capability. | Existing PR comment publication and read-only Git plumbing cannot secretly gain branch-write authority. |
| D15 | Local source captured from an ordinary changing folder is not falsely called an atomic filesystem snapshot. | Record observed stability; qualified snapshot backends may prove more. |
| D16 | One shared local owner; no database scraping, second writer or always-on HTTP service. | Follow the headless tour extension's direction without pretending its proposed code already exists. |
| D17 | Imported notes, snippets, reports and patch paths are untrusted data. | They cannot authorize execution, publication, verification or new disclosures. |
| D18 | Label intent remains optional and distinct from patch content and severity. | Reuse all twelve Conventional Comments labels, Plain and existing editors. |
| D19 | User syntax, exact source bytes and sensible native editing matter. | Preserve tabs, CRLF, deletion, virtual space and multi-caret behavior; do not add a generic IDE. |
| D20 | A complete local cycle is not recoverable from GitHub alone. | Only deliberately published PR facts are shared authority; portable backups are separate from default feedback exports. |
| D21 | No performance or score claim is assumed from batching. | Measure human steering turns, preparation time, missed feedback and rework; fewer tokens are not guaranteed. |

## Terminology

**Review Cycle** is the durable local review object. **Source revision** names captured
material; **feedback round** names a frozen assignment, which can include feedback
on several explicitly identified revisions. A mutable **round draft** is the next
assignment being composed. **Delivery** records an observed copy/save/read/send;
**response** records a recipient's statement. **Verification** is the reviewer's
explicit judgment on named item/source versions. **Publication candidate** is an
isolated, separately reviewed projection of the desired final code onto a selected
Git base. None of these identifiers is a provider PR number or an authorization token.

Do not call a local round “submitted GitHub review,” call a source checkpoint a Git
commit unless it is one, or label a copy operation “sent.” Do not call a successful
artifact self-test a product acceptance result.

## Deliberately not included

A hosted review server; real-time collaborative text editing; automatic cloud draft
sync; a new build/debug/refactoring IDE; automatic agent orchestration; always-on
watch-and-dispatch; automatic feedback generation; mandatory automatic summarization;
editing existing public PR branches by force; automatically executing Git hooks,
LFS filters, repository scripts or tests inside BPR; universal harness support without
reachability; covert export around corporate restrictions; or a new Review Tours format.
