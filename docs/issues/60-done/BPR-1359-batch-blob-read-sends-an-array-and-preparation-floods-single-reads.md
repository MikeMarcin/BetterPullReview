---
schema: "implement-rfc.card.v1"
id: "BPR-1359"
title: "macOS: the batched blob read sends a JSON array (HTTP 400), and preparation's single-read fallback holds the reviewer's cold read"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"bug","found_by":"BPR-1344 live probe on MikeMarcin/BPR-Test#43 (branch claude/bpr-1344-live-probe, 9bf77510d and c83e0596b, not merged)","reservation":"docs/issues/_reservations/BPR-1359--BPR-1359--e55491ea630f.json","related_cards":["BPR-1344","BPR-1327"],"acceptance":["BPR-1359:batch-body","BPR-1359:yield","BPR-1359:back-off"]}
acceptance: [{"criterion":"BPR-1359:batch-body","outcome":"pass","revision":"493adffc5","evidence":{"kind":"command","path":"tests/github_contract/gh_cli_git_smart_transport_tests.cpp","detail":"gh_cli_git_smart_transport: the fake gh parses the batched read's exact stdin with QJsonDocument and requires one object of a string query and an object of variables. Fails on the array body ('the batched read's body is one JSON object ...'), passes on glz::write_json(document). macOS arm64 Debug; no live call."}},{"criterion":"BPR-1359:yield","outcome":"pass","revision":"2bbd5e975","evidence":{"kind":"command","path":"tests/application/epoch_application_host_tests.cpp","detail":"application_epoch_host a_reviewer_selection_stops_preparations_exact_read: a prepared exact read held 20 s on the actor by the fake transport is stopped by a cold reviewer resolve (cancelled, stop observed; reviewer waited 1.5-7 ms in three runs); a prepared read queued ahead of reviewer work is not started; the reviewer's own blob is read. Without the host change the reviewer waits 20000 ms and the course fails."}},{"criterion":"BPR-1359:back-off","outcome":"pass","revision":"2bbd5e975","evidence":{"kind":"command","path":"tests/application/epoch_application_host_tests.cpp","detail":"application_epoch_host a_failed_batch_backs_off_preparations_single_reads: after a refused batch a prepared blob is not read alone; the reviewer's opened file is; a later successful batch lifts the suspension. Fails without the host change at 'after a failed batch, a prepared file's blob is not read on its own'."}}]
---

# BPR-1359: the batched blob read sends a JSON array, and preparation's single-read fallback holds the reviewer's cold read

## Finding

A live probe for [[BPR-1344]] (local branch `claude/bpr-1344-live-probe`,
commits `9bf77510d` and `c83e0596b`, measurement only, never merged) drove the
shipped composition offscreen against `MikeMarcin/BPR-Test#43` through the
GitHub CLI sign-in.

1. **The batch body is an array.** `compile_gh_cli_exact_blobs_request`
   (`src/adapters/github/src/gh_cli_transport.cpp`) built its body as
   `glz::write_json(glz::generic{ glz::generic::object_t{ … } })`. The braces
   construct a `glz::generic` holding an array of one object, so `gh api
   graphql --input -` sent `[{"query":…,"variables":…}]`. GitHub answered
   HTTP 400 (`gh: Body should be a JSON object`) to 36 of 36 batch requests:
   [[BPR-1327]]'s batched read has never worked live on macOS. The contract
   test's fake `gh` matched substrings of the body, so it passed either way.
2. **The fallback holds the reviewer.** With every batch refused, preparation
   still materialized each prepared file's blobs: the diff host computes a
   prepared comparison, and the epoch host reads each missing blob it names
   with one exact read on the repository actor (five `gh` processes, about
   2.1 s). A reviewer's cold selection queues behind it on the same actor and
   waited about 1.9 s. A selection already stops the diff host's prepared job,
   but only once the reviewer's own resolve has finished on the actor, which
   is the wait.
3. **The fallback floods.** A prefetch-on run made 710 `gh` processes against
   122 with prefetch off. The port's own contract says a transport that
   cannot batch reads nothing ahead; the per-blob fallback did the opposite.

## Fix

- The body is the object itself (`glz::write_json(document)`).
- A reviewer's resolve, and a read of a blob the reviewer's selected grant
  names, stop the actor's in-flight prepared exact read (its `gh` process is
  killed, as `cancel_preparation` kills the batch's) before queueing, and no
  prepared exact read starts while reviewer work is queued on that actor.
- Back-off: a batch that fails for any reason but cancellation suspends
  prepared exact reads under that blob authority until a later batch
  succeeds or the authority changes. A blob a working batch answers absent
  (binary, truncated, over 512 KiB, or not its object) may still be read on
  its own, as the port allows. The reviewer's own reads are never suspended.

## Relation to BPR-1344

The Windows station is redesigning preparation scheduling under [[BPR-1344]]:
preparation waits for an idle reviewer, and prepared blob reads count as
preparation. This card keeps to the transport body and to the fallback's
cancellation and back-off, all inside the epoch host's prepared exact read
(`authority_effect_actor`'s prepared-read fields and `materialize`'s prepared
branch). When BPR-1344's scheduler owns when prepared reads run, the yield
here becomes a second line of defence for a read already in flight, and the
back-off stays the rule for a transport whose batch fails. BPR-1344's live
figures from the probe were all taken with the array body, so they measured
the fallback, not batching; they should be retaken on this fix.

BPR-1344 has no card on `main` yet (only its reservation,
`docs/issues/_reservations/BPR-1344--BPR-1344--7a3656cbb78d.json`), so this
relation note is recorded here and waits to be copied onto BPR-1344's card
when the Windows station files it. [[BPR-1360]] (a cold open reads both sides
at once) is untouched by this card: both of a cold open's reads are the
reviewer's own and never yield.

## Acceptance

- `BPR-1359:batch-body`: the fake `gh` in `gh_cli_git_smart_transport`
  parses the exact standard input of the batched read and refuses anything
  but a JSON object whose `query` is a string and whose `variables` is an
  object. Fails on the array body, passes on the fix.
- `BPR-1359:yield`: a host-level test with a fake transport puts a prepared
  exact read in flight on the actor, then resolves a cold file as the
  reviewer: the prepared read is stopped and reports cancelled, and the
  reviewer's resolve completes without waiting out the read. Fails before
  the fix.
- `BPR-1359:back-off`: after a refused batch, a prepared file's blob is not
  read on its own; the reviewer's selected blob still is; a later batch that
  succeeds lifts the suspension.

## Verification

macOS arm64, `build/dev` (Debug) in worktree `/Users/jaeger/bpr-wt/bpr1359`,
tests run at `02e6b1057` and unchanged by the rebase onto `4ea2eb2a0` (no
upstream commit touches these files). No live GitHub call was made.

- `gh_cli_git_smart_transport` (`BPR-1359:batch-body`): the fake `gh` parses
  the batched read's exact standard input with `QJsonDocument` and exits 9
  unless it is one object of exactly a string `query` and an object
  `variables`. **Failing-before control:** on the array body it fails at
  "the batched read's body is one JSON object with a string query and an
  object of variables, not an array holding one". Passes on the fix.
- `application_epoch_host`, new
  `a_reviewer_selection_stops_preparations_exact_read` (`BPR-1359:yield`): a
  prepared exact read held 20 s by the fake transport (which ends early when
  stopped, as `gh` is killed) is on the actor; the reviewer resolves a cold
  file. The prepared read reports cancelled, its stop is observed, and the
  reviewer waited 1.5 to 7 ms in three runs. A prepared read queued ahead of
  reviewer work is not started; the reviewer's own blob is still read.
  **Failing-before control:** without the host change the reviewer waited
  20000 ms and the course fails at "the reviewer's selection stops
  preparation's exact read on the actor".
- `application_epoch_host`, new
  `a_failed_batch_backs_off_preparations_single_reads`
  (`BPR-1359:back-off`): after a refused batch, a prepared blob is not read
  on its own (no transport call); the reviewer's opened file is; a later
  successful batch lifts the suspension, and a blob it answers absent is then
  read alone. **Failing-before control:** without the host change it fails at
  "after a failed batch, a prepared file's blob is not read on its own".
- `gh_cli_transport` passes. clang-format 22.1.8 clean on every touched file;
  clang-tidy 22 on `epoch_application_host.cpp` clean, on
  `gh_cli_transport.cpp` only the two `using T` naming findings at lines 55
  and 66, which predate this card. The composition contract's three pinned
  digests still match (no pinned scope changed).

Not run: the composed application (`application_large_review_prepared_open`
and friends; a full Qt build was not justified on this loaded host for a
change inside the epoch host's sparse path), and any live course. The live
re-measurement on `MikeMarcin/BPR-Test#43` belongs to BPR-1344's course,
with `X-RateLimit-Remaining` checked first. Windows and Linux have not built
this change; the transport fix is macOS-only code, the host change is
portable.
