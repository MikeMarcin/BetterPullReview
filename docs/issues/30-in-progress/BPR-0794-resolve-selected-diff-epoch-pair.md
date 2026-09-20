---
schema: "implement-rfc.card.v1"
id: "BPR-0794"
title: "Resolve the exact epoch pair for a large pull request selected diff"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0793"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0c66306fc7c233b0a78eef02e5b09916ad349022","evidence":{"kind":"artifact","detail":"Exact signed r20 app independently reproduced v005 failure. Production transport/installed-Git probe names gitattributesMissing; enhanced 515-file external normal-UI course fails identically before repair. Independent evidence record binds fixed diagnostics and safe raster."}},{"criterion":"AC2","outcome":"pass","revision":"0c66306fc7c233b0a78eef02e5b09916ad349022","evidence":{"kind":"artifact","detail":"0c66306f changes only sparse missing-special-blob policy. Both malformed-present-object negatives and complete-path missing-blob refusals pass; sparse cache witness, authority, recovery, stale/cancellation and full current/historical composition checks remain passing. Real-target import succeeds."}},{"criterion":"AC3","outcome":"pass","revision":"0c66306fc7c233b0a78eef02e5b09916ad349022","evidence":{"kind":"artifact","detail":"Final git_process_contract passes 107.36 s; sparse cache contract 80.25 s; focused/integration 5/5 passes 265.05 s, including real setup composition and portable two-catalogue course; external CLI fixture 4/4, documentation/64 verifier tests, format/diff/signing pass. Repaired signed native source/anchor proof remains AC4 pending."}},{"criterion":"AC4","outcome":"pending","revision":"0c66306fc7c233b0a78eef02e5b09916ad349022","evidence":{"kind":"artifact","detail":"v005 was rejected at 441 and handed back at 442. Independent exact signed-r20 app reproduction confirms no pair/readable source; sparse import fails gitattributesMissing. Candidate 0c66306f repairs only sparse missing-special-blob policy and passes real import, but repaired native source/anchor proof remains pending because the Mac locked. No new attended candidate or request for user testing."}},{"criterion":"AC5","outcome":"pass","revision":"0c66306fc7c233b0a78eef02e5b09916ad349022","evidence":{"kind":"artifact","detail":"No live write, global gh account switch, credential extraction or device-session change. Real-target private pack stayed in owner-private disposable stores and was cleaned; retained native raster contains only synthetic source. Existing owner app was absent and never signalled. Unrelated .mask.lock.json preserved."}}]
---

# Resolve the exact epoch pair for a large pull request selected diff

The read-only tests-disabled GitHub CLI profile reached **Pull request ready**
for the owner-retained `EpicGames/UnrealEngine#1773` and presented 1,523 files
and 19 review threads. Two file selections each refused `selected_file_stale`;
the epoch control had no exact pair and no selected diff was readable. BPR-0643,
BPR-0792 and BPR-0793 are Done for their bounded outcomes and remain terminal.
This is the separate selected-diff follow-up authorized by mandate r14.
Mandate r15 chooses bounded on-demand selected diffs: acquire only exact current
selected-file objects needed for an immediate comparison and defer older-epoch
comparison objects until that epoch is selected. Lazy historical loading alone
cannot repair the current missing exact pair. The small-versus-large loading
policy from mandate r16 is tracked separately in BPR-0795; it does not replace
this card's readable selected-diff criterion.

## Acceptance

- Attribute the missing exact epoch pair and selected-file refusal through the
  public composition, retaining only fixed aggregate and failure metadata for
  the private target outside the checkout. Do not retain private response,
  path, object, thread, cursor, or diff content in repository artifacts or logs.
- Correct the responsible transition or host mapping without fabricating an
  epoch, relaxing exact authority/object checks, adding unbounded history, or
  silently treating incomplete returned commits as complete history. Focused
  positive and negative checks must cover the failure and stale selection.
- Focused core, host or Qt checks selected by the repair, a composed public-path
  check, documentation gate and one integration-focused check pass.
- A tests-disabled Mac application read through the already opted-in CLI
  profile shows the correct file list **and a readable selected diff** for the
  exact target, with source/anchor identity checked against the accepted epoch
  pair. A remaining explicit refusal is recorded honestly and leaves this
  acceptance open.
- No live pull request write, global `gh` account change, credential extraction
  or transfer, device-session change, or private content in the checkout occurs.

## Evidence

### Current correction — v005 independently reproduced

The v005 rejection (441) and hand-back (442) remain authoritative. The exact
signed r20 app independently reproduced the real failure; a narrow read-only
probe then attributed it to `gitattributesMissing` during sparse pack import.
The enhanced 515-file external CLI course reproduces the same failed public
composition in r20. Its prior fixture lacked both special Git files.

`0c66306f` permits only intentionally absent `.gitattributes` and `.gitmodules`
blobs on sparse imports. Complete imports and malformed present special blobs
still refuse. The failing-before process matrix and sparse cache checks pass;
the real-target transport plus installed import now succeeds. See the
[independent evidence record](../../verification/rfc-bpr-0013-sparse-special-blobs-20260920.md).
No readable-diff verdict is inferred: the Mac locked before the repaired signed
r21 app's source/anchor course could be driven. Resume independently once it is
unlocked; do not ask the owner to discover whether diffs load. No new attended
candidate has been prepared. [[BPR-0934]] tracks the separate folder-collapse ask.

### Historical correction — v004 hand-back, v005 prepared

The v004 BPR-0794 accept (432) is preserved alongside the later BPR-0795
reject (433): the latter explicitly reports no readable source or epochs and
includes a confirming screenshot. Loading presentation was approved, but the
concrete acquisition failure leaves AC4 open. The requested CLI-warning cleanup
is tracked separately in [[BPR-0933]]. Only the affected candidates were released.

`37343abf` fixes the public path missed by the direct bridge: both provider
account-factory wrappers discarded sparse capability, so the app selected
complete acquisition for the large target. The observed Git lease also omitted
exact-blob forwarding. Both now preserve the selected capability and route
blobs through the existing pre/post account, cancellation and revocation guard.
No resource limit, cache authority or history policy changed.

The old provider fails the new wrapper regression. Focused tests pass 5/5;
the portable two-catalogue integration and four external-fixture checks pass.
The exact signed tests-disabled r20 app was driven through a 515-file synthetic
CLI review: sparse-only acquisition, manifest-matching exact pair and readable
selected source `0 → 2`, with one semantic replacement. That current-pair
observation does not replace the real-target source/anchor criterion.

The [v005 packet](../../verification/reviews/rfc-bpr-0013-attended-v005/operator-packet.md)
and [identity](../../verification/reviews/rfc-bpr-0013-attended-v005/session-identity.json)
bind `rfc0013-v005-37343abf-5f956714`, ready/protected at 439 with evidence through
440. Only AC4 is refreshed for human review. Loading approval and r17 device
acceptance are retained. Builds, tests, probes and task-owned native apps are
finished; the attached loopback host remains for the coordinator's hand-back watch.

### Historical correction — attended v003 rejected

The owner rejected v003: selected files and epochs still did not load, and
participant rows should use one line each. The two affected candidates have
been released for development; the accepted BPR-0644 device result remains
bound to r17. Fresh fixed diagnostics show cache open now succeeds, but the
CLI protocol acquisition returns `transport/response_limit` before revision
preparation. This supersedes the earlier cache-open diagnosis for this run.
The new implementation owner is `/root/rfc0013_review_round_v003_implementor_astra_max`;
no further agent delegation is permitted.

`12e9bdb8` repairs the reproduced failure: Git removes or atomically renames a
temporary file between the directory entry check and `file_size`. The failed
sample returns `ENOENT` and the unsigned size sentinel; the supervisor treated
it as an oversized file. Only a disappeared file is now skipped. Other errors,
symlinks, per-file bytes, aggregate bytes, entry count and process cancellation
still refuse under the existing limits. The new churning-file regression fails
against the old scanner and passes against the repair, including the retained
size/entry/symlink negatives. Three fresh exact-target reads through the repaired
production C++ bridge succeeded in 5.9–6.1 seconds with 9,382,814-byte in-memory
packs. This is acquisition evidence, not a product source/anchor verdict.

`9c670d88` makes participant rows one measured line, with an elided login and
right-aligned standing; the full identity, role and standing remain in tooltips
and accessibility. The 64-participant long-name/narrow/larger-font regression
and the rendered composed Overview pass. BPR-0795 owns the same source's
immediate full-width loading Overview and bounded read-progress presentation.
The [v004 packet](../../verification/reviews/rfc-bpr-0013-attended-v004/operator-packet.md)
and [identity](../../verification/reviews/rfc-bpr-0013-attended-v004/session-identity.json)
bind the signed tests-disabled candidate `rfc0013-v004-9c670d88-71117a5c`, ready
and protected at sequence 427 with observations through 429. Human acceptance
remains open. The parent owns the later served review and watcher; neither was
started by this implementor. No live GitHub write, global account switch, device-session
change, credential extraction or private-content retention was performed.

### Historical correction — mandate r18, attended v002 rejected

The [v003 served-review packet](../../verification/reviews/rfc-bpr-0013-attended-v003/operator-packet.md),
source `eecdda2a`, candidate `rfc0013-r18-eecdda2a-f6db019a`, was prepared
at session sequence 416 and then rejected. Its signed tests-disabled bundle,
fixed-field diagnosis, live read-only probe and passing focused/integration
results are bound in [v003 identity](../../verification/reviews/rfc-bpr-0013-attended-v003/session-identity.json).
Its host and decision watch are stopped; exact-target source/anchor acceptance
is still required. The historical v002 rejection below is not overwritten.

The owner accepted the separate BPR-0644 relaunch criterion but rejected this
candidate: the 1,523-file count matched; participant text overlapped and selected
files never became readable. The fingerprint-valid verdict and screenshots stay
in the attended session. The accepted device result remains bound to r17 and is
not a verdict for these repairs.

Fixed-field diagnostics from that run show `cache_open/cache/invalid_manifest`,
then `revision_prepare/repair_required`, before any Git acquisition. A read-only
metadata comparison found the target's exact authority/config/HEAD/private layout
unchanged, and the same lock inode, but a different mounted-device number. The
target cache contains zero objects, zero refs and zero retained epochs. This is
not another provider permission failure and not evidence of a failed sparse
fetch. No private source, path, object ID or screenshot is copied into this card.

The current repair writes new Mac manifest lock identity as volume UUID plus inode.
Legacy caches still require their exact original identity; only the closed,
pristine, authority-matching empty layout with the same lock inode and changed
mount number can be preserved under a no-replace rename and initialized afresh.
Populated/unknown caches are never adopted or deleted. Negative checks cover
object/ref/unknown entries, authority/lock/UUID drift and cancellation. The
composed CLI refusal/restart course reproduces the empty old-mount manifest at
the actual installed-cache boundary before attempting exact diff recovery.

The participant sidebar now scrolls and uses measured wrapped login/metadata
lines. A 64-participant regression checks full heights, non-overlap, long names,
larger fonts, smaller windows and reachability of the last participant and action.
The exact-target product observation is still pending; do not infer it from
these repairs. A second latent blocker was found before preparing a candidate:
the installed `gh` is 2.101.0, accepted by discovery, but Git-object reads still
allowed only 2.100.0. The narrow allowlist now includes the directly checked
2.101.0 with the unchanged Apple Git version and containment limits. Older,
unknown future and prerelease versions still refuse before handing off objects.

A bounded, write-contained live read using installed `gh` 2.101.0 and Apple Git
2.50.1 verified two exact sparse commits and two SHA-checked UTF-8 blobs with
different contents. Viewer and configuration metadata remained unchanged. The
largest temporary store was 10,866,649 bytes; all temporary objects were removed
outside the checkout. Only these fixed aggregate facts are retained. This checks
the real CLI/backend prerequisites, not the attended application's rendering or
source/anchor verdict. The actual composed public-path fixture does render an
exact readable diff after the old-mount empty-cache recovery.

The previous fixed metadata-only product observation is recorded on BPR-0793
and BPR-0617. The append-only product diagnostic log attributes the absent
pair to `protocol_acquire/transport/unavailable` before epoch revision
preparation. A sandboxed, bounded `gh` Git probe reproduced nonzero full-clone
exits near the 32-MiB file ceiling. A blobless depth-one exact-base clone
fits the existing bounds and contains the accepted commit and root tree, but
not selected-file blobs. This is a possible first step, not a readable diff.
The fixed aggregate-only record is outside the checkout at
`~/Library/Application Support/BetterPullReview/BPR-0794-evidence-2026-09-13/diagnosis-d12.json`
(SHA-256 `a68be3bd2c2b65ab3f749703107b4fbd04d53287d5e607467c64d62dca317311`).
No child stderr, response body, private object or credential was retained.
An adapter-only sparse clone cannot satisfy the current strict connected-pack
Git cache contract. A synthetic complete-pack/missing-blob/missing-parent
fixture confirmed that the incomplete cases refuse. The d13 candidate now
adds a separate durable sparse commit/tree residency mode, exact SHA-1
commit/root-tree checks and shallow lineage marked unknown when needed. The
strict complete-pack path remains unchanged. A closed `gh api` exact-blob GET
checks the Git object ID, size, base64 payload, viewer/configuration and
cancellation before handing bytes to the host. The host requests a missing
blob only for an accepted large review and an exact authorized current pair.
Synthetic tests prove a sparse current pair can be accepted, the small path
remains complete, and unavailable/stale selected-object reads refuse. The
tests-disabled Mac bundle builds. Four focused cross-layer tests, the cache
and storage contracts, documentation gate, and format check pass. Independent
critical review found no remaining current-pair authority or recovery blocker;
its older-epoch selection finding remains assigned to BPR-0795. A task-owned
app instance reached signed-out Settings, but the macOS computer-use control
could not activate its GitHub CLI host selector, so no new target read or
source/anchor observation is claimed. The readable selected-diff acceptance
remains open.

The renewed unlocked-Mac d16 attempt launched the signed tests-disabled d15
candidate after quitting a stale task-owned d13 instance that held the Default
profile lease. The d15 window and Accounts & hosts page rendered, with the
selector still showing GitHub.com Native HTTPS. Computer-use accessibility
clicks did not open or change the selector; pointer actions returned
`AXError.notImplemented`, and keyboard traversal skipped the selector.
The d15 app was quit and its process confirmed absent. No CLI profile was
selected, no target request was made, and source, anchors and diff remain
unobserved. This is an automation-path limitation, not evidence that the
candidate either passes or fails the exact selected-diff behavior.

## Attended preparation at main `94f6f885` (dsp-003)

A tests-disabled bundle was built from `94f6f885`, signed with the stable
Developer ID identity, and passed `codesign --verify --deep --strict`
(`BUILD_TESTING=OFF`). It is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app`,
with executable SHA-256
`96fa16fea8983347303c6ce18095f147871b7d45ef51537d48097ac6004e1bfa`. A
`--self-test` run printed `AUTOMATED_PROFILE_PRIVATE_OK` and did not touch
the Default profile.

The BPR-0794/0795 focused set was rerun at that source and passed 13 of 13
(`build/r2-logs/dsp3-bundle-focused-ctest.log`).
`application_epoch_portable_occurrence_two_catalogues` also passed
(`dsp3-bundle-portable-ctest.log`).

[the rfc-bpr-0013-attended-v001 packet](../../verification/reviews/rfc-bpr-0013-attended-v001/operator-packet.md) holds three ready, protected questions in the checkout's attended-review
session: `BPR-0794-cli-selected-diff`, `BPR-0795-loading-visible` and
`BPR-0644-device-relaunch-8h`. The human part is open. The owner must quit
their own running BetterPullReview before launching the bundle.

## Resume checkpoint — 2026-09-19

The consumer session remains at sequence 389: the selected-diff question is
ready and protected with no decision or newer event. A read-only recheck
matched the executable and offline-page digests and passed deep strict
signature verification; the build cache remains tests-disabled Release.
The 13-test focused and one-test portable logs explicitly bind `94f6f885`.
Later source changes do not turn those results into evidence for current main.
The card is parked for the existing attended criterion. No application or
`gh` launch, target request, account/device-session change, or private-content
capture occurred. The separate protected BPR-0644 relaunch candidate remains
untouched. Resume from the same packet and record only the owner's exact
candidate verdict before considering this card or its milestone complete.

## Owner rejection and current-source correction — mandate r17

The owner rejected the dsp3 course in practice: opening the exact target from
Home reported that this app's connection could not read the pull request, then
Home returned to Sign in and Accounts & hosts appeared to lose the accepted
account. CLI was not reachable before device connection. Attended observations
390–392 retain those reports; releases 393–395 made the three affected questions
waiting and unprotected. The sequence-389 preparation above is historical, not
an active acceptance claim.

The supplied bundle was stale. Its source `94f6f885` predates BPR-0909/BPR-0913
CLI discovery, BPR-0914 (`1a74b36d`) accepted-CLI destination routing, and BPR-0915
(`cbd50b74`) shared sign-in. In the old composition, setup observations excluded
CLI profiles and opening required native credential authority; the resolver
could select an inactive native option and enter anonymous GitHub.com preview.
That explains the private read refusal and apparent sign-out without proving
that a permission response deleted a stored grant. Current main projects the
accepted exact-profile session and prefers it over an inactive same-host option.

New composed regressions cover GitHub.com with anonymous preview available,
visible CLI discovery before device connection, an exact-host CLI open, and a
403 read followed by an accessible retry. Both accepted CLI and synthetic device
sessions keep their account and connected server card; the denied read performs
no vault store/retrieve/delete, refresh-slot operation, new account construction
or device request. The read-host check additionally pins authentication-session
and transport identity across 403 and 404. The external tests-disabled CLI fixture
now serves current account discovery and the same synthetic 403 without supplying
consent. The composed course passes, and its fixture self-tests pass 3/3.

No new exact-target product observation or human acceptance is inferred. A fresh
tests-disabled bundle and actual offline HTML replace dsp3 before another review.

## Prepared current candidate — `7b591faa`

The replacement [offline review page](../../verification/reviews/rfc-bpr-0013-attended-v002/offline-package/index.html)
is generated at attended sequence 404. All three affected questions are ready,
protected and unanswered under revision `rfc0013-r17-7b591faa-ad50fe5d`.
The tests-disabled Release bundle is
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-rfc0013-r17/src/app/BetterPullReview.app`,
executable SHA-256
`ad50fe5d3d12b2056ded5bbbee4ccd4a9289850fe8ffa05c051b5b0408ae0657`.
Stable Developer ID signing and deep strict verification pass.

The actual bundle, driven through the native UI with an external synthetic gh,
offered CLI before device connection, retained Connected and the accepted account
after a 403, and retried an accessible review without another sign-in. It showed
two exact files, both loading messages, a readable selected diff and base/head
identities matching the synthetic object fixture. This is product evidence for
the repaired route, not a read of the private target.

Fifteen focused tests pass, plus the dedicated held-loading/refusal Qt gate and
the portable two-catalogue integration course (228.69 seconds). The full broad
Qt test still fails an unrelated wrapped-row layout assertion after correcting
its stale sign-in assertion. Release self-test retains the stage-0 CLI inventory
failure already recorded by BPR-0915; the native UI course above, not that probe,
is the passing product evidence. All 64 documentation tests, the documentation
gate and changed-file formatting pass. Exact results, logs and limitations are
in [the current evidence identity](../../verification/reviews/rfc-bpr-0013-attended-v002/session-identity.json).

The tested process used a verified disposable data root. A state query after
quitting automatically reopened the bundle against the ambient profile; it was
immediately quit with no further interaction. Fixed diagnostics show only that
start and shutdown, with no viewer, capability, PR or epoch operation. No claim
about stored credential state is inferred from that log. No explicit device or
global-account change, live target request, write, token extraction or private
content capture occurred. Disposable fixtures were moved to Trash. No task-owned
app remains. The exact-target readable-diff and anchor criterion is still open.
