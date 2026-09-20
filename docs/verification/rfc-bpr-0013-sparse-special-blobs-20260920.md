# RFC-BPR-0013: sparse special-blob import correction

Assignment: `selected-diff-exact-epoch-pair`, engineering contract v1,
coordinator-implementer recipe revision 2, child
`/root/rfc0013_postreview_r3`, mandate `r19-v005-independent-verification`.
Source candidate: `0c66306fc7c233b0a78eef02e5b09916ad349022`.

## Authoritative failure and diagnosis

The v005 attended rejection at event 441 and hand-back at 442 remain authoritative.
The failed candidate was `rfc0013-v005-37343abf-5f956714`, fingerprint
`af95465d3f110907a65c082dfcb1459b1bef863f90ec33e942f4910b4479dc4b`.
Only its affected development protection was released, at 443. No new attended
candidate is requested; the owner asked for independent end-to-end verification.

The exact signed tests-disabled r20 app independently reproduced the real-target
failure. Its fixed diagnostics at 08:54:36–08:54:43 UTC show successful sparse
acquisition followed by `revision_prepare/cache/unsupported_object`. No exact
pair was available and no selected source could be shown. The separately run
production transport plus installed-Git import identified the fixed fsck message
`gitattributesMissing`; no private paths, objects or source are retained here.

Git checks special blobs referenced by imported trees even with
`index-pack --fsck-objects`. An absent `.gitattributes` is an error; an absent
`.gitmodules` can add an object identity to standard output, violating the
adapter's exact output contract. Both are intentionally absent in a blobless
large-review pack. See Git's [fsck implementation](https://github.com/git/git/blob/v2.50.1/fsck.c)
and [index-pack contract](https://git-scm.com/docs/git-index-pack).

The correction ignores only `gitattributesMissing` and `gitmodulesMissing` on
the existing Git 2.44-or-newer sparse path. Complete imports, malformed present
special blobs, object identities, receipt/inventory authority, durable witnesses,
source materialization, cancellation and size limits are unchanged. Older Git
retains its previous strict sparse fallback. No core or Qt policy changed.

## Regression evidence

The new process contract failed against the old implementation at
`sparse import accepts intentionally absent special blobs without extra stdout`
(104.74 s). It then passed with the repair. Its matrix covers both special files,
strict refusal of missing blobs, sparse acceptance of missing blobs, and both
paths' exact `gitattributesLineLength` / `gitmodulesPath` refusals for malformed
present blobs. Existing error-class and sparse-cache refusal/recovery tests pass.

The external `large-positive` CLI course now constructs deterministic commits
whose trees contain both special files. Its local Git server supplies genuine
`--depth=1 --filter=blob:none` clones, and the fixture checks that both special
blobs and the selected source blob are absent before an exact blob request.
The same tests-disabled r20 app, driven through normal Home → CLI profile →
Open → Overview, reproduced the failure with 515 safe synthetic changed files.
The full-window raster shows “File comparisons unavailable,” a refused Git
object, and “no pair yet.” This is a production-composition negative control,
not an isolated painter or wrapper assertion.

The repaired real-target transport/import probe then succeeded: a 9,383,986-byte
pack, 5.885 s acquisition, and successful installed-Git sparse indexing. This
does not by itself establish a readable diff in the application.

Final checks against the committed source:

- `git_process_contract`: pass, 107.36 s after the named-case readability pass.
- `git_sparse_cache_contract`: pass, 80.25 s; sparse/strict residency and repair
  boundaries remain separate.
- `application_epoch_host`, `application_github_window_read_host`,
  `fsck_severity_fixture_contract`,
  `application_epoch_portable_occurrence_two_catalogues`, and
  `application_setup_destination_public_path`: 5/5 pass, 265.05 s serially.
  The two-catalogue current/historical integration itself took 225.65 s.
- External CLI fixture self-tests: 4/4 pass, 5.025 s.
- Documentation gate, all 64 documentation verifier tests, changed C++ format
  check, `git diff --check`, and deep/strict app signature verification pass.

Self-review checked the installed-Git policy boundary, version guard, special
message semantics, exact stdout contract, fixture determinism and absence of
new live writes or test hooks. The code-simplifier pass replaced nested test-case
selection with four explicit named cases; no production behavior changed in that pass.

## Initial page loading

The independently reproduced real read ran from 08:53:13.340 to 08:54:29.582 UTC
on macOS 26.6.2 arm64, Apple Git 2.50.1 and Qt 6.10.3. Its 48 successful page
responses were 16 REST file pages, 16 GraphQL viewed-file-state/thread pages,
3 commit pages, 6 timeline pages, 2 issue-comment pages and 5 other metadata
pages. The GraphQL file continuations share the `ListReviewThreads` operation
label, which must not be mistaken for repeated fetching of the 19 threads.

This is serialized bounded provider pagination before atomic snapshot acceptance,
not sparse tree traversal, an unbounded catalogue loop, or UI-thread blocking.
The loading Overview paints immediately; accepted content still waits for the
complete snapshot. This correction does not reduce that 76.24-second initial
wait. Publishing partial metadata safely would need a separately specified
acceptance/identity contract; this round does not silently accept partial reads.
Overview folder collapsing is independently tracked in [[BPR-0934]].

## Candidate and remaining proof

Signed Release bundle:
`build/app-release-rfc0013-r21/src/app/BetterPullReview.app`.
`BUILD_TESTING=OFF`; executable SHA-256
`c0fc8f508d1679b9f9bc7195fb2ecbcd943a0d3e4504a81b8c841dc4c4e7789a`.
Deep/strict signature verification passes with the existing Developer ID.

The repaired native exact-target and safe rendered source/anchor courses remain
pending: the Mac locked before the repaired app could be driven, and CUA could
not unlock it. Unlocking is an environment prerequisite, not a request for the
owner to test or judge the repair. No readable-diff acceptance is inferred from
the passing transport/import or regression checks.

Transient safe native evidence is retained locally under
`build/rfc0013-resume-20260919/`, including `r21-before-synthetic.png`. Private
transport data lived only in owner-private temporary stores outside the checkout
and was cleaned normally. No live GitHub writes, global account changes,
credential extraction or device-session changes occurred. The prior owner app
PID 15093 was already absent and was not signalled.

Task-owned probe sources/binary and the isolated synthetic CLI/home were moved
recoverably to `/Users/jaeger/.Trash/bpr-rfc0013-r21-temporary-20260920`.
The signed bundles, safe raster and fixed test/build logs remain for continuation.
All task-owned app/test/build/probe processes finished; no new review host exists.
