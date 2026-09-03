---
id: BPR-0532
title: Retain epoch history so old-revision continuations can anchor
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0521]
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
