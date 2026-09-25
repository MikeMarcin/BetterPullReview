# Clean PR publication without destroying the local cycle

## New scope, explicitly bounded

This is a deliberate extension of BPR's prior product non-goals: prepare a selected
finished local result, create a **new** outgoing branch and open a PR. It is not a
general merge client, branch manager, automated rebasing tool or replacement Git host.
Existing read-only installed-Git ports and review-comment publication do not acquire
write power implicitly. Add separate typed local candidate and GitHub publication
ports with their own admitted operation catalog and authorization.

The first native output is one squashed commit on a new branch. Preserving arbitrary
local commit sequences, creating forks, force-pushing existing branches, merging,
closing PRs and automatic post-publication updates are outside v1. The user can use
normal external Git tools for those and explicitly link a verified resulting PR.
The complete local review system remains usable when native publication is disabled.

## Three different baselines

`R0` is the exact source first inspected, potentially dirty/uncommitted/partial.
`Rn` is the selected final local source revision.
`P` is a selected immutable Git parent/base used for an outgoing candidate, associated
with a separately selected target repository/branch observation `T`.

The local cycle diff `R0 -> Rn` does **not** necessarily equal the PR diff. Capture the
preexisting-work contribution `P -> R0` where material is known, the iteration
contribution `R0 -> Rn`, and the actual candidate `P -> C`. A partial R0 cannot prove
anything about uncaptured paths; use the complete parent tree for untouched paths.

Offer an explicit content selection with two understandable starting choices:
**include the relevant starting work plus improvements** or **include only the chosen
cycle improvements**. Neither chooses unrelated dirty files by default. List all
selected additions, modifications, deletions, mode changes and other dependencies.
A path/range checklist is not proof that excluded changes are independent. If applying
only improvements depends on preexisting edits, produce a conflict/candidate that the
user must review, not a falsely clean cherry-pick.

Build `C` as a projection over the complete verified tree of P, replacing only the
explicit selected entries/patches. Never derive a whole outgoing tree from a module-
only capture and thereby delete every unobserved file. In a plain folder, require
explicit mapping to an existing repository and a captured/verified publication base;
no synthetic root commit or mass import without a separate visible choice.

## Local candidate preparation

Use a BPR-owned isolated object store and private index, not the developer's index,
branch or working-tree files. Construct and inspect the exact candidate bytes/modes
and one-parent commit metadata. Installed Git can supply object plumbing with a
minimal controlled environment. Never invoke reset, stash, checkout, clean, amend,
rebase or hook-running porcelain against the user's checkout. Local preparation
makes no network write and does not launch a model, test runner, signing program or
repository script.

Raw captured bytes and canonical Git content are different representations. Pure
built-in transforms (for example an explicitly chosen LF normalization) are permitted
only as named, previewed candidate transformations with before/after byte evidence.
Never execute configured clean/smudge/textconv filters or hooks. Changed LFS-managed
content, required signing or other executable filters without a qualified supported
path returns a capability limitation and an external-preparation handoff. A raw large
asset must not accidentally be uploaded where an LFS pointer is required. Existing
untouched Git entries remain exactly P's entries. Do not silently disable repository
signing/protection requirements to make publication succeed.

Git object format is explicit. The local engine may support SHA-1/SHA-256 while a
selected provider supports only one; reject unsupported publication formats rather
than convert identities informally. Preserve executable bits and symlink/gitlink
semantics only where the backend contract is implemented and qualified. Binary blobs
can use a bounded byte-preserving provider path within its admitted limits.

When T/P moves, refresh explicitly and rebuild a new candidate or retain the old
explicit parent with a visible stale-base comparison. Conflict resolution and
integration edits happen through the user's ordinary editor/harness in a separate
chosen workspace, followed by capture/review. They do not silently become an accepted
publication candidate. Every byte-changing transformation invalidates the corresponding
review/authorization. Do not claim whole-project tests passed from local note verification.

## Native backend decision

For v1, use a **narrow GitHub Git-data REST backend** over BPR's existing admitted
native HTTPS/account transport: create/reuse bounded blobs, construct the exact tree,
create the single-parent commit, create a new branch ref, then create the PR. This avoids
teaching the existing credential-free Git pack/object adapter to execute authenticated
pushes or arbitrary credential helpers. It is a new provider-write capability, not a
shortcut through the old comment publisher.

The official GitHub APIs expose blob, tree, commit, reference and PR creation; the
source audit links the relevant primary documentation. A created Git tree can be based
on an existing complete base tree; deleting an entry must be explicit. The implementation
must use the selected host's admitted API version/capability contract, not silently
adopt whatever version appears in public documentation at build time. GHES and
unsupported object/permission cases produce honest typed limitations. Do not assume
GitHub.com availability proves GHES or all account types work.

Compute/verify expected blob/tree identities locally where supported; admit provider
results only when their content/tree/parent/metadata match the finite plan. Commit
attribution and timestamps are explicit and pinned. Verify the returned commit identity
against the locally constructed object where the canonical encoding is supported;
otherwise refuse that native backend until its exact encoding contract is qualified.
No invented agent authorship/coauthor trailers. Use the user's confirmed author and
actual committer identity under repository policy; model provenance stays private
unless explicitly included in the public commit text.

New writes require the needed existing account permissions for that host/repository,
including additional protected path/workflow requirements where applicable. Check
capability first; provide normal explicit reauthorization guidance if unavailable.
Do not silently expand grants, extract CLI tokens, change identity, create a fork,
route through an unapproved provider or call raw `gh` as a fallback.

## Exact preview and authorization

One publication preview shows source cycle/final revision, target host/account,
source and target repositories, new branch, parent/target observation, exact outgoing
diff/tree, author/committer, title/body, draft-PR choice, optional selected public
rationale and capability/coverage limitations. A blank or equal-tree candidate is a
clear no-change result, not an empty PR attempt. A changed source/selection/recipient/
branch/body invalidates the plan. A current explicit human confirmation authorizes
only that finite plan. Local freeze/export, an implementation assignment, all items
verified or an imported approval string does not authorize publication.

Ordinary local/manual cycle use never requires Github. Publication preparation can
construct a local candidate without credentials when all required base objects are
available; verification of remote target/capability and the actual send require normal
host authentication. A headless caller without a supported real approval bridge gets
preview plus the normal interactive publication command. No generic `--yes`, forged
plan file or echoed affirmation creates a grant.

## Non-atomic effects and recovery

Journal the exact plan and intended operation before each possible external effect.
Phases are `prepared -> authorized -> uploading_objects -> branch_created ->
pr_creation_pending -> linked`, with `failed`, `uncertain`, `target_drift` and
`cancelled` observations. Cancellation after bytes left the machine does not undo
server storage, a branch or a PR. Do not silently delete/rewrite remote artifacts to
simulate rollback. Show confirmed effects and provide a deliberate next action.

Blob/tree/commit operations are content-addressable but still may create stored
remote material. On timeout read/verify the expected object before retrying. A new
branch create must refuse an existing ref; if it now points at the planned commit,
recovery needs the matching journal/authority evidence or explicit user association.
Do not adopt a coincidentally matching foreign branch solely by name or content.
Before PR creation/retry, read the exact source repository/branch, target and relevant
PR matches. A uniquely evidenced already-created PR is associated, not recreated.
Ambiguous matches remain uncertain and need user choice. Never post duplicate PRs
because a create response was lost.

GitHub does not make the whole upload/ref/PR sequence one transaction. Another actor
may move a branch or the target between checks and creation. Re-read the created PR's
actual head/base and record any drift; do not claim the preview still describes the
published state when it does not. Use a new non-overwriting branch, finite plan and
read-first recovery to reduce risk, not a false claim of atomic compare-and-swap PR
creation. A remaining remote-write step after recovery needs fresh current authorization.

## Local continuity and external fallback

After successful publication, retain original source revisions, note versions, rounds,
deliveries, responses and verification. Add an exact PR association and enter the
existing BPR PR workflow. GitHub is authoritative for subsequent shared PR facts;
local historical discussions do not become fabricated GitHub events or comments.
Do not automatically upload round history, scratch notes or agent transcripts.

The PR description reuses the manual problem/solution authoring path. Optional Review
Tours use existing ordinary GitHub comments and a separate selected preparation/
publication path; creating the PR does not automatically publish all local feedback
as a tour. Historical/off-hunk notes use the existing exact placement/fallback policy
when deliberately promoted, never retargeted to convenient current lines.

If native publication is unavailable, Save the selected patch/candidate description
and use the user's normal approved Git tools, then `publication link` validates and
associates the resulting PR. Report this as **externally published**, not as evidence
the native backend passed. This fallback preserves user value but does not waive the
native publication criteria for platforms/hosts claimed as supported.
