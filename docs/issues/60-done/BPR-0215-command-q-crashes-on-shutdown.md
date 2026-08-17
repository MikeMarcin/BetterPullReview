---
id: BPR-0215
title: Command-Q segfaults during shutdown
status: Done
rfc: RFC-BPR-0013
depends_on: [BPR-0186]
---

# Command-Q segfaults during shutdown

Found by the product owner on 2026-08-08, in the first attended launch of the
composed shell (BPR-0186). Every automated shutdown contract was green.

`EXC_BAD_ACCESS` at `0x5570`, which is a member offset from a null pointer, not
a wild address.

## The chain

```
main.cpp:3491                      ~unique_ptr<github_application>
github_application.cpp:1584        ~github_application
                                   ~unique_ptr<implementation>::reset
github_application.cpp:1034        ~implementation  -> processEvents(AllEvents)
                                   ... Qt delivers a queued functor ...
main.cpp:3159                      schedule_interactive_shutdown's poll
github_application.cpp:1604        github_application::status()  <-- crash
```

`schedule_interactive_shutdown` re-arms a 10 ms poll until shutdown completes,
so at the moment it calls `QCoreApplication::exit` there is normally still one
armed timer behind it. That timer outlives `exec()` returning.

`~implementation` then pumps `QCoreApplication::processEvents(AllEvents)` in its
own shutdown loop, which delivers that pending poll. The poll calls
`aApplication.status()`, and `status()` dereferences `Implementation` — which is
**already null**, because libc++'s `unique_ptr::reset` assigns the new pointer
before invoking the deleter:

```cpp
pointer __tmp = __ptr_;
__ptr_ = __p;          // null, before ~implementation runs
if (__tmp) __deleter_(__tmp);
```

So the object is mid-destruction, the owner's handle to it is already null, and
a queued callback reaches back in through the public API. The faulting address
being a small offset rather than garbage is the tell.

## Why no test caught it

The automated shutdown contracts drive `schedule_automated_run`, not the
interactive path: a fixture run completes its own shutdown stage and exits
without a window close. `schedule_interactive_shutdown` is reached only from
`QGuiApplication::lastWindowClosed`, which needs a real window a person closes —
which nothing could do until BPR-0186 made an attended launch possible at all.

This is the second time an attended pass found a defect that every marker
reported OK through, after BPR-0194's selection contrast and menu indicator. It
is the argument for BPR-0100 existing.

## The fix

`schedule_interactive_shutdown` returns a liveness token. The poll holds a weak
reference to it and returns immediately if it has expired. `main` keeps the
token in a variable declared *after* the application, so reverse destruction
order disarms the poll before the application it would have reached into goes
away. A late delivery becomes a no-op instead of a crash.

Nothing about the shutdown sequence changes: the same states are polled in the
same order and the same exit code is reported.

## Required direct evidence

- The product owner launches the composed shell and presses Command-Q, and the
  process exits without a crash report.
- The exit code still distinguishes a clean shutdown from a failed cleanup.
- The automated shutdown contracts still pass, so the fix did not buy interactive
  safety by weakening the path the fixtures drive.

## What must not be done instead

Making `status()` null-tolerant. That hides a callback reaching into a
half-destroyed object rather than stopping it, and the next accessor added to
the same path would crash the same way.

## Confirmed, 2026-08-08

The product owner relaunched the composed shell and pressed Command-Q, and the
process exited with no crash report. The automated shutdown and public-path
contracts stayed green, so the interactive safety was not bought by weakening
the path the fixtures drive.

## The fix was too narrow — see [[BPR-0217]]

`schedule_interactive_shutdown` was one of three pollers with this shape.
`schedule_automated_run` has the same one and was not touched, and the owner hit
the identical crash through it about an hour later. The token local to this
scheduler has been replaced by `github_application::liveness()`: one mechanism,
owned by the object whose lifetime it describes.

The diagnosis here was exact. What was wrong was fixing the observed path
without looking for the same shape elsewhere, which left the defect in place
under a different name.
