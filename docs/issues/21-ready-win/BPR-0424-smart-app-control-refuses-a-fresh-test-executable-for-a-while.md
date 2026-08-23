---
id: BPR-0424
title: Smart App Control refuses a freshly linked test executable for a while
status: Ready
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
