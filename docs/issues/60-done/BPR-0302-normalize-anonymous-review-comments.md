---
schema: "implement-rfc.card.v1"
id: "BPR-0302"
title: "Normalize anonymous REST review comments and unavailable state"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0300","BPR-0301"]
---

# Normalize anonymous REST review comments and unavailable state

## Required final state

- One REST review-comment operation is added to the existing closed read
  contract, paging, fixtures, provider, and normalizer; no parallel path exists.
- Reply linkage reconstructs comment grouping without inventing resolution;
  viewer, viewed state, resolution, and actionability remain explicitly absent.
- The normalizer admits the exact anonymous operation set instead of requiring
  ten responses or hardcoding a full read shape.
- Malformed/cyclic/missing linkage, paging, 404 neutrality, rate limits,
  oversized bodies, and authenticated non-regression have direct tests.

## Lane — 2026-09-02

[[BPR-0300]] and [[BPR-0301]] are terminal, so this card moves to
`20-ready/` as generic work runnable on any station. What they left at this
card's door: `perform_paged_github_read` now takes a
`github_provider_read_authority` and `provider_request_valid` requires the
account transport only under the authenticated authority, so the pager edit
this card makes lands on top of that; an anonymous `fetch_pull_request`
refuses any read shape that is not all-false, which is the shape this card's
availability-aware normalization must produce for the anonymous case; and
`github_anonymous_read_contract` observes only one eligible operation on the
wire because no anonymous snapshot corpus exists yet — the REST
review-comment operation and its fixtures, which this card owns, are where
that corpus starts. Core's `inline_review_comment_observation` already
distinguishes unobservable from observed-none, so the normalizer has a typed
target for "not observable" rather than an empty list.

## What this slice delivered — 2026-09-02

**One REST review-comment operation, on the one path.**
`read_operation::list_review_comments` routes to
`GET /repos/{owner}/{repo}/pulls/{n}/comments` under the name
`ListReviewComments`, with paging, `rest_headers()`, and the strict fixture
validation the other REST operations already have. `all_read_operations`
(eleven) now drives routing, naming, admission, and the anonymous match
tables; `read_operation_catalog` stays at ten and keeps its documented
meaning — the recorded authenticated manifest — because
`src/app/src/github_public_path_fixture.cpp` refuses any manifest whose
interaction count differs from the catalog's, entry by entry, and growing it
would have forced an eleventh interaction into the three shared
`contracts/v1` manifests: exactly the authenticated non-regression this card
requires. `anonymous_read_operations` is the nine-entry eligible set.
`anonymous_read_eligible` admits the new operation, and core's
`anonymous_read_intent` gains its ninth legal intent, `read_review_comments`
(count eighteen; catalog, index, refusal switch, and the "first nine"
comment all moved together).

**Grouping derives from `in_reply_to_id` and nothing else.**
`src/adapters/github/src/github_review_comment_grouping.hpp` holds one
`group_review_comments` shared by the paged provider read and the recorded
contract normalizer, so the two cannot disagree about what a conversation
is. A reply must name a comment present in the same read, must not name
itself, and must name a comment that replies to nothing — which refuses a
reply-to-reply chain and, with it, every cycle. Server order is preserved
within and between groups but decides no membership, and no resolution is
inferred from any of it. Identity is the REST numeric id as text, so linkage
is checked in one id space.

**Unavailable is a state on the snapshot, distinct from the GraphQL threads.**
`application::github_review_comment_group_record{Root, Replies}`, and on the
snapshot `ReviewCommentsAvailability` (leading `unavailable`) beside
`ReviewCommentGroups`. The GraphQL `ReviewThreads` model is untouched: its
cross-validation requires the availability to equal the read shape's
`ReviewThreads`, which an anonymous shape holds false, so REST comments could
not have lived there without either violating the invariant or inferring a
capability. Validation refuses `unavailable` with a non-empty collection as
`invalid_relationship`, requires a root to carry no `ReplyTo` and every reply
to name its root, and reads `Outdated` from a required, nullable `position`
rather than from ordering. Viewer, viewed state, resolution, and
actionability stay their typed `unavailable` values.

**The pager issues the REST read only under the anonymous authority.** The
authenticated path sends zero review-comment requests — tested — and its
recorded ten-interaction contract and the application's public-path fixtures
pass unchanged. For the new operation: a 404 is `permission`
(existence-neutral); a 403 rate limit is `rate_limit` carrying `Limit=60`
and `Reset=1787546881` with no retry-after and exactly one attempt; an
oversized body is `response_limit`; two pages exhaust by `Link`.

**The normalizer admits a set instead of assuming ten.**
`github_read_admission{Operations, Shape, Viewer}` with
`authenticated_read_admission()` (today's behaviour, the default) and
`anonymous_read_admission(viewer)` (the nine eligible operations, an all-false
shape); `normalize_github_read` and `perform_normalized_github_read` take it,
and the interaction-count check is against the admitted set.

**Fixtures are real where they can be and say so where they cannot.**
`tests/fixtures/github/rfc0017-anonymous-v001/captures/` holds the five
sanitized live bodies of the RFC-BPR-0017 probe review, byte for byte, plus
the review's derived rate-exhaustion artifact labelled `synthetic` in
`provenance.json`; `synthetic/` holds twelve hand-written linkage, paging,
and hostile cases the single-comment live capture cannot express, each with
its purpose and digest. The digest test binds each promoted body to the
minimized-body digest `capture-bindings.json` recorded and to the capture
file's own digest, and compares the promoted bytes to the capture's `body`
field. Reconciled by the integrator: every promoted file equals its
capture's `body` field byte for byte and hashes to that capture's
`minimized_body_sha256`; the capture file's separate `body_sha256` is the
pre-minimization digest and differs by design. The fixture catalog carries
the entry.

Not in this slice, named rather than implied: an end-to-end nine-interaction
anonymous recorded normalizer run — no anonymous manifest corpus exists yet
and the composition course that would carry one is [[BPR-0305]]'s; the
provider-level rate-limit fact is proven at the pager boundary
(`github_provider_error.RateLimitCause`), and `github_provider` passes
unchanged.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| The read contract, provider, pager, anonymous contract, transport tables, core policy, inventory gates, manifest gate, the application's public-path fixtures, the host, and the pinned round-trip contract | `ctest --preset dev -R '^(github_read_contract\|github_anonymous_read_contract\|github_paged_read\|github_provider\|github_capability_probe\|github_mutation_contract\|gh_cli_transport_contract\|native_https_anonymous_read_(contract\|fixture)\|core_anonymous_public_read\|core_github_policy\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_release_absence_negative_control\|test_network_isolation_manifest\|application_github_public_path_(github_com\|uiforetw_74)\|application_github_host\|application_review_round_trip_composition_contract)$' --output-on-failure` | 18/18 passed | this commit |
| The composed application still admits its fixtures end to end | `ctest --preset dev -R '^application_composition_contract$' --output-on-failure` | Passed in 124.65 s | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on the 15 touched or new C++ files | exit 0, no output | this commit |
| The ledger's `--all` inventory form | `python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all` | `RFC0017_TEST_INVENTORY_OK` | this commit |
| The linkage refusal is load-bearing | The worker removed both linkage guards in `group_review_comments` (the "target must be a root" clause and the unfound-group refusal — either alone still refuses a cycle through the other), rebuilt, ran `github_paged_read`; restored byte-identical | Failed: `a conversation whose linkage cannot be trusted is refused rather than repaired`; restored, passes | this commit |
| The promoted bodies are the review's minimized bodies | integrator reconciliation: sha256 of each promoted file against `capture-bindings.json`'s `minimized_body_sha256`, and bytes against the capture's `body` field | 5/5 equal on both counts; `graphql-refusal.json` also equals the capture's raw `body_sha256`, the other four differ from it by minimization, as recorded | this commit |
| The tree's documentation, fixture-catalog, and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
