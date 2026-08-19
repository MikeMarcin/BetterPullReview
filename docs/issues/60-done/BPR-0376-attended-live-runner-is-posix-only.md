---
id: BPR-0376
title: The attended live runner's macOS-only cases ran on Windows
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The attended live runner's macOS-only cases ran on Windows

`rfc0016_device_live_runner_security` failed with 11 errors and 1 failure. The
visible cause was:

```
AttributeError: module 'os' has no attribute 'getuid'.
Did you mean: 'getpid'?
```

`scripts/run_rfc0016_device_capture_attended.py` calls `os.getuid()` at lines
1118, 1701 and 1808, and resolves the home directory through
`pwd.getpwuid(os.getuid()).pw_dir` at line 65.

## The decision this card thought it carried, and did not

An earlier revision put a question here: refuse on Windows explicitly, or port
the ownership checks now. Both were wrong, because the runner **already
refuses**. It is the first statement of `main`:

```python
if sys.platform != "darwin":
    print("RFC0016_ATTENDED_CAPTURE_REFUSED:platform", file=sys.stderr)
    return 1
```

The runner is macOS-only by design, it says so in a typed refusal, and
[[BPR-0311]] independently keeps Windows device capability out of scope. There
was nothing here for a product owner to decide. The card asked the question
because it had read the `os.getuid()` calls and not the eight lines above them.

The tests were reaching those POSIX-only internals by calling **past** the
refusal — invoking helpers directly rather than through `main` — and then
reporting the platform's absence as a defect in the runner. The one case that
did call `main`, `test_recovery_reservation_is_one_shot_before_wire_construction`,
asserted `main() == 0` and got the refusal's `1`. That is the runner behaving
exactly as documented.

## Resolution

Twelve cases now carry `@requires_macos_runner`, an `unittest.skipUnless` on
`sys.platform == "darwin"` whose message says the runner refuses every other
platform.

`tests/CMakeLists.txt` already guards `designated_live_macos_capsule_contract`
with `if(APPLE)` for exactly this reason. This is the same judgement at case
granularity rather than whole-registration, which is what keeps the 28 cases in
this file that are genuinely portable running on Windows instead of vanishing
with the twelve that are not.

The symbolic-link case is among the twelve, so the `WinError 1314` privilege
error goes with them; it needed no separate handling here.

Nothing about the runner changed. Its ownership checks are load-bearing on the
platform it supports and are untouched.

## Result

```
py -3 tests/scripts/rfc0016_device_live_runner_tests.py
OK (skipped=12)

under the launcher:
OK (skipped=12)
```

28 cases run and pass on Windows, contained and uncontained. Twelve skip by
name.

## What this does not claim

No Windows device-capture behaviour is asserted by this card, and none is
implied. [[BPR-0311]] still owns making the capability available on Windows, and
when it does, these twelve cases become live there by the platform check
already written rather than by anything needing to be remembered.
