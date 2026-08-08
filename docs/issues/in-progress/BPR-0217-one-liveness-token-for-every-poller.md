---
id: BPR-0217
title: One liveness token for every poller that re-enters the application
status: In Progress
rfc: RFC-BPR-0013
depends_on: [BPR-0215]
---

# One liveness token for every poller that re-enters the application

BPR-0215 fixed a Command-Q segfault by giving `schedule_interactive_shutdown`
a liveness token. That fix was too narrow, and the product owner hit the same
crash through a different poller about an hour later.

## The second crash

```
main.cpp:3588                 ~unique_ptr<github_application>
github_application.cpp:1584   ~github_application
github_application.cpp:1034   ~implementation -> processEvents(AllEvents)
main.cpp:2402                 schedule_automated_run's tick
github_application.cpp:1598   github_application::window()   <-- EXC_BAD_ACCESS at 0x730
```

Identical in shape to BPR-0215 and identical in cause: `~implementation` pumps
the event loop in its own shutdown loop, and by then libc++'s
`unique_ptr::reset` has already nulled the owner's handle, because it assigns
the new pointer before invoking the deleter. `0x730` is a member offset from
null, not a wild address.

BPR-0215 guarded one of the three pollers. `schedule_automated_run` has the same
shape and was left alone, so the fix held only for the path that had already
been observed to fail.

## Why the first fix was the wrong shape

A token local to one scheduler describes nothing. It has to be created, kept,
and destroyed in the right order by whoever calls that scheduler, and it says
nothing about the other callers with the same problem. The next poller added to
this file would have had to rediscover all of it.

The lifetime that actually matters belongs to `github_application`, so the token
belongs there. `github_application::liveness()` returns a weak reference that
expires in the destructor **body**, which runs before any member is destroyed.
Every poller holds one and returns immediately once it has expired, so the
window between "unusable" and "says so" is empty by construction rather than by
each caller's care.

## Honest limit on the evidence

**The crash was not reproduced, before or after.** Five runs of the exact
command that produced it — three with the guard, two with it deliberately
removed — all exited `1` on the probe's ordinary focus failure and none
segfaulted. The condition needs a tick still armed at the precise moment
`exec()` returns, which this configuration does not reliably create.

So the fix rests on the stack trace, which names the path exactly, and on the
ordering argument, which is checkable by reading. It does not rest on a
reproduction, and this card should not be closed as though it does.

## Required direct evidence, still outstanding

- A test that forces the condition rather than waiting for it: construct the
  application, arm a poller, destroy the application, and require that the
  queued callback runs and does nothing. That is what would make this
  non-vacuous, and it is not written yet.
- Until then the card stays open even though the change is in.

## What must not be done instead

Adding a fourth ad-hoc token the next time a poller crashes. Three pollers with
three different guards is the state this card exists to end.

Making `window()` or `status()` null-tolerant. That hides a callback reaching
into a half-destroyed object rather than stopping it, and was already refused
for the same reason in [[BPR-0215]].
