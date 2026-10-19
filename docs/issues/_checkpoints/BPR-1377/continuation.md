# BPR-1377 milestone handoff

Status: accepted and Done. Assignment `/root/tour_ready`, milestone
`bpr1377-sparse-tour-source/pinned-source-over-sparse-gh`, local `main`.
Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh.
Run `bpr-closeout-20261018`, segment `tour-ready-1377`, mandate
`owner-ready-backlog-20261018-r2`.

## Mandate and amendments

Start closing ready/backlog work. Leave Insights to its named separate chat;
exclude RFC-BPR-0086, BPR-1608–1615 and all Insights work. Owner amendment:
"some of these older cards might be out of date and no longer relevant so
watch for that." Current baseline reproduction and live source verification
established BPR-1377 remains relevant. Its three original criteria are unchanged.
The coordinator authorized a narrow account-pinning prerequisite needed to
reach this card's current gh CLI path; no broad discovery system was added.

Discovery returned no authored plan for BPR-1377 or BPR-1378. This implementer
prepared/adopted one BPR-1377 milestone with its exact original criteria as
exits. [Selection](selection.json), [binding](binding.json),
[source plan](milestone-plan.source.json) and the adopted live plan preserve it.

## Revisions and engineering result

- Defect baseline: `55ea64194211eb42194d6dd9fc3e7ae11c9aefa4`.
- Existing RFC-BPR-0044 execution RFC_BASE_SHA:
  `89f23eeece835ecafb9f8752b524644b22fbf281`, unchanged.
- Plan: `6463853b8`; claim: `296b17d9d`.
- Source candidate: `530a7b94d` (eight explicit paths).
- Acceptance/finish: `120a9ea9c121a9493942084327d6199e4e202a5b`.

The source worker passes a sealed endpoint to a per-call epoch source read;
exact path/key/commit/blob, the 256 KiB bound, physical retained sparse
commit/tree, cancellation and final authority checks remain. No ordinary
selected/prepared blob grant is installed. CLI initialization observes and pins
one active, signed-in, keyring-held gh login on SourceEffects before shared
strict admission, with no global switch or credential/transport fallback.

Product/test paths: `src/app/src/tour_remote.cpp`;
`src/application/include/bpr/application/epoch_application_host.hpp`;
`src/application/include/bpr/application/review_tour_source.hpp`;
`src/application/src/epoch_application_host.cpp`;
`src/application/src/review_tour_source.cpp`;
`tests/application/epoch_application_host_tests.cpp`;
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`;
`docs/development/tour-cli.md`. Board finish also repaired BPR-1222's live link.
[Verification](verification.md) records failures, exact commands and evidence.

## Verification and acceptance

- Sparse public-source fixtures cover LEFT and RIGHT without a desktop selection.
- Replacing only the new source read with the former ordinary call fails the
  exact public sparse-source assertion (CTest exit 8); restored bytes and
  rebuild pass. Forged path/key, size, Stop, ordinary-read non-grant,
  comparison replacement and invalidation controls pass.
- Final focused integration: tour_cli_composition, application_epoch_host and
  application_review_tour_host, 3/3 pass in 4.51 s.
- Live BPR-Test#49: local add and show --source match RIGHT lines 1–2 of
  bpr-2acct/identity/review-me.txt; head
  2e1d8b415ce33ad1284b2f2ca34e82734c07ddc1, blob
  a4c178354da79ee0a3b425d7f0f319e5defe5fba. No remote writes or desktop.
  Header rate margin: 4978/4977 before, 4745 after.
- Inactive, environment-only and ambiguous active-account stub controls: 3/3
  refuse with authentication_required before any credential/API stub exists.
- Format 22.1.8, scoped diff check and provider-inventory/app source-manifest
  contract pass; production mutation pin remains unchanged. Concurrent BPR-1619
  app wiring was inspected for deterministic reseal, not certified as engine
  acceptance. Its source commit 381ec842b is on main and its work is preserved.
- Documentation gate: DOCUMENTATION_OK after the sibling's temporary missing
  BPR-1619 verification link was published. No sibling card was edited here.

The exploratory full gh loopback harness remains unchanged and unclaimed;
its setup drift was outside this defect. The original alternative sparse
fixture criterion is met by the committed public host fixture and old-call
negative control. Parent tour/package/native qualification gaps stay open.

## Ownership, cleanup and continuation

All task-owned build/test/live/diagnostic commands have exited. No background
writer, review server, protected candidate or human/native/independent decision
remains for this milestone. The task-owned build/bpr1377-tests tree and consumed
negative-control backup were removed; shared dependency caches and unrelated
writers/files remain. Temporary fixture/live roots were removed by their owning
contexts. Useful non-secret command evidence remains outside Git at
`/tmp/bpr-closeout-20261018/tour-*.log` for the coordinator.

The final runtime observation/check and return are at
`/tmp/bpr-closeout-20261018/tour-observation.json`, `tour-check.json` and
`tour-return.md`; these bind the final checkpoint commit without a self-referential
Git revision. The coordinator owns further dispatch. Next eligible issue:
BPR-1378 (Ready; no dependencies). First reassess its current Stop behavior under
the owner's obsolete-card amendment, then prepare/adopt its own milestone.
