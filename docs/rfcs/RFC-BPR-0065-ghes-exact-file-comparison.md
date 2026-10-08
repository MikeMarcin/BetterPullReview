---
id: "RFC-BPR-0065"
title: "Restore Exact File Comparisons in the Real GHE Environment"
status: "Proposed"
area: "BPR"
milestone: "Owner four-draft mandate"
depends_on: ["RFC-BPR-0015"]
pack_version: "four-drafts-v1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# RFC-BPR-0065: Restore Exact File Comparisons in the Real GHE Environment

## Adoption and current obligations

Registered on main under mandate four-drafts-r1 at baseline `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. The [adopted plan](plans/bpr-four-drafts-v1/milestone-plan.json) preserves every acceptance criterion; the [import record](assets/bpr-four-drafts-v1.LIVE_IMPORT.md) retains provenance and exclusions. The owner reports macOS Tahoe, gh 2.101.0 (2026-09-15), and Git 2.54.0 (Apple Git-157). Exact configured executable paths and direct qualification remain unobserved. The coordinator's local shell instead selects /usr/bin/git 2.50.1 (Apple Git-155) and /opt/homebrew/bin/gh 2.101.0 on macOS 26.6.2 build 25G83; that separate environment cannot qualify the affected installation.

The source Handoff below is retained as authoring history, not current execution state. Current acceptance is tracked by the new issue cards and [ledger](../verification/RFC-BPR-0062-0065.md). Existing Done cards and plan exits remain historical evidence for their exact candidates. Formal dependencies use accepted capabilities: RFC-BPR-0015 for the opted-in CLI profile.

## Problem and grounded current behavior

On 2026-10-07 the owner reported that their real GitHub Enterprise environment
loads the rest of a PR but cannot show file contents, displaying “No exact
comparison is available for this file.” Restore usable source review on that
environment, without weakening the exact base/head comparison contract.
The owner subsequently reported “GitHub returned data that BetterPullReview
could not safely use” and requested a second export be included in this same
RFC. Both the PR snapshot rejection and exact-file acquisition failure are
in scope; neither a successful metadata load nor a repaired version gate alone
is the required outcome.

### Attached diagnostic evidence

The two original diagnostic exports and full retrieval provenance remain only in the ignored local incident bundle. This source copy preserves sanitized structural observations and digests; no diagnostic bytes or private target are imported.

- Original size: 104,306 bytes, 129 JSON lines including the export header.
- SHA-256: `7970a126035da3f0269f932bc81f7937fa4310f9f2b9d177c7df49ee0d386372`.
- Export: anonymized, current session, recent context included, 128 records,
  zero omitted records, one session, Standard recording detail.
- Recorded interval: 2026-10-07 20:59:33.219–22:16:20.610 UTC. All records
  name the same 0.1.4 Release build. Its identifier does not establish a
  matching source revision or the actual Git executable/version.

The following observations use original one-based line numbers. Request/window
suffixes abbreviate opaque export labels, not real PR or account identifiers.

| Original lines | Observation |
|---|---|
| 4, 15 | Viewer identity and capability discovery succeed. |
| 29, 51–52 | PR request suffix 2 succeeds, then its epoch acquisition fails at `protocol_acquire / transport / unavailable / git_qualification`; `revision_prepare / epoch / unavailable` follows, in window suffix 3. |
| 64, 86–87 | PR request suffix 4 repeats the same success followed by the same qualification and revision-preparation failures in that window. |
| 98, 109, 120 | Later PR requests fail separately at `validation / invalid_relationship`, with `SearchPullRequests / relationship_actor_login`. |

The export contains no observed Git-version banner and no successful clone or
revision preparation for the two successfully loaded PR requests. It does not
identify the GHES version, target PR, selected path, client OS or selected Git
installation. Those facts must not be guessed from the owner's description.

The second original export was retained byte for byte outside source commits. Its size is 199,631 bytes; SHA-256 is
`1030374563f1542ee1024b6089bffa79bba885c164c8a8328e84e825423c80d8`.
It contains 306 JSON lines: a header and 305 records, including 69 legacy
metadata records and 236 detailed events (200 file records, 36 recent-context
records). The anonymized retained-session export covers two sessions from
20:59:33.219 through 22:34:12.679 UTC on 2026-10-07, with no export omissions
or duplicate records reported. Capture-time omissions cannot be recovered.
The detailed events name the same Release build as the first export; legacy
metadata does not carry that build identity.

| Second-export original lines | Observation |
|---|---|
| 98, 120–121; 133, 155–156 | The earlier PR successes and `git_qualification`/revision-preparation failures recur in the retained evidence; these are overlapping observations from the first session, not independent new successes. |
| 167, 178, 189, 200, 211 | Five PR loads in the first session fail at `validation / invalid_relationship`, with `SearchPullRequests / relationship_actor_login`; the first three overlap the earlier export. |
| 212–213 | Application shutdown followed by a new session startup. |
| 215, 226 | Viewer and capability discovery succeed in the new session. |
| 237, 248, 259, 270 | Four PR loads in the new session fail with the same actor-login relationship classification. |

The second export establishes persistence of this rejection after restart.
It supplies no later successful PR load or exact comparison acquisition, so
it does not establish that qualification has been repaired. It does not expose
the conflicting actor values, their originating collection, or the precise
provider payload. `SearchPullRequests` is the recorded page-operation context;
it does not prove that the conflicting actor originated in that response.

### What the source explains

In [the CLI Git transport](../../src/adapters/github/src/gh_cli_git_smart_transport.cpp),
`acquire_pack` probes `gh --version` and the selected Git's `--version` before
cloning. It admits `gh` 2.100.0 or 2.101.0 and requires Git output to equal
`git version 2.50.1 (Apple Git-155)\n` exactly. A returned Git result that
does not match that string becomes `git_qualification / unavailable`.
The branch does not separately classify a nonzero Git exit status versus an
unsupported banner. A missing process result uses a different step.

[Epoch diagnostics](../../src/application/src/epoch_application_host.cpp)
preserve that transport step during protocol acquisition, then report failed
revision preparation. [Selected-file loading](../../src/application/src/workbench_application_host.cpp)
reports comparison unavailability when there is no admitted exact comparison
context. The current Qt wording is “No exact comparison is ready for this
file. Check Review history, then retry.” That is similar to, but not byte-for-byte
the same as, the owner's reported message.

**Inference:** the two logged failures select the CLI object-acquisition
qualification boundary, consistent with the current exact-banner gate. This
is stronger evidence than assuming the enterprise server refused source reads.
It still does not establish which Git was selected, what it printed, whether
its version command exited successfully, or whether the inspected code exactly
matches the running build. Collect those bounded facts before choosing a fix.

[Snapshot validation](../../src/application/include/bpr/application/github_read_model.hpp)
maps each actor ID to one login string, starting with the viewer and checking
authors across the accepted snapshot. A different login string for the same
ID produces `relationship_actor_login`.
[The read host](../../src/application/src/github_window_read_host.cpp)
maps a rejected snapshot to `malformed_response`, and
[Qt presentation](../../src/adapters/qt/src/github_presentation_text.hpp)
renders the owner's new message for that reason. This explains the observed
classification in the inspected source. It does not establish whether the
environment supplies a legitimate identity variation, the adapter merges
incompatible observations, or the provider data is actually contradictory.

[BPR-0922](../issues/60-done/BPR-0922-source-files-exact-comparison-unavailable.md)
qualified a recorded GHES/CLI fixture and explicitly left the owner's live
cause unobserved. [BPR-1199](../issues/60-done/BPR-1199-a-private-pull-request-opens-but-none-of-its-files-shows-a-comparison.md)
fixed a different observed `response_limit / clone` failure. Neither supplies
acceptance for this `git_qualification` failure on the real environment.

## Proposed behavior and boundaries

### Establish and repair the failing qualification

Bind the incident to the actual release/build, client platform, selected
transport and configured Git/gh executables. Capture bounded tool-version
observations and process exit facts, without recording credentials, private
repository identities, arbitrary stderr or repository content. Reproduce the
observed classification through the real composed CLI acquisition path using
synthetic GHES data before changing the gate.

If the version probe is failing or selecting the wrong executable, repair
that invocation/selection and preserve fail-closed admission. If it succeeds
but selects a Git installation outside the current qualification, resolve the
qualification under the owner's support decision below. Do not label the incident fixed merely because
the error text is clearer or a fixture hard-coded to the approved banner passes.

For a supported installation, load verified objects for the exact accepted
base/head, admit the retained comparison and render file contents/diffs through
ordinary Overview file selection and Review navigation. Reopening or retrying
after repairing qualification must recover without deleting the owner's
profile, cache, private notes or drafts. Same-head refresh must not reproduce
a permanent refusal from an earlier failed preparation.

### Explain genuine unsupported cases

A remaining qualification refusal must distinguish a failed tool probe from
an unqualified Git/gh version and explain the concrete supported setup or
recovery. Preserve successful PR metadata and the file list, clear unavailable
source and never imply that metadata success proves source availability.
The existing typed host/core decision path supplies the reason to Qt; the
widget does not inspect a process or infer support from an error string.
This narrow recovery wording follows the existing native presentation language;
`frontend-design:frontend-design` was consulted for the UX requirement.

### Repair the actor-login snapshot rejection

Trace the conflicting actor observation through provider decoding, snapshot
aggregation and relationship validation. Capture only bounded, redacted
facts identifying collection/field provenance and the kind of disagreement;
keep any necessary private payload reproduction local and outside commits.
Build a synthetic fixture that reproduces the observed rejection through the
public composed read path, including the restart/retry course.

Determine the actual cause before choosing normalization or aggregation rules.
If the evidence establishes a legitimate GHES identity representation, define
its canonical treatment in the narrow owning adapter or policy boundary,
while preserving stable actor IDs and authority separation. If composition
mixed unrelated observations, repair their provenance and admission. Do not
unconditionally ignore conflicting logins, invent aliases, bind authority by
display name, or suppress relationship validation. Actually inconsistent
snapshots must continue to fail safely with an accurate typed diagnostic.

A corrected, supported snapshot must load the intended PR, then acquire and
render its exact base/head objects. Retrying after either repair must clear
the previous refusal without profile reset; account/host/PR changes and stale
completions must not publish unrelated metadata or source. Reuse existing
validation contracts unless demonstrated legitimate input requires a precise
proposed amendment during implementation.

### Preserve authority and scope

Keep [the CLI contract](RFC-BPR-0015-github-cli-production-enablement.md)
and [system architecture](../architecture/system-architecture.md):
explicit CLI opt-in, configured-host/account checks, process write denial,
bounded temporary stores and acquisition, exact object verification, actor
serialization, cancellation and stale-result rejection. No credential extraction,
TLS bypass, implicit native/anonymous transport fallback, approximate diff,
unbounded clone or API patch text substituted for exact source is proposed.

The two failure classes have separate reproducers and repair evidence. The
second log and owner's follow-up bring actor-login rejection into this RFC;
that rejection is not the established cause of the earlier successful PR
loads losing comparisons. Neither repair may bypass the other boundary.

No live mutation is needed. Synthetic fixtures establish the failing class;
an owner-attended read-only run on the affected enterprise PR establishes the
real repaired outcome. Keep private data and the attachment outside commits.

### Owner support decision — 2026-10-07

The owner selected: “Qualify the environment’s Git and restore file viewing
(recommended).” Qualify the actual selected installation using direct
installed-tool acquisition, confinement and exact-object evidence. A
version-range check alone is not qualification. This selects a proposed
extension to the adopted exact Apple Git constraint; it does not qualify
arbitrary versions or waive native platform evidence or safety requirements.
Actual Git/version facts remain missing implementation evidence. No material
support choice remains unanswered, and no runtime acceptance is implied.

## Acceptance criteria

- **AC01:** The selected tool/version-probe facts and matching acquisition
  failure are established with a composed reproducer, separating observed
  failure from source/build inference and the later actor-login failures.
- **AC02:** Under the chosen support policy, a stable GHES fixture that
  reproduces the incident renders exact base/head contents for multiple files
  through the public application; unsupported/probe-failure cases retain a
  precise actionable refusal without acquiring source under invalid authority.
- **AC03:** Retry/reopen over previously failed persisted state recovers after
  qualification is repaired, while head/account/host/selection replacement and
  cancellation cannot publish stale or mismatched file contents.
- **AC04:** The acquisition and focused public-path checks retain the defined
  confinement, bounds, credential isolation and exact-object guarantees;
  a support change has its own direct installed-tool qualification evidence.
- **AC05:** On an exact frozen repaired candidate, the owner can open the
  originally affected enterprise PR read-only without the supported-snapshot
  rejection and view its intended file
  contents/diffs. Ask that single rendered-outcome question in a fresh short
  attended session; record build/tool/transport qualification and keep the
  private target and log outside source commits.
- **AC06:** A synthetic GHES snapshot reproduces the actor-login rejection
  with attributable observation provenance. The evidence-backed repair admits
  legitimate input through the composed public PR read path; truly conflicting
  actor/authority/target observations still reject. Retry/restart and stale
  completion controls pass without weakening the other relationship checks.

## Handoff

This is an unadopted draft with its original incident attachment, separate
from the live RFC and issue board. No root-cause repair or runtime pass is
claimed. The accompanying [proposed qualification amendment](assets/bpr-four-drafts-v1/ghes-exact-file-comparison/proposed-qualification-amendment.md)
identifies the adopted constraint that the owner's selected direction changes. Registration,
issue allocation, live dependency assignment and acceptance belong to a later
authorized implementation session. Resume by collecting the missing bounded
tool and actor-provenance facts and reproducing both failures.
