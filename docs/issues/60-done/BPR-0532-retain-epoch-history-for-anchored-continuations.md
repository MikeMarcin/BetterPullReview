---
schema: "implement-rfc.card.v1"
id: "BPR-0532"
title: "Retain epoch history so old-revision continuations can anchor"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0521"]
---

# Retain epoch history so old-revision continuations can anchor

## Why

[[BPR-0521]] composes a side-chat turn against the kept session's old
revision: its supplement, the helper's pin, the frozen snapshot's binding,
and the disposable workspace are all the old revision's. What it cannot
supply is the old alignment. `core::epoch_state` holds one accepted revision
and one accepted comparison, and the workbench holds one alignment, so the
comparison digest, line counts, segments, groups, and moves of the revision
being continued against are retained nowhere once the head moves. Rather
than send the current comparison's identities beneath an older binding, the
continued turn freezes no per-path identities, and every finding it returns
arrives unanchored.

On 2026-09-02 the product owner accepted that as the shipped behaviour. This
card holds the alternative for whenever anchored continuations are wanted;
it is not part of RFC-BPR-0012's closeout and [[BPR-0507]] does not depend
on it.

## Required final state

- Either retained epoch history (the accepted comparison of a revision a live
  chat session still binds survives the head moving, bounded and evicted
  with the session) or a re-alignment request against the kept binding's
  epoch pair, chosen with a recorded reason.
- A turn continued against an old revision freezes that revision's own
  per-path identities and returns anchored findings validated against them;
  the silent re-anchoring [[BPR-0521]] refuses stays refused.
- Storage, memory, and eviction bounds are explicit and tested; core, host,
  and composition boundaries are unchanged in kind.
- `application_agent_chat_continued_contract` gains the anchored-continuation
  course and the empty-freeze course stays as the documented fallback when
  history is gone.

## Non-goals

- Any change to how a fresh session at the new head anchors — unchanged.
- Retaining history for sessions that are no longer live.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal. The decision the card
asks for is recorded here: of retained epoch history and re-alignment
against the kept binding's epoch pair, the slice implements re-alignment.
The reason: the tree's canonical diff records and per-path identities are
deterministic by contract, the SQLite epoch table already persists every
accepted revision's oids per ordinal, and the installed Git cache retains
history, so recomputing the old pair's alignment reproduces the same
per-path identities without a new persistent store, a retention bound, or
an eviction lifecycle; the cost is one recompute per continued turn, bounded
by the limits a fresh alignment already has. Retained history stays the
documented alternative. A scout confirmed the continuation path
(`dispatch_agent_invocation`'s empty freeze under continuation), the
single-slot overwrite of the accepted comparison, and the contract's
`foreach` course loop before any edit. The slice runs on `bpr-0532-work`
in a worktree after [[BPR-0533]], which edited the same dispatch.

## Part one landed — 2026-09-03

At `96804415`. The re-alignment request exists in the application layer:
`agent_realignment_request` names the kept binding's epoch pair and oids,
`agent_realignment_port` serves it, and `composed_agent_realignment`
resolves and materializes that pair through the workbench's selected-diff
and blob ports and computes the file diff with the existing core compute,
so the old revision's own per-path identities are reproduced rather than
retained. The bound is stated once — `maximum_agent_realignment_files`
(one file per request) and the request's own `diff_compute_limits` — and
a request past either is refused before any effect; missing objects refuse
as `objects_unavailable`. Three tests in `application_diff_host` cover the
exact retargeted lookup (the frozen digest and identity counts equal an
independent compute of the same pair), the missing-objects refusal, and
the bounds; deleting the retargeting assignments fails the first.

Reproduced on the merged tree: build clean, 20/20 selectors in 348 s, boundaries, tidy
and format on the touched sources; the pins held at their values (the slice adds no `src/app` source and touches no composition file).

Not yet delivered, and why part two exists: the composition half was
built and worked end to end — the old pair's seven segments frozen from
the kept binding — and then failed the new course and the previously
green `application_agent_chat_continued_contract` with `stale_result` on
the second turn, because the synchronous re-alignment on the gesture path
let the head's ingress supersede the turn (or delayed the dispatch past
it; unproven). It was reverted rather than landed red. Part two diagnoses
that with evidence and composes the re-alignment asynchronously or
authorizes the continued workspace against the kept generation, whichever
the evidence names.


## Part two diagnosed — 2026-09-04

Instrumented on the worktree and reverted. The continued turn's terminal
is `Superseded`, not the workspace `stale` check: about ten seconds after
the refresh a deferred epoch acceptance moves the comparison from ordinal
1 to 2, `observe` sees a changed binding and `supersede()` stops the active
invocation, whose `cancelled` completion the terminal rewrites to
`stale_result`. The same supersession happens with no re-alignment at all;
the shipped `continued` course passes only because its second turn starts
its process about 7.7 s before that window closes, and a synchronous
re-alignment — six seconds, of which the epoch host's `resolve` of the old
pair is 5.7 s queued behind the refresh's own work, the same off the GUI
thread — leaves 1.4 s. The course is latently racy today. A second
finding: at the moment of the continued send the accepted comparison is
still the old pair, so a re-alignment there is a no-op and a course that
sends then proves nothing.

Decided for part three: an invocation carrying a continued session is by
BPR-0521's design bound to a binding that already differs from the
observed one, so a binding change must not supersede it (a fresh session
is superseded exactly as today, with a negative case); the re-alignment
runs asynchronously off the gesture with a stated bound and the dispatch
follows its completion, the empty freeze remaining the refusal path; and
the anchored course waits for the comparison to reach the new pair before
it sends, so the retarget is real and the frozen identities are checked
against an independent compute of the old pair.

## Part three landed, and the card closes — 2026-09-03

At `317882df` on `main` (the slice rebased onto the merged platform lanes),
verified here on the integrator host after the two-pass repin.

**The supersession guard.** `agent_invocation_host.cpp` now tells a moved
binding from a changed review (`supersession_cause`), and `supersede` leaves
an invocation that carries a continued session running when only the
binding moved; a fresh one is superseded exactly as before. The terminal
judges a continuation against its own session's binding rather than the
revision on screen, and the delivery still states its staleness against
what is observed. Two host tests: a continued turn survives the binding
moving under it (no stop, `success`, delivery `head_moved`, finding
`anchored`), and a fresh turn under the same change is superseded (one
cancel, `stale_result`, workspace cleaned).

**The asynchronous seam.** `begin_continuation_realignment` in
`github_application.cpp` asks the epoch host to mint a re-alignment context
for the kept binding's pair — `epoch_application_host::realignment_context`
authorizes it into a second `RealignmentAuthority` slot beside the
reviewer's own selection, minted by the host and never assembled by a
caller, scoped to the review's actor and bounded to the same
host/repository/pull request — posts the recompute to the agent effects
lane, and dispatches from the completion through the one
`launch_agent_invocation` site; the gesture returns at once.
`maximum_agent_realignment_wait` (30 s) is armed on the gesture and
disarmed by the completion; past it the request is stopped and the turn
runs with the empty freeze, the reason recorded in
`LastContinuationRealignment` (`none`, `deadline`, or the refusal's stable
name). `ContinuationRealignmentsInFlight` joins quiescence.

**The anchored finding.** `agent_realignment_outcome` carries the re-aligned
old pair's `core::file_alignment` beside each frozen file (same one-file
bound). The gesture captures only where the reviewer is pointing
(`continuation_anchor`: file, side, line); the completion builds the
semantic context from the re-aligned old pair — its comparison digest,
preferred line, segments, and groups — at `approximate` confidence with no
segment or group identity claimed, so every identity the turn carries and
is validated against comes from the one alignment the recompute produced.
Out of the old revision's range nothing is said. BPR-0521's refusal of the
*current* revision's identities beneath an older binding stands. The
observation now sends the file path and review receipt under a
continuation (they name a file, not a pair of commits) while still refusing
the current semantic selection.

**The course.** `application_agent_chat_anchored_continuation_contract`
(the `anchored-continuation` item over `agent_chat_composition_public_path.cpp`)
drives the refresh, waits until the accepted comparison is the new pair,
sends the continued turn, and asserts the retarget was real
(`retargeted=1`), the frozen record byte-equal to an independent
`core::compute_file_diff` of the old pair, the finding `anchored (none)`,
and the turn `Finished: success`. The `continued` course keeps passing with
its truthful invariant. A host test,
`realignment_context_is_minted_scoped_and_never_assumed`, covers the new
slot: served for the kept pair, the reviewer's own selection still served
beside it, another pull request mints nothing, and a caller-assembled
retarget is `stale` before any cache work.

**Mutation.** With the continuation branch ignoring the re-alignment result
the anchored course fails at `continued-old-revision-composition`
(`realignment:none retargeted:1 frozen-files:0 frozen-segments:0
kept-segments:7`); restored, repinned, green.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The merged graph builds clean and the pins bind it | `cmake --build --preset dev`; the round-trip contract, two repins (`ef8d2fb9…`, `f2d2c4d5…`) | exit 0, zero `error:`/`warning:` lines; pass 3 green | this commit |
| Core, host, epoch, and every agent composition row including the new anchored course | `ctest --preset dev -j 2 -R '^(core_agent_session\|core_epoch_policy\|application_diff_host\|application_epoch_host\|application_epoch_contract\|application_agent_invocation_host\|application_agent_chat_(positive\|stale\|continued\|anchored_continuation\|construction_cut\|composition)_contract\|application_agent_invocation_(composition\|construction_cut\|gesture_cut)_contract\|application_agent_finding_(composition\|workshop_press)_contract\|composed_application_hosts\|application_composition_contract)$' --output-on-failure` | 18/18 in 181 s | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Strict tidy and format on the touched sources | `tidy.py --warnings-as-errors --paths` over the slice's `.cpp` files; `clang-format --dry-run --Werror` over its `.cpp`/`.hpp` | 7 translation units, zero errors, exit 0; exit 0 | this commit |
| The round-trip contract passes alone at HEAD | `ctest --preset dev -R '^application_review_round_trip_composition_contract$'` | passed (the timeouts seen in the worktrees today reproduced only under concurrent builds and never alone) | this commit |

**Left open, recorded rather than claimed.** The positional crossing can fall
outside the old revision's range on a file whose head grew; that turn carries
no selection and is not driven by a course. `Segments`/`Groups` are sent
without checking the semantic caps, mirroring the fresh path. The 30 s bound
and the `deadline` reason are not driven by a course. `agent_realignment.cpp`
has no unit test of its own; the widening is covered through the course. None
of these is required by this card's final state, and RFC-BPR-0012's closeout
([[BPR-0507]]) does not depend on this card.

