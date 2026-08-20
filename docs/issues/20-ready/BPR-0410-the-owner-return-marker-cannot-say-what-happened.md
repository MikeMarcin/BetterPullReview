---
id: BPR-0410
title: The owner-return marker cannot say what happened
status: Ready
rfc: RFC-BPR-0006
depends_on: []
---

# The owner-return marker cannot say what happened

The foreground appearance probe has now failed three times with an identical
signature, across two packet versions, one of which was written specifically to
fix it:

| attempt | packet | result |
|---|---|---|
| 2026-08-08 ×2 | v004 | `TRANSITION:Light->Dark`, then `ACTIVE_FOCUS_FAILED`, no rasters |
| 2026-08-20 | v007 | `TRANSITION:Light->Dark`, then `ACTIVE_FOCUS_FAILED`, no rasters |

[[BPR-0391]] diagnosed the first two and added a wait so the dark side would
stop measuring into the owner's absence. v005 shipped it. The third attempt
produced the same signature anyway.

## Why nobody can tell whether the wait worked

`NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN` is printed behind a latch
(`main.cpp:648`, set at `:4615`, cleared at `:4624`). It is emitted **once, on
the first tick where the owner-return condition is false** — which is the moment
the window appears, before the owner could possibly have clicked anything. In
the 2026-08-20 log it is line 3, ahead of `SELECTED_DIFF_READY` at line 40 and
`TRANSITION:Light->Dark` at line 48.

The clear at `:4624` runs only when the condition became true, and the comment
there says the latch is "armed again for a second episode". So exactly one
marker in a failing log is consistent with **two opposite histories**:

- the owner never satisfied the condition, and the single marker is the first
  and only entry into the wait; or
- the owner satisfied it early, the latch cleared, and focus was lost again
  before the measurement completed — with the second wait either never entered
  or entered and not reported.

Nothing in the log separates them. Three runs have now produced this signature
and in none of them can anyone say which happened.

## Why this is the card and a reworded packet is not

The obvious next move was a v008 packet with clearer steps, on the theory that
the procedure is confusing — and the procedure *is* confusing, because leaving
the window frontmost and changing a System Setting cannot both hold, which the
owner said plainly:

> leave it frontmost and change system appearance is oxymoron, you cant do both

But v005 was already a fix for that, and v007 already tells the owner to click
the same pane again. Writing a fourth wording fix for a failure nobody can
attribute would be guessing, and three attempts is enough evidence that guessing
is not converging.

## Required final state

- The log distinguishes entering the wait from leaving it. At minimum an
  `..._OWNER_RETURNED` marker when the condition is first satisfied, and a
  distinct marker for each subsequent re-entry rather than a silent latch.
- A failing run says which of the two histories above occurred.
- The markers carry enough to attribute the failure: whether the window was
  active, whether it was `QApplication::activeWindow()`, and whether
  `focusWidget()` matched — not one collapsed boolean. `ACTIVE_FOCUS_FAILED`
  today cannot say which of its three conjuncts failed, which is the same defect
  [[BPR-0381]] fixed for the public-path waits.
- No change to what the probe *requires*. This card makes the failure legible;
  it does not relax the condition.

## Non-goals

- Do not weaken `native_appearance_owner_returned` (`main.cpp:1139`). Requiring
  the same `focusWidget()` is the point: it is what proves the owner returned to
  the pane rather than to some other part of the window.
- Do not write a v008 packet under this card. Once a failing run can be
  attributed, the packet either needs new prose or does not, and that will be
  answerable instead of guessable.
- Do not touch anything else under `src/`.

## The cost of doing it

This edits `src/app/main.cpp`, which relinks the application and invalidates the
two attended packets that bind its bytes — including v007, the very packet this
serves. That is not an argument against doing it; it is an argument for doing it
**with** the other `src/` work rather than alone, so one relink pays for all of
it. [[BPR-0409]] is queued for the same reason.

See the dev-binary hold in the board README.

## Verification

A run that fails must name which conjunct failed and which history occurred.
The negative control is the one that matters: with the new markers in place,
replay the 2026-08-20 conditions and confirm the log now distinguishes them.

```sh
ctest --test-dir build/dev --output-on-failure -R '^application_appearance'
python3 scripts/verify_documentation.py --root .
```
