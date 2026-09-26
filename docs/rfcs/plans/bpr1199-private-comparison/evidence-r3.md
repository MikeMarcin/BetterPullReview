# BPR-1199 exact owner acceptance

Assignment `bpr1199-comparison`, mandate r3, segment `owner-acceptance`,
milestone `private-comparison`. This documentation-only round consumes the
repaired candidate's owner observation. The original criteria and plan exits
remain unchanged; earlier [r2 evidence](evidence-r2.md) and its pending
acceptance record retain their historical meaning.

## Recorded decision and candidate

Session `2c2e72e3af2e2fd7fa1a566ec49bc5ca` records Accept
`9ad49b46-a10f-4ed3-b84e-a302a9d15fc1`, saved
`2026-09-26T19:21:47.498665+00:00`, for
`BPR-1199-private-comparison-v002`. Hand-back at
`2026-09-26T19:21:58.951824+00:00` advanced the cursor to 13.
The decision has an empty note and no attachments. Its fingerprint is
`af4b9daf518dda96d1e4d62c4080c6d53f6dd7481bf88fc3555482ebe41c935a`.

The exact question asks whether, after reopening the owner-named private PR
read-only on the repaired candidate, a selected file shows its base and head
diff. The instructions require selecting a file in Read. This explicit human
Accept is evidence of that rendered comparison; it is not inferred from a
process exit or from the session hand-back. The v001 Reject remains intact.

Reviewed source: `f90cda1bb6ae175806fac7e05fb1836f10d4439b`.
Frozen executable SHA-256:
`2db146a6f3d01ed0830b56996392fcc5b851ccce127dc642d49441f971cbefae`.
The build is Debug with tests enabled, not a release qualification. Fresh
intake verified the executable and all seven inventory entries against their
recorded hashes. The seven protected product/test paths are unchanged from
the reviewed source. Inventory SHA-256 is
`367118d9475a8b1fb8dbf87cb97e9c1016ae257c4a7ca37bb6a61af9eaeb6a8c`;
launcher SHA-256 is
`41b67668726bdefcdf90737dacbd320e2fadce170884a9d100a7cb334f602455`.

## Bounded objective observation

The owner launcher wrote `observations.json` under
`/Users/jaeger/.mask/attended-review/bpr1199-comparison-v002-runtime`.
Its SHA-256 is
`23384a713c4625d957664897799097eb4601c60f5e808bf6e601dc069059ff36`.
Only this already allowlisted capture was read; this round did not open the
raw private diagnostics log or access the private service.

The capture identifies the same source and executable, orderly application
exit 0, 17 fixed-code/numeric records, and zero omitted records. PR request 1
succeeds in window session 43. Epoch request 1 in that session succeeds through
cache preparation, pack sink, smart acquisition, protocol acquisition, cache
inventory and revision preparation. No recorded failure accompanies that run.
These facts establish successful acquisition, not pixels; the explicit owner
verdict supplies the rendered base/head observation.

## Original acceptance reconciliation

- **AC1 passes**, reusing r2's exact failure join: successful PR request 1 and
  failed epoch request 1 in window session 43 on v001, with
  `protocol_acquire / transport / response_limit / clone`. The exhausted
  sub-limit remains unknown; that uncertainty does not erase the observed
  boundary or become a claim about private bytes.
- **AC2 passes for the supported private comparison.** The actual target now
  renders base and head according to the exact owner question and Accept,
  corroborated by successful acquisition in the same candidate's capture.
  The r2 composed fixture also recovered over the failed course's same durable
  store and rendered both complete expected pane texts. The original failure
  was repaired by avoiding unnecessary complete repository acquisition; this
  result does not use a legitimate-limit refusal as a substitute for rendering.
  No remaining legitimate-limit refusal was observed on the accepted target,
  so the criterion's conditional limit/recovery alternative is not invoked.
  Existing transport and per-file bounds remain; this acceptance does not
  claim universal repository support, improved wording for every possible
  refusal, or an observed in-process Refresh recovery.
- **AC3 passes**, reusing r2's synthetic small-PR/large-repository course:
  original composition reproduces the exact diagnostic failure class, repaired
  composition renders the exact fixture base/head with real local Git/cache.
  The affected epoch, window, transport, workbench, logging and architecture
  checks pass 6/6; read fixtures pass 7/7 and publication compatibility 6/6.
- **AC4 passes** only for v002's exact source, executable and fingerprint,
  using the recorded human Accept. It neither changes the v001 Reject nor
  accepts another build, platform or issue.

All four unchanged milestone exits have matching evidence. The decision is
recorded in [acceptance-r3.json](acceptance-r3.json); no RFC status changes.

## Verification and command accounting

This round changes only the card and its acceptance/continuation documents.
Unchanged product evidence is reused: the r2 public fail-before/pass-after
course, focused checks, fixture courses, four changed translation units' tidy,
format checks and 64/64 documentation regression tests. The coordinator's
main integration at `50e729af`, recorded by `7b69e944`, additionally passed
epoch/window tests 2/2 and the documentation gate. No new product test or
build is justified by this verdict intake.

Fresh checks cover the frozen inventory, unchanged candidate paths, scoped
board validation, milestone binding/completion consistency, the documentation
gate after these edits, and `git diff --check`. Exact final commands/results
and the resulting commit are retained in the current [handoff](handoff.md).

The companion recorded the bounded intake observation at cursor 14. Its
`stop` command returned `stopped: true`; only v002 was released through
`resume-development --candidate BPR-1199-private-comparison-v002` at cursor
15. Status now has no host and retains v001 Reject and v002 Accept with both
protections released. No owner application was stopped. Both frozen runtimes
and durable review decisions are retained; no new temporary probe was created.
