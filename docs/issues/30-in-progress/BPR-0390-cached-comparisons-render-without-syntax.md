---
id: BPR-0390
title: Cache-served comparisons render without syntax classification
status: In Progress
rfc: RFC-BPR-0006
depends_on: []
---

# Cache-served comparisons render without syntax classification

Found by the owner during BPR-0100's attended visual pass, minutes into
looking: `UIforETW/UIforETWDlg.cpp` — real C++ — renders monochrome in the
dark profile, no keyword, comment, or string coloring at all, while the
status line reports `Diff ready — 8 semantic changes (cached)` and every
composition marker stays green.

The mechanical facts, probed at the same binary:

- `SYNTAX_CLASSIFIER_COMPOSED_OK` — the classifier is composed.
- The KSyntaxHighlighting definitions are embedded in the executable.
- The automated `uiforetw-74` offscreen course reports
  `SYNTAX_CLASSIFICATION:1:2` — two submissions, one adopted — so even the
  green path adopts only half of what it submits.
- `adopt_classification` (`src/app/src/github_application.cpp:3993`) drops
  any publication whose blob no longer matches the current comparison's
  left/right key, deliberately and silently, and
  `adopt_token_spans` returning false is also uncounted.

The first hypothesis — cache-served comparisons never resubmit — is
contradicted by the live wiring and is withdrawn. `request_diff`
(`github_application.cpp:3938`) calls `classify_comparison` on every display
including diff-cache hits. The identified mechanism is a race with three
compounding parts:

- Span adoption and the diff result are two independent, unordered
  deliveries into the same pane: `diff_workbench::publish` never touches
  spans, while `clear_projection` (via `set_loading` on every new request)
  wipes them, so whichever arrives second decides whether code is colored.
- Classification identity rides a single latest-write-wins mutable context
  (`composition_blob_source::Context`, `github_application.cpp:1288`) read
  off-thread at execution time; there is no per-submission binding of blob
  to comparison beyond the cache key.
- `classification_in_flight` coalescing
  (`appearance_application_host.cpp:897`) starves resubmissions while an
  earlier comparison's classification is outstanding, and completions that
  land after supersession fail `adopt_classification`'s Current/Pending
  check and drop silently. The counters show it: the automated two-file
  course records `SYNTAX_CLASSIFICATION:1:2` with four blobs in play.

The instrumented run settled it, and both prior candidates were wrong. New
durable counters — cache hits, coalesced, dropped-at-adoption, failure
publications, terminals entering the decision lane — plus a
`SYNTAX_DISPLAYED:<left>:<right>` present-tense marker report
`SYNTAX_DELIVERY:0:0:0:1:2` with `SYNTAX_DISPLAYED:1:0` on the automated
course. Read together: nothing was coalesced, nothing was dropped at
adoption, and both submitted terminals eventually arrived — but one arrived
only when shutdown retired its record, meaning that classification stayed
outstanding for the entire session while its pane rendered monochrome. And
only two submissions exist where a two-comparison course owes four: one
comparison's `classify_comparison` never ran at all. Two independent
defects, then: **(A)** a display path that fails to submit for one of the
comparisons, and **(B)** a submitted classification whose effect never
completes — with no timeout, no retry, and no marker until shutdown refuses
it.

Defect B's blocking point is identified in code. The classification
effect's only blocking call is `composition_blob_source::blob_bytes` →
`epoch_application_host::materialize`, whose caller side waits on a
condition with **no timeout and no use of the caller's stop token**
(`epoch_application_host.cpp:3494`); the enqueued actor work is FIFO behind
whatever the repository actor is already running, and epoch maintenance can
hold that actor for most of a minute. Shutdown's `BlobIngressClosing` is
exactly what releases the wait — which is why the starved terminal arrived
only after `retire_all` had already refused its record. Once refused,
nothing retries: `classification_in_flight` keeps coalescing every later
display of the same key, so the pane stays monochrome for the session. The
repair therefore has three coupled parts: a bounded, stop-aware wait for
classification blob reads; in-flight bookkeeping that clears on failure so
a later display resubmits; and defect A's missing submission on the other
comparison, still to be traced. The markers
ask whether the classifier was composed and whether some classification was
ever adopted; nobody asks whether the comparison on screen is classified,
and no registered contract greps `SYNTAX_CLASSIFICATION` at all. This is
BPR-0216's shape wearing syntax colors.

## Required final state

- The dominant drop point is confirmed with one instrumented run:
  coalesced-before-submission versus completed-after-supersession.
- A displayed comparison's panes carry the classification for their exact
  blobs regardless of whether the comparison was computed or cache-served,
  with stale-result rejection preserved. The host shape follows the
  architecture: adopt synchronously from `AppearanceHost::cached` on
  display, bind classification identity to the request rather than shared
  mutable state, and keep the blob-identity drop policy as defense in
  depth, not the primary mechanism.
- A deterministic check fails when the selected comparison renders
  unclassified for a file whose grammar is embedded — asking about the
  screen, not the composition.
- The attended visual pass can be re-presented against repaired bytes.

## Verification

```sh
ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|qt_diff_workbench_scale_100|application_workbench_composition_contract)$' --output-on-failure
```

## Two of this card's own findings were stale — 2026-08-19

Re-measured before implementing anything, on the recorded `uiforetw-74` course:

```
SYNTAX_CLASSIFICATION:3:6
SYNTAX_DELIVERY:2:0:4:1:6
SYNTAX_DISPLAYED:0:0
```

**Defect A does not exist as written.** 6 submissions plus 2 cache hits is 8
`classify` calls, and the course displays four comparisons — eight side-blobs, of
which six are distinct because `Resource.h` is displayed twice. Every blob is
accounted for. The `SYNTAX_CLASSIFICATION:1:2` this card recorded is stale; no
submission is missing.

The hypothesised mechanism could not have produced the symptom either.
`set_loading` is the only writer of `diff_workbench::CurrentContext`, and it is
skipped in the *same* early return as `classify_comparison`, so had that branch
been taken the later publication would have been rejected as stale and the
comparison would not have displayed at all. "Displayed but unclassified" cannot
come from there.

**Defect B is not reproducing on this course either.** Terminals equal
submissions, so nothing is outstanding; the unbounded materialize wait is real
but is not what this course hits.

**What is actually wrong is adoption.** 5 parses plus 2 cache hits is 7
publications; 3 were adopted and **4 were dropped**, and both panes ended
uncoloured. That is the card's own third bullet — two unordered deliveries into
one pane, `set_loading` clearing spans on every new request, and
`adopt_classification` refusing anything whose blob is no longer the current
comparison's. A classification arriving slightly early is wiped; one arriving
slightly late is refused.

So the repair the card asks for is right and the diagnosis behind it was not.

## The repair — slice one

**Adopt at display time.** `adopt_published_classification` runs from the diff
publication sink and asks the surface itself what it is showing —
`current_result()->Alignment.Key == aPublication.Context.Key` — rather than
trusting the retained selection. That guard matters:
`composition_diff_projection::publish` forwards *every* publication to the sink
while `diff_workbench::publish` accepts only the current one, so a sink that
classified unconditionally could colour a pane showing something else.

Adoption reads `AppearanceHost::cached`, which existed with no callers outside
its own translation unit. The submission in `classify_comparison` is deliberately
kept alongside it, for a reason worth recording: `cached` resolves through
`retained`, which does **not** bump `CacheSequence`, so replacing the submission
with a cache read would stop refreshing the retention order and make a
repeatedly displayed blob evictable.

The definition needed to rebuild the cache key comes from a bounded
`blob → definition` binding recorded at submission, not from the file's name. That
is what lets display-time adoption work on the `--diff-workbench-probe` course
too, whose only diff request comes from a fixture helper that never sets
`SelectedDiffPublication` — a path that would have been silently uncovered had
adoption been keyed on the retained selection.

**Per-blob identity.** `composition_blob_source` no longer holds one mutable
`diff_request_context`. Each submitted blob is bound to the exact
`diff_blob_request` that reads it, insert-if-absent — a blob is content-addressed,
so two comparisons naming it want identical bytes, and keeping the first binding
means no later display can move a read already in flight. Bounded at 16
bindings, eight comparisons' worth; past that a read is **refused rather than
evicted**, because evicting would move an in-flight read, which is the defect
being replaced. Bindings are released when a terminal arrives and cleared with the
selection.

`adopt_classification`'s blob-identity drop policy is untouched, as the card
requires: defence in depth, not the primary mechanism.

## The deterministic check, and why it is not `1:1`

`SYNTAX_DISPLAYED` reports *presence* of a span list, which is too weak to be the
check. `definition_for_path` falls back to plain text rather than failing, and a
plain-text classification still publishes a non-null list, so a pane can hold a
span list and be entirely uncoloured. New `SYNTAX_DISPLAYED_SPANS:<left>:<right>:<display-adopted>:<shown>`
reports counts plus whether a comparison is on screen at all — without the last
field a count of zero cannot distinguish "shown and uncoloured", the defect, from
"nothing shown", which is not.

That fourth field immediately answered a question this card would otherwise have
got wrong. On the `application_diff_workbench_public_path` course the reading is
`SYNTAX_DISPLAYED_SPANS:0:0:3:0`: three display adoptions happened, and **no
comparison is displayed** by the time the status dumps. Its `SYNTAX_DISPLAYED:0:0`
was never evidence about classification, and [[BPR-0393]]'s deliberately loose
`[01]:[01]` term must stay loose — tightening it to `1:1` would assert something
false about a course that has nothing on screen.

`application_uiforetw_selected_diff_contract` now requires a comparison to be
shown and its left pane to carry a nonzero span count.

### Measured

| Course | Before | After |
|---|---|---|
| `uiforetw-74` | `SYNTAX_DISPLAYED:0:0` | `SYNTAX_DISPLAYED:1:0`, `SYNTAX_DISPLAYED_SPANS:1361:0:5:1` |
| `diff-workbench-probe` | `SYNTAX_DISPLAYED:0:0` | `0:0`, `SYNTAX_DISPLAYED_SPANS:0:0:3:0` — nothing shown |

The left pane went from no spans to **1361**. Removing only the
`adopt_published_classification` call fails the new assertion with `UIforETW light
rendered the selected comparison unclassified`; restoring it passes. That was run,
not assumed.

| Check | Result |
|---|---|
| `ctest -R '^(application_uiforetw_selected_diff_contract\|qt_diff_workbench_scale_100\|application_workbench_composition_contract\|application_diff_workbench_public_path)$'` | 4/4 passed |
| assertion negative proof | fails without the repair, passes with it |
| `clang-format --dry-run --Werror` on both app sources | clean |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| `application_composition_contract`, `application_review_round_trip_composition_contract` | passed after re-pinning two drifted digests |

Both digests were recomputed independently rather than copied from the failure
messages, and agreed: the eight-source manifest is
`e4ca325deacff0e876c679d162d75abb82dfe8440243cba292f845852cb83707` and the
full LF-normalized `github_application.cpp` is
`c847ec226967620d2bc38a9e1495d614ee194791c30d5657c3974948093ebb23`.

## What remains — the right pane, and slice two

The card's required final state says a displayed comparison's **panes** carry
their classification. One of two does. The final comparison's right blob is still
uncoloured, and the counters place it as the single recorded
`ClassificationFailures` entry — 6 submissions, 6 terminals, 1 failure, and no
cache entry for that blob at display time, so nothing exists to adopt. Whether
that failure is a refused blob read or a refused span list is **not** established
here, and this card does not claim it is.

Still open on this card:

- Which blob the single classification failure belongs to, and why.
- The attended visual pass re-presented against repaired bytes.

## Slice two: the wait that could not end — 2026-08-19

The card located this precisely and it was right: `materialize`'s caller side waited
on a condition with **no timeout and no use of the caller's stop token**. The stop
token was not merely unused — it could not have worked. `aStop` is forwarded into
the per-job stop source *inside the enqueued closure*, so a caller cancelling while
the work is still queued behind the actor's FIFO cannot reach it. Only shutdown's
`BlobIngressClosing` released the wait, which is why the starved terminal in the
card's instrumented run arrived after `retire_all` had already refused its record.

Both halves are now closed, and they do different jobs:

**Prompt release: a real stop token.** `composition_blob_source` passed a
default-constructed `std::stop_token{}` — one with no stop state, which can never
signal — so there was nothing for even a correct wait to honour. It now owns a
`std::stop_source` stopped by `clear_comparison`, so a read for a comparison the
reviewer has left ends when they leave. The source is swapped under the lock and
stopped outside it, so a read being released does not contend for the mutex its own
completion path wants.

**Backstop: a bound.** `epoch_host_limits::BlobWait` defaults to 90 seconds,
validated non-zero and at most ten minutes. Deliberately generous: work is enqueued
behind whatever the actor is running and epoch maintenance can hold it for most of a
minute, so a tight bound would fail legitimate reads. Promptness is the stop token's
job; this is what releases a read whose actor never finishes at all.

The wait now distinguishes its two exits — `cancelled` for the reviewer moving on,
`failure` for an elapsed bound — rather than collapsing both. Abandoning is safe
because `waiter` is a shared owner: the closure still completes and writes into a
record nothing reads.

The `std::stop_callback` is registered **before** the lock is taken. A token that has
already been stopped invokes its callback during construction, and doing that while
holding the waiter's mutex would deadlock. `Abandoned` is set under the mutex before
notifying, so no wakeup is lost — the predicate is evaluated on entry.

### Evidence

| Check | Result |
|---|---|
| `application_epoch_host` with three new limit cases | passed |
| blob-wait validation deleted, re-run | fails: `a zero blob-read bound is rejected at composition` |
| validation restored | passed |
| `application_uiforetw_selected_diff_contract`, `application_diff_workbench_public_path`, `application_epoch_persistence_github-com`, `application_epoch_maintenance_public_path`, `application_host` | 5/5 passed |
| `application_composition_contract`, `application_review_round_trip_composition_contract` | passed after re-pinning both drifted digests |
| `clang-format --dry-run --Werror` on all three changed files | clean |

### Deliberately not done

The three sibling waits in the same file — `resolve`, `materialize_agent_workspace`,
and `cleanup_agent_workspace` — share the identical unbounded shape. They are real
latent hangs and they are **not** this card's: bounding them changes the behaviour of
the selected-diff resolution and agent workspace paths, which this card makes no
claim about. Factoring the four into one helper is the obvious follow-up and wants
its own card and its own matrix.
- The attended visual pass re-presented against repaired bytes.
