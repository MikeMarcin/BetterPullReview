---
id: BPR-0114
title: Verify System mode follows a live OS scheme change
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0106, BPR-0183, BPR-0184]
---

# Verify System mode follows a live OS scheme change

On the real macOS composition, prove that System mode follows an
operating-system colour-scheme change while an explicit mode does not.

## This card was misfiled, and BPR-0180 says why

It was parked as owner-attended, which asserted that the behaviour existed and
only a person was missing. That was wrong. BPR-0180 measured that the
application composes no `appearance_profile` and does not link
`appearance_application_host` at all, so there is no appearance mode to select
and no variant to observe. RFC-BPR-0006 names this claim's surface as **Real
application** and its observation as **profile variant change**; neither
existed.

Attendance was never what was missing. BPR-0183 makes the profile drive the
composed window, and BPR-0184 supplies the gesture that selects an explicit
mode. Only then is there something for a person to watch.

The product owner authorised the appearance change on 2026-08-06 and that
authorisation is recorded here, but the run could not proceed. Authorisation is
per-session and must be sought again when this card is actually run.

## What is missing, once the dependencies land

The desktop owner, once, to authorise changing and restoring the global
appearance and to observe the result. The host must start in Light, because the
existing probe's transition is Light to Dark.

## Required direct evidence

- With System selected, `appearance_profile::variant()` changes when the owner
  changes the operating-system scheme, and the composed surfaces follow.
- With an explicit mode selected through BPR-0184's gesture, the same
  operating-system change alters neither the variant nor the surfaces.
- The scheme is changed by the owner in System Settings. Nothing in this
  repository changes the global appearance programmatically.
- The global appearance is restored afterwards, and the restoration is
  confirmed rather than assumed.
- The observation is read from the running application, not from a raster
  captured earlier.

## What must not be done instead

Showing that Qt's native palette propagates and calling it System mode. That is
`QMacStyle` following the operating system, which happens whether or not
BetterPullReview has an appearance model at all, and it is exactly what kept
this gap invisible for as long as it was.

The static fresh-install-defaults-to-System claim is separate and is proven
unattended by BPR-0106.

## The blocker, attributed 2026-08-08

[[BPR-0154]] is closed and it was not this card's blocker after all. Its
signature had nothing to do with a fixture or a stall: decoding the evidence bits
showed the fixture working perfectly, and `PUBLIC_PATH_VERDICT` in the same log
already named the failing fact.

**`presentation=0`.** The probe's presentation never passes, `RENDER_FAILED`
appears alongside it, and every other core fact — shutdown, cleanup, epoch
projection and handoff delivery — is fine. `GITHUB_FIXTURE_FAILED` was printed
only because the fixture verdict is emitted from a `passed` that presentation had
already narrowed, which is [[BPR-0213]].

So the attended observation is blocked on presentation failing in
`--native-appearance-probe`, not on a fixture and not on a stall. That is a
single, named fact to chase rather than a signature to attribute, which is a
considerably better position than this card has been in.

Evidence: `docs/verification/reviews/appearance-probe-open-stage-v001/`, lines 8
and 9 of the recorded run.

### The chain, as far as it can be followed without a display

From the recorded run and the code, in order:

1. `PUBLIC_PATH_STAGE:2` with `PUBLIC_PATH_FAILED` — the run failed in the
   `open` stage.
2. No `NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS` in that log, so it failed
   *before* the focus wait. In that branch the only failure is
   `request_fixture_diff` returning false, which calls `fail_automated_run`.
3. `request_fixture_diff` requires
   `state.Authority.has_value() && state.AcceptedComparison.has_value()`.
4. The same log reports `PUBLIC_PATH_EPOCH_PAIR:::-1:-1:0:0` and
   `PUBLIC_PATH_EPOCH_CORE_STATUS:-1` — **no accepted comparison**.

So: no accepted epoch comparison → `request_fixture_diff` false → fail at stage
2 → `presentation=0`. That is the whole chain, and it is deterministic because
nothing in it depends on timing: the branch fails on its first attempt rather
than retrying.

### The remaining question, and why it needs the display

`--diff-workbench-probe` uses the same `request_fixture_diff` with the same
fail-fast structure and succeeds every time. So the difference is *when* each
probe reaches that branch relative to the epoch handoff, not what the branch
checks.

**That cannot be investigated on this host.** The appearance probe refuses
offscreen at argument parsing — `NATIVE_APPEARANCE_CONFIGURATION_FAILED` — which
is deliberate and correct, and it also means the failing path cannot be reached
without a real display. Every attempt to narrow it further here would be reading
rather than measuring.

**Next step, when a display is available:** run the probe and print the epoch
state at the moment the `open` branch is entered. The candidate is that the
appearance run reaches `open` before the epoch handoff has been accepted, and
fails immediately where the diff run happens to arrive later. If that is it, the
fix is to wait for the accepted comparison rather than to fail on the first
attempt — which is what the surrounding code already does for
`current_result() == nullptr` one line below.

## Run on a real display, 2026-08-08, and it is no longer attendance that blocks

The product owner gave standing authorisation to change the global appearance
for testing, so this was driven without a person at the keyboard: host set to
Light, `--native-appearance-probe` started, the scheme changed to Dark while it
ran, and the host restored to Dark afterwards with the restoration read back.

The probe **reached the display and refused nothing**, which is new — its
automated contract can only prove it declines offscreen. It then failed before
any appearance stage:

```
NATIVE_APPEARANCE_EPOCH_CONTEXT:1:1:1:1:0
NATIVE_APPEARANCE_FOCUS:1:1:1:1:QPlainTextEdit:diffLeftSource:11
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:2
PUBLIC_PATH_VERDICT:presentation=0 ...
PUBLIC_PATH_EPOCH_COMPARISON:Whole PR. No exact pair is resolved.
```

Stage 2 is `open`, and the pull request did load — the status line reads *"Pull
request ready."* and the account resolved. Of the five context fields, four pass:
focus is preserved, the epoch control's palette is unchanged, that palette is
readable, and no spurious appearance update was counted. The fifth,
`native_appearance_context_passed`, is `0`, and the comparison line says why:
**no exact epoch pair is resolved**.

So `APPEARANCE_SYSTEM_FOLLOWS_OK` and `APPEARANCE_EXPLICIT_IGNORES_OK` were never
reached. Nothing here says the appearance behaviour is wrong; it says the probe
cannot get far enough to ask.

### What actually blocks this card now

An accepted epoch comparison in the fixture run. The `github-com` fixture opens
a pull request whose review history is inactive and whose comparison never
resolves an exact pair, so the light-state precondition the probe checks before
touching appearance can never hold.

That is a fixture-content problem, and it is the same shape as [[BPR-0187]]: the
served fixture is a synthetic three-commit graph, and what this needs is a
comparison the epoch machinery will accept. Whether that is a wider synthetic
graph or a served recording is the decision to take first.

**Attendance is no longer the missing thing.** The standing authorisation and
the working probe removed that. Recorded here so the card stops being filed as
owner-attended when it is not.
