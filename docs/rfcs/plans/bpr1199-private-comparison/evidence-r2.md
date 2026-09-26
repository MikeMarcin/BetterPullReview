# BPR-1199 owner rejection and comparison repair

Assignment `bpr1199-comparison`, mandate r2, milestone `private-comparison`.
Source candidate is recorded in the current [handoff](handoff.md). The original
AC1–AC4 remain intact.

## Owner observation consumed

Session `2c2e72e3af2e2fd7fa1a566ec49bc5ca`, hand-back cursor 6, records final
Reject `a74d529b-4aff-4662-b97a-a78d0277b255` for
`BPR-1199-private-comparison-v001`, fingerprint
`ef60b7926f20db034da29907f0d9e30c4d5bf2f7aeb48e4981f3083599d0d212`, source
`c982586fbfc774614c6f4993c4e6c81d450d15be`, executable SHA-256
`4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58`.
The owner again saw no file comparison and objected that this had not been
confirmed before asking for review. The separate Overview folder clarification
is assigned elsewhere. No private image, file path, title, author or source is
copied here.

The owner-launched frozen app remained running at intake (PID 9045), so its
quit-time capture had not written observations.json. Direct bounded local-log
inspection retained only the existing allowlisted codes and numeric IDs. At
2026-09-26 18:24:33 UTC, PR request 1 succeeds in window session 43. Epoch
request 1 in that same session succeeds through catalogue, cache and pack-sink
preparation, then fails at 18:25:11 UTC with `protocol_acquire`, domain
`transport`, reason `response_limit`, step `clone`. The later preparation
terminal maps this to `unavailable`. This establishes AC1's boundary and join.
It does not distinguish stdout/stderr, per-file or aggregate temporary-store
limits, nor prove which bytes exhausted the owner's particular clone.

## Demonstrated defect and correction

A valid PR with at most 100 files and commits selected complete repository
closure even on a sparse-capable CLI connection. PR counts do not bound
repository history or unrelated blobs. The fix permits selected-object loading
for every valid accepted shape; absent/inconsistent/out-of-range counts retain
the conservative fallback. Actual sparse capability on the exact bound lease
still gates current and historical acquisition, recovery and selected blob
materialization. Non-sparse leases retain complete acquisition and their
explicit missing-blob result.

The existing sparse implementation supplies exact base/head commit trees,
durable residency markers, and only the selected tree-authorized blob. Stale
selection, authority replacement and unsupported leases still refuse remote
materialization. No clone limits, qualified tools, process bounds, credential
handling, publication admission, or native transport were broadened.

This fixes a reproducible failure class consistent with the owner's record.
It does not claim that the repaired candidate has already rendered the
owner's private repository. AC2 and AC4 remain pending that exact observation.

## Direct verification

Runtime evidence is outside Git at
`/Users/jaeger/.codex/tmp/bpr1199-r2-hpt7dqm5`. Builds used parallelism two;
Qt ran offscreen and serially with the other milestone owners.

- `red-unit.log`: new shape and host assertions fail against the original
  product. Focused final epoch host, window host and real local CLI/Git bridge
  pass 3/3. The host asserts small-PR sparse residency, exact selected-object
  authorization, no fabricated blob grants, revocation and non-sparse behavior.
- The new external `small-large-repository` fixture has two changed files and
  an unrelated, deterministic 34 MiB ancestor blob. A real local Git fetch of
  complete closure exceeds the unchanged clone bound. The sparse path retains
  under 1 MiB of commit/tree material and zero blobs. No remote service runs.
- `public_probe_red` links the original classifier into the actual production
  composition, with a closed external synthetic gh executable, isolated
  profile and real Git/cache. Home → Use GitHub CLI → one read opt-in → Open PR
  fails with the same `protocol_acquire / transport / response_limit / clone`
  class. `red-public-final.log` and the synthetic profile log record that
  terminal. Source was restored in a finally block and rebuilt.
- `public_probe` links the repaired composition. Reopening over that failed
  course's same durable data succeeds, and the real Read modality renders
  review.cpp. Both complete pane texts equal the expected Git fixture blobs
  (apart from presentation trailing whitespace). The full-window
  `baseline-course/window.png` was inspected: six base lines 11–16 and six
  head lines 21–26 are visible. `green-public-final.log`: exit 0, one read
  opt-in, ready epoch, exact base/head, screenshot and orderly shutdown.
- The first probe attempt failed before CLI discovery under an additional
  outer macOS sandbox before the product CLI launch could succeed. It is
  setup failure, not comparison evidence. The corrected probe uses the
  product's normal CLI write-denial boundary and only the closed local fake
  executable. An intermediate screenshot remained on Overview; the final
  probe explicitly selects Read and is the rendering evidence.
- Final workbench host, composed logger and architecture boundary checks pass
  3/3. Final read fixture tests pass 7/7, publish fixture compatibility 6/6,
  documentation regression tests 64/64, and the documentation gate and
  formatting pass. Four changed translation units pass tidy; existing
  unrelated naming/string-view warnings remain. One fixture rerun exposed an
  underscore in a temporary clone-root name, outside its allowed grammar;
  the test now uses an exclusive random hex root and passes. No independent, private-live,
  tests-disabled, release or native-platform qualification is inferred.

The external probe and captures are disposable verification material, not
product hooks. The committed synthetic fixture and host regressions preserve
the failing-class coverage. The previous review host is stopped and its sole
consumed candidate released at cursor 7; the frozen owner app is untouched.

## Repaired candidate

Product source `f90cda1bb6ae175806fac7e05fb1836f10d4439b`; frozen Debug,
tests-enabled executable SHA-256
`2db146a6f3d01ed0830b56996392fcc5b851ccce127dc642d49441f971cbefae`.
Runtime `/Users/jaeger/.mask/attended-review/bpr1199-comparison-v002-runtime`
holds the app, seven-file inventory, identity, absolute launcher, offline HTML
and future allowlisted observations. Launcher canary self-test passes. The
owner must quit the previous app before this launcher starts the new one.
The full review identity and live server are recorded in the handoff.
