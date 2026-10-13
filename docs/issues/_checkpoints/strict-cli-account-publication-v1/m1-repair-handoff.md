# M1 critical-review repair handoff

Mandate `strict-cli-publication-r2`; strategy `coordinator-implementer`; plan `strict-cli-account-publication-v1`; milestone `strict-credential-execution`; constituent obligations BPR-1425 and BPR-1426. This fresh post-review leaf owned only `/Users/jaeger/.codex/worktrees/22e7/BetterPullReview` on `codex/strict-cli-account-publication`. Baseline is `aa3d71fe470627e1e78488eb51a9114a788f33bd`; starting tracker revision was `25647104beeacfc6db11e1f350913e890e349e57`, prior product `29b4002e509991da38522a209d8a7f6e7be7765e`. Repaired frozen product is `cb41d1a1609d69373871bc3dd9321f59dcd60cc0`. Only documentation bookkeeping follows that product commit.

## Milestone result

P1 and P2 were independently reproduced on the prior product and repaired. Whole-review recovery loads and validates the exact durable SQLite row before credential acquisition. Confirmed receipts replay locally; possibly-transmitted rows remain indeterminate on credential refusal/cancellation/tool error and stay pending. New requests that never acquired or dispatched remain definitely unpublished with the draft payload retained. Tour refusal likewise preserves its durable unknown/confirmed record. No same-key retry repeats POST.

Acquisition now carries both its fixed process failure category and whether a completed named lookup refused. Only that named refusal maps to credential unavailable; a failed capability probe is a tool failure, including a completed nonzero probe. Read, inline, review, tour and Git blob/clone consumers preserve distinct tool/cancel versus credential categories without raw token/process errors. Credential ownership remains entirely adapter-owned, one immutable owner per operation, with no native fallback, global cache or BPR plaintext/persistent token store.

The independent bounded critical recheck of `cb41d1a1609d69373871bc3dd9321f59dcd60cc0` is supplied in `/tmp/bpr-strict-cli-run/critical-m1-recheck.md`: both blockers no longer reproduce, no blocking repair finding. The original report `/tmp/bpr-strict-cli-run/critical-m1-review.md` is retained as history. Done cards remain terminal; original candidate acceptance is preserved and current revision-bound engineering records/review disposition are added in metadata and the correction addenda. No new ID, counter, plan, reservation, index or main change was made.

## Verification

The six focused contracts pass: architecture boundaries, CLI transport, sensitive runner, whole-review provider, staged tour and real strict Git fixture (68.14 s). The architecture mutation contract passes 151 cases. Three current full application fixture-transport regressions pass after relink (22.74 s): inline consent/publication, whole review and tour reconciliation. The prior twelve composed courses are regression evidence only, not strict credential proof.

Two explicit courses select the real strict adapter and production runner over a canonical copy of the same test executable, with real SQLite. Whole-review uses one acquisition, five API calls and one write for each initial unknown/confirmed case. Tour uses one acquisition for 47 API calls/five writes; its cancelled unknown case uses eight API calls/one write. Both remove chosen lookup availability after POST, exercise real named missing refusal and injected cancelled/timeout/crash/reap failures, preserve exact draft/unknown/confirmed records, and recover read-only with no second POST. A new explicit tour Resume reacquires and sends only remaining steps. Runtime-random synthetic canaries are absent from generic arguments/environment/stdin/output and BPR-created configuration, recovery/SQLite and response trees; the fixture credential-source file is explicitly external test input.

macOS refuses nested sandbox initialization, as the existing Git strict fixture already documents. The new strict courses are explicit local-only selectors outside the CTest network wrapper, retaining the production write-denied child sandbox. No new test-isolation exception was added. Replay them from any directory with:

```sh
TMPDIR=/private/tmp /Users/jaeger/.codex/worktrees/22e7/BetterPullReview/build/dev/tests/github_contract/better_pull_review_live_review_submission_provider_tests --strict-cli-fixture
TMPDIR=/private/tmp /Users/jaeger/.codex/worktrees/22e7/BetterPullReview/build/dev/tests/github_contract/staged_review_session --strict-cli-fixture
```

Actual credential-bearing descendants now verify inherited credentials while the sensitive owner is held; timeout/cancel terminate and drain the leader/descendant before release. Existing quarantine evidence stalls before execution and proves queued reap-failure ownership only. Actual credential-bearing descendant reap failure was not safely forced and remains a material evidence limitation; no concrete lifetime flaw was reproduced. Qt, OS and child copies remain outside precise-erasure claims.

Ten changed C++ files pass exact clang-format 22. Seven changed translation units pass targeted tidy. The staged test TU has six diagnostics on unchanged baseline lines; `/tmp/bpr-strict-cli-run/m1-repair-baseline-gates.json` proves that, and no changed-line diagnostic remains. Prior global syntax-fixture format and 14 application naming diagnostics remain untouched; no global passing claim is made. Documentation contract passes 67 tests. The final bookkeeping gate is recorded by the completion observation.

[Repair evidence](m1-repair-evidence.json) names the ten changed paths, commands, log hashes, failed attempts, acceptance records and independent receipt. Failed fixture/probe attempts remain recorded. The one failed task-owned strict tour scratch root `/private/tmp/bpr-staged-9046830` was removed after command completion; successful fixture roots cleaned themselves. No token-bearing logs or build/test outputs were committed.

## Review handoff and continuation

M1 engineering and its bounded independent repair acceptance are complete. Native official-gh supported-macOS AC04 remains M2/BPR-1427, with the original host classes and strict helper negatives still required. Popup M3 is untouched and cannot begin until actual first-RFC native acceptance. Historical BPR-1373 owner judgment remains bound to its original candidate. The r2 credential-unavailable amendment and original proposal provenance are preserved. `frontend-design` was loaded for status-facing scope; native palette, fonts, controls and text were not redesigned.

The coordinator starts a fresh M2 owner with this checkpoint, selection, exact product and material limits. That owner may retrieve live credentials only under its own M2 authorization, use only `MikeMarcin/BPR-Test` for remote testing, record actual response rate headers, and preserve the shared active gh account/configuration. This M1 round retrieved no live credential and made no live GitHub, owner-desktop/browser, push/merge or main mutation.

## Ownership and command state

At handoff all task-owned build/test/tidy/architecture/documentation commands and fixture descendants are complete. The implementer releases source and tracker writer ownership; no review server is running. Product source is frozen at `cb41d1a1609d69373871bc3dd9321f59dcd60cc0`. The only subsequent commit records tracker/evidence/handoff state; the private completion observation binds its final clean HEAD. Strategy host loading stays `not_verified`.
