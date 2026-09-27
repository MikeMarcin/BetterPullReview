# RFC-BPR-0022 Verification Ledger

Status: **milestones A–C landed; D unattended evidence/documentation and attended
preparation complete; live acceptance and canonical closure pending**.

Assignment `/root/rfc0022_m4`, mandate **r2**, run `rfc0022-20260926`.
RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.
D production candidates: `0ff88046` (PR2 rebind), `6526f525` (exact optional
profile selector), `3a517b86` (bounded publication diagnostics). The original
profile2 text-line60 attempt remains witnessed and unresolved; no retry/fallback
was performed. [D evidence](../issues/_checkpoints/BPR-0575/m4-evidence.json)
records the final candidate, commands, log hashes and limitations.

## Architecture allocation

| Region | Implemented responsibility | Direct evidence |
|---|---|---|
| Core | Pure placement/capability/body policy and legal durable transitions | M1 policy tables and M3 illegal-observation sweep |
| Application | Exact source/actor admission, effect serialization, cached root normalization | M1 host, M3 normalization/public composition |
| GitHub | Native/file wire, canonical URL grammar, exact classifier, session-owned evidence | M2 contracts and M3 real session course |
| SQLite | Transactional schema4 parent/children, legacy/whole-review preservation and recovery | M2 migration and M3 interruption course |
| Qt | One gesture/editor, canonical selection, incremental FILE semantic placement and support disclosure | M3 three scales/three themes, D Qt100 |
| Composition | Existing user/attended authority only, exact successful receipt refresh | M3/D actual app course; D sealed PR2 capsule |

## Final criterion matrix

Each criterion below is copied from the canonical RFC. Expected behavior and
observations are separate. Automated evidence is reused only for unchanged
owning source; D reruns the complete public path after the attended target edit.
M1, M2 and M3 refer to the exact commits/commands in their durable evidence files.
They do not assert native service or human acceptance.

| # | Canonical acceptance criterion | Direct surface | Command or interaction | Expected result | Observed result | Gate class | Producing evidence |
|---:|---|---|---|---|---|---|---|
| 1 | Every materialized current/right source line in a text file already changed by the pull request exposes the ordinary comment gesture; file eligibility, exact head, path, line, and acting identity are revalidated before any write. | real line gesture + exact host admission | core_review_round_trip; application_review_mutation_application_host; qt_composed_github_scale_100; application_review_round_trip_composition_contract | Eligible right lines expose one gesture; invalid target/head/path/line/actor refuse before POST | PASS automated/rendered; final composed candidate confirms native and file selection | automated+rendered | M1/M3; D public replay |
| 2 | A selected anchor accepted by the legacy line-comment operation publishes as one native line review comment on GitHub.com and GHES, independent of the off-hunk capability state. | policy + exact GitHub/GHES wire/session | core_review_round_trip; github_mutation_contract; github_review_comment_permalink | Legacy accepted line remains native for every off-hunk capability state | PASS fixtures; native GHES unobserved per owner amendment | automated | M1/M2/M3 |
| 3 | An off-hunk anchor on a repository whose exact operation is proven supported publishes as one native line review comment and records scoped positive evidence. | designated native GitHub.com write | Prepared PR2 attended application; off_hunk_fixture.txt RIGHT line60 | One exact native comment and positive scoped evidence | PENDING native success; text-line60 is witnessed/indeterminate with no receipt; no support inferred | external-authority | D prepared capsule |
| 4 | An off-hunk anchor proven unsupported publishes as one ordinary file-level review comment whose first paragraph is the canonical immutable permalink and whose remaining prose is byte-preserved. | exact synthetic rejection + real session/journal/file wire | github_review_comment_permalink; application_review_round_trip_composition_contract | One FILE comment, canonical prefix, byte-identical prose | PASS synthetic contract; production tuple absent, live fallback PENDING | automated+external-authority | M2/M3; D public replay |
| 5 | An unknown capability or placement attempts the intended native write; only a fully received exact unsupported rejection can authorize fallback. | policy/response classifier/serialized session | core_review_round_trip; github_mutation_contract; github_review_comment_permalink | Unknown starts native; only exact complete non-creation can authorize file | PASS automated and live native witness; production classifier has no admitted tuple | automated | M1/M2/M3 |
| 6 | Timeout, cancellation, connection loss, created-but-unparsed response, generic `422`, permission failure, rate limiting, malformed response, 5xx, or ambiguous reconciliation never starts a fallback POST. | wire/host/recovery counterexamples | github_mutation_contract; github_review_comment_permalink; native_https_transport_fixture | Ambiguous, generic422, permission/rate/network/malformed outcomes cause zero fallback POSTs | PASS bounded hostile courses; live indeterminate native journal has no fallback | automated | M2/M3 |
| 7 | One logical publication produces at most one GitHub comment across retries, process death, schema migration, and restart; a possibly transmitted native attempt is reconciled GET-only and can never coexist with an eligible fallback POST. | schema4 CAS journal and fresh-session recovery | sqlite_review_mutation_recovery_storage; github_review_comment_permalink; live_review_mutation_restart_recovery | At most one server candidate after durable interruptions and restart; preserve legacy and whole-review rows | PASS66 deterministic before/after interruptions with3 fresh sessions each; no OS-kill/power-loss claim | automated | M2/M3 |
| 8 | Capability evidence is repository-, actor-, profile-, server-contract-, operation-, API-, and classifier-revision scoped, freshness-bounded, and never inferred from host name, broad GHES version, readable comments, or status code alone. | session-owned exact-anchor cache | application_review_publication_capability_cache; github_review_comment_permalink | Exact authority/revisions/generation/head/path/line,128 entries,maximum300s; stale means unknown | PASS scoped invalidation/admission tests; host/version inference absent | automated | M2/M3 |
| 9 | BetterPullReview retains both the semantic source anchor and the honest GitHub remote subject; it renders a valid compatibility thread at its source line without claiming GitHub stored a line subject. | read normalization + incremental thread map | qt_composed_github_scale_100; application_review_round_trip_composition_contract | Remote FILE stays FILE; validated exact semantic anchor selects source | PASS final composed replay;3 scales/3 themes retained from M3 | automated+rendered | M3; D public replay |
| 10 | A GitHub-only collaborator can see the linked code, read the complete prose, reply through ordinary GitHub UI, and retain that conversation after BetterPullReview is uninstalled. | ordinary GitHub web UI | Prepared PR2 course followed by GitHub-only inspection/reply | Visible code, full prose, author and ordinary reply continuity without local database/app | PENDING human/live course; no actual uninstall or collaborator verdict claimed | external-authority | D prepared capsule |
| 11 | Deleting the local database and refreshing from GitHub reconstructs a canonical compatibility anchor and reviewer prose; invalid, ambiguous, or foreign permalinks remain honestly file-level. | fresh host without journal/receipt | qt_github_conversation_panel; github_review_comment_permalink | Canonical link reconstructs exact anchor; foreign/invalid/ambiguous stays file-level | PASS fresh-host fixture with no DB or receipt; not an actual uninstall | automated | M3 |
| 12 | External edits, replies, resolution changes, and deletion reconcile by canonical GitHub identity. Removing or changing the permalink updates or removes the semantic placement according to server truth and never revives stale local placement. | canonical IDs plus edited-root source cache | qt_github_conversation_panel; qt_composed_github_scale_100 | Edits/removal/remap/replies/deletion follow server truth, no stale resurrection | PASS automated normalization/selection fixtures; live mixed-client course still pending | automated | M3; D public replay |
| 13 | A body requiring line-only semantics is not silently degraded through the file representation; it receives a typed incompatibility with no POST. | body policy + actual editor/session | core_review_round_trip; github_review_comment_permalink; qt_review_draft_editor | Line-only constructs refuse required FILE fallback without POST | PASS typed incompatibility and no-write assertions | automated | M1/M2/M3 |
| 14 | The complete gesture follows Qt intent -> core decision -> application host -> mutation coordinator -> GitHub adapter -> durable recovery -> server refresh -> incremental Qt projection, with no provider/widget bypass. | public application native/file course | application_review_round_trip_composition_contract | Real gesture through coordinator/session/SQLite/restart/refresh selects canonical post | PASS final source; native2/file1/stored2,3 journal opens,2 session constructions asserted | automated+rendered | M3; D public replay |
| 15 | Automated tests cannot reach a non-designated live target, and live native and fallback evidence is produced only through the existing attended, identity-bound mutation authority. | network guard, exact target bootstrap, signed mounted capsule | test_network_isolation_manifest; test_network_isolation_structure_contract; live_mutation_isolation_contract; designated_live_public_observer_fixture; application_designated_review_authority_bootstrap | No ordinary test can publish; live lane bound to exact PR2/build/actor/one-use grant | PASS available-platform guards and exact attended launch/write witness; feature result pending; Windows gaps retained | security+external-authority | M3; D authority/manifest/capsule |

## Durable producing evidence

- [M1 evidence](../issues/_checkpoints/BPR-0575/m1-evidence.json): policy, exact
  source admission and codec; milestone landed at `6e8ca8a8`.
- [M2 evidence](../issues/_checkpoints/BPR-0575/m2-evidence.json): native/file wire,
  classifier mechanics, cache and schema4 session recovery; landed at `2deb889b`.
- [M3 evidence](../issues/_checkpoints/BPR-0575/m3-evidence.json): final focused20/20,
 66 interruption courses,3-scale/3-theme rendering, native/fallback public course,
  production build and final merged3/3. Landed main at `dc1a836b`.
- [D capsule identities](../issues/_checkpoints/BPR-0575/m4-capsule-identities.json):
  exact source58-file manifest, binary, supervisor, inventory, image, canonical
  approval and fresh replay identity. Production build, signatures/inventory and
  mounted preflight passed. No live write occurred during preparation.

These are deterministic fixture interruption/restart observations, not OS kill,
power loss, live database deletion or actual uninstall. Native GHES behavior is
unobserved. No independent review report is claimed by the implementer.

## Designated GitHub.com course

The owner explicitly authorized creation and test comments on
[MikeMarcin/BPR-Test PR2](https://github.com/MikeMarcin/BPR-Test/pull/2).
Read-only refresh found it open with zero comments, repository
1328277631 / R_kgDOTyvofw and PR4650736621 / PR_kwDOTyvof88AAAABFTSb7Q.
Base `a939a85a5525994fe9e9cb4c973ad9362ed427c7` belongs to
`codex/rfc0022-fixture-base-20260926`; head
`61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a` belongs to
`codex/rfc0022-live-comments-20260926`. Both harmless text fixtures change line10;
line60 is outside that hunk. The first course selects `off_hunk_fixture.txt`,
RIGHT line60, with prose “BetterPullReview semantic source-line verification at line 60.”

Use the exact prepared application through the existing captured stage-zero,
read-only mounted capsule and identity-bound attended authority. Record the
fresh actor, exact selection, terminal classification, remote subject and
comment ID. An ambiguous outcome permits GET-only observation and no fallback
or blind resend. Direct gh/API writes cannot replace this shipped course.
Historical PR1 packets remain immutable evidence for their original candidate;
new builds of this candidate bind only PR2. Reconcile any older queued course
before launching it.

The owner's mandate r2 substitutes GitHub.com live evidence for unavailable
native GHES. GHES automated contracts remain. **Production has no admitted
unsupported tuple.** Synthetic contracts establish mechanics only; they do not
activate fallback or prove native support. If GitHub.com accepts the native
line, record that exact result. If it refuses without an exact approved
non-creation contract, preserve the limitation. Fallback/web acceptance cannot
be claimed from a generic422.

## Documentation and simplification

The state machine and bounds live in [review round trip](../architecture/review-round-trip.md#semantic-source-line-publication-policy-rfc-bpr-0022),
wire/classifier details in [provider contract](../architecture/github-provider-contract.md),
and user guidance in [source-line comments](../development/source-line-comments.md).
Product, architecture, diff, UX, security/threat, fixture, master matrix and release
surfaces now refer to these owning boundaries and explicitly state unavailable
production fallback and unobserved live evidence. Core policy, session authority
and changed-root parsing were reviewed for duplication/broad capability/paint
work. No additional behavior refactor was warranted after A–C's bounded design;
stale preparatory wording was removed. D adds no full model reset or second codec.

## Closure remains pending

BPR-0573 awaits a conclusive native result, an admitted exact unsupported
contract for fallback, and GitHub-only verdict. BPR-0574 is Done for independent
documentation/simplification. BPR-0575 remains Backlog while its
required predecessor is open. RFC0004/0005/0008 are Auto-Verified, but the
canonical index says RFCs always close in order and RFC0020/0021 remain Deferred.
No order exception is inferred from this implementation mandate. All15 criteria
and all card acceptance, the documentation gate and clean whitespace are
necessary before any Auto-Verified transition.

The immutable imported packet remains source-only. Import checks proved pack
integrity and mapping, never product acceptance. The authored four-milestone
plan and the baseline remain unchanged. The owner requires landing each accepted
milestone before its successor; D's independent commits may land while external
acceptance remains open, without claiming RFC completion.
