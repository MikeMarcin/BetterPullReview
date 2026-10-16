# Fixture Catalog

RFC-BPR-0023's local private-review courses use synthetic identities, exact
source digests and canary bodies. The native/file/retry public-path courses
exercise draft separation, mutation admission and recovery without a live
repository. The M9 render set and SHA-256 manifest live in
[`rfc-bpr-0023-m9-rendered/`](rfc-bpr-0023-m9-rendered/); the
[evidence ledger](RFC-BPR-0023.md) names the full focused commands. Designated
GitHub mutation and independent server readback remain BPR-0584 evidence, and
fixture results must not be relabeled as that observation.

The implementation must maintain sanitized deterministic fixtures covering at least:

## Implemented GitHub read contract v1

`tests/fixtures/github/contracts/v1/` contains paired `github-com` and
`ghes-3.18` manifests, plus the recorded `uiforetw-74` described below, for the
ten RFC-BPR-0002 read operations. Each manifest
pins the product, synthetic authority, API roots, request method, relative
path, canonical query, non-secret headers, GraphQL document and variables,
and selected response projection. The two response trees contain the same
synthetic semantic facts.

### `uiforetw-74`, a recorded public pull request

The third `--github-fixture` name. Where `github-com` and `ghes-3.18` are
hand-written payloads written to satisfy the code that reads them,
`uiforetw-74` is generated from a recording of the real public pull request
`google/UIforETW#74` — five modified files, 2026-08-08. It exists because a
presentation judgement made against content authored to please a reader is
weak evidence, and because BPR-0214 needs a comparison with two sides.

Every served byte is produced by
`scripts/build_recorded_pull_request_contract.py` from
`tests/fixtures/github/pull-requests/uiforetw-74/recording.json`, so the served
tree cannot drift from the recording; `recorded_pull_request_fixture_tests.py`
regenerates it into a temporary directory and compares bytes.

**The served commit identifiers are not GitHub's.** The recording names
`7c519e97…` and `9b01c587…`; the fixture serves `8b70ee17…` and `e40058be…`,
which are the commits `build_recorded_pull_request_pack.py` builds from the
recorded blobs into `objects.pack`.

| | recorded | served |
|---|---|---|
| base | `7c519e97eb906bdc6b83c8f576c91c063d6ceee9` | `8b70ee179fe55b705ee4e613ecbc89a0acebc1bc` |
| head | `9b01c587b3e46637ff69c3801841993bd054f106` | `e40058be7d5fe319342dffb567ce30e42bc6f4de` |

This divergence is deliberate and was the product owner's decision on
2026-08-08. A commit OID hashes its trees, parents and author records, none of
which the recording carries, so GitHub's real objects cannot be rebuilt from
it — and `index-pack --strict --check-self-contained-and-connected` plus
`fsck --connectivity-only` require every named ancestor to be present. Serving
the real OIDs beside a pack built from generated ones is the half-divergence
that connects and then fails at import. The alternative was committing a clone.

The served metadata therefore carries placeholders — `1`×40 for the base and
`2`×40 for the head — substituted at load time by the same mechanism
`github-com` already uses. That the real OIDs appear nowhere under
`contracts/v1/` is asserted by reading bytes, not intended.

It claims `github-com`'s capability directory rather than a copy of it: the
recorded pull request lives on github.com, so the honest capability claim is
GitHub.com's pinned schema snapshot.

### `nested-folders-v1`, a constructed pull request with nested folders

The fourth `--github-fixture` name, added for [[BPR-1213]]. It is constructed,
not recorded: `scripts/build_nested_folders_fixture.py` writes its 17 files'
base and head text into
`tests/fixtures/github/pull-requests/nested-folders-v1/construction.json`, and
the same pack and contract builders as `uiforetw-74` turn that into
`objects.pack` and the served tree. It exists because the recorded pull request
has one folder and no subfolder, which is too little to judge the Overview's
folder rows and their disclosure. Its files sit in 12 folders, including
folders nested three deep and intermediate folders with no file of their own.
The construction carries no `recorded_from`, names no URL, and its fixture test
regenerates it byte for byte. Like `uiforetw-74`, it serves the builder's
commits, one head, and github.com's capability snapshot.

### `conventional-comments-v1`'s next push

Added for [[BPR-1331]], so the private progress button can be judged when the
pull request gets an update. `scripts/build_conventional_comments_next_push_pack.py`
builds a third commit on the nested-folders head that changes only
`src/engine/renderer.cpp` by two lines, into
`tests/fixtures/github/pull-requests/conventional-comments-next-push-v1/next-push.pack`.
The descriptor names it as the fixture's next push, served as the second head
only under an epoch phase: `single` over a data root serves the first head,
`reuse` over the same root the push. Without a root the fixture serves its one
head as before. `application_review_progress_push_contract` pins what the
button shows across the push.

### `inline-drawer-v1`, conventional comments plus a long file with threads

Added for [[BPR-1290]], so the inline Comments drawer can be judged against its
prototype on a long file. `scripts/build_inline_drawer_fixture.py` writes the
nested-folders files plus `src/ui/conversation_cards_view.cpp` (this
repository's `conversation_cards_view.cpp` at `fbbabb3ef`, 1557 lines, the
prototype's code) into
`tests/fixtures/github/pull-requests/inline-drawer-v1/construction.json`; the
same pack and contract builders serve it, and the script's `--threads` step
gives the served tree conventional-comments-v1's six threads and the
prototype's eight on the long file. Constructed, not recorded; its fixture test
regenerates the construction and the served tree byte for byte.

### `overview-labels-v1`, many labelled conversations across the folders

Added for [[BPR-1089]], so the Overview's label chips, Open and All, the ?
count and the drill-down can be judged on more than a handful of threads.
`scripts/build_overview_labels_fixture.py` copies the nested-folders contract
(its pull request, files and pack) and serves 34 review threads on all 17 files
and one on a file the pull request no longer lists, three pull request comments
and two review summaries: every Conventional Comments label, several on one
file, seven resolved and one outdated thread, a plain root and a custom label
(Other), and a file comment that begins with a link, whose label the counter
cannot read (?). The comments and summaries have no resolution, so Open counts
them under ?. No conversation has an unknown resolution: that needs the REST
fallback, which a GraphQL fixture does not take. Constructed, not recorded;
`application_overview_labels_public_path` pins its counts.

### `markdown-parity-v1`, the GitHub-parity Markdown bodies

Added for [[BPR-1543]], so the Markdown reader's hidden comments, line breaks,
alerts and footnotes can be judged in the application beside GitHub.
`scripts/build_markdown_parity_fixture.py` copies the nested-folders contract
(its pull request, files and pack) and serves `tests/fixtures/markdown/github-parity-v1/pr-template.md`
as the description and that directory's comments, breaks, alerts and footnotes
bodies as four pull request comments, byte for byte. GitHub's own rendering of
each body is kept beside it as `*.github.html`. Constructed, not recorded.

### `markdown-html-v1`, the GitHub-parity HTML bodies

Added for [[BPR-1544]], so `<details>`, GitHub's HTML subset and emoji
shortcodes can be judged in the application beside GitHub.
`scripts/build_markdown_html_fixture.py` copies the nested-folders contract and
serves `tests/fixtures/markdown/github-parity-v1/pr-html.md` as the
description and that directory's details, html and emoji bodies as three pull
request comments, byte for byte. The hostile-HTML body holds forbidden byte
patterns by design, so it is judged from its tests rather than served.
Constructed, not recorded.

### `conversations-v1`, prototype v2's pull request

Added for [[BPR-1293]], so Threads (named Conversations until [[BPR-1341]]) can be judged against its prototype
(`docs/design/BPR-1290-inline-drawer-v2`). `scripts/build_conversations_fixture.py`
writes the prototype's ten files, verbatim from its `code.js`, into
`tests/fixtures/github/pull-requests/conversations-v1/construction.json`, with
the prototype's changed-line ranges; the same pack and contract builders serve
it as `bpr-fixtures/conversations#418`, and the script's `--threads` step gives
the served tree the prototype's twenty-one conversations (91 posts, its draft
excepted), extracted verbatim into `sources/conversations.json`. Line 667's is
branching-thread-v1's shape with an aside that has replies of its own. Posts
are in the order their ages say they were written, so the prototype's new posts
end every conversation, and the builder's `read_state.json` names the last post
seen in each; `--attended-open-file … --attended-read-state` records that
through the review ledger before a person looks. Constructed, not recorded; its
fixture test regenerates the construction and the served tree byte for byte.

### `line-details-v1`, long changed lines

Added for [[BPR-1334]], so Line details can be judged on the lines it is for.
No other fixture had a changed line longer than 148 characters.
`scripts/build_line_details_fixture.py` writes four files into
`tests/fixtures/github/pull-requests/line-details-v1/construction.json`: a
minified one-line JSON document with two values changed, unwrapped Markdown
paragraphs (one clause rewritten, one paragraph replaced), a C++ file whose
long SQL literal and long call gained columns and arguments, and a minified
line of about twelve thousand characters that Line details cuts at 4,096 bytes.
The same pack and contract builders serve it as `bpr-fixtures/line-details#34`
on github.com, signed in as `fixture-author`, with no review, thread or
comment. `application_line_details_public_path` reads the strip on it, and its
`--attended` mode stops on the SQL line for the owner's look. Constructed, not
recorded; its fixture test regenerates the construction and the served tree
byte for byte.

### `intraline-v1`, everyday changed lines

Added for [[BPR-1537]], so intraline emphasis can be judged on the kinds of
change a reviewer meets every day. No other fixture had more than a handful of
similar pairs. `scripts/build_intraline_fixture.py` writes five files into
`tests/fixtures/github/pull-requests/intraline-v1/construction.json`, each
changed line a one-for-one replacement between unchanged lines: a C++ file with
renamed identifiers and parameters, an inserted and a removed argument, edited
strings, a changed operator, realigned spacing, changed numbers, swapped
operands and a rewritten line; Markdown prose with a changed word, a rewritten
clause, an added phrase, a moved link and a replaced sentence; JSON with a
number, a revision hash and a string changed; Python with a renamed function,
an added argument and an edited f-string; and accented, CJK, symbol and emoji
text. The same pack and contract builders serve it as
`bpr-fixtures/intraline#37` on github.com, signed in as `fixture-author`, with
no review, thread or comment. Constructed, not recorded; its fixture test
regenerates the construction and the served tree byte for byte.

### `markdown-images-v1`, images in a description and comments

Added for [RFC-BPR-0084](../rfcs/RFC-BPR-0084-inline-markdown-images.md), so
inline images can be judged beside GitHub's own presentation.
`scripts/build_markdown_images_fixture.py` copies the nested-folders contract
(`bpr-fixtures/nested-folders#21`, signed in as `fixture-author`) and gives it a
description with five uploaded attachments (one `<img width>`, two in a
table), a proxied transparent PNG, a linked image, an animated GIF at a set
height, a `<picture>` with a dark source, an SVG badge and an `<img>` in code,
and two pull request comments: an attachment at a set width, and an animated
GIF beside an image that is gone. Beside the contract, under
`tests/fixtures/github/markdown-images/v1/markdown-images-v1/`, it writes what
GitHub's rendering serves for them: `bodies.json` (each node's `bodyHTML`, with
camo and `private-user-images` addresses signed `jwt=fixture`), `manifest.json`
(each served address's file and content type) and the images, drawn from code.
A fixture run answers `resolve_body_images` from `bodies.json` and image
fetches from `manifest.json`, never the network; `missing.png` has no file and
fails. Every name, address and image is synthetic. `markdown_images_fixture_contract`
regenerates both trees byte for byte and checks they agree;
`application_markdown_images_public_path` opens it through the real composition.

These fixtures are hand-minimized, sanitized provider projections, not
literal complete wire dumps. They deliberately omit authorization, cookies,
transport metadata, remote URLs, patches, source content, email addresses,
home paths, request identifiers, and provider fields that the normalizer does
not consume. The strict recorded-contract harness rejects additions or
omissions from this selected projection. A production transport must validate
the raw envelope and project the allowlisted fields before this strict
normalization boundary; additive raw GitHub response fields are not an excuse
to expose provider records inward.

The paired baseline contains repository, pull-request, viewer, author, review,
thread, comment, commit, check, and search identities. GraphQL repository and
pull-request node IDs are correlated with REST, and actor node IDs plus logins
are correlated across every normalized author. GraphQL file paths map
bijectively to the REST file collection and carry viewed state; threads carry
per-target resolve/unresolve availability, non-null `diffSide`, and separate
current and original line coordinates. Every GraphQL pull-request-bearing page
also carries `headRefOid`; acceptance requires it to match the REST head OID, so
a cross-endpoint head race cannot become one normalized snapshot. Contract variants directly
exercise null/deleted and non-User actors, fork heads, nullable review commit
IDs, renamed files, every viewed state, queued checks, and both line- and
file-level review threads. Every selected response is explicitly marked
complete; unknown completeness or a transport-declared continuation fails.
The files, threads, and nested comments GraphQL connections each have
terminal `pageInfo`; a true `hasNextPage` result fails rather than being
normalized as a truncated snapshot.

A current line thread requires a known `LEFT` or `RIGHT` side and its current
one-based root line; an outdated root may retain only its original line. A
line-thread reply may omit both of its own nullable coordinates and then
inherits the root anchor; any present reply coordinate is still validated. A
file thread carries neither line.
Missing, null, wrong-type, or unknown side values and invalid subject/line
relationships reject the complete response before projection. Application
projection preserves current and original lines independently; it never
substitutes an original line into the current comparison.

The fixture security gate verifies the exact tree and operations, rejects
duplicate JSON members and encoded secret/path/email/foreign-authority
bypasses, and checks both hosts against one independent normalized
expectation. No live or private repository response was recorded.

## Constructed deep conversation projection v1

`tests/fixtures/github/conversations/deep-thread-v1/` is deterministic,
constructed test data for BPR-0417 and the BPR-0418 thread-map renderer. It is
not a GitHub recording, carries no `recorded_from` or `recorded_on` claim, and
is not evidence that a public or private pull request contained this
conversation. It bypasses no provider validation and has no production loader.

The valid case models the exact `github_conversation_projection` field set in
stable parent-before-child order. One review-thread root contains a first
comment whose provider `replyTo` is null, a five-reply-edge primary chain, and
sibling branches at reply depths one and three. Those facts make the fixture
deep and wide enough to exercise the selected Treeverse map without changing
the byte-identical `github-com`/`ghes-3.18` contract pair or the fixed-length
public-path courses.

Two separate hostile projection cases contain one absent parent and one
two-comment cycle. Each is machine-labelled non-provider-reachable and scoped
only to Qt defensive row preservation. The validated GitHub read model rejects
unknown and forward parents before projection, so the hostile shapes cannot be
cited as provider acceptance evidence or mixed into the valid tree.

`bpr0417_deep_conversation_fixture_contract_tests.py` independently enforces
the exact schema and constructed provenance, canonical JSON, stable identities
and order, root/null semantics, minimum depth, sibling depths, hostile-case
classification, missing-parent/cycle shapes, and disjoint valid/hostile
identities. Its ten direct tests pass, and the ordinary network-isolated
`github_conversation_fixture_contract` registration in `tests/CMakeLists.txt`
runs them in the default plan with no isolation exemption. The reviewed
BPR-0256 v006 plan ran once before this registration landed and is frozen as
safely cancelled, non-retryable evidence, so the added test drifts no reviewed
live plan. BPR-0418 has now taken that rendered consumption: the
`qt_github_thread_map_scale_{100,125,200}` suite reads this file directly
rather than restating its nodes, so a change to its topology reaches the
renderer's own assertions instead of passing them silently. That consumption
adds no production loader and does not upgrade this constructed content into
real-world need or shipped-public-path evidence — the map is proven to render
the shape, not that any pull request ever carried it.

## Promoted RFC-BPR-0017 anonymous capture set v001

`tests/fixtures/github/rfc0017-anonymous-v001/` holds the signed-out read
evidence for BPR-0302. It has two halves that must never be confused, and
`provenance.json` states which half every file belongs to.

`captures/` is real. Each file is the response body of a live unauthenticated
request to api.github.com, promoted byte for byte out of the probe review at
`docs/verification/reviews/rfc0017-anonymous-probe-v001/`: the public pull
request, its inline review comments at `per_page=1`, the rate-limit resource,
a 404 for a repository that does not exist, and the GraphQL refusal. The sixth
file, `rate-exhaustion.json`, is that review's own derived artifact and says
`synthetic` in its provenance entry rather than claiming to be an observed
403.

Nothing here re-records those bodies, and nothing here is authority for them
on its own. `github_anonymous_read_contract` binds each promoted body to the
digest the probe review recorded for it, binds the capture file it came from
to that file's own recorded digest, and compares the promoted bytes against
the capture's `body` field. Editing a promoted body to make a test pass fails
all three checks. This is the `uiforetw-74` rule applied to a smaller set:
generated content is compared against its recording rather than trusted
because it looks plausible.

`synthetic/` is hand-written and never presented as service behaviour. The
live comments capture holds exactly one comment, so it cannot express a reply,
a second page, or any hostile linkage. The synthetic files supply those cases:
reply-linkage grouping where a second root sits between two replies to the
first, a full first page plus a second page whose replies belong to roots from
the first, an observed-empty listing, a self-reference, a two-comment cycle, a
reply to a comment nobody returned, a reply chained onto another reply, a
duplicate identity, `in_reply_to_id` carried as text, an absent anchor field,
and a null anchor meaning the comment's line has left the diff. Each carries
its purpose and its digest in `provenance.json`, and each is consumed by
`github_paged_read` as a refusal or a grouping case, never as evidence that
GitHub returned such a payload.

## Recorded RFC-BPR-0016 device-authorization contract v1

`tests/fixtures/github/device-authorization/v1/` now holds the real sanitized
attended capture recorded by BPR-0276 on 2026-08-24, replacing the earlier
`pending_real_capture` placeholder: one registration record, the seven closed
endpoint outcomes, the ten read-only route probes, and the binding manifest,
all captured in one owner-attended sitting under the single repaired runner
digest `d00583ab…` and admitted as
`RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture` against the
owner's accepted review at
`docs/verification/reviews/rfc0016-attended-live-capture-v001/`. The five
attended course trees, both slow-down authority/spend chains, and the strict
`live-run-attestation.json` live at
`docs/verification/rfc0016-live-capture-v001/`.

`scripts/capture_rfc0016_device_contracts.py` is an offline-only sanitizer and
validator for that recorded course. It has no live-network or write mode. The
library surface accepts response bytes in memory, projects only closed
structural fields, omits access/refresh tokens and device/user codes, and
requires canonical JSON before an unattested durable fixture tree can validate.
Error responses retain the closed error enum and a boolean confirming that the
observed error URI used the official documentation authority. `slow_down`
additionally retains only GitHub's bounded positive integer `interval`, because
that non-secret value is the rate-limit transition the contract must prove;
the runner requires it to equal the active interval plus five. No other error
may carry an interval. Free-form description and URI text remain
observed-but-omitted fields, so user-code-, cookie-, JWT-, path-, or
identity-shaped text cannot become durable evidence.
The closed schema requires exactly one projection for device-code
success, token success, `authorization_pending`, `slow_down`, `access_denied`,
`expired_token`, and `device_flow_disabled`; synthetic hostile parser inputs
cannot satisfy an unattested-capture manifest. A separate exact registration
record constrains the GitHub App to private visibility under owner
`MikeMarcin`, owner-only installation with selected-repository access only to
`MikeMarcin/BPR-Test`, and the approved repository permissions. Its OAuth
client type remains `public`; the record contains that public client identifier
and its SHA-256 digest, never a client secret.

The unattested manifest pins every artifact path and digest plus the prior
accepted authority-ceiling decision identity; the narrower private-owner-only
implementation selection is enforced independently by the closed registration
schema. Every non-registration artifact carries the registration-record
digest, so outcomes and probes from a different registration cannot be mixed
into the set. Neither the public sanitizers nor the manifest builder can emit
or accept `real_service` provenance. The offline validator can therefore
establish durable schema integrity but cannot claim that GitHub was contacted
on its own. `--require-complete` remains fail-closed for an unattested tree
unless `--attestation-review` names one direct repository-recorded review
under `docs/verification/reviews/` and `--course-evidence-root` supplies the
exact five attended course trees. The validator closes every course inventory,
execution record, transition, outcome, route projection, registration binding,
and tree digest. The same exact root also carries canonical copies of the
original replacement authority and spent marker that record the parser-rejected
attempt, plus the parser-repair recovery authority and its distinct
before-traffic spent marker. The recovery authority binds the original record,
marker, failed-runner digest, and repaired-runner digest; the final slow-down
course binds both reservation digests. Both authority chains and their ordered
authorized-before-spent-before-course timestamps are closed. All four record
bytes are review identities but are deliberately excluded from the five runner
course-tree digests because they were not stored inside those course roots. The
validator requires one repaired repository runner digest throughout the final
five-course set and
reassembles the final fixture bytes and manifest from the prescribed course
sources. The review must bind every course, authority, and fixture artifact plus the
runner's strict `live-run-attestation.json` as identity files in one fixed
`rfc0016_attended_live_capture` gate. It requires an exact accepted verdict,
false-only downstream claims, byte triples for the current capture runner,
input, rendered page, manifest, verdict, and repository's official
attended-review validator. It then revalidates the verdict and rebuilds the
page through that bound validator. Unknown fields or types, links, escaping
paths, byte drift, missing, extra, or swapped course evidence, failed audit,
rejection, mixed registrations, changed assembly sources, and self-authored
records without the independent validation record fail closed.
Acceptance means only that a human accepted the exact sanitized bytes as
attended BPR-0276 run evidence; it does not claim cryptographic GitHub or
transport authenticity. The unit suite's generated verdict exercises this
contract only; it is not a real user export or immutable attended record. Its
read-only route metadata is closed to the designated
`MikeMarcin/BPR-Test` repository and pull request 1:
Metadata REST, Contents smart-HTTP discovery/upload-pack, Checks GET,
pull-request REST reads, and the exact review-thread GraphQL query. It cannot
admit a review publication route or infer permission to mutate.
`rfc0016_device_capture_security` runs the validator under the repository's
network-isolation wrapper and proves exact tree/schema/size rejection,
duplicate-key and secret canaries, path/IP/email/foreign-URL rejection,
raw-transcript refusal, digest integrity, registration binding, route/method
closure, the one-`slow_down` inventory, private owner-only registration, and an
adversarial synthetic-to-real provenance reproduction. Dedicated regressions
also prove lowercase user-code-shaped text, `session_id` cookie material, and a
JWT-shaped credential are omitted from canonical error artifacts.

### The documented-not-recorded renewal shapes

`tests/fixtures/github/device-authorization/documented-v1/` is a **separate**
tree, and the separation is the point. BPR-0276's attended sitting captured
five courses and none of them was a token renewal, so [[BPR-0450]] could not
file its refresh shapes in `v1/` without either breaking the manifest that
digest-pins exactly nine artifacts or claiming membership in a sitting that
never happened. The recorded tree is byte-identical to what BPR-0276 admitted
and stays that way.

The documented tree carries its own schema names —
`bpr.rfc0016.device_documented_contract` and
`bpr.rfc0016.device_documented_contract_manifest` — a `state` of
`documented_not_recorded`, an explicit `"recorded": false` on the manifest and
on every artifact's provenance, and `source_kind: documented_not_recorded`.
That value is deliberately **not** added to
`capture_rfc0016_device_contracts.py`'s `ARTIFACT_SOURCE_KINDS`: the capture
sanitizer must remain unable to emit or accept a documented shape, so the
frozen set stays `{synthetic, unattested_capture}` and the documented tree is
validated by `github_device_authorization_contract` instead. Its two artifacts
are digest-pinned exactly as the recorded ones are, and the contract test
recomputes both digests.

Two shapes live there: `refresh_success` and `bad_refresh_token`. The success
body is asserted field-for-field identical to the recorded `token_success`
projection, so the genuinely unrecorded surface narrows to the request's
`grant_type=refresh_token` pair and the one admitted refusal code. Everything
else about the renewal — trust, redirects, response bounds, cancellation, and
the wire body itself — is proved on a real socket by the `refresh-*` scenarios
under `github_device_authorization_refresh_contract`, whose `refresh-lifecycle`
course drives the real application host through the real production adapter and
rotates a vault refresh slot over that socket.

## Implemented GitHub capability contract v1

`tests/fixtures/github/capabilities/v1/` contains independent complete
introspection responses derived from the official public GitHub.com and GHES
3.18 schema snapshots with the probe's exact static query. Per-host manifests
pin the official source URL and digest, retrieval metadata, query digest,
generator version, complete response digest, and actual Mutation and
PullRequest field counts. Synthetic missing/incompatible mutation-contract
descriptors derive only from the GitHub.com response. The contract covers the
exact review-thread query fields, viewed-file state, mark/unmark inputs and
payloads, resolve/unresolve inputs and payloads, and their dependency
relationships.

These are public schema-contract observations, not live account-permission
claims or authenticated API recordings. Unsupported results are synthetic
because both selected schemas support all four capabilities. Operational
failures and partial/malformed responses are constructed in the focused probe
test and must remain all-Unknown. The fixture security gate pins the exact
tree, provenance and response digests, rejects duplicate JSON members and
secret-bearing content, proves the host responses are independent, and keeps
every relative descriptor reference inside the capability fixture root.

## Public epoch composition and persistence fixture

The test-enabled public application combines the sanitized GitHub.com or GHES
recording with a deterministic local three-commit Git repository and a strict
in-process smart-protocol-v2 transport. The transport serves only the accepted
base/head advertisement and complete self-contained pack requested by the
application-owned protocol client. Pack bytes then follow the production
credential-free installed-Git cache path; the fixture never gives installed
Git a URL, network, credential, hook, filter, or worktree authority.

The exact graph is recorded in
`tests/fixtures/git/epoch-history/v1/manifest.json` with its semantic
regeneration recipe. Both heads are direct children of the base, and the
replacement pack contains the base and head two but not head one. The
`epoch_history_fixture_contract` target binds the embedded pack bytes to exact
sizes and SHA-256 digests, imports each pack independently with installed Git,
checks every commit/tree/blob/parent/identity/timestamp/message fact, and
proves neither head is an ancestor of the other. This makes the second public
observation a real non-fast-forward replacement rather than a label-only
force-push simulation.

The single-process rendered cases drive the real connect/open actions and
require an exact visible base/head comparison. The two-process persistence
cases share one disposable owner-private application-data root. `seed` accepts
head one, triggers the real refresh action to head two, and requires two smart
acquisitions plus the rendered ordinal-two `Force-push/rebase` cause. `reuse`
reopens the durable SQLite catalog and bare cache, loads retained history, and
requires zero smart acquisitions while head one remains selectable even
though the replacement pack omitted it. Both phases exercise
Whole PR, Previous upload, My last reviewed, arbitrary, reverse, and identical
selections through the actual Qt control and verify their complete OIDs. The
root is removed after the contract; recordings and evidence contain only
synthetic identities and fixed counters.

[[BPR-1340]]'s `application_epoch_update_line_public_path` drives the Updates
line on the same fixtures. Its drag half opens the recorded pull request with
three force-push updates, opens the line from the review bar's disclosure and,
from the whole-PR pair whose two handles share the newest node, drags the left
handle to the oldest update and the right handle back one update, then steps
the right handle with Shift+Right. Each gesture is delivered to the window as
a pointer or key is, and the course waits for the comparison the core
publishes back to put the handle there. Its date half observes one ordinary
update, clears the recorded date as an earlier session that could not read the
commit left it, and opens the review again at the same head in a new process:
the update must read "Commit date" again.

The `--epoch-fixture-root` and `--epoch-fixture-phase` arguments, phase
markers, fixture factory, recordings, and deterministic Git recipe are
test-build-only. The tests-disabled Release contract rejects all of their
paths, strings, and symbols from the compile graph and executable. These
fixtures are public-composition integration evidence, not live GitHub,
target-native Windows, native system-theme, or final presentation-polish
evidence.

The GitHub application-host contract additionally acquires a transport from
the exact accepted handoff, switches to an unrelated pull-request authority,
and requires both the authority lease and already issued transport to be
revoked before new provider work. The injected handoff observer failure is
still contained and reported, so revocation does not depend on a successful
observer callback.

## macOS Keychain integration fixture

The BPR-0022 target-native fixture uses the fixed
`org.betterpullreview.github-native-credential.file-keychain.integration-test.v2`
service and one exact synthetic authority in the user's default file-based
Keychain. It never selects the production service. A resource-locked CTest
setup/lifecycle/cleanup sequence proves initial absence, add, retrieve,
in-place replacement, retrieve, delete, post-delete absence, and exact final
cleanup.

The parent generates runtime-only printable canaries, sends them to a
same-executable child over framed standard input, uses a neutral audited
working directory and bounded child deadline, and captures only fixed
pass/fail output. It checks arguments, the explicit child environment,
executable, source, and isolated scratch/XDG tree for raw canaries. Combined
source-boundary checks reject settings, SQLite, environment, file, process,
logging, UI, and alternate-Keychain fallbacks from the production adapter.
The scratch scan does not inspect the user's complete home directory, and the
fixture does not claim cross-rebuild cleanup after an ad hoc code-signature
change.

BPR-0022's original closeout run exercised the historical hostname-free v1
service. BPR-0029 changed the current fixture to the disjoint v2 service and
574-character hostname-bound authority, then repeated the macOS setup,
lifecycle, and exact cleanup. Production has no v1 read, enumeration,
migration, or fallback.

## Windows Credential Manager integration fixture

The BPR-0023 fixture uses the fixed
`org.betterpullreview.github-native-credential.windows-credential-manager.integration-test.v2/`
prefix and one exact synthetic authority. It never selects the production
prefix. A resource-locked CTest setup/lifecycle/cleanup sequence is prepared
to prove initial absence, create, retrieve, single-call replacement, retrieve,
delete, post-delete absence, and exact final cleanup on Windows 10/11 x64.

The parent generates two runtime-only printable canaries, including one at the
complete 2,560-byte portable/native bound, and sends framed bytes to a
same-executable child through anonymous standard input. It starts the canonical
executable directly with an exact inherited-handle list, a sorted minimal
Unicode environment, and a unique neutral scratch directory. The child emits
only fixed pass/fail text. The parent bounds execution, requires confirmed
child termination before credential cleanup or scans, checks parent/child
arguments and explicit environment, and searches fixed stdout/stderr,
executable, source, and the isolated scratch tree for random 32-byte prefix
and suffix probes from both canaries. The short probes detect truncated
replacement leaks without placing either complete credential on an audited
surface. Successful scratch removal must leave the exact path absent.

If termination cannot be confirmed, the test fails closed and deliberately
does not delete storage beneath a potentially live child; the independent
CTest cleanup still attempts exact credential deletion. The fixture does not
enumerate Credential Manager. It also does not inspect arbitrary application
settings, SQLite files, Qt UI text, Windows Event Log, ETW, or the user's
complete profile. Settings, SQLite, UI, logging, file, registry, environment,
DPAPI, Credential Locker, and alternate-vault exclusions come from the
production source boundary and must not be attributed to the runtime fixture
alone.

This fixture and its WIN32 registration are checked in but have not run on a
native Windows environment. Host-stub compilation and an x64 MinGW PE
cross-compile do not satisfy that lifecycle claim. BPR-0036 retains the
target-native run and exact cleanup as open work.

BPR-0023's preparatory source and cross-PE evidence exercised the historical
v1 target format. BPR-0029 changed the current source and fixture to the
disjoint v2 target and 574-character hostname-bound authority. The v2
target-native lifecycle and cleanup remain unclaimed until BPR-0036 runs them
on Windows.

## Linux Secret Service integration fixture

The BPR-0024 fixture uses the fixed
`org.betterpullreview.github-native-credential.secret-service.integration-test.v2`
schema and one exact synthetic authority. It never selects the production
schema. Each item has only the 574-character hostname-bound `authority` value
plus the `xdg:schema` attribute supplied by libsecret, and resides only in the
default collection.

The resource-locked CTest setup/lifecycle/cleanup sequence proves initial
absence, a 73-byte store and retrieve, atomic replacement with a 2,560-byte
value and retrieve, delete, repeated absence, and exact final cleanup. Store
labels the value `application/octet-stream`; retrieve accepts that type or
exact `text/plain` because the tested gnome-keyring 50/libsecret 0.21.7 stack
was observed normalizing only the content-type metadata while preserving the
opaque bytes. After the runtime-canary lifecycle, the same native child repeats
the 73-byte and 2,560-byte lifecycle with deterministic opaque values
containing NUL, high-bit, and invalid UTF-8 byte sequences.

The parent generates both runtime-only printable canaries, sends framed bytes
to a same-executable child over standard input, provides an explicit minimal
environment and neutral isolated scratch directory, bounds child execution,
and accepts only fixed output. It checks parent and child arguments,
environment, stdout/stderr, executable, source, and isolated scratch for
canary probes. Parent and independent fixture cleanup both delete only the
exact fixture authority and verify that item absent. A separate controlled
D-Bus query verifies schema-wide integration-test absence after the matrix.

Controlled direct modes separately prove typed unavailable with no desktop
session bus, typed locked with a truly locked default collection, and
rejection of a hostile dependency-level `SECRET_SERVICE_BUS_NAME`. Production
does not read or mutate that variable: the adapter opens with the literal
canonical service argument and `SECRET_SERVICE_NONE`, rejects any opened proxy
whose exact name is not `org.freedesktop.secrets`, and establishes a session
only after that check. The live hostile owner observed only proxy-property
inspection and no `OpenSession`, collection, or credential-item method.
Prompt overrides deny interaction and make one bounded, no-auto-start
`Dismiss` when given a prompt. Source boundaries prove the absence of unlock,
prompt display, collection creation, alternate-vault, plaintext, settings,
SQLite, environment, file, process, network, logging, and UI fallbacks; the
runtime fixture alone does not claim arbitrary user-home or system-log
inspection. Fake-backed operation failures and direct native-status
classification exercise access-denied and failed-delete terminals without
claiming a desktop policy can reliably force those two states.

The historical BPR-0024 v1 fake contract and all three native fixture CTest
cases passed in an Ubuntu 26.04 x86_64 full-system QEMU VM with an emulated
CPU, Linux 7.0, Qt 6.10.2, libsecret 0.21.7, and gnome-keyring 50. The four
focused cases ran under Xvfb, a desktop D-Bus Secret Service session, and a
dedicated real user; the controlled unavailable, locked, hostile-redirection,
and cleanup checks also passed. This directly verifies the historical v1
Linux vault stack in a full-system VM, not bare metal. BPR-0026 separately
verified the Linux internal native HTTPS wire surface in a full-system Ubuntu
26.04 QEMU guest.

BPR-0029 changed the current fixture to the disjoint v2 schema and
hostname-bound authority. Production has no v1 read, enumeration, migration,
or fallback. A separate BPR-0029 run in an Ubuntu 26.04 x86_64 full-system
QEMU guest passed the current v2 setup/lifecycle/cleanup cases 3/3,
independent exact cleanup, and hostile, unavailable, and locked cases. Both
composed public-path fixtures also passed on that pinned BPR-0029 snapshot.
This v2 evidence is not inferred from or substituted for the historical v1
result.

## GitHub CLI process and trust fixtures

The BPR-0025 compiled-child fixture launches its own canonical executable
through the production `QProcess` runner. Helper modes directly inspect the
element-wise argument vector, requested child-environment allowlist, standard
input, and canonical directory currently outside repository ancestry. A
random parent environment name/value unknown to production is absent in the
child. Raw macOS inspection
allows and separately verifies only Darwin's deterministic
`__CF_USER_TEXT_ENCODING` addition; raw Linux inspection requires the
requested fact and explicit locale with no additions. Exact limit and
limit-plus-one cases cover input, retained output, error, duration,
cancellation, nonzero exit, crash, simultaneous stream pressure, and
termination/reaping. Every failing terminal exposes only a fixed category and
discards partial standard output and all standard error.

The Unix fixture directly observes closed inherited descriptors, disabled core
dumps, reset signal disposition, and that the child is the new session and
initial process-group leader. It proves clean leader exit, nonzero failure,
timeout, and cancellation signal descendants that remain in that initial
process group while the direct child is reaped. An escape case proves a
descendant that moves to another group survives until explicit fixture
cleanup. The fixture also proves a request whose absolute deadline expires
behind the runner's sole worker never launches later.

A compiled seam proves completion-watchdog quarantine, zero launch for another
request while quarantined, and recovery after the worker resumes. Source and
hostile architecture gates, not a compiled live-process failed-reap fixture,
require the owner-managed worker to retain a live `QProcess` after actual
bounded reap failure and continue kill/reap work without invoking the Qt
destructor inside the completed call.
Allocation-free request-shape validation and pre-cancellation precede
work-item allocation and request copying. Marker-backed N+1 cases cover path,
argument, environment, input, stream, time, and reap dimensions and require
zero launches.
Reads use a fixed buffer and request at most the remaining retained bound plus
one byte. This does not prove a hard cap on Qt's private in-flight pipe
allocation. Windows Job Object descendant containment, private pipe bounds,
and target-native CLI behavior are deferred to RFC-BPR-0015. BPR-0036 retains
the Windows native-HTTPS, vault, and composed-public-path fixtures.

The invocation fixture compiles every selected GitHub.com and GHES 3.18 read
operation. It checks exact REST-prefix removal, literal GraphQL routing,
hostname/method/header arguments, standard-input bodies, fixed hardening
environment, platform credential-session facts, and the absence of arbitrary
environment or caller argument input. Profile, correlation, server, route,
method, header, body, executable, environment, and working-directory drift
fail before the process port is called. The production trust gate is also
tested with a fake successful child: it remains `cli_unavailable` and makes
zero calls.

`tests/scripts/verify_gh_cli_trust.py` is a separate direct selected-binary
fixture. It resolves one absolute `gh`, makes and executes an owner-private
copy, hashes that copy before use, and verifies its digest again after every
probe. It creates a private temporary config and random synthetic startup
token, generates a disposable loopback certificate, and captures only fixed
TLS/redirect categories, counts, booleans, version, and digest. No GitHub
endpoint is contacted, and every child runs from the canonical
non-repository temporary directory. The certificate fixture requires the
selected fixed bad-certificate alert and zero HTTP requests. The `302`,
`307`, and `308` POST redirect fixtures compare the body digest and
authorization value without retaining either, and record whether a distinct
loopback hostname is contacted.

`gh` rejects the fixture's ephemeral `host:port` as a `--hostname`, so the
verification-only invocation retains the synthetic `github.com` startup
hostname while its full URL selects loopback. Official selected-version
source establishes that relative production endpoints use the same Go
client. This is exact-binary behavior characterization, not exact
production-authority proof.

The current selection is the official macOS arm64 `gh` 2.96.0 release dated
2026-07-02. Its downloaded ZIP SHA-256 was
`f23a0c37d963aacc3bed703ccbd59b41c5ca22101fab7f00eb2b7cad23aba463`,
and the extracted binary SHA-256 was
`b1d6c442fde99ca27c04e1e74d624895abe37785f4a3e9e9b684bf7586ce4bc8`.
It produced one bad-certificate alert with zero HTTP requests but followed the
cross-authority `302` once. Its selected origin received the exact 47-byte body
and synthetic authorization; the distinct sink received neither. The `307`
and `308` origins received both, while their sinks were not observed and
therefore remain unproven rather than accepted as redirect rejection. The
installed macOS `gh` 2.95.0 binary, SHA-256
`798882434e7f6ae5846194191263ecc59d56bc201f13f016270f44cb4f34499e`,
was rerun and produced the same material outcomes.

The fixture separately proves that persisted `http_unix_socket` is an active
transport setting. For both selected binaries it verifies that the private
configuration was loaded, then observes one exact authenticated `gh api`
request through an AF_UNIX sentinel with the expected synthetic Authorization
value. This evidence must not be reduced to a telemetry concern. The temporary
executables and configurations are verification-only and removed after the
probe; production never admits `GH_CONFIG_DIR`.

Selected command/source inspection records that `gh api` exposes no
per-invocation `--user`, `gh auth token --user` reveals credential material,
and `gh auth switch --user` mutates persistent active-account state. The
selected source also resolves `ActiveToken` per round trip, falls back from a
keyring error to a blank-host token, and launches `/usr/bin/security` through
the macOS keyring implementation. Production path/name validation cannot bind
an eventually launched image to the exact copied-byte digest across
replacement or update; CA paths cannot bind later-opened contents; working
directory identity is not bound through launch; Unix descendants can escape
the initial process group; and reused numeric PID/process-group identities can
receive later signals.

Milestone one keeps native HTTPS as the sole production authentication
transport. The fixed CLI gate reports eight typed reasons: product-scope
deferral, executable-image identity, cross-authority redirect behavior,
ambient configuration, credential/account identity, working-directory
identity, descendant containment, and numeric PID/process-group identity
reuse. A fake successful child still receives zero calls, and the public path
returns `cli_unavailable` with zero process launches. RFC-BPR-0015 owns any
future positive CLI enablement.

## Installed Git local-process contract fixture

`tests/git_contract/git_process_contract_tests.cpp` exercises the BPR-0042
application port and designated `QProcess` adapter through both the selected
real installed Git and same-executable fake-child modes. Construction binds a
canonical executable and matching exec path, builds the environment from
nothing, uses fixed private configuration/home/template/hook/temporary roots,
and runs the SHA-1 and SHA-256 initialization, pack, object, and ref
capability probes. Exact child logging proves the fixed argv, environment,
working directory, and standard input without a shell, generic command, URL,
credential, advertisement, proxy, or network verb surface.

Real-Git cases derive the exact
`incoming/v1/<64-lowercase-hex-operation-id>.pack.part` path, retain a
no-follow verified spool handle across queue delay, reject a swapped symlink
or reparse target, run strict bounded import, require the exact pack identity,
inspect the resulting pack and nonempty index, and reject an unexpected
`.rev`. They also cover bare-repository initialization, commit/tree
inspection, ancestry, atomic exact private refs, invalid repository state,
and SHA-1/SHA-256 object formats.

Hostile cases arm ambient and repository configuration, credential helpers,
askpass, hooks, filters, LFS, submodule recursion, textconv, external diff,
alternate objects, replace refs, protocol handlers, editors, pagers, tracing,
templates, and repository executables. The combined evidence is the hostile
real repository plus the port's exact closed invocation surface; it does not
claim that every excluded mechanism was naturally activated by one Git verb.
Directory symlinks/reparse points, invalid permissions/ownership where the
platform exposes them, final spool replacement, and nonempty configured hook
or template roots fail closed before the affected launch or repository
mutation.

Fake-child cases cover serialized admission, queue expiry, bounded standard
input and retained output, simultaneous stream pressure, deadline,
cancellation, process failure, Unix descendant termination, bounded
two-stage reap, and permanent zero-launch lane quarantine after catastrophic
reap failure. Guarded Windows Job Object, pipe, ACL, junction, and reparse
source is not target-native evidence; BPR-0053 retains that run.

This fixture does not exercise native smart transport, protocol-v2 packets,
cache identity or reuse, durable journaling/recovery, public composition, or
target-native Linux/Windows behavior unless it is run on that target. Those
surfaces remain assigned to later RFC-BPR-0003 cards.

Commit `b65b0a9` extends the same process fixture with exact
filesystem-identity binding, cross-instance process-boundary serialization,
and hostile cleanup/replacement cases. In the focused BPR-0043 component
replay, `git_process_contract` passed together with the four protocol/native
cases. The guarded Windows implementations remain source-only until
BPR-0053 runs them on a real Windows x64 machine.

## App-owned Git protocol-v2 contract fixture

`tests/application/git_protocol_v2_tests.cpp` exercises commit `d903290`'s
Qt-free application client. It validates strict protocol-v2 discovery,
required `ls-refs`, `fetch`, and object-format capabilities; exact SHA-1 and
SHA-256 request forms; the accepted base and fixed pull-request-head refs;
exact advertised OID agreement; deduplicated base/head wants; and bounded
catalog-visible, object-verified haves from the same repository-cache scope.
Cross-cache, catalog-invisible, unverified, mixed-format, and excessive haves
fail before a transport round.

Control replies are complete bounded values. Their decoder rejects an empty
or greater-than-64-KiB reply, malformed/duplicate capabilities, extra,
duplicate, missing, or mismatched refs, and unexpected packet markers. Fetch
uses an incremental parser that is tested with one-byte raw transport
fragmentation. It accepts one `packfile` section, streams sideband 1 into the
pack sink, bounds and discards sideband 2, rejects sideband 3 and every other
channel/section, requires the terminal flush, and aborts the sink exactly once
on transport, parser, limit, cancellation, or sink failure.

`tests/scripts/git_protocol_boundary_tests.py` proves that the complete
`acquire_git_protocol_v2_pack` operation is the only public application
acquisition surface, its raw round helpers have internal linkage, and no other
production file calls raw smart-transport rounds. Two exceptions are named,
each allowed exactly once within its own definition ([[BPR-0850]]):

- the `observed_git_transport` decorator in `github_provider.cpp`, which
  forwards the three rounds through its guard;
- the testing-only refusal probe `verify_history_fetch_refusals` in
  `github_public_path_fixture.cpp`.

The focused
`application_git_protocol_v2` and `git_protocol_boundary` cases passed after
`d903290`. They use a scripted transport and pack sink; they do not prove
native TLS, filesystem cache behavior, durable catalog state, or public
composition.

## Native Git smart-HTTPS loopback fixture

`tests/github_contract/native_git_smart_transport_tests.cpp` and
`tests/github_contract/verify_native_git_smart_transport.py` exercise commit
`edd762e` through the native `QNetworkAccessManager` smart-Git path. The
verifier creates a disposable CA, starts loopback-only CONNECT proxies, and
passes one random credential plus the CA to each compiled child over framed
standard input. No GitHub.com or GHES service is contacted.

The GHES profile runs 19 scenarios: discovery, `ls-refs`, fetch, three
redirect status variants, HTTP 401/403/503, response-header limit, wrong media
type, response limit, consumer rejection, direct-to-proxy fallback, timeout,
cancellation, active-lease revocation, unrelated-lease revocation isolation,
and active shutdown. The GitHub.com profile separately runs discovery,
`ls-refs`, and fetch success. These 22 child runs require exactly one CONNECT
and one origin request apiece. The fixture checks exact route, Host, SNI,
media types, `Git-Protocol: version=2`, and one Basic Authorization value for
`x-access-token:<token>` inside TLS. CONNECT receives no Authorization or
Proxy-Authorization.

The origin distinction is direct evidence: GitHub.com smart Git uses
`github.com`, not its `api.github.com` REST/GraphQL authority, while GHES uses
the exact configured enterprise hostname. Discovery and `ls-refs` response
bodies are retained only within the 64-KiB control limit. Fetch reads are at
most 16 KiB and are delivered through multiple consumer calls rather than
retained as one response body.

Production declares a 15-minute smart total deadline and a 30-second
no-progress idle deadline. The compiled contract asserts the total value, and
the timeout fixture replaces it with a reduced test-only absolute deadline.
The fixture does not wait for the production 30-second idle interval, so its
timeout case is not independent elapsed idle-time evidence. Qt's private
HTTP, socket, TLS, and proxy-discovery allocations are likewise outside the
adapter's accepted/retained-byte claims.

The focused `native_git_smart_transport_contract` and
`native_git_smart_transport_fixture` cases passed after `edd762e`. They prove
the component wire and lease boundary, not the BPR-0044 durable operation
record, the BPR-0045 application-host sequence, BPR-0047 public application
selection, or BPR-0053 target-native Windows behavior.

## App-owned cache and retained-ref fixture

Commit `2801241` adds `tests/git_contract/git_cache_contract_tests.cpp` as the
direct BPR-0043 cache/ref surface. `git_cache_contract` passed 1/1 in 162.58
seconds. It creates real SHA-1 and SHA-256 app-owned bare repositories and
exercises the exact opaque repository path and immutable manifest,
account/profile-revision isolation, configuration and topology drift, true
cross-process writer exclusion, lock and repository substitution,
request-owned receiving/completed spools, bounded pre-import inventory,
credential-free local import, exact pack/index reconciliation, paired
staging/final refs, preexisting-pack reuse, hard quotas, cancellation,
abort/finish races, and exact identity-bound prune/delete/tombstone retry.
Hostile cases require uncertain or substituted files and directories to be
preserved for repair rather than removed by path alone.

The force-push case is chronological: it first imports and retains the old
head, then updates and prunes the fixture remote and directly proves that old
remote commit absent, then imports and retains the replacement. Direct local
object inspection and retained-ref inspection prove the old commit and its
connectivity remain resident in the cache. This is stronger than a
final-state-only remote-absence assertion, but it is not catalog restart or
public-selector evidence.

The adjacent protocol/native/process/architecture CTest replay passed 7/7 in
92.95 seconds. The standalone protocol boundary suite passed 3/3 and the
architecture mutation suite passed 104/104. A tests-disabled Release build
contained zero cache fault-injection markers, and a corrected-snapshot
independent review completed cleanly. The simplification pass made no change
because the explicit validation and cleanup guards encode the safety
invariants.

The completed-spool receipt and prepared pack/index result expose versioned
fixed-size no-path artifact identities, roles, and byte counts for later
persistence. Opaque durable-record or intent tokens in this component test
remain authorization-shape evidence only. BPR-0044 persists those facts in the
catalog component described below. The BPR-0045 host fixture composes these
ports with journal-before-import and restart recovery sequencing; BPR-0047
owns public composition, and BPR-0053 owns target-native Windows execution.

## SQLite epoch catalog fixtures

`tests/sqlite_contract/epoch_catalog_storage_tests.cpp` is the direct storage
fixture registered as `sqlite_epoch_catalog_storage_contract`; its executable
target is `test_better_pull_review_sqlite_storage_contract_tests`. It creates only
disposable synthetic app-data roots. The fixture covers the fixed
`state/v1/epoch-catalog.sqlite3` path, nonzero random incarnation, authority
allocation and reopen, private bootstrap and exact no-replace publication,
interrupted-bootstrap orphan recovery, cancellation, canonical path and local-
filesystem admission, owner permissions and single-link identity, hostile
symlink/reparse and sidecar substitutions, BPEC application/version identity,
the exact 18-table/16-index schema, unexpected-object and integrity failure,
foreign keys, WAL/FULL policy, the 1-GiB main-database ceiling, and SQLite
result classification. The bootstrap fault seam is compiled only when tests
are enabled.

`tests/sqlite_contract/epoch_catalog_behavior_tests.cpp` is registered as
`sqlite_epoch_catalog_behavior_contract`; its executable target is
`test_better_pull_review_sqlite_behavior_contract_tests`. It exercises exact
repository/PR authority allocation and restart, append reservations and
never-reused ordinals, history and recovery paging, exact anchors and
availability, association-only review updates, semantic tuple and composite-
scope corruption rejection, phase/evidence/operation-revision compare-and-
swap, nonterminal revision reserve, catalog and logical quota exhaustion with
zero mutation, sustained WAL checkpointing, exact prune and cache-delete sets,
abort lifecycle restoration, tombstoned-cache repair, supersession and repair
provenance, and repeated cache-delete/reparent/retention-rehydration cycles.
Prune coverage admits only `available`, `unavailable`, or `corrupt` starting
anchor states, persists their exact base/head expectations, rejects revision or
availability drift on begin/replay/finalize/restart, atomically marks both
anchors and linked non-destructive review associations `pruned`, and proves the
retained quota is released exactly once across replay.

The rehydration case preserves the old cache-delete tombstone, permits one
semantically corresponding replacement with a nonzero host-supplied 256-bit
source-observation identity not previously published in the catalog incarnation
only while the requirement is set, and
proves that finalization clears the requirement across restart. The fixtures
use deterministic identities: they prove persistence and rejection of an
already published identity, including the prior epoch's identity, not entropy
generation. BPR-0045 adds recovery-before-mint ordering, production operating-
system CSPRNG ports, and a fresh retry after a catalog-reported collision.

The adjacent `application_epoch_catalog_contract` validates the bounded port,
operation algebra, evidence requirements, durable artifact shapes, destructive
availability restrictions, and rehydration cause. `core_epoch_policy` directly
tests deterministic retention-rehydration planning in addition to the existing
cause, alias, exact-pair, stale-completion, and overflow tables.

On macOS, a fresh `dev` matrix passed 58/58, the focused
core/application/storage/behavior matrix passed 20/20 across five repetitions,
the AddressSanitizer/UndefinedBehaviorSanitizer matrix passed 4/4, and the
Qt-free `core-only` matrix passed 25/25. A tests-disabled Release build retained
the pinned static SQLite dependency and contained no catalog fault-injection
seam. Tracked and on-disk GitHub Actions workflow counts were 0/0.

A clean MinGW cross-build compiled and linked 47/47 targets. Its CTest
inventory listed 25 tests, exactly two labeled `BPR-0053` and
`target-native-windows`; no Windows executable was run. Windows-only live-
namespace, sidecar, TokenUser/DACL, reparse, hard-link, delete-pending, and
retained-handle cases remain BPR-0053 target-native work.

These fixtures supply synthetic exact ref/object outcomes to the catalog; they
do not themselves perform or recover real Git mutations. The BPR-0045 host
fixture composes their durable facts with the installed-Git/cache capabilities
described earlier; BPR-0047 owns selection through the public BetterPullReview
composition, BPR-0055 composes maintenance authorization/recovery, BPR-0056
completes history paging through the host/projection path, BPR-0046 owns its Qt
consumption, and BPR-0053 owns all native Windows behavior. No live private
repository, GitHub mutation, or GitHub Actions workflow is involved.

## Epoch application-host fixtures

`tests/application/epoch_application_contract_tests.cpp` is registered as
`application_epoch_contract`; its executable target is
`test_better_pull_review_application_epoch_contract_tests`. It proves that
`accepted_pull_request_handoff` carries exact accepted authority, lease, and
provider identity without a caller-supplied source-observation identity or
clock value. It also exercises bounded invalidation and the production runtime
ports: nonzero operating-system entropy and a system-clock value within the
core UTC domain. The entropy port uses `getentropy` on macOS, `getrandom` on
Linux, and system-preferred `BCryptGenRandom` on Windows. Only the current host
is exercised by a native run; BPR-0053 retains Windows-native verification.

`tests/application/epoch_application_host_tests.cpp` is registered as
`application_epoch_host`; its executable target is
`test_better_pull_review_application_epoch_host_tests`. Deterministic capability
fakes and separate manual effect/decision executors prove recovery before
source mint and core acceptance, fresh collision retry, journal-before-import,
the exact durable phase sequence, direct commit-object inspection, and exact
restart continuation. The matrix covers no-op and association-only paths,
same-repository FIFO with exact PR cache-session switching, stale load,
preparation, and comparison terminals, pre-PONR cancel and abort, post-PONR
mandatory success, applied-then-error postcondition resolution, recovery
mismatch-to-repair, decision-lane-only projection, projection-exception
containment, deferred reentrant ingress without recursive publication, and
shutdown quiescence. BPR-0055 adds exact core-authorized prune/cache-delete
composition, replay-stable durable authorization, session-close-before-delete,
incremental exact-ID/bulk availability projection, cancellation on both sides
of the durable point of no return, every maintenance restart phase, and
fault-injected lookup/ref/quarantine/catalog/cleanup failures. BPR-0056 adds
bounded fake-catalog pages and records the
exact revision-bound request. Its manual-executor cases cover empty, one-row,
multi-page, exact-bound, failure, cancellation, catalog-revision drift, and a
loaded terminal deliberately reordered after source replacement. The matching
core table covers invalid and skipped cursors, duplicates, bounds, malformed
row order/identity, authority/source/generation drift, retry, and catalog
reload. Loaded projections contain stable sanitized rows and never request a
model replacement.

BPR-0068 adds the residency cases. The fixture constructor accepts an
`application::epoch_host_limits` value, so a case can inject a small job-ledger
or ordinary-ingress capacity and then exceed it while effect and decision
terminals are withheld. The matrix covers refresh, blob-bridge, executor-refusal,
FIFO, invalidation-priority, and mixed-shutdown floods; it proves the
core-request `quota_exceeded` terminal, the blob generic-host failure outcome,
lease cleanup, fail-closed retained-cache authorization, projection depth one,
reserved cleanup admission, and shutdown quiescence. The manual executors expose
job, actor-queue, decision-completion, and ordinary-ingress counts directly, and
no Qt or application process is involved. Only these two capacities are driven to
overflow. The 100 prepared observations, 1,000 retained repository actors,
1,000-operation recovery scan, and 32-attempt identity retry are compile-time
constants enforced by an admission refusal at their single call site.

BPR-0086 adds the deterministic allocation-failure seam. Under `BUILD_TESTING`
the application library compiles `BPR_APPLICATION_TESTING`, which exposes
`bpr::application::detail::testing::fail_next_epoch_job_admission`. Arming it
selects one of the three recoverable allocation boundaries inside job
admission: the `job_control` allocation, the `decision_task` type-erasure
allocation, and the `job_ledger` growth. The next admission reaching that
boundary throws `std::bad_alloc`, and the arming is consumed exactly once, so a
single arming cannot leak into unrelated later work. Each boundary is verified
to leave no partial job, actor entry, or scheduled effect; to revoke the
rejected offer's lease and leave the active authority unchanged; to report
exactly one `unavailable` diagnostic; and to still reach shutdown quiescence.
The public limit-validation table now also rejects zero deferred-ingress
capacity and values above both hard maxima while admitting both maxima
themselves. The two seam symbols are listed in the tests-disabled Release
forbidden-byte contract, and that contract passes, so the seam cannot reach a
production binary.

The adjacent `git_process_contract`, `git_cache_contract`, and SQLite storage
and behavior contracts provide the real physical and durable operations used
by that fake-port orchestration. Together they are direct component evidence,
not proof that the public executable selects the composition. BPR-0047 retains
that public path, BPR-0048 retains the broad combined fault/reordering matrix,
BPR-0055 completes prune/cache-delete maintenance orchestration, and BPR-0046
retains the Qt history trigger and row model. BPR-0056 completes paging beyond
the catalog port. These fixtures use disposable
local data and no live private repository, GitHub mutation, or GitHub Actions
workflow.

`tests/git_contract/epoch_maintenance_integration_tests.cpp` is registered as
`epoch_maintenance_integration`; its executable target is
`test_better_pull_review_epoch_maintenance_integration_tests`. It is a real
component-composition fixture: the production epoch host drives the production
SQLite catalog and installed-Git cache while a deterministic smart-protocol
transport supplies one fixed valid SHA-1 pack. In separate disposable roots it
dispatches the real core prune and whole-cache-delete intents, drains the
serialized host lanes, shuts down, and reopens durable state. It requires the
pruned catalog tombstones and exact paired-ref absence to agree; for cache
deletion it requires `cache_deleted` anchor tombstones, exact repository
absence, a terminal deletion tombstone, and refusal to reopen that cache
scope. It does not contact GitHub, use credentials, mutate a remote, or invoke
GitHub Actions.

## Native HTTPS loopback transport fixture

`tests/scripts/verify_native_https_transport.py` drives the compiled
`test_better_pull_review_native_https_transport_tests` child through the real
BPR-0026 request/TLS/redirect/cancellation/response path. It creates a
disposable one-day CA and leaf certificate for the canonical synthetic
`ghe.bpr.invalid` authority, starts loopback-only HTTP CONNECT proxies, and
requires the TLS SNI and origin Host to remain that authority. No GitHub or
other remote endpoint is contacted.

The parent generates a different random bearer token for every scenario and
sends the token and fixture CA to the child through big-endian
length-framed standard input. Tokens are absent from arguments and
environment. The proxy rejects Authorization and Proxy-Authorization on
CONNECT, then compares the exact `Bearer` value only inside the encrypted
origin request. The fixture also checks the exact method, path, body shape,
Host, `Accept-Encoding: identity`, and `Accept-Language: *`. Child output is
bounded and must be one fixed pass line with empty standard error; the parent
scans both captured streams for the live token and overwrites its mutable
token/frame buffers. Python, Qt, TLS, and socket internals can make immutable
or private copies that do not expose precise wiping, so this is not an exact
process-memory erasure claim.

The complete 15-case matrix is:

| Scenario | Direct observation |
|---|---|
| `success` | One CONNECT, trusted TLS origin request, exact authorization, bounded JSON success |
| `proxy-fallback` | A refusing first proxy is tried before the second loopback candidate succeeds |
| `same-redirect` | A REST `GET` follows one query-free path on the exact authority and sends a fresh exact request |
| `cross-redirect` | A distinct authority is rejected without contacting it |
| `graphql-redirect` | GraphQL `POST` redirect is rejected after the initial request |
| `untrusted-certificate` | Omitting the disposable CA produces typed certificate failure with no origin HTTP or authorization |
| `response-limit` | Limit-plus-one response content produces typed response limit |
| `header-limit` | An 8 KiB-plus-one response field produces typed response limit |
| `cancel` | A stop request aborts a deliberately stalled in-flight operation |
| `timeout` | The remaining absolute deadline aborts a deliberately stalled operation |
| `pre-cancel` | Cancellation wins before any proxy, TLS, HTTP, or credential network effect |
| `http-401` | Status maps to typed authentication |
| `http-403` | Status without rate-limit facts maps to typed permission |
| `http-429` | Status plus bounded `Retry-After: 7` maps to typed rate limit with seven seconds |
| `content-encoding` | A compressed-content declaration is rejected as malformed |

Every networked bootstrap scenario also attempts a replay and requires fixed
unavailability, proving that a claimed bootstrap credential is one-shot.
The untrusted-certificate server may observe a completed TLS 1.3 server-side
handshake before the client rejects trust, or it may observe a handshake
failure; in both allowed observations the client sends no origin HTTP bytes or
authorization. The fixture therefore proves peer-verification rejection, not
one particular server-side handshake timing.

The fixture's adapter-private configuration is compiled only when
`BUILD_TESTING=ON`. It replaces production system-proxy discovery with one
or two ordered loopback proxies and may augment system roots with exactly one
fixture CA. A separate tests-disabled build must contain no
`make_native_https_*_transport_for_test` symbol. The fixture directly proves
candidate fallback and wire policy, while `--runtime` separately reports Qt
TLS support/backend and the first raw system-proxy candidate kind/count. The
runtime count is not proof that every returned system candidate is reachable.

Both the compiled contract and all 15 loopback scenarios passed on the
directly exercised macOS arm64 host. Runtime facts reported Qt Network 6.11.1,
OpenSSL, TLS supported, and one direct candidate. A tests-disabled dependency
audit found no test-only factories, retained the GitHub adapter's Security and
CoreFoundation dependencies, and observed the current ad-hoc linker
signature. This is internal wire and development-runtime evidence, not
composed-provider or package certification. BPR-0029 supplies the separate
public application fixture on the directly exercised platform.

The Linux matrix passed in an Ubuntu 26.04 x86_64 full-system QEMU guest with
Linux 7.0.0-28, GCC 15.2, and Qt 6.10.2. Its frozen
`native_https_transport.cpp` SHA-256 was
`fd7976b48241795302ded4d1a73c851064edce66364a3b50b990294ca95ab134`;
the frozen harness SHA-256 was
`bd0f771cd261a7fb6b4f1f4691af93c12adc7d4fe7ced96cb1182ee8bf2369dd`.
The contract and fixture passed 2/2 in 0.12/11.29 seconds, with all 15
scenarios directly passing. Both architecture tests passed 2/2 in
5.00/144.35 seconds with all 92 hostile mutations. The uninstrumented
focused suite passed repeatedly, and
runtime facts were
`native-https-runtime:tls=openssl:proxy=direct:candidates=1:supported=true`.
The ELF directly needs Qt6Network and Qt6Core. A controlled dependency trace
of only `success` under `strace` opened
`qt6/plugins/tls/libqopensslbackend.so`, `libssl.so.3`, and `libcrypto.so.3`.
A full-suite `strace` is not evidence: tracing perturbs cancellation timing,
so functional evidence comes from the uninstrumented passes and the
single-success trace proves only dependency selection. Tracked and on-disk
scans found no GitHub Actions workflow file, and the source/build
configuration selected no GitHub Actions integration.

Windows execution remains BPR-0036 work and is nonblocking for all other
slices and later-RFC work; it blocks only affected Windows claims and final
RFC-BPR-0002 Auto-Verified closeout.

BPR-0030 extended the verifier to 32 scenarios on 2026-07-30 without
changing the frozen 15-case BPR-0026 evidence above. The added `same-query-redirect`
scenario returns a same-authority REST `Location` with a nonempty query and
requires a typed redirect failure after exactly one CONNECT, TLS handshake,
origin request, and exact Authorization observation. Any attempted second hop
violates the fixture counts and request-target contract. The complete
32-scenario verifier passed on the directly exercised macOS arm64 host that
day.

The `default` suite has held **35** scenarios since 2026-08-12, when
[[BPR-0263]] added `mutation-witness`, `mutation-witness-failure`, and
`mutation-proxy-no-fallback` for indeterminate comment recovery. Every
32-scenario figure in this catalog and in the RFC-BPR-0002 ledger records the
BPR-0030 run and is not the present list; the count grew, so each of those
runs is a subset of what the verifier does today. The live number is whatever
`SCENARIOS` in `tests/scripts/verify_native_https_transport.py` holds — read
it there rather than from a sentence with a date attached to it.

The transport's 30-second deadline and prompt cancellation cannot preempt
synchronous Qt system/PAC proxy discovery or system-root enumeration.
Response/header limits bound bytes accepted and retained by the adapter, not
Qt's private HTTP parser, network/TLS buffers, or pre-metadata allocation.
`QNetworkReply` may also normalize or collapse duplicate response headers,
including `Location`, before exposing raw header pairs and redirect
attributes; policy enforces the metadata Qt exposes, not an exact count of
on-wire Location fields. Likewise, the explicit Authorization buffer is
overwritten but Qt-held copies cannot be exactly wiped. GraphQL
HTTP-200/body-defined secondary-rate-limit interpretation and `ListChecks`
expected-`HeadOid` binding are exercised by the BPR-0027 provider harnesses
below.

The loopback matrix directly exercises the response-body limit and one
8-KiB-plus-one header-field limit. It has no separate 65-field-count or
aggregate-header-only scenario; those two adapter checks remain
source/contract evidence.

## Application host and provider harnesses

The BPR-0027 pager suite drives synthetic bounded wire pages through the
production pagination and normalization code. REST cases cover canonical
`Link` progress, repeated and malformed continuations, exact authority,
per-page and aggregate count/byte limits, expected totals, and atomic
failure. GraphQL cases cover every selected connection, independent cursors,
missing/repeated/non-progressing cursors, capability-shaped field omission,
repository and pull-request identity, expected head OID, and completeness.
No case contacts GitHub or contains private repository content.

The provider suite uses injected standard-C++ transports and an effect
executor to exercise profile resolution, one-shot bootstrap `/user`,
account-bound `/user`, capability probing, full pull-request assembly,
cancellation, stale completion, shutdown, submission failure, and exact
completion correlation. It validates GitHub.com-required and GHES-optional
resource tuples, independent `core`/`search`/`graphql` accumulation across
pages, bounded `Retry-After`, status/body-aware primary and secondary limits,
and structured GraphQL `RATE_LIMITED` errors.

The application host suite uses fake provider/vault ports and the manual
decision lane. It reorders and duplicates terminals; switches profile and
account during provider, store, retrieve, and delete phases; exhausts retries;
injects projection exceptions and repeated reentrant feedback; and proves
FIFO generations, terminal-priority shutdown, exact staged-snapshot
acceptance, and stale-result suppression. Credential cases establish bindings
only through a real strict bootstrap observation, prove exact A-to-B cleanup
while A retrieval is active, retain cleanup tombstones, reject caller-selected
account authority, and prove an independently populated capability cache
cannot enter the production host.

Catalog sharing across host fixtures proves same-process recreation only.
BPR-0029 adds and directly tests the public host's
`delete_retained_on_shutdown` policy: the exact delete terminal is required
for quiescence, and failure retains the tombstone. There is still no
restart/durable-selector or enumeration fixture; a crash can leave an item the
next process cannot recover. Windows target-native execution remains in
BPR-0036.

## GitHub Qt presentation harness

The BPR-0028 Qt harness feeds synthetic provider-free session and pull-request
projections into the real GitHub window and conversation model. It covers
profile-only identity input, read-only authenticated-account output, semantic
Connect/Open/Refresh intents, Qt-local review-thread focus, command-owned
action state, all status and diagnostic categories, capability tri-state,
rate-limit presentation, identity invalidation, refresh retention, new-target
clearing, selection restoration, plain-text hostile markup, accessibility,
focus stability, and minimum-size scroll behavior.

Reviews, review threads, nested review comments, and pull-request comments use
distinct stable IDs. The model cases require exact insert, remove, minimal
move, and data-change notifications without reset; both reorder directions
must produce the requested full order and preserve persistent indexes.

BPR-0028 recorded a direct adversarial run covering 3,000 mixed cases, 5,000
arbitrary 100-row permutations, and both 50,000-row rotations. That result
remains historical BPR-0028 evidence; it was not a standalone checked-in
property target in that slice. BPR-0030 checks the corpus into
`test_better_pull_review_qt_github_conversation_model_property_tests`, registered
once as `qt_github_conversation_model_property`. Fixed seeds make the 3,000
mixed projection updates and 5,000 permutations reproducible. The target
requires exact tree/order and every public projection role, deletion-aware
display/accessibility invariants, deterministic variation of every mutable
role on preserved rows, LIS-minimal permutation moves, exact changed-row
counts, insert/remove coverage, correct persistent-index preservation or
invalidation, one move for each large rotation, and zero model resets.

Scale-specific CTest cases render at 1.0x, 1.25x, and 2.0x and save
minimum-top, minimum-lower, and normal-size PNGs beneath the ignored build
tree. Headless Linux selects Qt's basic Windows style after a direct Qt 6.10.2
backtrace identified a fractional-scale offscreen Fusion cached-painter fault;
native-style real-application verification remains a separate surface.

BPR-0031 extends the same harness without adding a provider or effect. It
directly asserts the `disconnected`, `connecting`, `empty`, `loading`,
`error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
`loaded-cancelled` presentation states; collapsed disclosures and keyboard
order; narrow and wide splitter layout; retained content after refresh failure
or cancellation; and empty first-load failure and cancellation. In addition
to the minimum and loaded-success rasters, each scale saves representative
`disconnected`, `loading`, `first-load-error`, `session-cancelled`,
`first-load-cancelled`, `retained-refresh-error`, and
`retained-refresh-cancelled` PNGs. These transient files are inspected for
clipping, overlap, hierarchy, and retained-state behavior.

Conversation rows use a nonuniform wrapped delegate. Tests require literal
multiline remote text, increasing height at narrower widths and larger fonts,
nonoverlapping rows, native palette/style/font inheritance, zero model resets,
and preservation of current selection, keyboard focus, and an explicitly
collapsed thread through accepted refresh and controlled application
palette/font changes. A scoped accessibility handler requires one combined
polite status-plus-diagnostic announcement and suppresses identical repeats.

The harness contains no credential, provider port, transport, URL, request,
or private repository data and performs no GitHub effect. Its forced
offscreen style remains deterministic layout evidence only; it cannot certify
native operating-system appearance integration.

## Epoch comparison Qt harness

`tests/adapters_qt/epoch_comparison_control_tests.cpp` is the BPR-0046
provider/Git/filesystem-free projection consumer. Its stable model expands
each delivered epoch into exact base/head anchor choices and proves page and
catalog insert/remove/move/data-change signals, persistent indexes, exact
per-epoch `pruned` and bulk `cache_deleted` values, and zero routine resets.
The widget cases drive both selectors, all three named comparison actions,
bounded history paging, refresh/cancel actions, reverse and identical pairs,
a distinct current-viewer review anchor, unavailable/no-fallback state, and
complete 40- and 64-character OID presentation. Loading, retained refresh,
empty, failed, canceled, pruned, cache-deleted, and never-observed states are
asserted as visible text.

The same executable checks native focus retention, label/buddy and accessible
name/read-only surfaces, one bounded deduplicated announcement, and a live font
change without a model reset. It then publishes the control into the real
loaded `github_window`, interacts with the actual widgets, and renders the
complete composition. CTest registers `qt_epoch_comparison_scale_100`,
`qt_epoch_comparison_scale_125`, and `qt_epoch_comparison_scale_200`; PNGs stay
under the ignored build tree. These offscreen runs prove deterministic layout
and interaction only. BPR-0049 retains target-native system Light/Dark UX,
BPR-0047 retains production host wiring, and BPR-0053 retains real Windows UI
execution. No GitHub endpoint, mutation, credential, private repository, or
GitHub Actions workflow is involved.

## GitHub public application composition fixture

The BPR-0029 test-enabled executable supports
`--github-fixture github-com` and `--github-fixture ghes-3.18`. Each choice
selects one profile whose authentication mode is native HTTPS and uses the
same concrete profile-selection, QAction, provider, host, core, and Qt
composition as the interactive application. The test-enabled branch
substitutes a recorded transport factory and fixture vault; it therefore
verifies authenticated-viewer observation, capability probing, complete
pull-request assembly, application-host staging, exact core acceptance,
incremental rendering, and orderly shutdown without making a real native
network or native-vault lifecycle claim.

The fixture transport separately records bootstrap and stored-account reads.
Its vault retains the complete credential operation context and accepts
retrieve/delete only when the exact request, application session, identity
generation, profile, profile revision, host identity, canonical hostname,
account, and server contract match. Success requires the recording and
capability fixture to be consumed, both transport selections to be observed,
the credential to be stored and retrieved, the exact shutdown delete to
complete, and no credential to remain in fixture-vault state. Output contains
only fixed markers and bounded structural evidence:

```text
PUBLIC_PATH_OK
GITHUB_FIXTURE_OK:github.com
RENDER_OK
```

or the corresponding `ghes-3.18` marker. PNGs stay under the ignored build
tree. Headless Linux additionally selects Qt's basic Windows style. These
rasters prove deterministic composition and layout, not native light/dark
following or runtime appearance switching. BPR-0031 separately records native
system-Dark startup, controlled application-local response, and the completed
owner-authorized native gate in which one real process started under system
Light and followed an operating-system-global transition to Dark.

Since [[BPR-0909]] no server card carries a precomposed GitHub CLI profile;
the CLI is the discovered Use GitHub CLI entry. The `--self-test` checks once,
before any gesture and pressing nothing, that no card carries a CLI profile
and that the entry is offered exactly where the composed run offers the read
([[BPR-0982]]): `CLI_ENTRY_ABSENT_OK` inside the automated test boundary,
`CLI_ENTRY_OFFERED_OK` in a tests-disabled macOS run. The default set reports
`CLI_ENTRY_CARDS:1`; the explicit GHES case
`application_public_path_ghes_cli_unavailable` reports `CLI_ENTRY_CARDS:2`.
Provider counters independently require zero
transport-factory, wire, capability, or CLI-specific effect calls after the
provider receives each viewer request, and the CLI transport contract proves a
fake successful process port receives zero calls. Public selection itself
performs one bounded noncredential profile-resolution effect per row from
clean start. Host lifecycle coverage separately proves that switching from an
authenticated native profile first exact-deletes the predecessor credential
binding and does not enroll or retrieve a credential for the CLI profile.
Native HTTPS remains the sole milestone-one production authentication
transport.

The fixture implementation, command-line parser branch, vault, recordings,
paths, labels, and evidence strings exist only when `BUILD_TESTING=ON`.
`application_composition_contract` creates a fresh tests-disabled Release
tree and inspects its public header, compile commands, build graph, and raw
application bytes. It fails if the fixture translation unit, test
definitions, fixture argument/status strings, recorded label/identity, fixture
paths, or fixture-access symbols survive. It also rejects CLI process-runner
factory symbols and requires production to compose only the native wire
transport factory. This proves structural production selection; the separate
native HTTPS loopback fixture above supplies direct wire evidence.

The macOS current-tree public fixture runs passed for both profiles and
reported the fixed success markers above through the substituted fixture
transport/vault. The pinned BPR-0029 Linux v2 run passed both recorded profiles
with the exact host labels and rendered state in an Ubuntu 26.04 x86_64
full-system QEMU guest. That snapshot's fresh tests-off Release build completed
36/36 targets, omitted the fixture seam, reported `PUBLIC_PATH_OK` and
`NATIVE_PLATFORM_VAULT_SELECTED:true`, and passed dependency, runtime, and ELF
audits. Separate target-native HTTPS and Secret Service fixtures supply the
wire and native-item lifecycle evidence. These Linux PNGs remain
layout/composition evidence only, not presentation-polish or system-theme
certification. Windows public-fixture execution remains BPR-0036.

`application_epoch_maintenance_public_path` extends the selected test-enabled
executable with a four-process, persistent fixture-root contract. It seeds two
retained epochs through the recorded GitHub path, triggers the real Qt prune
action and confirmation, restarts to prove exact retained-ref absence and
durable `pruned` state, triggers the real whole-cache-delete action and
confirmation, then restarts again to prove durable `cache_deleted` state and
the old physical cache scope's deletion marker. The contract requires no
routine model reset, no extra smart/provider traffic during maintenance, exact
typed targets, and orderly shutdown. Its fixed markers include
`EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK` and
`EPOCH_MAINTENANCE_DELETED_RESTART_OK`; all storage remains under the disposable
fixture root and no GitHub or developer worktree mutation occurs. The epoch
catalog and review-ledger database persist under that same root, but this
maintenance course does not use `REVIEW_LEDGER_PRESENT_ON_ENTRY` as existence
proof: the marker reports a receipt hit by the selected-material observation
itself (`MaterialReceiptHits`, [[BPR-0830]]), which the prune branch does not
execute before maintenance. The dedicated
`application_review_ledger_composition_contract` owns exact seed/reopen receipt
load evidence.

### The modality a course states

A pull request named in the Home bar opens on the reviewer's opening modality,
Overview by default ([[BPR-0661]]), and since [[BPR-0771]] that is how every
fixture course opens one. Overview shows the pull request in place of the
comparison and keeps every drawer collapsed. Four courses written against the
older entry path were stranded there and each failed as a stall or a missing
label rather than as "wrong modality": [[BPR-0823]], [[BPR-0858]],
[[BPR-0859]] and [[BPR-0860]] ([[BPR-0863]]).

What each modality shows, once. The product's rule is owned by
`workbench_window::choose_modality` and the core's
`set_workbench_modality_intent`; RFC-BPR-0028's Modalities paragraph is its
normative description.

| Modality | Centre | Files drawer | Conversations drawer | Repositories drawer |
|---|---|---|---|---|
| Overview | the overview page | collapsed | collapsed | collapsed |
| Read | the comparison | collapsed | expanded | collapsed |
| Review | the comparison | expanded | expanded | unchanged |
| Discuss | the conversation, full width | collapsed, hidden | expanded, its panel moved to the centre | unchanged, hidden |
| Threads ([[BPR-1293]], [[BPR-1341]]) | the finder, full width | unchanged, hidden | unchanged, hidden | unchanged, hidden |

- Overview collapses all three drawers, and the core refuses a drawer opened by
  hand until another modality is chosen ([[BPR-0727]]). Read and Discuss
  collapse the files drawer ([[BPR-0674]]); Read collapses the repositories
  drawer ([[BPR-0724]]). Review is the only modality whose files drawer is
  expanded.
- "Unchanged" keeps the reviewer's last choice. A manual toggle overrides a
  drawer until the next modality is chosen.
- With the drawer controls hidden, the default ([[BPR-0721]]), a collapsed
  drawer is hidden and takes no width, so the splitter handle beside it moves
  nothing. With them shown it is an 18-pixel rail.
- While the core offers no drawer command, because no workbench is active, the
  files and repositories drawers present expanded whatever the modality says.

A course that reads the comparison or a drawer states the modality it needs
and the surfaces it needs there in a `course_modality_step`, and advances
`github_application_fixture_access::enter_course_modality` until it answers.
The step presses the strip's public modality action once, as a reviewer does.
It answers `entered` when the window and the host agree on the modality and
on all three drawers, with no host request outstanding, so the drawer store
the modality causes has landed, and every needed surface is shown. Otherwise
it fails by name after five seconds. The bound is time, not ticks, because
the automated run advances the step every 10 ms and the large-review
benchmark every millisecond. These lines are from the negative controls
recorded on BPR-0863:

```text
COURSE_MODALITY_STALLED:wanted=Review waited_ms=5008 wait_cap_ms=5000 waits=482 modality=Overview host_modality=Overview centre=overview offered=none withheld=comparison,files_drawer,conversations_drawer,repositories_drawer ...
COURSE_MODALITY_WITHHOLDS:wanted=Read needs=files_drawer missing=files_drawer waited_ms=5009 wait_cap_ms=5000 waits=483 modality=Read host_modality=Read centre=comparison offered=comparison,conversations_drawer withheld=files_drawer,repositories_drawer ...
```

`STALLED` means the window never reached the stated modality. `WITHHOLDS`
means it did, and that modality does not show what the course needs. Both end
with `course_modality_facts`: the modality in the window and in the host, the
centre, what is offered and withheld, each drawer on both sides, the drawer
controls, outstanding requests and the splitter sizes. `offered=` and
`withheld=` are read from the widgets, not from the table above. A course's
own failure lines end with the same facts, so a course that never entered a
modality still names the one it is in:

```text
DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED:modality=Overview host_modality=Overview centre=overview offered=none withheld=comparison,files_drawer,conversations_drawer,repositories_drawer ...
```

The courses that state a modality:

| Course | Modality | Needs |
|---|---|---|
| `--diff-workbench-probe`, used by `application_diff_workbench_public_path` and the appearance theme-customization and store-fallback contracts | Review | comparison |
| `--workbench-composition-probe`, before its files-drawer drag | Review | files drawer |
| `--uiforetw-selected-diff-evidence`, before its first file selection | Review | comparison |
| the large-review benchmark's ordinary, `--unavailable` and `--approximate` courses | Read | comparison |

## GitHub hosts

- GitHub.com current contract
- GHES 3.18 REST and GraphQL contract recordings
- Missing GraphQL capability
- Permission denied
- Rate limited and secondary-rate-limited responses
- Proxy and corporate CA configuration
- Stale head during mutation

## Pull request histories

- Single upload, simple edit
- Multiple uploads with unchanged files
- Force-push/rebase with retained old head
- Base branch advance without head movement
- Base branch change
- Pure rename
- Rename with edits
- Added/deleted/binary/submodule files
- Review associated with historical commit

## Diffs

RFC-BPR-0004's complete semantic, measured-geometry, input, lifecycle, and
scale fixture definitions are in the dedicated
[diff and scroll fixture catalog](diff-scroll-fixture-catalog.md). Its final
rendered session follows the
[manual interaction protocol](RFC-BPR-0004-manual-visual-verification.md) and
must use the real application or closest deterministic composed harness; a
static mockup or isolated painter does not satisfy the interaction claims.

Run only the selected `qt_diff_workbench_scale_*`,
`qt_diff_link_map_scale_*`, and `application_diff_workbench*_public_path`
CTest cases for the composed RFC-BPR-0004 evidence. They are intentionally
serial and headless; the scale rasters and fixed public-path markers must
agree before the evidence is accepted. Native System-theme and presentation-
polish claims remain outside this deterministic fixture boundary.

- Replace, insert, delete, and mixed hunks
- Unequal old/new group heights
- Multiple nearby groups
- Long lines and tabs
- Unicode and invalid byte replacement policy
- Multiline comment/string crossing hunk boundary
- Whitespace-only policy changes
- Large generated file and highlighting fallback

## Conversations

- New thread, replies, edits, deletion, resolution, reopening
- Outdated anchor
- File-level comment
- Pending review drafts
- External-client mutation between refreshes

## Local state

- Read and acceptance invalidation
- Account/repository isolation
- Schema migration
- Abrupt termination and WAL recovery
- Deleted local database

## AI

- Codex one-shot JSONL
- Codex App Server session and approval events
- Claude stream-json
- External terminal fallback
- Head changes during analysis
- Invalid finding anchor
- Corporate host mapped to forbidden personal provider
- Structured finding payload on each carrying protocol, and no carrier on the
  external terminal
- Read-only MCP context helper over a frozen snapshot root

`tests/fixtures/agent/agent_canary_main.cpp` is the sanitized BPR-0240 process
boundary fixture. It performs the App Server initialize/thread/id-2-turn
exchange and emits a correlated `turn/completed`; Codex JSONL emits its
thread/turn lifecycle; Claude emits `system/init`, assistant, and result; and
external mode emits text. Frames are deliberately split into three-byte
writes. A preemptive App Server mode emits the complete otherwise-valid
id-0/id-1/id-2/delta/completed transcript after reading only `initialize` and
before reading either host followup; the launcher must refuse that transcript
as a causal protocol violation. Modes also add 256 KiB of stderr pressure,
malformed JSON, structurally wrong or oversized frames, failed/interrupted
remote terminals, nonzero exit, indefinite execution for cancel/timeout, and a
descendant heartbeat.
`qt_agent_launch_canary` inspects the child's full argv, environment,
stdin, partial events, terminal classification, recovery, and heartbeat
cessation; no credential or repository-private fixture content is present.
On Windows the same target first ran green target-native on 2026-08-26/27:
every selected protocol reaches a typed successful terminal through
`windows_agent_process.cpp`, a child whose Job Object assignment fails never
runs its first statement, and a spawned grandchild's heartbeat stops on
teardown (BPR-0240).

`application_agent_invocation_composition_contract` uses that canary as a test
replacement for the immutable external-terminal executable while traversing
the production profile/policy, visible workbench panel, application host,
retained-cache actor, concrete Git materializer, process launcher, and terminal
projection. The child reports its working directory and Git topology; the
contract requires the exact fixture head, absent origin/alternates/hooks/
worktrees, no planted GitHub token, and removal of only the owned workspace
and its exact empty parent root after terminal delivery. The Git contract
replaces the parent root and makes it nonempty in separate courses; both are
retained rather than swept recursively. Construction- and gesture-cut
companions require zero core/workspace/process calls, preventing a fixture-only
bypass from satisfying the positive marker. Cancellation and staleness
companions hold the real child open, then respectively stop its descendant tree
or move the accepted head; both require empty stale output, distinct typed
terminal state, exact cleanup, and quiescence. The positive course also
compares every admitted version-two identity and requires zero note/draft
counts and no coordinate vocabulary.

The cleanup-retry companion replaces the running child's workspace path before
terminal cleanup. The first identity-bound removal refuses the substitute and
keeps the host non-quiescent. After restoring the original directory identity,
composed shutdown requests the actor-confined retry, removes the exact workspace
and empty parent root, reaches its bounded terminal state, and leaves the
substitute untouched until explicit fixture teardown.

### The finding-payload canary

RFC-BPR-0012 gave the same `tests/fixtures/agent/agent_canary_main.cpp` three
more switches rather than a second process fixture, because a finding rides
inside the protocol framing that fixture already speaks. `--emit-findings`
attaches `{"session":<n>,"findings":[…]}` to the frame each protocol actually
carries it on — `params.turn.bprFindings` on the App Server `turn/completed`
notification, top-level `bprFindings` on Codex JSONL's `turn.completed`, and
top-level `bprFindings` on Claude's successful `result`. `--findings-session`
names the session the payload claims, which is what the parent's
unknown-session course provokes a mismatch with. `--findings-transcript`
writes one complete payload-bearing transcript and reads no standard input, so
a parent can replay it through an ordinary pipe; on the external terminal,
which has no result frame at all, it writes a `BPR-FINDINGS/1` sentinel line
instead — a carrier nobody specified, present so the host has something to
refuse as `unsupported_finding_transport` rather than a silence that could be
read as agreement. Every identity in the payload is a fixed repeated hex
digit, so a parent compares it field for field instead of searching output for
a substring, and so the delivered-but-unanchored path is what the ordinary
canary produces — an unanchored finding is the one a reviewer most needs to
still see (BPR-0500, `application_agent_finding_protocol`).

`tests/end_to_end/agent_finding_canary_main.cpp` is the other half, and exists
because the first canary cannot prove it. It reads the `name<TAB>value`
supplement the composed context handed it — the same payload
`encode_agent_supplement` wrote — and anchors its finding at
`selection.preferred`, the exact `core::source_line_ref` the reviewer selected,
so a correctly placed answer validates against the snapshot the application
froze. It speaks Codex JSONL for the reason above. One executable serves seven
registered courses — the positive course, the full-agent course, and one
construction cut per composed host — and each course is a separate `ctest`
selector whose pass marker is its own (BPR-0504,
`application_agent_finding_composition_contract` and its siblings).

### The MCP helper canary

`tests/application/mcp_context_helper_tests.cpp` drives the shipped
`tool_better_pull_review_mcp_context_helper` as a real child over real stdio
rather than calling the library in-process, because what the helper refuses is
a property of the process a stranger's agent would talk to. It links
`BetterPullReview::AdapterMcp` and is compiled with `BPR_MCP_TOOL_SOURCE`
pointing at the adapter's own header, so the advertised list is compared
against three independently sourced ones: the `enum class mcp_tool` text read
out of that header, the compiled `mcp_tools` array, and a fourth list written
in the canary itself — a tool added to the first three still fails until a
person edits the test.

The refusal sweep drives twenty-one entries through that executable — a URL
argument, a `//host:port` authority, a UNC authority, `net/connect`,
`net/listen`, `../../../etc/passwd`, `/etc/passwd`, `../binding`, a real
directory symlink out of the snapshot on both path-taking tools, ten
write/draft/publish/note/network tool names, and a helper started with
`--listen 127.0.0.1:8080` — and requires each to be refused by its own exact
typed cause with no result beside it. The parent holds planted `GITHUB_TOKEN`,
`ANTHROPIC_API_KEY`, and `AWS_SECRET_ACCESS_KEY` while the child runs; the
child's environment reduces to exactly `BPR_MCP_SNAPSHOT_ROOT` and
`BPR_MCP_PINNED_BINDING`, and no secret value reaches either child channel.
With the pin unchanged and the snapshot's binding record moved to a new head,
every `tools/call` answers `revision_binding_stale` and the transcript carries
neither the frozen bytes nor the frozen paths. The registration declares no
`LOOPBACK_PORT_COUNT`: the helper has no socket surface at all, so zero is the
claim rather than an omission (BPR-0501, `application_mcp_context_helper`).

## Architecture and concurrency harnesses

- Core-only build environment with Qt, SQLite, GitHub, Git, AI, and platform provider packages unavailable
- Deterministic event/observation replay corpus for navigation, receipts, defects, queue state, and AI staleness
- Manual executor fixtures that reorder, delay, fail, cancel, and deliver stale completions
- Qt model harness asserting exact insert/remove/move/data-change signals and persistent-index behavior
- QAction projection harness covering enabled, checked, and disabled-reason state across capabilities and selections
- Large-review structural counters proving active-set/changed-set work and absence of routine whole-model resets
- Shutdown fixture with pending GitHub, Git, diff, syntax, store, queue, and AI operations

## RFC-BPR-0005 workbench fixtures

`tests/core/workbench_interaction_tests.cpp` is registered as
`core_workbench_interaction` and runs in the Qt-free `core-only` preset. It
covers cursor well-formedness, rejection of an ordinal past the actionable
count and of an ordinal without a selection, zero-identity rejection,
neighbour-fact consistency, forward and backward stepping, stop versus wrap at
both ends, single-entry and empty orders, and the nonresident-selection case
where only the host can name a neighbour.

`tests/application/workbench_application_host_tests.cpp` is registered as
`application_workbench_host`. It uses the manual-executor idiom so terminals
can be withheld, reordered, and duplicated deterministically, and covers
bounded cursor assembly, changed-domain-only publication, layout persistence
and restore, cancellation, and the terminal rejection algebra: unknown request,
inexact terminal, superseded, and cancelled. Both disjuncts of the exactness
guard carry their own case, since a terminal with a matching domain but a
forged context is the forged-session case the guard exists to catch.

`tests/adapters_qt/workbench_window_tests.cpp` is registered as
`qt_workbench_window_scale_100`, `_125`, and `_200`. It runs offscreen and
serial and covers the toolbar, compact overflow keyboard association, drawer
collapse returning space to the centre measured against the central viewport,
incremental drawer model signals, the composed comparison surface with exactly
one shared vertical scrollbar and no synthetic alignment rows, the full-width
line-over-line detail pane checked against the canonical mapping, epoch
selectors with exact untruncated OIDs, explicit textual states, declared focus
order, and window-wide uniqueness of accessible names and key sequences.

Its large-file case is the virtualization evidence. It presents 60, 6,000, and
60,000 source lines through the same composed workbench and requires the widget
count to be identical at all three: 68 window widgets, 29 surface widgets, and
5 right-pane widgets in every case. The assertions bind the ratio rather than
an absolute, and separately require the source to have grown by orders of
magnitude, so a harness that merely rendered one large file could not pass.
Elapsed time is recorded and deliberately not asserted, because one machine's
timing is not a frame-rate guarantee.

## RFC-BPR-0006 appearance and syntax fixtures

`tests/core/appearance_model_tests.cpp`, `theme_exchange_tests.cpp`, and
`appearance_contrast_tests.cpp` are registered as `core_appearance_policy`,
`core_theme_exchange`, and `core_appearance_contrast`. All three run in the
Qt-free `core-only` preset.

The contrast suite pins the ratio against known values rather than
self-consistency: black on white is 21.00:1, any colour against itself is
1.00:1, the canonical 4.54:1 grey measures 4.54:1, and the three primaries
against white order green, red, blue. Alpha does not change a measured ratio.
Its checked pair set is enumerated — three fixed text pairs, every syntax role
over each canvas background, and each review-state indicator — so the check can
be audited rather than merely trusted.

The theme-exchange suite round-trips a valid document *and* refuses unknown
keys by name, near-miss identifiers, partial documents with each unassigned
role named, duplicate keys, oversized input, and external references. The
external-reference scan runs before any key is interpreted, so a reference in a
key position is refused as a reference rather than as an unknown field.
[[BPR-1479]] adds version 2: the suite round-trips each of the four bases,
refuses a version-2 document without a base, with an unknown or repeated base,
and a version-1 document carrying one, infers a version-1 base from its code
background on both sides of the white/black text crossover, and fills only the
roles a document omits.

`tests/fixtures/appearance/older-theme.bprtheme` is `contrast-safe-theme`
as it stood before RFC-BPR-0072: version 1, black code canvas, no namespace or
enum-member colour and, since [[BPR-1503]], no Find wash.
`application_appearance_older_theme_contract` runs the theme-customization
course over it with `BPR_THEME_FILLED_ROLES` naming `syntax_namespace`,
`syntax_enum_member`, `find_match_background` and
`find_current_match_background` at Dark's built-ins. The theme is kept on the variant offscreen resolves (Light on
macOS, printed as `APPEARANCE_THEME_EXPORT_BASE:light`), and the course
requires the exported document to be `bpr-theme 2`, `base` that variant, every
fixture line and the four filled lines. Before BPR-1479 the commit stage
refused it with the 2.88:1 finding. Both theme courses compare the export
against that shape rather than the imported bytes.

`tests/syntax_contract/syntax_classification_contract_tests.cpp` is registered
as `syntax_classification_contract` and uses `tests/fixtures/syntax/`:
`multiline_comment.cpp`, `CMakeLists.txt`, `notes.md`, `payload.json`,
`records.jsonl`, `script.py`, and `notes.txt`. Each selects its expected
definition from packaged records; an unpackaged name resolves to nothing rather
than falling back.

Its central case asserts that a hunk beginning inside a multiline comment
classifies exactly as the same region of the complete blob. The suite also pins
one classification to one lexical pass, refuses an absent blob before any span
bound applies, refuses rather than truncates a classification above the bound,
and reports cancellation.

## Public-preview composition evidence

The promoted anonymous capture set above anchors real service observations;
synthetic paging/linkage/rate cases exercise limits without claiming a live
service response. `application_anonymous_public_preview_public_path` follows
the composed controls through the normal provider, cache/epoch authority, and
rendered selected-file diff, including exact base/head/path/content, REST comment
groups, mode invalidation, and zero credential/header/vault effects. It does not
prove native live service access. BPR-0316 separately records one successful
Mac native read of the owner-designated public `MikeMarcin/BPR-Test#1`, with nine
HTTP 200 responses and the exact accepted head; no private content or token is
committed as evidence.


## Large-review workload inputs (RFC-0013)

`tests/fixtures/github/pull-requests/large-review-v1/` supplies a wholly synthetic
515-file Git comparison: 512 source files, one 20,000-line comparison, one
8,192-line generated table and one 4,096-byte binary blob. Its 32 nonempty head
revisions share one base; ten bounded pages contain 1,000 comments anchored to
changed source lines. The recipe and manifest record original object identity,
shape, sizes, explicit pagination and artifact digests. Generation never fetches
or executes repository content.

`large_review_fixture_contract` imports the stored pack into isolated local Git,
checks strict fsck, trees, blobs, changes and the exact revision graph, then
checks every comment/page/anchor. Wrong-anchor, duplicate-identity and corrupted-
pack negatives update the corresponding digests so semantic validation has to
reject them. The generator is separate from those acceptance observations.
`application_large_review_benchmark` now serves that stored latest head through
exact paged replay, the production provider/pager, installed Git and real
application selection. It verifies 515 files and 1,000 threads/comments, all
29 account requests, exact 20,000-line source panes and a rendered full window.
It measures change navigation, 1,000 idle native wheel inputs and final-selection
precedence across 32 rapid selections with background work. Glaze writes the
platform/build/Qt/scale, phase/event-loop and host/scroll observations to
`tests/end_to_end/large-review-measurement.json` in the build directory.
`large_review_replay_contract` rejects skipped pages, changed cursors and
noncanonical bodies while retaining strict single-page fixture validation.

`application_rate_limit_render_recovery` (BPR-0968) uses the same 515-file,
1,000-thread workload and safe recorded wire transport. It selects the real
20,000-line comparison through installed Git, selects right-side line 100 and
sets a nonzero scroll position. Its three cases cover primary exhaustion with
Retry-After 2 seconds, secondary throttling with Retry-After 2 seconds, and
primary reset 4 seconds ahead with a shorter Retry-After 1 second. Account →
Details renders each distinct diagnostic. Twenty current-window Refresh and
sibling-window Open gestures per case spend no further request before the
minimum; the third case also retries after the shorter Retry-After expires.
Expiration alone starts no work. Explicit recovery retains the exact repository,
viewer, PR/head, files, threads, comparison key, semantic selection and scroll.
A final sibling is closed while its real provider files-page request is held;
its cancellation drains without altering the surviving window's reading state.

The complete course records 119 ordinary account requests, eight repository
requests (three rate refusals, four successful starts, one cancelled start), and
zero mutation submissions. Request arrival intervals are printed in microseconds.
`BPR_RATE_LIMIT_RENDER_DIR` saves Settings and full-window recovery rasters in an
existing local directory. Every ordinary request still matches the recorded
exchange. The default CTest row now requires backoff; `--require-backoff` remains
a compatible alias. Core and host courses cover monotonic clock changes, both
minimum orderings, fallback/attempt/registry caps, resource/account partitioning,
session retirement, late terminals and transient retries during a sibling delay.
Removing admission or the Last problem mapping fails this same rendered course.
These are macOS offscreen fixture observations, not native appearance, a live
account, other-platform or release acceptance.

`application_offline_inventory` (BPR-0969) runs the real desktop composition in
five processes over the same private data root and synthetic vault pipe. It
uses `profile_window_restoration_v1`, installed Git and a test-only transport
refusal switch under denied network access. The course checks retained live
snapshot/source after failed Refresh, a visible unknown-freshness notice,
changed server-thread adoption on recovery, another-repository failure, Recent
Resume while offline after account validation, and a fully offline account
restart. The driver compares retained Git-pack hashes and exact catalog
authority, material trees and commit anchors after each orderly Quit.
`BPR_OFFLINE_RENDER_DIR` optionally saves synthetic full-window rasters to an
existing local directory. The [inventory](BPR-0969-offline-inventory-2026-09-25.md)
distinguishes unavailable content from empty filtered views, durable Recent
navigation from snapshots, and profile-private Recent titles from account-bound
epoch content. This regression inventories current limits; it does not certify
complete BPR-0969 AC2/AC3 or offline restart content availability.
The catalogue continuation asserts both exact historical file rows and reported
counts after authenticated offline Resume, an inert file activation, hidden
unavailable progress/conversation columns, legacy payload refusal to claim empty,
corrupt-payload refusal and fresh server recovery. Separate host/SQLite/Qt checks
cover changed base/head replacement, rename provenance, observed empty and
bounded omission. See the [catalogue evidence](BPR-0969-file-catalogue-2026-09-25.md).
This exposes no cached source or conversations.


`application_recent_account_isolation` (BPR-0969) extends the same offline
inventory executable through four actual process lifetimes. It generates two
viewers in one profile, drives public Sign out/Sign in, Open and Resume, and
checks both switch directions, settled sign-out, a held old-account completion,
each account's title/destination after restart, offline Resume and unavailable
content, and a v4 database with unknown account ownership. Closed-store reads
check durable account/destination/position rows and unchanged Git packs and exact
epoch authority/anchor rows. Synthetic vault material stays in bounded memory
pipes. The old product fails the account exposure assertion. Offscreen QWidget
rasters are deterministic presentation evidence; native appearance/human
acceptance and complete cached-review mode remain pending. See the
[bounded evidence](BPR-0969-recent-identity-2026-09-25.md).


BPR-0624 records the initial Mac baseline; BPR-0627 tracks its measured UI stalls.
These checks do not establish 32 application epochs, memory/pruning ceilings,
scrolling responsiveness during active work, universal timing budgets or native
Windows/Linux results. The tests run locally with denied network access.


`application_unavailable_diff_public_path` reuses the same stored workload and
the real provider, Git and composition path, with a test-only compute-limits
override. Since [[BPR-0719]] a work limit degrades the alignment rather than
refusing the file, so the course now sets `MaximumSegments` to 2
([[BPR-0858]]). The 20,000-line file needs 40 segments, which yields a real
typed compute-limit refusal. The 2-segment recovery file still computes. The
course enters Read first and then checks:

- the visible, selectable reason;
- the empty source and selection;
- a full-window render;
- a distinct binary-file status;
- exact source recovery when a supported file is selected.

`application_approximate_diff_public_path` covers the degraded path. It disables
the optional ordered index, then requires three things: a text comparison marked
`AlignmentApproximate`, the status "Diff ready — N semantic changes
(approximate alignment)", and exact source in both panes.

Default production limits and live sign-in are unchanged. `qt_diff_workbench_scale_100` additionally checks genuine equal text
and cached unavailable publications. BPR-0626 records the passing Mac result.

## RFC-BPR-0022 semantic publication fixtures

`core_review_round_trip` covers placement/capability/body/state decisions
and illegal observations. `github_review_comment_permalink` composes the real
serialized session and SQLite journal for native, file, exact-anchor cache,
legacy alias and 66 reached interruption courses; each course restarts three
fresh sessions. The original unsupported tuple is explicitly synthetic. The
later classifier2 fixtures exercise the separately owner-approved exact PR2
contract and its refusal cases; fixture success supplies no broader service
capability evidence. `sqlite_review_mutation_recovery_storage` covers schema
1–3 migration into schema 4 with whole-review state preserved; D integration
retains these records in the current schema-5 store.

`application_review_round_trip_composition_contract` begins with the real Qt
line gesture and ends with canonical server-post selection after durable
recovery and ordinary refresh. Its native/file counters, journal reopen and
new-session counts are asserted. `qt_composed_github_scale_100`, `_125` and `_200`
cover semantic selection, collapsed support details, focus and accessibility
in light, dark and high contrast. State-loss reconstruction uses a fresh host
without a database or receipt; it is not an actual uninstall or power-loss test.
The [criterion ledger](RFC-BPR-0022.md) records exact producing candidates.

## Header/source navigation course (RFC-BPR-0074)

`header-source-v1` is a synthetic 16-file PR with cross-folder pairs, exact-folder
priority, closest trailing directories, equal-best ambiguity, unmatched source
and header files, an unsupported Markdown file and an added header absent from
its base. `scripts/build_header_source_fixture.py` writes the construction;
`build_recorded_pull_request_contract.py` and `build_recorded_pull_request_pack.py`
write the committed contract and Git objects. No source is executed or remote data
read. The base is `71dc98a8939742f51fa98dd44936ba727c3e5796` and the head is
`24537f5b9f51511d4ee94c420192f04a972e7548`.

`application_header_source_navigation` drives the real View QAction and window-system
Option/Alt+O through the installed-Git/cache actor and full rendered window. Its
older base/base tab reads base blobs and reports the added header absent from both
versions. `application_header_source_navigation_large` runs 100 real no-match
commands against the existing 515-file large-review fixture. These offscreen
courses establish deterministic composition and input routing, with no claim of
native device or system-appearance acceptance.

## C++ identifier appearance (RFC-BPR-0072)

`cpp-identifiers-v1` is a synthetic one-file pull request, served through the
existing test-build composition. `scripts/build_cpp_identifier_fixture.py`
creates its construction, and the recorded-pull-request contract and pack
builders create its committed HTTP tree and Git objects. It contains uppercase
and mixed-case namespace declarations, scoped and unscoped enums, qualified
namespace/member uses and a defined `GCT_ASSERT` beside them. Base and head
change only the final integer. No recording or remote data is used.

`application_cpp_identifier_appearance` opens it on Overview, enters Review,
opens `review.cpp` through Next file, and closes the drawers through their public
commands so both source panes display the qualified names. It asserts token
layout colors and visible raster pixels in Light and Dark, saves full-window
offscreen renders when `BPR_CPP_IDENTIFIER_RENDER_ROOT` is set, and proves that
repainting retains both span identities and the comparison. All four built-in
palettes pass the 374 published contrast pairs.

The same course uses five fresh OS processes: rendered comparison, loading older
stored palettes and customizing, restart, importing new exports, and another
restart. Existing palette documents omit only the two new fields; old import
files are valid customized themes under the host's existing all-variant contrast
floor. Every old value, mixed stored flag and unrelated typography preference is
checked before editing. Picker edits and old/new imports change only their named
variant, and every new export includes all 62 stable identities. Since
[[BPR-1479]] an old import's two new roles come from the document's base, which
its white or black canvas makes Light or Dark, so the high-contrast variants
receive Light's or Dark's values rather than their own. Stored old palettes keep
the sentinel and their slot's defaults. Explicit unreadable new-role ink remains
a typed refusal in `application_appearance_host`.
