---
id: "RFC-BPR-0022"
title: "Semantic Source-Line Comments and Off-Hunk Publication Compatibility"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0008"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# RFC-BPR-0022: Semantic Source-Line Comments and Off-Hunk Publication Compatibility

## Status and scope

This is a new post-pack RFC. It does not alter the canonical fourteen-RFC
pack-v2 sequence and it does not edit, replace, or reopen RFC-BPR-0008.

RFC-BPR-0008 remains the owner of ordinary GitHub review synchronization,
publication, acting identity, stale-head protection, mutation isolation,
idempotency, and server-truth reconciliation. This RFC is an additive amendment
implemented through those existing seams: the semantic target of a review
comment may be any eligible current-head source line in a file already changed
by the pull request, even when GitHub's connected API cannot create a native
thread on that off-hunk line.

The 2026-09-26 owner mandate activates implementation and main integration
through implement-rfc, superseding the 2026-09-04 import-only deferral. The
[adopted milestone plan](plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json)
preserves the source pack's four authored milestones. It waives no criterion,
live authority or canonical closeout requirement.
RFC-BPR-0022 cannot become `Auto-Verified` before every listed dependency is
`Auto-Verified`. Its direct live-mutation criteria also require
a designated safe GitHub.com target and, for the legacy enterprise claim, a
designated GHES target or an exact product-owner-approved recorded contract.

## Owner amendments for milestone D

Mandate r2 permits GitHub.com live validation in place of native GHES; automated
GHES contracts remain and native GHES is unobserved. Mandate r3 accepts the
[three exact owner decisions](../issues/_checkpoints/BPR-0575/m4-r3-amendments.md).
AC-03 and the native-only live course use supported-path fixtures while native
GitHub.com success remains unobserved. The narrow recorded rejection contract
may enable prospective classifier2 only for its exact PR2/base/head/actor/profile
and revalidated source/patch evidence. All four earlier witnesses stay GET-only.
Actual fallback publication and GET-only receipt recovery are observed in the
[verification ledger](../verification/RFC-BPR-0022.md). GitHub-only collaborator
continuity is accepted for the exact review candidate; the signed app's subsequent
read-only refresh displayed the external reply at the original semantic line.
Mandate r4 accepts the [two final owner amendments](../issues/_checkpoints/BPR-0575/m4-r4-amendments.md)
for production source `0f3188093c846cbbb43f624dd48c817e0ea4eec4`: AC-10 uses
observed app-closed GitHub continuity plus fresh-host reconstruction; actual
installed desktop uninstall remains unobserved. The owner explicitly waived
separate independent review for that candidate and accepted the disclosed
evidence; no independent report is claimed. The uninstall invariant remains.
RFC-0022 is Auto-Verified on 2026-09-27 under these amendments and its satisfied
explicit dependencies. All fifteen rows and cards are reconciled in the ledger.
The canonical criterion text below remains intact. The r3 exception for closure
ahead of Deferred RFC-0020/0021 remains recorded; index position is not a gate.
The coordinator retains Milestone D integration to main.

## Owner amendment r5: measured placement evidence (2026-10-22)

The owner's decision on [[BPR-1735]], verbatim: "Accept option P (hold
off-hunk comments before Submit and publish them as file comments; single
Publish goes straight to a file comment), plus marking a refused single
comment rejected. Build it."

The amendment changes these rules; the canonical criterion text below stays
intact and is read with them:

- **Placement evidence.** The core's measured placement is admitted as
  evidence in place of an exact unsupported response contract for a line
  outside every hunk. On the Whole PR diff, and on a commit-filtered diff's
  sides that are GitHub's own, a line is placeable as a native line comment
  only within three lines (`core::github_comment_context_lines`) of a change
  of the merge base to the head on its side; hunks whose context touches are
  one, and a range must lie wholly in one (`core::place_on_github_diff`). The
  rule was measured by RFC-BPR-0089 on BPR-Test #116 to #119 and again on
  #124, where GitHub.com refused head line 60 of an 80-line file whose line 10
  changed, in a review and as a single comment. The epoch host computes the
  merge base to the head with the review's own diff engine and exact
  whitespace (`resolve_github_diff_lines`). This replaces "That geometry is
  not GitHub's API authority and may not be used by itself to declare a line
  commentable or un-commentable" for this one rule; nothing is inferred when
  GitHub names no merge base or the file cannot be read, and support is never
  inferred from the host's name or version.
- **Single Publish.** Such a line is `review_placement::unresolvable_line` and
  is admitted straight to the file comment (`admit_unresolvable_line`, the
  third first-attempt authorization of invariant 2, now on the Whole PR diff
  too), with no native attempt. Its permalink names the line at the head, or
  at the merge base for a base line. Start a review refuses it.
- **Review.** A comment pending in a review that the core places outside
  GitHub's diff is held before Submit, as RFC-BPR-0090 holds a suggestion,
  with Publish as file comment on the comment and on Finish your review. It
  publishes the comment alone, through this RFC's journal, and takes it out
  of the review before the review's POST; Submit stays blocked while one is
  held, and the review is still one atomic POST ([[BPR-1345]]).
- **Rejected.** A fully received 4xx answer to a native or file child, with
  GitHub's complete comment list holding no comment of it, retires the child
  as `rejected`. GitHub holds no comment of the publication, so it is
  reported not published with GitHub's reason, and publishing again starts a
  new attempt under the same logical key, natively or, once placed off the
  diff, as the file comment. The classifier's exact unsupported rejection is
  still native rejection, not this. Timeout, a lost or malformed answer, a
  redirect, a server error and GET absence never reject.
- **Kept answers and migration.** Each child's POST answer is kept beside the
  journal (recovery schema 8, `publication_post`): "no answer" before the POST
  may leave, then its class. A version 7 journal keeps every logical
  publication as answered before answers were, at the migration time. A child
  left possibly sent by an earlier version settles as rejected once ten
  minutes have passed since the migration and GitHub's complete list holds no
  comment of it, as [[BPR-1715]] settles a review; a new child with no
  definite answer stays GET-only.

Unverified on GitHub Enterprise Server 3.20, which the owner's report concerns
and no designated target serves: that it accepts `subject_type: file` on
`POST /pulls/{n}/comments`, and that it resolves line comments by the same
three-line rule. If it refuses a file comment, that refusal is a fully
received 4xx: the file child is rejected and the draft kept.

## Product decision

The product contract is:

> A reviewer comments on a semantic source line. BetterPullReview chooses the
> best ordinary GitHub representation supported by the connected host.

The normal interaction has one line affordance, one draft editor, one Publish
action, one pending state, and one resulting conversation. The reviewer does
not choose between a native line comment and a compatibility comment.

The provider representation remains inspectable in diagnostics and support
evidence. It is not a normal workflow decision.

## Problem

GitHub's traditional review-comment API and older GitHub Enterprise user
interfaces place line comments only on lines represented in a pull-request
diff hunk. Reviewers frequently need to discuss an unchanged line because the
pull request should have changed it, because a changed caller invalidates an
unchanged callee, or because the relevant invariant spans beyond the displayed
hunk.

GitHub.com's newer Files changed interface can place comments on unchanged
lines of a file already changed by the pull request, but the feature was rolled
out per repository and GitHub described public API support as limited. GitHub
Enterprise Server 3.21 later removed its release-note claim for the feature as
premature. The public REST contract still describes a line comment as applying
to a line in the pull-request diff, while also supporting an ordinary
file-level review-comment subject.

BetterPullReview must therefore neither hide the source-line affordance on old
hosts nor assume that a web feature implies that the public mutation operation
used by this application supports it.

## Terminology

- **Semantic source anchor** — the exact BetterPullReview identity selected by
  the reviewer: comparison, current/right side, zero-based source line,
  repository path, and exact head commit.
- **Remote subject** — what GitHub records as the review comment's target:
  either a line subject or a file subject.
- **Legacy diff line** — a source line that the host's ordinary line-comment
  operation is known to accept without the comment-anywhere feature.
- **Off-hunk line** — a line in a changed file that is outside that known
  legacy line-comment domain.
- **Placement evidence** — bounded evidence that an anchor is a legacy diff
  line, an off-hunk line, or unknown. BetterPullReview's painted hunk geometry
  is not provider placement evidence.
- **Publication representation** — `native_line` or
  `file_comment_with_permalink`.
- **Definitive unsupported rejection** — a fully received response whose exact
  operation-, server-contract-, and classifier-revision-specific structured
  fields prove that this native off-hunk placement is unsupported and that no
  comment was created.
- **Indeterminate transmission** — a state in which the native write may have
  reached GitHub and its creation outcome is not authoritatively known.
- **Fallback rehydration** — deriving the semantic source anchor from the
  canonical visible permalink in a file-level GitHub review comment.

## Goals

- Show the same comment affordance on every materialized current-head source
  line in every text file already changed by the pull request.
- Preserve native GitHub line threads whenever the exact mutation operation
  supports the selected anchor.
- Transparently use an ordinary file-level review comment containing a stable
  code permalink when native off-hunk placement is definitively unsupported.
- Preserve exactly one logical publication across retries, crashes, response
  loss, and fallback negotiation.
- Keep the semantic source anchor distinct from GitHub's remote subject so the
  workbench can place a compatibility thread at the correct line without
  falsifying the server record.
- Reconstruct compatibility anchors after local database loss from visible,
  ordinary GitHub content alone.
- Preserve the uninstall, GitHub-only collaborator, mixed-client, acting-
  identity, and server-truth invariants of the product contract.

## Non-goals

- Commenting on a file that is wholly absent from the pull request's changed-
  file set.
- Base/left-side off-hunk comments, deleted-only lines, or arbitrary historical
  epoch publication. This RFC's first vertical slice remains the existing exact
  current/right/head endpoint.
- Multi-line source ranges. The existing review draft carries one
  `source_line_ref`; range publication requires a separate semantic design.
- Reproducing GitHub's one-click **Apply suggestion** behavior through a
  file-level fallback. A line-bound construct that cannot retain its semantics
  must fail explicitly rather than masquerade as equivalent.
- Scraping GitHub's web UI, using browser automation as a production transport,
  or calling undocumented internal endpoints.
- Adding a bot, label, status check, hidden HTML comment, BetterPullReview
  marker, opaque token, or private server to preserve the anchor.
- Inferring GitHub capability from hostname, marketing name, a broad GHES
  version threshold, or the ability to read an existing off-hunk comment.
- Absorbing RFC-BPR-0021's general comment-to-historical-revision join. This RFC
  owns its own publication representation and canonical fallback rehydration;
  RFC-BPR-0021 continues to own general historical conversation revision
  identity.

## Facts established from the current tree

The following are existing seams to extend, not invitations to build parallel
subsystems:

1. `core::review_comment_draft` and `core::review_publication_proposal` already
   carry RFC-BPR-0004's `source_line_ref`, not a visual row.
2. `review_mutation_anchor_binding` already binds that semantic anchor to an
   exact repository path and commit.
3. `review_mutation_coordinator` already fixes the designated target, observes
   it twice, derives an idempotency key, and delegates one serialized
   publication to `review_mutation_port`.
4. The current GitHub mutation compiler always emits `body`, `commit_id`,
   `line`, `path`, and `side`, and the response parser requires those exact
   fields. A file-subject variant does not exist yet.
5. The current recovery schema v2 represents one exact wire attempt. It stores
   a baseline, marks `possibly_transmitted`, and then permits GET-only
   reconciliation. A second representation cannot be bolted onto that record
   as an unrelated retry without risking duplicate comments.
6. The general GitHub capability cache is account/server-contract scoped and
   describes read/schema capabilities. It is not repository-scoped write
   evidence and must not be broadened casually.
7. `github_thread_map` can project conversations onto source lines, while
   GitHub IDs remain canonical. This RFC must extend that mapping rather than
   insert inline full-width comment widgets into the diff panes.

The detailed path inventory is retained in
`references/CURRENT-SOURCE-ALLOCATION.md` in the source pack.

## External authority and uncertainty

As of 2026-09-04, official GitHub sources establish four facts:

1. GitHub.com's new Files changed interface can comment on any line of a file
   already changed by the pull request, but the rollout was per repository and
   public API support was described as limited.
2. The public REST create-review-comment contract still defines `line` as a
   line in the pull-request diff. It also supports `subject_type: file`, for
   which `line` is not required.
3. A `422` response is documented as either validation failure or spam
   detection. Status code alone is not capability evidence.
4. GitHub Enterprise Server 3.21 removed its comment-anywhere release-note claim
   as premature on 2026-06-23.

Therefore this RFC deliberately does not assert that GitHub.com's web
capability is available through the exact REST or GraphQL write used by
BetterPullReview. The first authorized real off-hunk publication is the
operation-specific negotiation unless prior exact recorded evidence exists.

## Eligibility

A source line is eligible for this RFC only when all of the following are true:

- the accepted GitHub snapshot says the file is in the pull request's changed-
  file set;
- the exact current/right endpoint is the accepted pull-request head;
- the path resolves at that exact head to a text blob accepted by the selected
  diff authority;
- the semantic line is within that blob;
- the draft, target, acting identity, transport lease, and head pass the
  existing RFC-BPR-0008 admission checks;
- no line-bound body construct would silently lose required semantics under a
  file-level representation.

Eligibility is semantic and authority-bound. Visibility on screen, a Qt row
number, a y-coordinate, an expanded fold, or a ribbon path grants nothing.

## Architecture allocation

| Region | Owns | Must not own |
|---|---|---|
| Functional core | Eligible semantic anchor shape; publication-representation vocabulary; pure native/fallback planning from bounded placement, capability, body-compatibility, and prior-attempt observations; legal terminal transitions and diagnostics | URL construction, HTTP status parsing, GitHub JSON, clocks, persistence, Qt state, or provider version guesses |
| Application host | Exact changed-file/current-head binding; bounded placement/capability observations; logical-publication orchestration; serialized native/fallback state machine; cancellation, restart recovery, cache lifetime, and projection publication | GitHub response syntax, permalink escaping, duplicated core policy, or widget commands |
| GitHub adapter | Native-line and file-subject wire requests; exact response/error decoding; operation-specific unsupported classifier; permalink compiler/parser; provider reconciliation records | Semantic policy, UI prompts, live-target selection, or hidden metadata |
| SQLite adapter | Atomic parent/child publication recovery, schema migration, baselines, transmission witnesses, definitive-rejection and receipt transitions | Network retries, classifier policy, or inferred GitHub truth |
| Qt adapter | One line affordance, one draft editor, pending/result presentation, accessibility, focus, and diagnostic details | Capability selection, URL construction, provider calls, fallback choice, or canonical shared state |
| Composition root | Concrete cache/store/provider/coordinator wiring and designated-live authority | New mutation bypasses or tests-enabled production authority |

The governing rule remains: **The core decides. The host performs. Qt
presents.**

## Semantic and remote identity

The model must retain both truths:

```cpp
struct semantic_source_anchor
{
    git_commit_oid Commit;
    repository_path Path;
    diff_side Side;
    std::uint32_t Line;
};

enum class review_publication_representation : std::uint8_t
{
    native_line,
    file_comment_with_permalink,
};

enum class github_remote_comment_subject : std::uint8_t
{
    line,
    file,
};
```

Names are illustrative; live code follows the repository naming standard and
uses existing strong identities where available.

A normalized conversation created through the fallback therefore carries:

- the canonical GitHub comment/thread IDs;
- `RemoteSubject == file`;
- the exact semantic source anchor recovered from the publication record or
  canonical visible permalink;
- `Representation == file_comment_with_permalink`;
- the authoritative raw GitHub body and the reviewer prose projection.

BetterPullReview may render that thread at the semantic line. It must never
rewrite the remote subject to `line` or claim GitHub stored a line thread.

## Placement evidence

Placement has three states: `legacy_line`, `off_hunk`, and `unknown`.

The project computes and paints its own full-file diff. That geometry is not
GitHub's API authority and may not be used by itself to declare a line
commentable or un-commentable. An exact provider patch/range observation may be
added as a bounded optimization if its completeness and head identity are
validated; absence, truncation, malformed data, or a different head produces
`unknown`, never a guess.

A successful native line creation proves that exact anchor was accepted. A
recognized definitive off-hunk rejection proves that exact anchor was not. A
repository-wide capability fact may generalize support only when the evidence
contract explicitly permits it.

[Owner amendment r5](#owner-amendment-r5-measured-placement-evidence-2026-10-22)
admits one geometry rule as evidence: a line more than three lines from every
change of the merge base to the head on its side is not placeable, and goes to
the file comment with no native attempt.

## Write capability

Introduce one narrow tri-state fact, conceptually:

```cpp
enum class off_hunk_line_creation : std::uint8_t
{
    unknown,
    supported,
    unsupported,
};
```

The authority key includes at least:

- profile identity and profile revision;
- canonical hostname and server contract;
- authenticated account identity;
- repository database and node identity;
- exact mutation operation and API contract revision;
- unsupported-response classifier revision;
- evidence freshness generation.

Repository identity is mandatory because GitHub announced a per-repository
rollout. Existing readable off-hunk comments, schema fields, a GitHub.com host
name, or another repository's success are not write proof.

Evidence sources are:

- `recorded_contract` — an exact fixture or external authority approved for the
  exact operation and server contract;
- `observed_native_success` — a 201/created result whose returned identity,
  path, commit, side, line, body, and actor match the intended native write;
- `observed_definitive_unsupported` — a fully received exact structured
  rejection recognized by the pinned classifier revision.

All other outcomes leave the capability `unknown`. Observed evidence is
freshness-bounded so a future rollout can be adopted. The durable publication
record, not the cache, is the authority for whether one in-flight logical
publication may enter fallback after restart.

## Publication decision table

| Placement | Off-hunk capability | Action |
|---|---|---|
| `legacy_line` | any | Publish native line comment through the existing RFC-BPR-0008 path |
| `off_hunk` | `supported` | Publish native line comment |
| `off_hunk` | `unsupported` | Publish the file-level permalink representation |
| `off_hunk` | `unknown` | Attempt the intended native write once; fallback only after definitive unsupported rejection |
| `unknown` | any | Attempt the native write; never choose fallback from an inference |

A known unsupported repository does not authorize fallback for an anchor whose
placement is unknown. It remains safe to attempt the native write: a legacy
line can succeed, while an exact unsupported response can prove the selected
anchor's off-hunk status.

## Native negotiation

The actual user-approved publication is the probe. No disposable comment is
created and deleted merely to test capability.

```text
logical publication prepared
    -> native child attempt prepared with exact baseline
        -> created and exactly parsed
            -> resolve logical publication as native_line
            -> record supported evidence

        -> fully received, exact unsupported rejection
            -> durably resolve native child as definitively_rejected
            -> durably authorize fallback on the parent
            -> record unsupported evidence
            -> prepare file-permalink child attempt

        -> any created-but-unparsed, timeout, cancellation, connection loss,
           5xx, rate limit, authentication/authorization failure, generic 422,
           malformed response, or unknown result
            -> no fallback
            -> preserve or reconcile the native attempt according to its
               exact transmission state
```

A fully received non-creation response may retire a child attempt as rejected,
but only the exact unsupported classifier authorizes fallback. `422` by itself
never does.

## Fallback representation

The fallback is an ordinary **file-level pull-request review comment**, not a
PR timeline comment and not a BetterPullReview protocol message.

The conceptual REST body is:

```json
{
  "body": "<canonical immutable permalink>\n\n<reviewer prose>",
  "commit_id": "<exact current head OID>",
  "path": "<exact changed-file path>",
  "subject_type": "file"
}
```

It omits `line`, `side`, `start_line`, and `start_side`.

The canonical permalink is built from the connected web origin, exact owner and
repository, full commit OID, segment-wise encoded repository path, and one-based
line fragment:

```text
https://<host>/<owner>/<repository>/blob/<full-oid>/<encoded-path>#L<line>
```

For Markdown files the canonical form inserts `?plain=1` before the fragment.
No other query, fragment, authority, or scheme is accepted. The compiler uses
the validated profile/target/anchor fields; it never interpolates a URL from
reviewer prose or provider response text.

The body grammar is intentionally visible and minimal:

```text
<one canonical bare permalink>
<one blank line>
<non-empty reviewer prose, byte-preserved>
```

GitHub renders same-repository permalinks as code snippets in comments. A
GitHub-only collaborator therefore sees the exact code and ordinary prose and
can reply using GitHub alone. There is no hidden marker to lose or trust.

## Fallback rehydration

A file-level root review comment may be projected at a semantic line only when
all of the following validate:

- the remote subject is exactly `file`;
- the comment belongs to the expected pull request and changed file;
- the first paragraph matches the canonical permalink grammar exactly;
- scheme, hostname, owner, repository, path, commit algorithm/OID, and line are
  valid and authority-equal;
- the full commit and blob are available through the retained authority, or the
  host can otherwise validate the line without inventing history;
- the line is within the exact blob;
- there is one unambiguous anchor;
- the remainder is non-empty reviewer prose.

A BetterPullReview publication record supplies exact immediate evidence. After
local database loss, the same public grammar can reconstruct it. A coincidental
external comment is harmless only when it independently satisfies the complete
contract; it is still ordinary GitHub content.

If a collaborator edits or removes the permalink, introduces ambiguity, moves
it to another authority, or makes it invalid, GitHub wins. BetterPullReview
keeps the canonical GitHub comment identity and degrades the conversation to an
honest file-level placement. It does not restore the old local anchor, search
for nearby text, or choose the nearest line.

Replies inherit the root conversation and never parse their own prose as
anchors.

## Durable idempotency and recovery

The current schema models one exact wire attempt. This RFC introduces one
logical publication with deterministic child attempts.

```text
logical key
    = hash(target + exact head + acting identity + semantic anchor
           + reviewer prose + disposition + logical-contract revision)

native attempt key
    = hash(logical key + native_line + native-wire revision)

fallback attempt key
    = hash(logical key + file_comment_with_permalink
           + permalink/body/wire revisions)
```

The exact serialized fields, length prefixes, algorithms, and revisions are
normative implementation details recorded in the live architecture and tests.
The representation must be part of each child fingerprint.

Minimum durable parent states:

```text
native_prepared
native_possibly_transmitted
native_definitively_unsupported
unsupported_evidence_admitted
fallback_prepared
fallback_possibly_transmitted
succeeded_native
succeeded_fallback
```

Additional explicit rejected/failed states are permitted when they preserve the
same transition rules.

The invariants are:

1. At most one child attempt can be eligible to POST at a time.
2. A fallback child requires one of two atomic durable authorizations: an exact
   definitive unsupported rejection of this publication's native child, or a
   first-attempt admission with proven off-hunk placement and fresh, exactly
   scoped unsupported evidence under the decision table. The latter is legal
   only when this logical publication has no native attempt; it records the
   placement and capability evidence on the parent without inventing a rejected
   native child. A cache lookup alone cannot authorize a resumed publication.
   RFC-BPR-0089 adds a third, for commit-filtered diffs only ([[BPR-1706]]):
   a first-attempt admission of a line the core placed where GitHub resolves
   no line comment, measured on BPR-Test #116, #117 and #119. Its permalink
   names the filtered diff's own commit; it has no native child.
3. While a native outcome is indeterminate there is no transition to
   `fallback_prepared`. A fully received exact non-creation rejection may
   authoritatively resolve a transmission witness through the classifier;
   timeout, lost/malformed response, or GET absence cannot do so.
4. A possibly transmitted child performs GET-only reconciliation after restart;
   absence is not permission to POST another representation.
5. Success records the canonical GitHub comment ID and chosen representation on
   the parent atomically.
6. Replaying a succeeded logical publication returns the original GitHub ID as
   a duplicate and sends no POST.
7. Baselines and exact-match reconciliation remain bounded and digest-protected.
8. Migrated unresolved schema-v2 rows are native attempts. They cannot gain a
   fallback child until their native outcome is authoritatively reconciled or a
   fully received exact unsupported rejection is newly recorded.
9. Migrated succeeded rows retain their existing canonical receipt.
10. Corrupt, over-capacity, ambiguous, or multiply matching state fails closed.

## Response and error classification

The GitHub adapter owns structured response parsing. The core receives typed
facts only.

The unsupported classifier is pinned by server contract, operation, API
version, and classifier revision. It matches a complete bounded structured
error shape, including the relevant field/resource/code/message tuple where the
service supplies one. Human-readable text alone is insufficient unless an
exact recorded contract proves that no stable structured discriminator exists
and the product owner accepts that bounded dependency.

The following never authorize fallback by themselves:

- status `422`;
- status `403` or a permission message;
- secondary rate limiting or spam detection;
- `404`/non-disclosure;
- an unknown GHES version;
- a timeout, cancellation, TLS error, proxy error, redirect, or connection loss;
- a 5xx response;
- malformed or oversized JSON;
- failure to parse a created response;
- zero results from post-attempt reconciliation.

## Qt interaction

For every eligible line in the current/right source pane:

- the same comment affordance is present;
- the same draft editor opens;
- the same Publish action is used;
- the same pending state is announced;
- the resulting conversation is selected at the semantic line;
- keyboard, focus, screen-reader, high-DPI, and theme behavior match ordinary
  hunk comments.

Qt receives no `Use fallback` command and owns no capability boolean. A
representation may appear in an expandable diagnostic such as:

> Published as a file review comment with linked code because this host could
> not create a native thread at the selected line.

That diagnostic is explanatory only. It is not an error and does not move the
thread away from the selected source line inside BetterPullReview.

When fallback would destroy required semantics—for example, an applyable
line-suggestion construct—the core returns a typed incompatibility. The UI must
state the limitation; it must not silently publish a different kind of content.

## Synchronization and mixed clients

GitHub IDs remain canonical for publication, reply, edit, delete, resolve, and
refresh. The representation is not a second thread identity.

- Native comments normalize through the existing line-subject path.
- Compatibility comments normalize as file subjects plus a separately
  validated semantic anchor.
- Replies attach through GitHub's canonical parent/thread identity.
- External prose edits update the same comment.
- An external anchor edit is accepted only if the new visible permalink
  validates; otherwise placement degrades to file level.
- Deletion removes the shared comment and leaves only any private workflow
  history already permitted by other RFCs.
- Database loss reconstructs shared state from GitHub; the canonical permalink
  reconstructs only the semantic placement it visibly encodes.
- Uninstalling BetterPullReview leaves an ordinary file review comment and code
  permalink usable in GitHub.

## Security and limits

- All existing designated-target, acting-identity, transport-lease, current-
  head, and attended live-mutation gates remain mandatory.
- Fallback is not a second mutation authority; it runs through the same
  coordinator, provider session, transport, and one-use production carrier.
- The generated URL is same-host and same-repository. Redirects, userinfo,
  alternate ports, encoded separators, dot-segments, and arbitrary queries are
  rejected.
- Path and body limits are checked before durable admission. The generated
  prefix counts against the existing maximum body bytes.
- Parsing a public permalink grants only a presentation anchor. It grants no
  credential, mutation, cache-retention, or historical-import authority.
- Capability evidence and diagnostics contain no credential, private blob
  content, raw response body beyond bounded sanitized classifier fields, or
  user IP/proxy data.
- Automated tests remain network-isolated. Live writes use the designated
  attended path only.

## Performance and boundedness

Routine interaction remains proportional to the active source selection and
changed conversation set.

- Capability lookup is O(1) or logarithmic in a fixed bounded cache.
- Publication planning consumes one bounded observation record.
- Permalink compile/parse is linear in the bounded path and comment prefix, not
  repository size.
- Rehydration validates one root comment once per changed record; it does not
  scan every blob for matching text.
- Thread-map updates remain incremental by canonical comment ID and semantic
  anchor.
- Recovery retains at most the existing configured publication bound, with at
  most two child attempts per logical publication.
- No paint, wheel, hover, or scroll path performs provider capability work or
  permalink parsing.

## Relationship to RFC-BPR-0023

RFC-BPR-0023 owns private source history, composition, durable draft correlation,
and local publication lineage. This RFC owns public representation and logical
publication recovery. Both extend the one RFC-BPR-0008 draft/coordinator path.
The accepted draft body is reviewer prose. A file representation prepends only
this RFC's visible canonical code permalink; the prose remains byte-preserved.
Lineage records the approved prose digest separately from the exact public wire
body digest, chosen representation, logical publication identity, and canonical
server receipt. No private provenance enters the generated prefix or public body.
RFC-BPR-0023 consumes the successful logical receipt, never an individual child
attempt as a second publication. An indeterminate attempt or a failure to save
local lineage cannot authorize another POST.

## Relationship to RFC-BPR-0021

RFC-BPR-0022 can be implemented without waiting for RFC-BPR-0021 because every
publication in this RFC is bound to the exact current head and the fallback URL
visibly carries that commit. That fact is specific to comments created with
this representation.

RFC-BPR-0021 remains necessary for the general case: learning and projecting
the revision identity of arbitrary native and externally created comments,
including historical/outdated conversations. RFC-BPR-0022 must not introduce a
second general revision-join model or infer arbitrary comment revisions from
line numbers.

## Acceptance criteria

1. Every materialized current/right source line in a text file already changed
   by the pull request exposes the ordinary comment gesture; file eligibility,
   exact head, path, line, and acting identity are revalidated before any write.
2. A selected anchor accepted by the legacy line-comment operation publishes as
   one native line review comment on GitHub.com and GHES, independent of the
   off-hunk capability state.
3. An off-hunk anchor on a repository whose exact operation is proven supported
   publishes as one native line review comment and records scoped positive
   evidence.
4. An off-hunk anchor proven unsupported publishes as one ordinary file-level
   review comment whose first paragraph is the canonical immutable permalink
   and whose remaining prose is byte-preserved.
5. An unknown capability or placement attempts the intended native write; only
   a fully received exact unsupported rejection can authorize fallback.
6. Timeout, cancellation, connection loss, created-but-unparsed response,
   generic `422`, permission failure, rate limiting, malformed response, 5xx,
   or ambiguous reconciliation never starts a fallback POST.
7. One logical publication produces at most one GitHub comment across retries,
   process death, schema migration, and restart; a possibly transmitted native
   attempt is reconciled GET-only and can never coexist with an eligible
   fallback POST.
8. Capability evidence is repository-, actor-, profile-, server-contract-,
   operation-, API-, and classifier-revision scoped, freshness-bounded, and
   never inferred from host name, broad GHES version, readable comments, or
   status code alone.
9. BetterPullReview retains both the semantic source anchor and the honest
   GitHub remote subject; it renders a valid compatibility thread at its source
   line without claiming GitHub stored a line subject.
10. A GitHub-only collaborator can see the linked code, read the complete prose,
    reply through ordinary GitHub UI, and retain that conversation after
    BetterPullReview is uninstalled.
11. Deleting the local database and refreshing from GitHub reconstructs a
    canonical compatibility anchor and reviewer prose; invalid, ambiguous, or
    foreign permalinks remain honestly file-level.
12. External edits, replies, resolution changes, and deletion reconcile by
    canonical GitHub identity. Removing or changing the permalink updates or
    removes the semantic placement according to server truth and never revives
    stale local placement.
13. A body requiring line-only semantics is not silently degraded through the
    file representation; it receives a typed incompatibility with no POST.
14. The complete gesture follows Qt intent -> core decision -> application host
    -> mutation coordinator -> GitHub adapter -> durable recovery -> server
    refresh -> incremental Qt projection, with no provider/widget bypass.
15. Automated tests cannot reach a non-designated live target, and live native
    and fallback evidence is produced only through the existing attended,
    identity-bound mutation authority.

## Verification matrix

| Claim family | Direct surface | Required proof |
|---|---|---|
| semantic eligibility and planning | core review-round-trip policy | table/replay/property cases for every placement × capability × body-compatibility state and every illegal anchor |
| wire representations | GitHub mutation contract | byte-exact native and file-subject requests, exact response parsing, hostile URL/path/body fixtures, and classifier counterexamples |
| no duplicate publication | coordinator + SQLite recovery | native success, exact rejection then fallback, death before/after each witness, lost 201, generic 422, ambiguous GET, migration, and replay |
| honest normalization | read normalizer + thread map | native line, canonical fallback, database loss, edited link, foreign link, ambiguous link, reply inheritance, delete, and incremental remap cases |
| invisible ordinary UX | shipped Qt workbench | same gesture/focus/pending/result on hunk and off-hunk lines at required scales; representation appears only in diagnostics |
| GitHub-only interoperability | designated GitHub web UI | native and fallback comments are readable/replyable with no BetterPullReview-specific marker; fallback displays the code permalink/snippet |
| capability truth | recorded/live operation contract | one exact supported result and one exact unsupported result or approved equivalent, keyed to the operation and repository actually exercised |
| security boundary | mutation-isolation and designated-live course | no automated executable can send either native or fallback write outside the existing designated authority |

The live verification ledger expands these rows one-for-one against the fifteen
canonical acceptance criteria.

## Documentation impact

At minimum, implementation reconciles:

- `docs/architecture/product-contract.md` — semantic anchor versus GitHub
  representation and uninstall/database-loss behavior;
- `docs/architecture/system-architecture.md` — core/host/adapter allocation,
  capability evidence, and logical-publication recovery;
- `docs/architecture/review-round-trip.md` — state machine, fallback body,
  reconciliation, and diagnostics;
- `docs/architecture/github-provider-contract.md` — native/file wire shapes,
  error classifier, URL validation, and capability scope;
- `docs/architecture/diff-pipeline-and-rendering.md` — comment gesture on full
  current-side source lines and prohibition on using Qt geometry as provider
  placement evidence;
- `docs/security/foundation-security.md` and threat model — same-authority URL
  construction, untrusted permalink parsing, and no second mutation path;
- `docs/ux/workbench-specification.md` and review-state semantics — one
  representation-blind interaction plus honest support diagnostics;
- `docs/verification/RFC-BPR-0022.md`, master matrix, fixture catalog, and
  release gates;
- root README and agent guidance only where the implemented stable boundary
  makes current text false or dangerously incomplete.

## Prohibited designs

- Editing RFC-BPR-0008 in place to conceal the new behavior.
- Hiding the comment affordance because the selected host is old.
- Asking the user to choose a compatibility mode.
- Posting on a nearby changed line and explaining that the comment is really
  about another line.
- Using a PR timeline comment when an ordinary file review comment is
  available.
- Treating all `422` responses as unsupported.
- Starting fallback after any outcome in which the native comment may exist.
- Reusing one idempotency key for two different wire bodies without a parent/
  child publication model.
- Parsing arbitrary URLs in arbitrary comments as trusted anchors.
- Persisting a private-only anchor that cannot be reconstructed after database
  loss while claiming GitHub-only interoperability.
- Replacing canonical GitHub IDs with a BetterPullReview thread identity.
- Running capability probes by creating disposable comments outside the user's
  approved publication.

## External references

- [GitHub changelog: comment anywhere in a changed file](https://github.blog/changelog/2025-09-25-pull-request-files-changed-public-preview-now-supports-commenting-on-unchanged-lines/)
- [GitHub REST: pull request review comments](https://docs.github.com/en/rest/pulls/comments)
- [GHES 3.18 REST: pull request review comments](https://docs.github.com/en/enterprise-server@3.18/rest/pulls/comments)
- [GHES 3.21 release notes and 2026-06-23 errata](https://docs.github.com/en/enterprise-server@3.21/admin/release-notes)
- [GitHub Docs: permanent links to code snippets](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-a-permanent-link-to-a-code-snippet)
