---
id: BPR-0114
title: Verify System mode follows a live OS scheme change
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0101, BPR-0106, BPR-0183, BPR-0184, BPR-0220, BPR-0249]
---

# Verify System mode follows a live OS scheme change

## Current attended state from BPR-0225

Do not run the existing foreground operator packet. It is digest-bound to an
older application binary, and subsequent `main.cpp` changes invalidate that
binding. Its BPR-0220 addendum also asks for the existing marker log, while
BPR-0220 requires per-rebuild cause attribution to distinguish two legitimate
turns from one duplicated appearance application. Regenerate the packet only
after that attribution exists in the native probe and bind it to the rebuilt
binary and updated card bytes.

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

### Withdrawn the same day: attendance *is* still the missing thing

The paragraph above claimed the epoch comparison was the blocker and that this
card was no longer owner-attended. Both were wrong, and were drawn from a single
run.

Re-run with the aggregate context check split into its two halves, the epoch
state came back healthy — *"Review history is ready. Left available; right
available. Ready for diff."* — and the failure was somewhere else entirely:

```
NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS
NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED
```

The probe requires its window to be the **active, focused, frontmost** window,
and says so in the marker's name. Driving it from a background shell never
satisfies that. An attempt to raise the process with `System Events` did not
either: the following run reported `APPEARANCE_VARIANTS:-1:-1:-1:-1`, meaning no
baseline was ever captured, and `NATIVE_APPEARANCE_TRANSITION:Dark->Dark`
despite the desktop having been set to Light first.

So the standing authorisation to change the global scheme removes one of the two
things a person was needed for, and not the other. Somebody still has to put
this window in front. That is a deliberate gate — an appearance judgement made
against a window that was never frontmost is a judgement about a window nobody
was looking at.

**What is actually needed:** the owner launches the probe and clicks the window
to the front, then changes the scheme in System Settings. The scheme change can
now be scripted under the standing authorisation; the focus cannot.

The earlier epoch-context reading is not retracted so much as demoted: it was
one observation, it did not reproduce, and the aggregate `0` it came from could
not distinguish forty terms. The split marker below exists so the next run says
which half failed instead of that it failed.

### Localised, 2026-08-08: it is the `Clean` group, not epoch, focus or thread

The aggregate context check is now reported as five named groups. With the
desktop genuinely in Light and the owner's window clicked to the front:

```
NATIVE_APPEARANCE_EPOCH_CONTEXT:1:1:1:1:0
NATIVE_APPEARANCE_CONTEXT_DETAIL:1:1:1:1:0     Epoch Diff Thread Window Clean
NATIVE_APPEARANCE_FOCUS:1:1:1:1:QPlainTextEdit:diffLeftSource:11
```

Epoch, diff, conversation-thread and window-activation all hold. The failing
group is `presentationClean`:

- the platform name and style class are unchanged from capture,
- no full GitHub presentation reset happened,
- **the window carries no `WA_SetPalette`, `WA_SetStyle` or `WA_SetFont`, and no
  stylesheet.**

The shell applies the appearance profile with
`AppearanceProfile->apply_to(*Window)`, which sets a palette on the window and
therefore sets `WA_SetPalette`. That is the prime suspect and it has **not** yet
been isolated to a single field — the group is reported, the individual terms
are not.

If it is `WA_SetPalette`, the rule itself is what needs deciding rather than the
code: the check was written to catch presentation being overridden behind the
appearance model's back, and the appearance model applying its own palette is
the opposite of that. Distinguishing "the profile set this" from "something else
set this" is the actual question, and it is not answerable from
`testAttribute` alone.

Two other findings from the same runs:

- `prepare_native_appearance_probe` still does
  `qobject_cast<QScrollArea*>(aWindow.centralWidget())` to scroll the comparison
  into view. The shell's central widget is a plain `QWidget` since BPR-0191, so
  that cast fails and the call silently does nothing. Harmless here, wrong
  everywhere.
- The earlier claim that the probe "waits for owner focus and never gets it" was
  wrong for this run: `NATIVE_APPEARANCE_FOCUS` reports the window active, and
  the probe did not wait at all. The wait path is real but was not what stopped
  these runs.

## Both claims proven, 2026-08-08

With `!testAttribute(WA_SetPalette)` removed from the window check, the probe
reached the appearance stages and the transition was driven under the owner's
standing authorisation, unattended:

```
NATIVE_APPEARANCE_TRANSITION:Light->Dark
APPEARANCE_SYSTEM_FOLLOWS_OK
APPEARANCE_EXPLICIT_IGNORES_OK
APPEARANCE_VARIANTS:0:1:0:1
NATIVE_APPEARANCE_SIGNAL_OK
```

The variants are the claim in full. System resolved **light → dark** when the
operating system changed (`0:1`). With an explicit mode selected through
BPR-0184's gesture the same operating-system change left the variant at
**light** (`0`). Returned to System, it resolved **dark** again (`1`).

The second of those is the one this card exists for, and the one QMacStyle
following the operating system cannot fake.

### The term that was blocking it asserted the feature was absent

`presentationClean` required the window to carry no `WA_SetPalette`. That was
written when nothing composed an `appearance_profile` — BPR-0180 measured
exactly that — so the window's palette could only have come from the platform,
and "no palette was set" meant "nothing bypassed the model". Since BPR-0183 the
model sets the window's palette itself, which is the opposite of a bypass.

The provenance question the term was reaching for is answered better elsewhere:
`APPEARANCE_APPLIED` reports `from-profile` by comparing the window's palette
against the profile's own semantic roles, which is stronger than nobody having
set one. Style, font and stylesheet checks stay, because nothing legitimately
sets those on the window.

## Still failing, and deliberately not fixed here

```
NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED
NATIVE_APPEARANCE_STATE_FAILED
NATIVE_APPEARANCE_PIXELS_DIFFER_FAILED
NATIVE_APPEARANCE_DIFF_PALETTE_INHERITED_FAILED
NATIVE_APPEARANCE_DIFF_FONT_INHERITED_FAILED
NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_FAILED   (delta 2)
NATIVE_APPEARANCE_DIFF_SELECTION_FAILED
NATIVE_APPEARANCE_CHANGED_RENDER_FAILED
```

`ACTIVE_FOCUS_FAILED` is an artefact of this run: the process was launched into
the background so the window was never frontmost, and several of the rest
plausibly cascade from it.

**Correction.** The paragraph originally here claimed
`DIFF_PALETTE_INHERITED` was the same latent assertion one level down. That was
a guess and it was wrong. The check is

```cpp
InitialDiffPalette != ObservedDiffPalette && StatePassed
```

which asserts the pane's palette *did* change — the opposite of an absence
assertion — and the palette did change, from light to dark, at the comparison
surface itself:

```
NATIVE_APPEARANCE_DIFF_PALETTE_BEFORE:#fff6f7f9:#ffffffff:#ff1f2328:#ff1f2328
NATIVE_APPEARANCE_DIFF_PALETTE_AFTER :#ff1c2026:#ff0d1117:#ffe6edf3:#ffe6edf3
```

That is RFC-BPR-0006's claim holding one level below the window, and it is
evidence *for* the product rather than against it. The marker reads FAILED only
because it is conjoined with `StatePassed`.

So the eight failures have **one root**: `StatePassed`, which is
`stateAfterRender`, which is gated by the window being active. That is
`ACTIVE_FOCUS_FAILED`, and that is the launch artefact — the process was started
into the background and never became frontmost. The font marker is gated the
same way, and the font correctly did not change.

The next step is therefore a foreground run rather than eight judgements: if
`ACTIVE_FOCUS` passes, most or all of the cascade should resolve on its own, and
whatever is left will be a much smaller question.

**These were not repaired in the same sitting, on purpose.** Removing one term
after establishing that it encoded the feature's absence is a judgement.
Removing eight because a probe is red is how a probe stops meaning anything, and
this card's own rule — *"Showing that Qt's native palette propagates and calling
it System mode"* — is a warning against exactly that direction of travel. Each
remaining term needs the same question asked separately: is this catching a
bypass, or asserting that nothing happened?

`DIFF_GEOMETRY_REBUILD_DELTA:2` in particular is a product signal, not a rule
question: an appearance change rebuilding the comparison geometry twice may be
correct or may be a double application, and nobody has looked.

## The successful run's log does not exist, 2026-08-08

The markers quoted above under *"Both claims proven"* were read from a run whose
log was never committed, and no copy survives. Searched: the whole repository
tree including untracked build directories, for `APPEARANCE_SYSTEM_FOLLOWS_OK`
and `APPEARANCE_EXPLICIT_IGNORES_OK`. The only hits are this card and
`src/app/main.cpp`, which is the source that emits them.

`docs/verification/reviews/appearance-probe-open-stage-v001/` holds exactly one
file — `native-appearance-probe-run.log`, 2.2 KB, 7 August — and it is the
**failing** run: fourteen `FAILED` lines including
`APPEARANCE_SYSTEM_FOLLOWS_FAILED` and `APPEARANCE_EXPLICIT_IGNORES_FAILED`.

So the evidence directory currently records only the failure, and the card's
strongest claim rests on quoted text with no committed artefact behind it. That
is a real gap in this card's evidence, recorded rather than papered over.

**Deliberately not closed by re-running the probe now.** Regenerating it would
mean driving a second global appearance change on the owner's desktop to
produce a log that the imminent foreground run supersedes anyway: the Session 1
attended run captures the same two markers *plus* the frontmost-focus cascade
these eight terms depend on. One complete log is better evidence than two
partial ones, and it costs the owner one appearance change instead of two.

**Therefore:** the Session 1 foreground run must capture and commit its complete
marker log to `docs/verification/reviews/`, beside the failing one. That is now
part of this card's completion evidence, not an optional extra.

## Documentation corrected, 2026-08-08

`docs/verification/RFC-BPR-0006.md` described this card as blocked and
attended-only, which stopped being true when both claims were proven. Three
places were corrected: the claim-matrix row, the deferred-gates section, and the
closeout blockers.

That edit also removed an obsolete attribution. The ledger said this card
"inherits BPR-0154's open-stage attribution". It does not, and this card said so
first at the top of *"The blocker, attributed 2026-08-08"*. Recording the
correction precisely, because the campaign plan that prompted the sweep
described BPR-0154 as *withdrawn*: it was not. BPR-0154 is `status: Done` under
RFC-BPR-0003 and lives in `60-done/`. It was closed, and it was not this card's
blocker — a different thing from being withdrawn.

## Prepared for an attended run, 2026-08-09

The operator packet is
`docs/verification/reviews/appearance-scheme-foreground-v001/operator-packet.md`.
It binds the shipped binary's digest, so a rebuild reopens the gate.

Composition was re-verified before asking for owner time, both ways: `nm` finds
the appearance symbols in the shipped binary, and a run reports
`APPEARANCE_HOST_COMPOSED_OK`. Linking is not composing and this card is on the
board partly because that distinction was learned here.

The run must also **capture and commit its marker log**, which closes the gap
recorded above: the earlier successful run's log was never committed and no
copy survives.

## Packet regenerated, 2026-08-18

The live procedure at the time was
`docs/verification/reviews/appearance-scheme-foreground-v004/operator-packet.md`.
The v001 through v003 packets were never run and each bound a dev executable
that no longer exists at its recorded digest; v004 rebinds the same procedure
to the executable built at commit `0e34519` and the unchanged BPR-0249
implementation record at its current `60-done/` path. Its two deterministic
preflights, `application_native_appearance_probe_contract` and the focused
appearance selector, pass against those exact bytes.

## The live packet is v007 — corrected 2026-08-19

The paragraph above stopped being true three regenerations ago and was still
being read as an instruction. The live procedure is
[`appearance-scheme-foreground-v007`](../../verification/reviews/appearance-scheme-foreground-v007/operator-packet.md).

v004 must not be attempted: the owner tried it twice and could not complete it,
because the dark-side measurement fired before a person returning from System
Settings could restore focus. [[BPR-0391]] found and repaired that, and v005
through v007 followed as later work rebuilt the executable.

The cause was structural. This packet family serves three cards — this one,
[[BPR-0220]], and [[BPR-0391]] — and each regeneration updated only the card
that ordered it. [[BPR-0398]] carries the verifier check that now fails when a
card names a superseded packet.

One prediction is on the record and will be checked rather than assumed. Seven
of the eight failing terms should resolve when the window is frontmost, because
they root in `StatePassed`. `DIFF_GEOMETRY_REBUILD` should **not** — it is
[[BPR-0220]], a product question about an appearance change rebuilding
comparison geometry twice. If it goes green, that is a result to record, not a
relief.

## The final-head diagnostic packet is v008 — 2026-08-21

v007 was attempted and remains historical evidence; its collapsed focus/state
marker could not attribute the failure. [[BPR-0410]] now supplies an indexed
reachable wait episode, independent owner-return operands, and factual capture/persistence
dispositions. The live procedure is
[`appearance-scheme-foreground-v008`](../../verification/reviews/appearance-scheme-foreground-v008/operator-packet.md),
bound to the settled shared-batch executable at
`sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28`.
This card remains backlog because it still depends on parked [[BPR-0220]]; the
packet prepares evidence and does not bypass that dependency.

## v008 returned an attributable observed failure — 2026-08-21

The owner completed the exact foreground procedure against the bound final-head
binary, restored and visibly confirmed the original desktop appearance, and
returned the packet's exact statement with the complete unedited log. The
immutable
[`operator response`](../../verification/reviews/appearance-scheme-foreground-v008/operator-response.md),
[`raw log`](../../verification/reviews/appearance-scheme-foreground-v008/foreground-run-attempt-1.log),
and [`validation record`](../../verification/reviews/appearance-scheme-foreground-v008/validation-record.json)
show the selected `review.cpp` comparison, Light-to-Dark local and global
transitions, System-follow and explicit-ignore markers, strict owner return,
and all three final focus operands true.

This is not an acceptance of BPR-0114. The run exits `1` because the state
predicate expects one comparison-geometry rebuild and the attributable native
measurement records zero, with no rebuild causes. That keeps changed capture
and persistence from running. [[BPR-0391]] and [[BPR-0410]] are Done because
their wait and diagnostic claims are directly satisfied. Notification v006 is
also accepted, and [[BPR-0220]] has implemented the supported zero-expectation
repair. The owner accepted that repair without requiring another digest-only
appearance run.

## Owner acceptance; terminal procedure retained — 2026-08-21

The v008 native run directly observed the real System Light-to-Dark transition,
the global transition, explicit-mode isolation, restored desktop appearance,
strict owner return, retained selected diff, and zero attributed geometry
rebuilds. BPR-0220 changed only the two stale exact-one expectations to that
measured zero and passed the focused public-path contract. The owner accepted
the result and asked that no redundant evidence-preservation rerun be required.
The current checkpoint is accepted. BPR-0114 remains open behind BPR-0101 only
because the RFC closeout card uses it as the terminal native procedure; no
additional interim run is pending.

## Parked on the terminal-sitting decision — 2026-09-01

Every dependency is done and RFC-BPR-0006's automated rows replayed 18/18 on
[[BPR-0115]]. What this card still anchors is one owner decision: whether the
owner-accepted v008 observation stands as the RFC's terminal attended row
under the board's reuse rule — no appearance-path commit since 2026-08-21, and
the owner's own request that no redundant digest-only rerun be required — or
whether one more v009 foreground sitting against the closeout executable is
wanted. It is a choose-one subject in
`docs/verification/reviews/run-boundary-decisions-v001/offline-package/index.html`
(group `rfc-0006-appearance-row`). Accepting reuse closes this
card and unblocks [[BPR-0115]]; choosing the sitting has the integrator
regenerate `appearance-scheme-foreground-v009` bound to the final closeout
binary, after every other portable slice that could rebuild it; until then
`appearance-scheme-foreground-v008` remains the family's live, answered packet.

## Closed on the accepted v008 observation — 2026-09-01

The product owner accepted the v008 observation as RFC-BPR-0006's terminal
attended row on the `run-boundary-decisions-v001` page (batch
`bpr:/attended-review/betterpullreview/c7e77830ef4fa92c6047c99b`, subject
`decision:/betterpullreview/bpr-0114/accept-v008-as-terminal` accepted; the
v009 sitting recorded as not chosen; validated verdict and record beside the
page). The evidence this card closes on is the v008 operator response, raw
log, and validation record — `APPEARANCE_SYSTEM_FOLLOWS_OK`,
`APPEARANCE_EXPLICIT_IGNORES_OK`, `APPEARANCE_VARIANTS:0:1:0:1`, strict owner
return — plus [[BPR-0220]]'s accepted zero-rebuild repair, under the board's
reuse rule: no appearance-path commit since 2026-08-21, and the owner's own
2026-08-21 request that no redundant digest-only rerun be required. The
`appearance-scheme-foreground-v009` directory stays empty; v008 is the
family's live, answered packet.
