---
schema: "implement-rfc.card.v1"
id: "BPR-0217"
title: "One liveness token for every poller that re-enters the application"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0215"]
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

## Forced, 2026-08-08

The outstanding evidence exists.
`tests/end_to_end/github_application_liveness_tests.cpp`, registered as
`application_poller_liveness`, constructs the real application against a
temporary data root, arms one tick in the production pollers' shape, destroys
the application, and requires that the tick is delivered, sees an expired token,
and reaches nothing back inside.

Nothing was opened up to make this testable. `github_application` already
compiles into the linkable static library `BetterPullReview::Composition`, and
`tests/end_to_end/` already links it — `worker_executor_tests.cpp` is the
precedent.

### Why the condition can be forced rather than waited for

The five runs that failed to reproduce it were waiting for a tick to survive
`exec()` returning. Destroying the application with a tick deliberately armed
does not wait for anything:

- `~github_application` runs `Liveness.reset()`, then destroys `Implementation`.
- libc++'s `~unique_ptr` calls `reset()`, which assigns the new pointer *before*
  invoking the deleter (`unique_ptr.h:266`), so `Implementation` is already null
  while `~implementation` runs.
- `~implementation` (`github_application.cpp:1029-1037`) loops
  `poll_shutdown(); processEvents(AllEvents);`. `begin_shutdown()` does not set
  `ShutdownComplete`, so the loop body always runs at least once and the pump
  always happens at least once.

A zero-millisecond single shot armed before destruction is therefore delivered
*during* destruction, every time.

### Both directions, dev build

Guard removed (`Liveness.reset();` at `github_application.cpp:1589` commented
out), rebuilt, `ctest --preset dev -R application_poller_liveness`:

```
1/1 Test #124: application_poller_liveness ......***Exception: SegFault  0.56 sec
```

Under lldb, the production trace frame for frame:

```
stop reason = EXC_BAD_ACCESS (code=1, address=0x5570)
  frame #0  bpr::app::github_application::status()      github_application.cpp:1615
  frame #1  the armed tick                              github_application_liveness_tests.cpp:76
  frame #17 QEventDispatcherGlib::processEvents
  frame #18 github_application::implementation::~implementation  github_application.cpp:1034
  frame #21 std::unique_ptr<implementation>::reset      unique_ptr.h:266
  frame #24 bpr::app::github_application::~github_application    github_application.cpp:1590
```

The card's claim that `0x730` was a member offset from null rather than a wild
address is now checked rather than argued:

```
(lldb) p Implementation
(std::unique_ptr<bpr::app::github_application::implementation>) nullptr
(lldb) p (void*)&Implementation->Closing
(void *) 0x0000000000005570
```

The faulting address *is* the member's address off a null handle.

Guard restored, rebuilt, rerun: `Passed 0.52 sec`.

**One correction to the card's own description.** The reported crashes faulted
in `window()` at offset `0x730`; the forced one faults in `status()` at
`0x5570`. The tick calls `status()` first, so that is the member it reaches
first. Same fault, different member — which is the point of the card: the guard
is not about one accessor.

### Both directions, sanitized build

Debug plus `-fsanitize=address,undefined -fno-omit-frame-pointer`, the
`build/bpr0068-sanitize` precedent. Guard removed:

```
github_application.cpp:1615:53: runtime error: member access within null pointer
    of type 'bpr::app::github_application::implementation'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior github_application.cpp:1615:53
AddressSanitizer: SEGV on unknown address 0x000000005570
```

Same frames as the dev build. Guard restored:
`github-application-liveness-tests:pass`, clean under both sanitizers.

### The anchor, also measured

`expect(delivered, ...)` is what stops the other two assertions from becoming
statements about an event that never happened. Two probes:

- Tick interval widened to 100 s so it is never delivered:
  `FAILED: the pump inside ~implementation delivers a tick armed before destruction`.
  The anchor fires.
- `processEvents` deleted from `~implementation` outright: the destructor never
  completes and the run had to be killed at 60 s (exit 137). Recorded as what it
  was — a hang caught by the test's `TIMEOUT 60`, not an assertion failure.
  Either way the test does not stay silently green.

### A measurement that contradicted the design

The first run threw `Application data or installed Git is unavailable`. Not git.
`prepare_application_data_root` (`github_application.cpp:187-208`) returns an
empty path unless `canonical(root) == root.lexically_normal()`, and on macOS a
`QTemporaryDir` lives under `/var`, a symlink to `/private/var`. Measured
directly:

```
root:      "/var/folders/k7/.../T/tmp.18tkQUFo1A"
canonical: "/private/var/folders/k7/.../T/tmp.18tkQUFo1A"
equal:     0
```

That check is production policy about what a data root may be, so it was not
touched. The test canonicalises on its own side with
`QFileInfo::canonicalFilePath()` — which is what
`github_public_path_fixture.cpp:1035` already does, for this exact reason.

### Two stale comments from `a9ecaf3`, same commit

- `src/app/main.cpp` — the three-line tick comment was pasted twice
  back-to-back. One copy deleted.
- `src/app/main.cpp` — the comment above the `lastWindowClosed` connect still
  described BPR-0215's caller-owned token, which that commit deleted. Replaced
  with one true line about what the connect does. The surviving guard is
  documented on `schedule_interactive_shutdown` and on `liveness()`.

### Gates

`dev` and `core-only` configure/build/ctest, `verify_documentation.py`,
`git diff --check`, `coding-format-check`, `coding-tidy`.
