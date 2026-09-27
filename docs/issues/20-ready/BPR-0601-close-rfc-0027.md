---
schema: "implement-rfc.card.v1"
id: "BPR-0601"
title: "Verify and close RFC-BPR-0027"
status: "Ready"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-0600","BPR-0602","BPR-0603","BPR-1215","BPR-1216","BPR-1217","BPR-1218","BPR-1219"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0027"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: automated controls/restart/invalid-value courses pass; files human Accept at cursor23 and revised controls Accept at cursor34; producing revisions `75700f4d`, `92c98361`, `0fb249a2`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Composed unfamiliar-error failure reaches correlated JSONL; useful classified prose, unchanged UI failure and explicit retry pass; fixture only; producing revisions `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M1\u2013M4 canary, fixed-capacity encoding, queue/ring/export and fail-closed parser checks pass within the exercised inputs; producing revisions `ac64e5ad`, `75700f4d`, `3fd1224e`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M3 barriers pass; M5 fixes deferred refresh and stamps captured epoch generation; native verification recorded below; producing revisions `75700f4d`, `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: frozen rich exports and direct Help preview course pass; revised export human Accept at cursor34; producing revisions `3fd1224e`, `189d24f9`, `0fb249a2`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"M2 bounds/rotation/concurrency and M5 actual isolated macOS ENOSPC pass; native Windows courses pass; producing revisions `3381a02c`, `92c98361`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Startup/cancel/shutdown and partial-tail checks pass; native killed-process restart preserves only explicitly flushed records; producing revisions `3381a02c`, `92c98361`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Architecture and composed lifetime/ownership checks pass; M5 repairs tests-disabled state visibility and Windows active-file sharing; producing revisions `189d24f9`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence.","outcome":"pass","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"PASS: pinned Quill/package proof, native macOS/Windows/Linux developer courses and all three human judgments accepted; release/package limits retained below; producing revisions `189d24f9`, `7ab11993`; exact commands and limitations: docs/verification/RFC-BPR-0027.md."}},{"criterion":"Canonical sequential RFC closeout","outcome":"pending","revision":"e2eec1d69340bbf0f519b6cae28eb462fb40966e","evidence":{"kind":"observation","detail":"Current AGENTS.md/index require sequential closeout; RFC0020/0021/0023 Deferred and RFC0022 Proposed. No explicit mandate r2 sequencing waiver. All nine product criteria accepted and M5 implementation pushed to main."}}]
---

# Verify and close RFC-BPR-0027

## Objective

Reconcile [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) against all linked
implementation cards, direct evidence and the shipped diagnostic path.

RFC base: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Requested RFC set: RFC-BPR-0027; workstream position: 1 of 1.
Planned cards: BPR-0600, BPR-0601, BPR-0602, BPR-0603, BPR-1215, BPR-1216,
BPR-1217, BPR-1218 and BPR-1219. Registration is not implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data. | Composed Diagnostics controls, four detail/privacy combinations, restart and invalid-setting fixtures | PASS: automated controls/restart/invalid-value courses pass; files human Accept at cursor23 and revised controls Accept at cursor34 | `75700f4d`, `92c98361`, `0fb249a2` |
| AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged. | Credential-free native/provider-to-host-to-file failure fixture with unfamiliar code and useful sanitized message; compare UI/authority | Composed unfamiliar-error failure reaches correlated JSONL; useful classified prose, unchanged UI failure and explicit retry pass; fixture only | `92c98361` |
| AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure. | Secret/private-content canaries across all sinks, queues, ring, export and failure fallbacks | M1–M4 canary, fixed-capacity encoding, queue/ring/export and fail-closed parser checks pass within the exercised inputs | `ac64e5ad`, `75700f4d`, `3fd1224e` |
| AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context. | Manual-executor mode-change, Off, Clear, sign-out and queued-event transition cases | M3 barriers pass; M5 fixes deferred refresh and stamps captured epoch generation; native verification recorded below | `75700f4d`, `92c98361` |
| AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically. | Preview/snapshot export course with joins, mixed modes, malformed lines and zero-network observation | PASS: frozen rich exports and direct Help preview course pass; revised export human Accept at cursor34 | `3fd1224e`, `189d24f9`, `0fb249a2` |
| AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out. | Bounded overload/error-storm, concurrent-process, rotation, retention and disk-full fixture | M2 bounds/rotation/concurrency and M5 actual isolated macOS ENOSPC pass; native Windows courses pass | `3381a02c`, `92c98361`, `7ab11993` |
| AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash. | Startup/cancel/quiescence, partial final line and sink-failure course | Startup/cancel/shutdown and partial-tail checks pass; native killed-process restart preserves only explicitly flushed records | `3381a02c`, `92c98361` |
| AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports. | Architecture guards, composed lifetime, fake filesystem attacks and profile-scope partition cases | Architecture and composed lifetime/ownership checks pass; M5 repairs tests-disabled state visibility and Windows active-file sharing | `189d24f9`, `7ab11993` |
| AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence. | Pinned dependency/license/packaging inspection plus native logging/UI lanes on each claimed platform | PASS: pinned Quill/package proof, native macOS/Windows/Linux developer courses and all three human judgments accepted; release/package limits retained below | `189d24f9`, `7ab11993` |

## Completion rules

Record exact executed commands, results and producing commits for each criterion.
All required linked cards must be done, including slices allocated after entry.
Use focused changed-surface checks and a composed public path, with native and
attended claims limited to actual evidence. Preserve unsupported-platform and
crash-durability limitations. Run the documentation gate and git diff hygiene,
then reconcile RFC/index status only when canonical order and dependencies allow.

## Foundation evidence and resume

BPR-0602 completed the owner-authorized phase 1 in `3d109fd4`. Its card records
passing native/provider/sanitizer, file persistence/cap, blocked-stderr cleanup,
composed application and dependency/boundary checks. Those establish the basic
subset of AC-02, AC-03 and AC-06 through AC-09; they do not complete the full
numbered criteria above. No new live sign-in ran and no live root cause is claimed.

Historical resume note: BPR-0600 and BPR-0602 completed the foundation.
The owner has now requested BPR-0603's full advanced phase under mandate r2
below; this closeout remains pending, with every full criterion retained.

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).
The bounded BPR-0602 confidentiality/lifetime review resolved all three findings;
no blocker remains in that subset. Opposite-family review was unavailable.

## Active continuation — mandate r2, 2026-09-26

The owner requests full RFC implementation and landing on main after each
accepted milestone. The old advanced-phase deferral is superseded; full criteria
and immutable RFC base remain unchanged. No live sign-in or PR mutation is
authorized. BPR-1215–1219 are the required source-owned milestones. Current continuation: all three human judgments are accepted at hand-back
cursor34. `/root/rfc0027_closeout` owns final engineering/upkeep under mandate
r2 in worktree `57ed`, branch `codex/rfc0027-advanced`. See the final checkpoint
for the accepted main landing and the canonical-order closeout hold.

Historical foundation and BPR-0971 evidence remain preserved. The table now reconciles all nine product criteria. Historical native and
attended checkpoints below retain their original meaning; the final checkpoint
records current acceptance and the remaining ordering obligation.

### M1 checkpoint

Source candidate: `ac64e5ad0dc4694dad4c6db3a40f12479b11503e`.
[Evidence and commands](../../verification/RFC-BPR-0027.md);
[plan](../../rfcs/plans/rfc-bpr-0027-advanced/MILESTONES.md);
[bound assignment](../../rfcs/plans/rfc-bpr-0027-advanced/m1-binding.json).
Changed surfaces: logging adapter/CMake, maintenance JSONL classifier, direct
adapter/public logging tests, format-dependent profile/benchmark assertions,
and owning architecture/privacy/operator documentation. No provider sanitizer,
core policy, UI controls or dependency version changed.

BPR-1215 supplies the structured-event subset of AC-02/03/06/07/08. All full
criteria in the table remain pending their advanced tests; no native/human gap
is waived. Next: BPR-1216 / `session-retention`, then controls, richer exports,
and native/attended closeout. No protected attended candidate or review server
was created for M1. All configure/build/test/tidy/documentation commands have
completed and no task-owned subprocess or descendant remains. The preserved
strategy and binding sit beside this continuation; the coordinator starts a
fresh implementer for the next milestone after the main push is recorded.

M1 landed and pushed as `857d93624aa3acb1e7f590969ae25aecf93a8993` (main and
origin/main matched after push). Source behavior is `ac64e5ad`; the intermediate
merge preserved concurrent review-tour documentation from `1248c0b1`. BPR-1215
is Done. [Completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m1-observation.json)
binds all M1 obligations/exits to that landed candidate. All commands are stopped;
there is no review URL, session, event cursor or protected candidate to transfer.
The appended handoff commit carries documentation only. The next engineer can
reuse this worktree and its offline `build/dev` cache, promote/claim BPR-1216,
and bind `session-retention` with `structured-events` accepted. Preserve the
full RFC criteria, mandate r2 and the existing BPR-0971 privacy boundary.

### M2 checkpoint — accepted and landed

Mandate r2; assignment `/root/rfc0027_m2`, BPR-1216 / `session-retention`.
Source candidate: `3381a02c5a78f4611fd7ab593cc0f6086ec32208`. Main `6e8ca8a8`
was synchronized before claim `fb27f251`; immutable RFC base is unchanged.
[Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m2-binding.json) and
[evidence/commands](../../verification/RFC-BPR-0027.md) bind this result.

Changed paths: logging adapter/CMake (native session files and limits), shared
pure filename grammar, SQLite maintenance enumeration, logger/retention and
public logging/profile/maintenance tests, registrations/support, and owning
architecture/operator docs. The provider sanitizer and core policy are unchanged.

Final exact-source integration passed 15/15, format 15/15, all three touched
product units passed tidy, and the documentation gate plus 64 tests passed.
The evidence ledger retains actual failures and fixes, partial-tail/resource
boundary self-review, and scope limits. BPR-1216 supplies the M2 retention and
lifecycle subset; all full-RFC criteria above remain pending later milestones.
No live service was used. Native Windows/Linux, packaged and human acceptance
remain open; the real native write failure uses RLIMIT_FSIZE, not a filled disk.

All owned build/test/tidy commands finished. There is no review URL/session,
event cursor, protected candidate or server. The source/acceptance landing was pushed as `8ff94031e1279c913cdb59512c2a9609e0779410`;
main and origin/main matched after push. This final documentation checkpoint
releases `/root/rfc0027_m2` writer ownership. The
[completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m2-observation.json)
binds every M2 obligation and exit to that landed candidate. Next eligible scope is
BPR-1217 / `diagnostic-controls` for a fresh implementer. Preserve mandate r2,
M1/M2 evidence and the full native/human criteria.

### M3 checkpoint — accepted and landed

Mandate r2; assignment `/root/rfc0027_m3`, BPR-1217 / `diagnostic-controls`.
Source candidate `75700f4dd0ce88f86e59940cddf690a6a3e2f45b`; merged candidate
`6089117d7bd13ad52d2b34c49713b842cd0f3954` preserves concurrent main `2deb889b`.
Claim `b56d0b2f`; immutable RFC base unchanged.
[Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m3-binding.json) and
[evidence/commands](../../verification/RFC-BPR-0027.md) define this checkpoint.

Changed paths: application diagnostic ports/settings host and metadata classifier;
logging encoder/queues/ring/native Clear; SQLite preferences/purge inventory;
Qt Settings/Help and composition lifecycle/sign-out; the fixed Verbose provider
producer; direct/provider/public tests and owning architecture/operator docs.
No credential-owner, core authority or dependency version change.

Affected backend/provider gates pass 9/9 and public composition 3/3, all 26 C++
files pass format, all 12 product units have passing tidy, documentation and its
64 tests pass. The ledger preserves the initial failures, fixes and earlier long
profile/lifecycle/180-second age checks. The merged-source rebuild passed and focused
public integration passed 5/5 in 35.90 seconds at `6089117d`. BPR-1217 is Done; source/acceptance landed and pushed as
`db4acde09b6346a3f88a251b2aeea6014bc7e0e3`. Main and origin/main matched after push.

This checkpoint releases `/root/rfc0027_m3` writer ownership. All task-owned
build/test/tidy commands have completed; process inspection found no remaining
owned test/build command. No review URL/session/event cursor, protected
candidate or server exists. Required full-RFC rich export and native/human
acceptance remain open. Next after accepted main landing: a fresh implementer
for BPR-1218 / `diagnostic-export`, preserving mandate r2 and all prior evidence.

The [completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m3-observation.json)
binds every M3 obligation and exit to that main landing. The appended handoff
commit changes documentation only. Reuse this clean checkout and offline build
cache, bind `diagnostic-export` with M1/M2/M3 accepted, and promote/claim BPR-1218.
The new recent-context/control ports are available to M4; current metadata
preview still reads files only and excludes all prose/identities. No source
writer, command, review server or protected candidate is transferred.

### M4 checkpoint

Mandate r2; assignment `/root/rfc0027_m4`, BPR-1218 / `diagnostic-export`.
Source candidate `3fd1224ebe72e9f2f22707eadb04cacb5586d466`; merged candidate
`9a46ddd802423c4749836a4e6094986452ea5092` preserves concurrent accepted
review-tour main `80a9551c`. Claim `d234a24f`; immutable RFC base unchanged.
[Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m4-binding.json) and
[evidence/commands](../../verification/RFC-BPR-0027.md) define this checkpoint.

Changed paths: core maintenance policy/options/confirmation; application rich
snapshot classifier and logger capture port; native file scope/flush/freeze;
Quill frozen session/ring capture; Qt selection/preview/disclosure; composition
wiring; core/parser/Qt/public tests; owning architecture/operator documentation.
The legacy metadata exporter remains compatible. No dependency version change.

Dev integration passed 9/9, core-only/boundary checks 6/6 before the final
preview corrections, final core/parser 2/2 and final Qt/public 4/4. All 18 C++
files pass format and seven product TUs have clean tidy. Documentation plus 64
regression tests pass. The ledger preserves actual failures/fixes and precise
claim limits. M4 implements full-RFC AC-05's export behavior and adds relevant
AC-01/03/08 evidence; M5 must reconcile the complete criteria without inferring
native, packaged, human or crash acceptance. No live service was used.

The next scope after accepted main landing is BPR-1219 / `native-closeout` for a
fresh sole engineering writer. Retain all M1–M4 evidence, mandate r2, native
Windows/Linux/package/crash limitations and the required human acceptance.

The exact merged-source rebuild passed and integration passed 5/5 in 26.45
seconds at `9a46ddd8`. No product edit followed verification. BPR-1218 acceptance
is bound to that candidate. All task-owned build/test/tidy commands are finished;
there is no attended review URL, session, cursor, protected candidate or server.

M4 landed and pushed as `5ef925d960c958509f535651e4ed3f4b72d8719a`; main and
origin/main match. This checkpoint releases `/root/rfc0027_m4` writer ownership.
The [completion observation](../../rfcs/plans/rfc-bpr-0027-advanced/m4-observation.json)
binds all M4 obligations and exits to that accepted landing. The final handoff
commit carries documentation only. No source writer, background command,
protected candidate or review server is transferred. Reuse the clean checkout
and existing offline dev/core-only caches. Bind `native-closeout` with M1–M4
accepted, then promote/claim BPR-1219 under mandate r2. Preserve every pending
native, package, crash and attended criterion; required human acceptance must
be prepared through the configured attended-review companion rather than
self-certified. No further M4 work is owed.


### M5 checkpoint — human pending, writer released

Mandate r2; assignment `/root/rfc0027_m5`, BPR-1219 / `native-closeout`.
Final product candidate `7ab119933439878d858100410083080ab4d1ea7b`; prior source
slices `92c98361` and `189d24f9`. [Binding](../../rfcs/plans/rfc-bpr-0027-advanced/m5-binding.json)
and [full evidence](../../verification/RFC-BPR-0027.md) retain every result,
failed intermediate course, package digest, command and qualification limit.

Changed paths: diagnostic settings refresh, epoch context generation, Windows
logging file sharing/deletion, host enum refusal, cache serializer comparison,
production review-source state visibility; diagnostic/provider fixture and
public/native failure tests; owning architecture/build docs and acceptance map.
All edits are committed in explicit slices. The Mac source scope passes 17/17,
then portability 5/5, then final affected 4/4; Linux native 15/15; Windows native
logging/control 12/12 plus direct architecture. Windows AppContainer boundary
launch failed, not counted as a pass. Actual macOS ENOSPC and abrupt native
termination/restart pass within their stated limits. All seven touched product
units pass tidy. The tests-disabled universal unpublished package and its
separate corresponding-source, notices, SPDX and relocated launch pass at
`189d24f9`; the later product correction is Windows-only. This does not qualify
Windows installers, supported Linux package baselines, distribution trust,
legal/source-offer approval or unflushed crash durability.

BPR-1219 is parked for human review; BPR-0603/BPR-0601 and RFC/index remain open.
M5 has not been accepted or landed on main. No human or independent acceptance
has been invented. The next engineering round must interpret recorded decisions
and then land accepted M5 under the user's mandate.

Attended session `c1565558e5fd3fdc277e62622604873b`, cursor **19**, candidates
`BPR-1219-controls`, `BPR-1219-files`, `BPR-1219-export`: one Accept/Reject
criterion each. The copied signed-out review app is `7ab11993`, SHA-256
`3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`, at
`/Users/jaeger/.codex/worktrees/57ed/BetterPullReview/build/rfc0027-m5/review/BetterPullReview.app`.
The manifest/identity/offline HTML are beside it. Every launch path is absolute;
three separate owner-private test profiles keep the review independent of the
owner's data. Prepared paths and the immutable copied app remain protected.

Only the explicitly transferred loopback review server remains running:
PID 8289, command session **64739**, port 63107. The complete token-bearing URL
was sent to the coordinator. All engineering commands and task-owned native
build workers are finished/stopped; the station's shared checkouts remain clean.
The isolated Windows/Linux build clones and local ignored build/evidence/package
paths remain available for inspection. There is no source writer to transfer.

The coordinator arms the exact watch before ending its human-facing turn:

```sh
python3 -B /Users/jaeger/.codex/worktrees/57ed/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/.codex/worktrees/57ed/BetterPullReview wait --after 19 --kind handed-back --timeout 43200
```

On return, read `status --after 19`, preserve partial/skip decisions, and bind
acceptance only to the recorded candidate. Source changes require the attended
conflict/release workflow before editing protected material. Reuse passing
unchanged evidence; main may advance independently and must be reconciled before
any eventual non-force M5 integration/push. No live sign-in or PR mutation is
authorized.


### M5 revision checkpoint — human pending, writer released

Mandate r2; assignment `/root/rfc0027_m5_revision`, BPR-1219 / `native-closeout`.
UI source `e0a870b3`, synchronized with main `f1e0c652` at
`0fb249a2499fa4752c645f29d19e1c768e9472e4`. The separate portable serializer
comparison correction is `41cce4bae80542a56fa842e8c98f00ffb9d9d57d`.
[Evidence and exact commands](../../verification/RFC-BPR-0027.md#m5-post-attended-ui-revision)
retain the checks, native results and original package/crash limits. The renewed
`m5-revision-binding.json` and `m5-revision-observation.json` in the existing
[plan directory](../../rfcs/plans/rfc-bpr-0027-advanced/MILESTONES.md) bind this
human-pending round. The full RFC criteria and authored milestone are unchanged.

Changed product paths: Qt diagnostic_settings_panel and release_maintenance_panel
headers/implementations; workbench_window Help actions; app
release_maintenance_controls header/implementation and github_application
composition; SQLite review_tour_codec.hpp. Focused public/Qt tests and owning
architecture/operator/RFC documentation changed with them. New native paired
selectors, short disclosures, direct Help exports and an automatically prepared
compact preview preserve capture barriers, frozen selection and local-only export.
The codec edit compares numeric bounds as uint64_t so MSVC accepts bool fields;
its format and behavior remain unchanged. It is outside the frozen UI candidate.
Exact integrated Mac composition passed 4/4 (50.44 seconds); changed Linux public
courses passed 2/2 (13.56 seconds); changed Windows public courses passed 2/2
(29.05 seconds). The codec's Mac storage course passed 1/1; its optional Windows
storage target could not compile because of a pre-existing missing platform
include, so no passing claim is made for that target. Existing logging, ENOSPC,
crash and unpublished package evidence is reused at its recorded revisions.

The owner accepted `BPR-1219-files` at cursor23 with no note, decision
`413e836b-5898-4aeb-a48c-43b21595d54b`. The immutable original app remains at
`/Users/jaeger/.codex/worktrees/57ed/BetterPullReview/build/rfc0027-m5/review/BetterPullReview.app`,
source `7ab119933439878d858100410083080ab4d1ea7b`, binary SHA-256
`3875a0f1704eef5c0655de82579960050acb9683ecd23257d2c1be753341b2f5`.
Its files fingerprint remains
`6eabaa3590e43c125423db6cd31edba6d685f2f919e94d55b7bd1a7cc0836ab7`.
The shared path release also cleared its protection flag; its decision and app
are unchanged and reserved by this checkpoint. Do not ask files again or overwrite
that app. Controls/export were rejected with the AllTheThings references; the
three attachments and the read-only source reference informed these revisions.

The same attended session `c1565558e5fd3fdc277e62622604873b` is ready at cursor
**31**, with exactly two pending Accept/Reject criteria:

- `BPR-1219-controls`: the paired settings are clear and ready to keep;
  fingerprint `86f20e8f40401168249b2c48b610d8d364af07d88f5d08fdae341cecbb377cd8`.
- `BPR-1219-export`: direct Help export and compact preview are clear and ready
  to keep; fingerprint `31b491264e3f3dfaaee7861c3de28adaf06b5ee39d7e712ad16fe2fbc0859d4a`.

Both candidates are protected. Exact source is `0fb249a2`; binary SHA-256 is
`9c4ae94b217cfd20585c648dca0bd5e74ce53adad91fc40a8cb4966a459e5186` at
`/Users/jaeger/.codex/worktrees/57ed/BetterPullReview/build/rfc0027-m5/review-v2/BetterPullReview.app`.
The identity, manifest and offline HTML are beside it. Each launch command uses
absolute paths and a separate owner-private test profile. Review stays signed
out; no live service or PR mutation is authorized. The native Mac inspection is
objective evidence only; replacement human judgments have not been supplied.

BPR-1219 is parked for human review. BPR-0603/BPR-0601 and the RFC/index remain
open. M5 acceptance and main landing await the two actual new verdicts. Main was
clean at `f1e0c652` when synchronized; recheck it before eventual non-force landing.
All task-owned engineering/build/test/native commands have completed. Isolated
native clones and ignored local evidence remain available for inspection. No
source writer is transferred. Only the inherited loopback review server remains:
PID **8289**, command session **64739**, port **63107**. Its complete token-bearing
URL is supplied directly to the coordinator, not stored in Git.

The coordinator arms the watch before yielding the human-facing turn:

```sh
python3 -B /Users/jaeger/.codex/worktrees/57ed/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/.codex/worktrees/57ed/BetterPullReview wait --after 31 --kind handed-back --timeout 43200
```

After handback, a fresh engineering owner reads `status --after 31` without
printing attachment base64, binds the milestone to the current obligations,
applies the two exact recorded decisions and preserves the original files accept.
If changes are requested, query conflicts then release only the affected
candidate/resource; a candidate and path selector are combined as OR by the tool.
Reuse unchanged evidence. When all required acceptance actually passes, reconcile
BPR-1219/BPR-0603/BPR-0601 and RFC/index, run the focused closeout gate, and safely
merge/push accepted M5 on main under the continuing mandate r2.


### M5 final checkpoint — accepted engineering, canonical closeout pending

Mandate r2; assignment `/root/rfc0027_closeout`, `native-closeout`. All three
human judgments are Accept at hand-back cursor34. The [final evidence](../../verification/RFC-BPR-0027.md#m5-final-engineering-round--mandate-r2)
binds exact decisions, fingerprints, source revisions and rechecked binary digests.
All nine product criteria have passing evidence; the complete original criteria,
immutable RFC base and native/package/crash limitations remain unchanged.

Changed paths in this round are the three live cards, RFC/index, verification
ledger, owning architecture qualification note and final binding/observation.
No product code or protected copied candidate changes. Earlier pending-review
checkpoints above are historical. Final build/test, landing and command state
are appended after their actual completion.

RFC terminal status is held by the current sequential closeout rule: earlier
RFC-0020/0021/0023 are Deferred and RFC-0022 is Proposed. The listed RFC-0027
dependencies 0001/0002 are Auto-Verified. Mandate r2 authorizes implementation
and main landing, not an inferred sequencing waiver. Next action: the coordinator
may request an explicit ordering amendment, or resume this same closeout after
those predecessors. No new human product verdict or unchanged broad rerun is due.


Accepted implementation and exact human decisions landed/pushed on main at
`e2eec1d69340bbf0f519b6cae28eb462fb40966e`. BPR-1219 is Done at `e42cc7e9`;
BPR-0603 is Done at `f4545095`. The primary checkout was clean and main/origin
matched immediately after the non-force push. Final public integration passed
4/4 in 46.21 seconds at source-identical `f0104f80`; documentation and diff
hygiene passed. Later changes are acceptance/card/link bookkeeping only.

All owned build, test, board and push commands completed before handoff. The
attended server was stopped through its companion CLI; PID 8289 is absent.
Coordinator watcher 47764 exited 0. No source writer, command or server is
transferred. Accepted copied apps, local native/package evidence and recorded
protections remain intact. This checkpoint releases `/root/rfc0027_closeout`.
The final binding and blocked observation in the adopted plan directory preserve
all obligations. Next action remains a specific ordering amendment or completion
of the four earlier RFCs, followed by BPR-0601/RFC/index closeout; no product
rebuild is required absent a relevant change.
