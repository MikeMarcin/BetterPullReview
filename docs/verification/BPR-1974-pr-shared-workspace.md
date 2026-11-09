# BPR-1974 shared PR workspaces

Mandate `pr-shared-workspace-r1`, implementer `/root/shared_pr_workspace`, branch
`main`. Owner instruction: one workspace across tour side chats, Agent tabs and
agents; conversations may remain separate because files and Git are shared.
Plan/import `fd94f951d`, claim `7a0ca3528`; product `aa0eacdbd` and truthful
refusal correction `d8b79055c` (full identity in external manifest). The user
subsequently required independent critical review before finish: acceptance
amendment `pr-shared-workspace-critical-r2` preserves all r1 work and adds AC5.

## Contract and implementation

RFC-BPR-0096 carries the explicit owner amendment superseding per-generation
mutable workspaces and per-turn replay trees. The shared key contains the same
stable profile/account/viewer/host/repository/PR/profile-revision/access-mode
identity as native scope, extended with PR number and host/cache/scope fingerprints,
plus base/head OIDs; agent, chat, stop, generation,
window and epoch ordinals do not partition it. Full conversation staleness still
uses the original binding. Changed base/head starts a distinct source binding;
old trees stay available for explicit old-revision continuation. Refresh never
checks out, restores or resets a shared tree.

One profile-owned native catalog serves all window Agent and tour hosts. Its
standard binary-semaphore turn lease spans preparation, process execution and
durable completion. A competing explicit turn refuses with an actionable busy
sentence and no question sent; this is exclusion with manual retry, not a queue.
There is no automatic retry. Other PR/source bindings remain independent. The
existing profile runtime OS lease excludes another BPR process for that profile;
this does not lock arbitrary third-party clients or promise to coordinate their
out-of-band edits.

An additive SQLite table references the original retained conversation's workspace
metadata. The first pre-send transaction atomically publishes that reference and
pending metadata. New sessions and replay reuse the tree without copying native
handles or other chats' transcripts. The original owner row survives selection
changes. Retention/recovery guard identities and journal phase7 remain; no automatic
cleanup or pruning is introduced. Pre-send storage failure never publishes a
transient in-memory Pending flag that a terminal might accidentally retry-save.

Legacy migration is lazy. If there is no shared reference, the first sent legacy
conversation adopts its own tree; a fresh scope instead creates one. A legacy
session with a different tree refuses before native resume and preserves both
files/history, offering New session to join the shared tree. Existing edits are
never merged, copied or deleted. A missing or unverifiable canonical tree refuses
rather than authorizing replacement, including through New session or replay.

Complete-workspace inspection validates original pinned head/base commits and root
trees plus the current HEAD commit/tree: at most six named objects, fixed1KiB
output and the existing deadline, with no history walk. Agent commits/branches
are therefore retained. Sparse workspaces keep their original missing-object
allowlist and validate both pinned and current HEAD closures. Exact ownership,
no-alternates and actual Git/filesystem/cancellation failures remain enforced.

## Verification

The exact final product source is `d8b79055c`, after implementation `aa0eacdbd`.
The tested source manifests and binary identities are external. The final host
and six-course public run rebuilt the candidate, then passed2/2 in24.88s
(host0.57s, native courses24.28s). Earlier relevant passing evidence is reused
where the source did not change; build/commit timestamps do not substitute for
source-content identity.

- Focused host/SQLite/real-Git:3/3 pass,31.47s total. Final source-isolation
  host:pass0.51s; final real-Git and six public courses:2/2 pass48.85s
  (Git25.13s, native23.70s), followed by the wording-only exact-candidate run above.
  Host rows cover11identity dimensions, distinct stops/agents in one workdir,
  competing-window host refusal, cancellation/release, legacy conflict, replay
  reuse, large-project admission, retained refusal and storage faults. SQLite
  covers atomic sharing, restart with unselected owner, New session, conflict
  rollback, changed-source refusal and independent turn keys. Real Git proves
  retained edits, commits and branch HEADs, wrong-account/fingerprint refusal,
  missing pinned objects, sparse allowlists and bounded six-object inspection.
- Public composition now requires exactly one retained tree across Agent-tab
  providers and tour whole/problem/solution/stops. Native first/reopen, tour
  first/reopen and replay first/reopen pass. Conversation/native process scopes
  stay separate while their files share that tree. Changed source/old-revision
  and tampered-marker/no-dispatch controls pass through the real composition.
- Dev architecture:pass20.12s; core-only architecture:pass24.03s. No core source
  changed. Qt canary, tour session and side-chat:3/3 pass25.75s.
- Clang-format22.1.8 checks all16touched C++ files. Scoped clang-tidy22 passes
  the initial6production units and2composition units. Final2unit isolation
  recheck also passes, including all five installed-Git compilation entries.
  Non-user dependency warnings are suppressed as reported.
- Documentation gate:DOCUMENTATION_OK; documentation-script tests67/67 pass.
  The amended acceptance/tracker state will receive its final gate before commit.
  `git diff --check`:pass. No dedicated Debug timing or optimized Release course
  is claimed; wall times above are observations only.

Frontend-design and its desktop reference were loaded for the existing replay
and refusal text surfaces. The isolated offscreen transcript-replay course passes,
and its `renders/transcript-replay.png` was inspected: the ordinary Agent panel
visibly states prior tool memory is absent and PR files/Git are shared, with the
reply and omission counts readable. No new controls, icons or layout were added.
This is offscreen functional/render evidence, not native system-appearance or
installed-agent qualification. The guidance correction removes New session as a
false remedy for a missing canonical shared tree, since it reuses that same tree.

## Required independent critical review — pending

The user requested `rfc-review-critical` before finish. The coordinator launched
`/root/shared_workspace_critical` against the frozen product above. Source paths
remain frozen while the independent reviewer attempts to falsify the claims.
No final acceptance or Done transition is authorized yet; AC5 and the added
milestone exit remain pending. The review includes source authority, lease and
lifecycle, durability, legacy migration, currentHEAD handling, replay isolation,
and internal core-session restoration identities. Any finding returns to the
same engineering owner for repair and renewed focused verification.

## Ownership and continuation

External state `/Users/jaeger/.codex/tmp/bpr-1974-shared-workspace/` holds resolved
selection, r1/r2 plans and bindings, checkpoint, source/binary manifests, commands,
offscreen evidence and handoff. The original adopted `source.json` is unchanged.
The r2 addition is an explicit user acceptance amendment, not a refinement that
removes old obligations. `check-amendment` refuses changed requirements by design
with "amendment must preserve every obligation; reconcile changed requirements
separately"; the recorded user instruction is the separate authority for adding
AC5 and the review exit. Explicit-plan validation/binding must still succeed.
Global checkpoint bind separately hits the existing unrelated release-v041
plan's unsupported `amendments` field. The explicit-plan binding plus official
repository-state helper supply the checkpoint without editing historical plans.

The corporate machine, Windows/Linux native runs, installed Claude/Codex and
release/trust acceptance are outside this bounded behavior fix. No owner-only
judgment is required by this card, so no attended server or protected human
candidate exists. Required independent acceptance remains explicit above.
Foreign `.codex-attachments/` is preserved and excluded from commits. No remote
mutation, push, credentials or live desktop access occurred. All task-owned build, test, render, format, documentation and tidy commands
have finished; no review server or engineering writer is handed off. Next:
receive critical review and reconcile findings before acceptance.
