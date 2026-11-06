---
schema: "implement-rfc.card.v1"
id: "BPR-1898"
title: "Separate Overview progress readiness from network status"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1897"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1898--BPR-1898--605c229d551e.json","milestone_plan":{"id":"responsiveness-latency-v10","path":"docs/rfcs/plans/responsiveness-latency-v10/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance; retain existing Status sections."}
---

# BPR-1898: Separate Overview progress readiness from network status

Continue the owner's direct responsiveness work after independent network
Status sections. Local receipts and private records have their own asynchronous
loads; an unfinished load must not look like zero completed work or keep ready
reviewer/merge/check sections in a loading state.

## Acceptance

- AC1: Represent local progress readiness separately from network Status;
  publish available progress as its scoped local inputs complete and explicitly
  identify still-loading or unavailable inputs rather than inventing zeroes.
- AC2: Preserve durable progress meanings, file/filter/account/PR identity,
  action authority, cancellation/retirement, and existing final values. Reuse
  the current asynchronous store ownership and avoid duplicate effects.
- AC3: A composed held-local-load course demonstrates usable network Status
  while progress loads, then correct progress completion without disturbing
  check selection or disclosures; failure and replacement remain explicit.
- AC4: Pass focused optimized functional/interaction and coding/architecture/
  documentation checks, quantify the improvement or independent readiness,
  add a journal line and land the verified round on main.

## Plan

Inspect the existing ledger completion boundaries and use narrow readiness and
presentation updates. Preserve the accepted authority required to open private
storage. Reuse the My review section and current loading presentation, applying
frontend-design desktop guidance; no new dependency or general-purpose system.

## Implementation and direct evidence

The accepted Overview carries separate readiness for file receipts and private
records. A store still opening and an uncompleted read show Loading; an actual
failure shows Unavailable. Known file counts can appear while private notes still
load. Unknown counts never appear as zero, and the read meter waits for receipts.
The existing My review header also identifies pending or unavailable data when
collapsed. Completed network Status sections remain usable. The file heading
also qualifies unread progress, and the last-review marker waits for private
records instead of prematurely saying that no previous review exists.

On store opening, file receipts are requested before conversation receipts and
private records. All effects retain the existing serialized store ownership,
generation guards, exact file/filter keys and accepted account/PR authority.
Failed initial reads publish their state without recursively retrying themselves;
later refreshes retain their existing behavior and known values.

Direct implementation applies frontend-design desktop and code-simplifier
guidance to the existing surface. No delegated agents, global host setting
changes, owner desktop access, new dependency or new storage lane are involved.

Evidence root:
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`.

The composed Release course holds all local effects while network Status
completes, then delivers local completions individually. Both PRs show ready
file progress while private records are still held. A durable nonzero read mark
survives reopening its own PR and is never inherited by the other PR. The fixture
then hides only its own SQLite receipt table: the actual failed read shows
Unavailable while private records finish and checks remain usable. Check
selection and section disclosure survive the independent publications.

The old-order control fails the exact early-file-progress assertion
(`progress-old-order-control.log`), and the candidate order is restored. This is
evidence of independent readiness, not a claim about total or live-network load
time. No Debug timing course is run or credited.

Initial failures are retained: a Qt test tried to find a widget subclass without
Q_OBJECT and now finds its QWidget base; the first composed check caught the
accepted projection preceding store attachment, which now correctly remains
Loading until the scoped store answers. The corrected composed course passes
(`progress-functional-2.log` and its detailed log). Those earlier failed runs are
not passing evidence.

## Refresh fixture coverage

The broader public courses exposed stale fixture ordering from the earlier
conversation-first work: repeated paged scripts and the second head of a seed
script still expected files before conversation reads. Refresh therefore failed
the fixture's exact request match before either course reached its later checks
(`progress-functional-final.log`, `progress-scaling-diagnostic.log`).

The existing refresh-recording adapter now also covers those paths and advancing
history. Thread/comment pages precede viewed-file pages, retaining the recorded
cursors and payloads and recompiling the canonical sequence numbers. Repeated
paged reads select their own refresh script; seeded second heads keep their
identity and use the refresh order. Production transport validation and ordering
are unchanged. The scaling course now reports its refresh state on failure.

## Verification

Thirteen distinct focused Release courses pass across the recorded runs:
ledger host, overview policy/widget, composed window at 100%/125%/200%, overview
chips at 125%/200%, progressive opening, active-changed scaling, commit filters,
ledger composition and the history-session race. The initial run's nine passing
courses are in `progress-functional-final.log`; both corrected replay courses
pass in `progress-replay-functional.log`. After the final loading-label change,
all six affected UI courses and ledger composition pass again in
`progress-integrated-functional.log`. That run also records a missing history
test executable, with no coverage credited; its target was then built and the
course passes in `progress-history-functional.log`.

The final composed render course passes (`progress-render-final.log`, command
in `progress-render-final-command.json`). It proves network-ready/local-pending
and files-ready/private-pending on both PRs, exactly two receipt reads and one
private-record read per opening, and explicit SQLite failure with usable checks.
All three final offscreen renders were inspected in `progress-render/`:
`status-progress-loading.png`, `status-files-before-notes.png` and
`status-progress-unavailable.png`. The native layout stays legible without
overlap; unfinished last-review history and file progress are also explicit.

The full durable-progress course covers seed, reopening and unavailable storage;
the commit-filter course covers split receipts, filtering and force-push rematch.
The scaling course preserves incremental receipt work and the exact fixture
transport. These are focused functional and structural checks, not a new latency
benchmark. The prior round's unchanged core-only and documentation-script
regression evidence is reused.

Main was concurrently rebased after BPR-1897 landed. Only this round's own
promotion, claim and implementation were replayed onto
`cf574d4fc5cbf313a91b73aca6b3fb1130bb850a`; no earlier history was merged back. The
BPR-1897 card now cites its current source hash,
`7ab0a3f2d287428cfd2e2c803ca6c4af694bb831`. The explicit milestone binding remains
valid; the prior global discovery limitation on unrelated amended plans is
unchanged. One journal line records the local-progress improvement.

Both architecture courses pass (`progress-architecture.log`, 2/2), as do
clang-format 22 on all nine touched C++ files (`progress-format-final.log`),
documentation (`progress-docs-candidate.log`) and scoped board validation
(`progress-board-candidate.json`). The source manifest records the verified C++
file hashes; no measurement ran alongside a build or analyzer started here.

Full clang-tidy 22 ran on all seven touched translation units
(`progress-tidy.log`): six pass, and application composition reports only an
existing fixture-local naming violation. Renaming that local and its four uses
to `goOnToReviewers` changes no behavior; its focused naming recheck passes
(`progress-naming-recheck.log`) and the composition rebuild passes
(`progress-build-naming.log`). The otherwise unchanged full-analysis evidence
is reused after that identifier-only correction.

The rebuilt final composition passes the progressive public path again
(`progress-final-public-path.log`), including independent readiness, exact local
read counts, durable replacement and the real SQLite failure. The final naming
format check passes (`progress-naming-format.log`). All acceptance evidence is
complete; source landing and guarded card completion follow.
