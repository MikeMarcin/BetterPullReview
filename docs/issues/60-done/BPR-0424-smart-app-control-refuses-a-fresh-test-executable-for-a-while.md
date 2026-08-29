---
id: BPR-0424
title: Smart App Control refuses a freshly linked test executable for a while
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# Smart App Control refuses a freshly linked test executable for a while

`application_github_capability_cache` failed on the Windows host before its
first line ran:

```
BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551
```

`net helpmsg 4551` reads *An Application Control policy has blocked this
file.* It was not the AppContainer: launched directly from a shell, outside
the launcher, the executable was refused the same way (exit 126), and the Code
Integrity operational log records each refusal as events 3033 and 3077 — "did
not meet the Enterprise signing level requirements" — naming
`better_pull_review_application_github_capability_cache_tests.exe` and policy
`{0283ac0f-fff1-49ae-ada1-8a933130cad6}`. The host's
`HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState`
is `1`: Smart App Control is on.

## It is transient, and this card was nearly wrong about that

The first draft of this card called the refusal deterministic, because
deleting and relinking the image (hash `5a50bcd0…` to `32425c0b…`) was refused
identically within a minute. The full-suite rerun forty minutes later then
**passed** the same `32425c0b…` image, and a direct launch now exits 0. The
event log gives the shape exactly:

| time | event |
|---|---|
| 19:18:56 | refused, first suite run (image linked 19:06) |
| 19:29:06, 19:29:25 | refused, two reruns |
| 19:30:21 | refused, relinked image (new hash) |
| ≈19:45 | **allowed**, same relinked image, in the full suite |

So Smart App Control refuses an unknown freshly linked image until its
reputation query is answered, and the answer arrived for this one some time
between fifteen and forty minutes after the link. The other 87 test images,
linked in the same minute, were never refused — whatever makes this one
different is content the policy weighs and this card does not guess at.

The practical consequence is for the matrix, not the product: a fresh
Windows build can fail this test for a quarter of an hour and then pass with
no change, which reads as flakiness and is not.

## Required final state

- On the next fresh link, the refusal and its clearing are measured again
  with event-log timestamps, so the window has two samples rather than one.
- [[BPR-0036]]'s target-native procedure records the condition: a `4551`
  launch refusal on a freshly linked image is a host-policy wait, to be
  re-run after the Code Integrity verdict clears, and not a matrix failure
  until it persists past that.
- If it ever persists, the decision is the owner's and is written down here:
  sign the test executables, switch Smart App Control off (a one-way change
  without a reinstall), or record the exclusion.

## Explicit non-scope

Does not touch the AppContainer boundary, does not change what the test
asserts, and does not bisect the binary for the content that draws the
policy's attention.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^application_github_capability_cache$'
```

immediately after a relink, and again once the Code Integrity log shows no
new 3077 for the image.

## It is not only tests — 2026-08-22, later

The Code Integrity log for the session, every 3033/3077 pair by image:

| time | image | what it was doing |
|---|---|---|
| 19:18:56, 19:29:06, 19:29:25, 19:30:21 | `better_pull_review_application_github_capability_cache_tests.exe` | the test above; allowed from ≈19:45 |
| 19:37:06 | `cmTC_d5f9a.exe` | a CMake try-run probe, during a fixture configure |
| 20:05:46 | `cmTC_c53f9.exe` | a CMake try-run probe, during a fixture configure |
| 21:02:47 | `katehighlightingindexer.exe` | KSyntaxHighlighting's own build tool, RelWithDebInfo, inside the nested build the syntax provider runs at configure time |

The indexer refusal was fatal: the nested build reported
`'…\katehighlightingindexer.exe' was blocked by your organization's Device
Guard policy` and `MSB8066`, the provider failed the configure, and the same
tool had just run for Debug, Release, and MinSizeRel minutes earlier. The
configure was simply retried.

Two things follow. `cmake/WindowsTestIsolationSetup.cmake` already retries
its launcher try-run three times for "a freshly linked executable
occasionally unrunnable for a moment while a scanner holds it" — that is this
policy, seen before it had a name, and the `cmTC_*` rows are it hitting
try-run probes elsewhere. And [[BPR-0426]]'s fixture configures ran at both
`cmTC_*` times, so any of its five failures that dies inside a compiler check
rather than on its assertion has to be separated from this before it is
attributed.

The required final state above stands; the procedure it asks for covers
build tools and try-run probes as well as test images.

## Second sample — 2026-08-23

The required final state asked for a second measurement, and the next relink
supplied it. `tests/network_isolation/windows_network_sandbox.cpp` changed for
[[BPR-0433]] and the `core-only` launcher was relinked at 08:11; the policy
refused it at 08:11:07 (events 3033/3077) and a probe run through it every
minute was first allowed at **08:32**, twenty-one minutes later. The
`windows-dev` build's copy of the same source, linked at 08:11 as well with a
different hash, was allowed on its first run at 08:12. So the window is not a
fixed delay and not a property of the source: it is per image, between about
fifteen and forty minutes on the two samples, and it lands on whichever fresh
image the reputation service has not yet answered for — a test, a try-run
probe, a build tool, or the launcher that every contained test needs.

Practical consequence, now recorded for [[BPR-0036]]'s procedure: after any
relink of the launcher, the whole contained suite is unrunnable until the
policy answers, and `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551` on
every test is that wait, not a boundary failure.

## Third sample — 2026-08-23, the whole relinked suite

[[BPR-0434]] linked a new unit into every automated executable, so every
test image was fresh at once. Between 08:30 and 08:50 the policy refused, by
image: the `core-only` launcher three more times before it cleared, the
framework indexer once more inside a nested build (which the provider
retried), and `better_pull_review_github_application_liveness_tests.exe`
during an uncontained control run, where the helper saw it as
`[WinError 4551]`. Everything else relinked in the same minute ran on first
use. Three samples now; the shape holds: per image, unpredictable, a quarter
to three quarters of an hour, and a relink of the launcher stalls the whole
contained suite until it clears.

## Closed — 2026-08-23

The required final state is met by the three samples above and by the
procedure they produced:

- The window was measured again with event-log timestamps — twice, in
  fact: the launcher relink for [[BPR-0433]] (refused 08:11:07, allowed
  08:32) and the suite-wide relink for [[BPR-0434]] (per image, between
  08:30 and 08:50). Three samples, one shape.
- [[BPR-0036]]'s procedure now records the condition under "Procedure —
  host policy on a freshly linked image": a `4551` refusal on a fresh image
  is a host-policy wait, confirmed from the Code Integrity log and re-run
  once the verdict clears, and not a matrix failure until it persists past
  that; it covers test images, the contained launcher, try-run probes, and
  nested build tools.
- Nothing has persisted. Every refused image in the session cleared without
  intervention, so the owner's decision stays unmade and written here as
  the three options it would choose between.

Verification as written — `application_github_capability_cache`
immediately after a relink, then again after the log showed no new 3077 —
was the first sample itself: refused four times from 19:18:56, allowed from
≈19:45, passing since.

Implemented by the integrator; documentation only, no code.

## A refusal has now outlived every measured window — 2026-08-28

The "everything cleared without intervention" sentence above gained its
first counterexample, measured by the Remote Control lane during BPR-0476's
from-scratch build attempt. `katehighlightingindexer.exe` (Release, the
nested KSyntaxHighlighting build tool) in
`C:\code\git\BetterPullReview\build\windows-dev` was linked 17:59:25 and was
still refused at 20:01:14 — 2h 01m 49s, 80 probes across two capped loops, a
fresh 3033/3077 pair on each — while its Debug sibling, linked 93 seconds
earlier in the same nested build, cleared inside the hour. Same host, same
session: one image cleared, one exceeded two hours without a verdict,
against this card's recorded ~15–45 minute range. The polling stopped at the
cap; the refusal stands unresolved on that tree only (the `windows-lane`
worktree needed no reconfigure and was unaffected). The sign / policy-off /
record-the-exclusion decision remains the owner's, untaken.
