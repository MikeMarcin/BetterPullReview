---
id: BPR-0391
title: The native appearance probe's dark-side measurement outruns the owner
status: Parked
rfc: RFC-BPR-0006
parked_on: awaiting-owner-verdict
depends_on: []
---

# The native appearance probe's dark-side measurement outruns the owner

Found by the owner attempting the v004 foreground packet twice. The probe's
Light baseline politely waits for owner focus — on cocoa it loops printing
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` until the window is active with
a focused widget (`main.cpp:3962`). The Dark side never got the same
courtesy: once the local, global, and style-hint schemes all read Dark, the
appearance stage counts `SettleAttempts >= 50U` ticks (`main.cpp:4527`) —
around half a second at the observed ~90 ticks/second — and measures once,
requiring `isActiveWindow`, `activeWindow == &window`, and
`focusWidget == InitialFocus` at that instant (`main.cpp:956`).

A human operator necessarily surrenders frontmost to System Settings to
change the appearance, and cannot return focus to the exact initial widget
within a second of the scheme settling. The 2026-08-08 pass succeeded only
because the owner's standing authorization allowed scripting the scheme
change, so the app never lost focus. The attended procedure as the v004
packet writes it — a person in System Settings — is therefore humanly
impossible at these bytes: the owner's first attempt shows the exact
signature (`NATIVE_APPEARANCE_TRANSITION:Light->Dark` observed,
`NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED`, `NATIVE_APPEARANCE_STATE_FAILED`,
no rasters), preserved at
`docs/verification/reviews/appearance-scheme-foreground-v004/foreground-run-attempt-1.log`.

## Required final state

- After the schemes settle, the dark side waits for the owner's focus to
  return — the same window-active-and-focused condition the Light side
  already waits for, reported with its own waiting marker — bounded only by
  the run budget, before the one-shot measurement is taken.
- The geometry-rebuild capture semantics are unchanged: the instrumentation
  window still opens at the accepted Light baseline, so rebuilds caused by
  the transition are attributed whether or not the owner has returned yet.
- The offscreen contract proves the new wait cannot hang an automated run:
  on a platform where focus is synthesized the wait is satisfied
  immediately, and budget expiry through the wait still produces the
  complete marker record.
- The v004 packet is regenerated against repaired bytes with the procedure
  unchanged for the operator: click, switch, return, and the probe now
  waits for the return.

## Verification

```sh
ctest --preset dev -R '^(application_native_appearance_probe_contract|application_attended_launch_contract)$' --output-on-failure
```

## The dark side now waits — 2026-08-19

`native_appearance_owner_returned` (`src/app/main.cpp`) asks exactly what the
measurement asks: `InitialFocus != nullptr`, `isActiveWindow()`,
`QApplication::activeWindow() == &window`, and
`QApplication::focusWidget() == InitialFocus`. The appearance stage now consults
it after the settle counter elapses and before the one-shot measurement, prints
`NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN` once, and breaks out of the tick
until the answer changes.

Deliberately the *measurement's* condition rather than the Light side's weaker
one. The Light gate is `!isActiveWindow() || focusWidget() == nullptr`, which is
all it can ask before a baseline exists; waiting on that here would let the
measurement proceed with focus on some other widget and fail on a term the wait
could have waited for.

Geometry-rebuild attribution is unchanged in kind. The capture window still
opens at the accepted Light baseline — `InitialDiffGeometryRebuilds` and
`InitialDiffGeometryHistorySize` are recorded in
`prepare_native_appearance_probe`, untouched — so transition-caused rebuilds are
attributed whether or not the owner has returned. What changed is that the
window now closes when the owner returns rather than half a second after the
scheme settles, and `DiffGeometryAttributionComplete` still reports history
overflow rather than hiding it.

### Cannot hang: what was proven, and how

The card asks the offscreen contract to prove the wait is satisfied immediately
where focus is synthesized. It cannot, and the narrower truth is better than a
claim shaped to the request: **the appearance stage is unreachable offscreen.**
`prepare_native_appearance_probe` returns `platform_not_cocoa` when
`InitialPlatformName != "cocoa"`, before `Stage` becomes `appearance`, so no
offscreen course reaches the wait at all. The predicate is written without a
platform test — it would pass immediately anywhere focus is synthesized — but
nothing offscreen exercises it.

What is proven instead, and directly:

1. `native_appearance_probe_contract.cmake` now requires
   `NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN` to be **absent** from all three
   of its offscreen courses — the real-selection course, the 1 ms
   startup-budget course, and the cut-selection-wire course. If the wait ever
   becomes reachable in an automated run, that is the assertion that says so.
   Seeding the marker into the refusal path made the contract fail with
   `offscreen course entered the owner-return wait`, so the check is wired, not
   decorative; the seed was removed and the contract re-run clean.
2. The bound is structural and already covered. The budget is evaluated at the
   top of every tick, ahead of the `switch` that contains the wait
   (`src/app/main.cpp`), and expiry calls `capture_automated_timeout_evidence`,
   which records the failed stage and transitions to shutdown. The
   startup-budget course is the registered case that shows an expired budget
   still produces the complete marker record. A `break` out of the appearance
   stage therefore cannot outlive the run, by the same mechanism that already
   bounds the Light-side wait.

### Evidence

| Check | Result |
|---|---|
| `ctest --preset dev -R '^(application_native_appearance_probe_contract\|application_attended_launch_contract)$'` | 2/2 passed, 47.20 s |
| marker seeded into the refusal path, contract re-run | fails: `offscreen course entered the owner-return wait` |
| seed removed, contract re-run | 1/1 passed, 26.90 s |
| `clang-format --dry-run --Werror src/app/main.cpp` | clean |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Documentation: `docs/development/build-and-test.md` gains the dark-side wait
beside the Light-side one it already documented, including why the procedure was
impossible before and what bounds the wait.

### Corrected after review — 2026-08-19

An independent review confirmed both cannot-hang grounds and found three things the
record overstated. All three are corrected here rather than defended.

**"The startup-budget course is the registered case" generalizes across a branch
boundary.** `capture_automated_timeout_evidence` splits on
`!Appearance.StageEntered`, and `StageEntered` is set true *before* `Stage` becomes
`appearance`. So an expiry inside the new wait takes the other branch: the
precondition refusal stays `none` and the selected-diff evidence is not refreshed.
The startup-budget course expires with `StageEntered == false`, so it exercises the
branch the wait does not. An operator slower than the run budget therefore sees
`NATIVE_APPEARANCE_PRECONDITION_REFUSED:none:…` rather than anything naming the
wait. The complete marker record is still produced — the cannot-hang claim is
unaffected — but it does not name the wait, and no registered case covers that
branch.

**"Geometry-rebuild attribution is unchanged in kind" understates a
packet-visible effect.** `GeometryRebuildHistory` is a fixed 32-entry array that
stops recording and sets an overflow flag, and `DiffGeometryAttributionComplete`
requires no overflow. The window now stays open for however long the operator
takes rather than roughly half a second, so it has correspondingly more room to
reach 32. Separately,
`NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_OK` requires a delta of exactly one, and
the window now spans the operator's return trip, during which a window deactivate
and reactivate can add rebuilds. Neither term is part of `PresentationPassed`, so
both would report failure without failing the run, and the appearance stage is
unreachable offscreen so nothing automated covers either. That is a real change in
what the attended packet is likely to see, and the packet's audit list should be
read with it in mind.

**The waiting marker was armed once.** `WaitingForOwnerReturnReported` was never
cleared, so a second wait episode — scheme settles, owner returns, focus is lost
again before the measurement completes — would have been silent, leaving a gap in
the log nobody could attribute. It is now reset when the wait is satisfied.

What the review confirmed rather than broke: no argv or fixture reaches the
appearance stage off cocoa; the budget check precedes the stage switch on every
path and `transition_to_shutdown` sets the stage before the switch is evaluated, so
the wait's `break` cannot outlive the run; `SettleAttempts` is a `std::size_t`
incremented at roughly a hundred a second and cannot practically overflow; and the
geometry history stops recording rather than wrapping, so no stale entry can be
read back.

It also named honestly what the three absence assertions in the contract do and do
not prove: they are satisfied by unreachability, so nothing automated exercises
`native_appearance_owner_returned` in either polarity. The card already said the
stage is unreachable offscreen; this states the consequence for the predicate
itself.

### What remains

Only the attended bullet: the v004 packet regenerated against these bytes, and
an operator willing to click, switch, return. That is a person, not a gate.

The regeneration is deliberately held until the run boundary rather than done
here. A packet binds an executable digest, and v004 exists only because v003
bound a `dev` executable that no longer existed at its recorded digest — the
procedure became unexecutable by construction. [[BPR-0392]] has already changed
the executable's path and bytes, and [[BPR-0390]] will change them again, so a
packet generated now would be a v005 that needs a v006 before anyone could run
it. The card stays in progress until the bytes settle, then gets one packet and
parks.


## Parked on the operator — 2026-08-19

The packet is regenerated at
`docs/verification/reviews/appearance-scheme-foreground-v005/operator-packet.md`,
bound to the bundled executable
`sha256:6ad6ad39b7c7b593b222c2c7c20ee567e78331f3034f73ce8d48a6f201bf8753` and to
BPR-0249's implementation record. It was deliberately held until the bytes settled,
because v004 exists only because v003 bound an executable that no longer matched
its digest.

The procedure is unchanged for the operator — click, switch, return — and the one
substantive difference is stated up front: after the schemes settle the probe now
prints `NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN` and waits, so there is no
longer half a second to beat.

Two honest warnings are in the packet rather than discovered during the run. The
geometry capture window now spans the whole return trip, so attribution overflow
and a rebuild delta above one are both likelier and neither is a reason to rerun.
And a budget expiry while waiting is a clean result to report rather than an error
to edit around — which is the branch the review found no registered case covers.

## The bytes had not settled after all — v006, 2026-08-19

v005 was never run, and this card's own earlier paragraph said why it would not
be: "[[BPR-0390]] will change them again, so a packet generated now would be a
v005 that needs a v006 before anyone could run it." That is what happened.
BPR-0390's blob-read repair landed after v005 was written, and the bundled
executable is now
`sha256:4e93884097bc92d4c56eb8f12712245ed4e0c2b173e1bed7a441cc8072e20c64` at
51,050,832 bytes.

A changed byte reopens the gate, so v005 is preserved as history and the live
packet is
`docs/verification/reviews/appearance-scheme-foreground-v006/operator-packet.md`.
Nothing the operator does changed between v005 and v006: same procedure, same
markers, same two warnings, same acceptance statement, new digest and new
artifact paths. The packet says so at the top so a reader of v005 can skip
straight to the procedure.

The three pending attended packets — this one, BPR-0100's, and BPR-0168's — now
bind the same executable, so one attended session can serve all three.

The card stays parked on the operator.

## Rebound once more, and this time the bytes are settled — 2026-08-19

[[BPR-0397]] bounded three unbounded waits in the epoch host and rebuilt the
executable, so the previous packet's digest no longer matched anything on disk.
Third rebuild in a row to invalidate a pending packet; caught before an operator
hit it rather than after.

The live packet is
`docs/verification/reviews/appearance-scheme-foreground-v007/operator-packet.md`,
bound to
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93` at
51,085,872 bytes.

Why this one should hold where three did not: every card that could change these
bytes is in `60-done/`, and nothing in an active lane is executable without a
Windows or Linux host, a product decision, or an owner verdict. The bytes are
settled because the board is.

The preflights are stronger than any earlier version's, too — the complete
193-test `dev` matrix passed at exactly these bytes rather than a focused
selector set.

The card stays parked on the operator.

## Operator direction — drive it — 2026-08-20

Recorded verbatim:

> you can drive this

This extends the standing authorisation. It previously covered scripting the
appearance change but **not** focus, and the probe requires a frontmost window —
which is why what remained was described as "one click". That click is now mine
to make.

The extension is to *driving the probe*, not to judging its result. [[BPR-0220]]
rides the same run and asks a causal question — whether an appearance change
rebuilds the comparison geometry twice because there are two legitimate causes or
because one change is applied twice. Producing the log is now unattended;
reading it is analysis, and if it turns out to need a judgement about acceptable
behaviour rather than a fact about causes, that comes back.

The packet's bound binary is unchanged at
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93`, so the
run is against exactly the bytes the packet describes.

One consequence worth stating before it happens: the probe changes the operating
system appearance setting and restores it. Earlier runs of this family recorded
restoring the owner's original System Dark setting, and this one must do the same.
