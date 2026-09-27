# Private review history and publication

In an authenticated pull request, open **Review → Notes → Private history**.
The **History**, **Sources**, and **Write** tabs keep local conversation separate
from GitHub review threads. Create a PR or code thread in Write; code subjects
retain their exact revision and line. History shows each immutable revision,
its actor, and whether its source is current, stale, or unplaceable. An edit
adds a reviewer revision; it does not rewrite the earlier text. Older notes
migrated from the previous ledger show `legacy_unknown` rather than an invented
actor or current code location.

Use **Sources** to select exact turns or findings. **Ask agent using only
selected sources** is an explicit send: selection alone does not disclose
private text to an agent. The run records requested model and effort, configured
defaults, protocol reports, and content self-reports separately. A missing
report remains unknown; conflicting reports remain visible as conflicts. A
profile label or requested model is not proof of the runtime that answered.

**Squash to Draft** opens a local composition view. Choose reviewer-authored
prose, an exact copy of one source, or explicit agent-assisted composition.
Its source list retains exact identities and body digests. If sources have
different targets, choose an exact target or **Split into exact target drafts**;
unplaceable sources need a new explicit selection. A stale source can still be
used for local reasoning, but the app never silently moves its code anchor to
the current head. **Accept as Draft** saves a local review draft. It performs
no GitHub mutation.

Open the ordinary draft editor to inspect and edit the resulting text and use
its separate **Publish** action. The existing publication path checks actor,
source, and target against the current pull request. The public body contains
the approved prose; the authorized file representation also adds its visible
canonical permalink. Private thread IDs, derivation, and model metadata are
not silently appended to public text. Publication can be pending locally after
a remote success until exact server reconciliation completes; do not retry by
creating a fresh draft merely because the local receipt is missing.

Private history, drafts, and lineage live in the profile's private ledger.
Deleting them or purging that profile removes local state only; it does not
delete a GitHub comment. The store uses bounded pages and quotas and refuses
oversized writes rather than clipping text. See [storage and diagnostics](../development/storage-and-diagnostics.md)
for the precise limits and profile-purge boundary. Local fixture and native
workbench paths have been verified for RFC-BPR-0023; designated live GitHub
publication with independent server readback remains pending in BPR-0584.
