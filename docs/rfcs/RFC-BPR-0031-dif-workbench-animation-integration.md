---
id: "RFC-BPR-0031"
title: "Dif Workbench Integration: Profile Preferences, Bounded Qt Playback, and Truthful Cues"
status: "Auto-Verified"
area: "BPR"
milestone: "Dif Integration v1"
depends_on: ["RFC-BPR-0030","RFC-BPR-0005","RFC-BPR-0006"]
metadata: {"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# RFC-BPR-0031: Dif in the real BetterPullReview workbench

## Summary and scope

Integrate the accepted RFC-0030 ten-clip Dif catalog in the normal BPR C++/Qt app.
Home receives calm presence; the existing explicitly opened AI tools area receives
short, truthful gestures derived from existing accepted presentation facts. This is
not a second AI agent, an author/avatar, or a review-status authority.

This additively supersedes the named 0020 restrictions on three clips, landing-only
placement, old single-window counts and unimplemented global preference assumptions.
Its architecture, native presentation, lifecycle, nonessential status, provenance and
runtime-isolation principles remain. Current profile/window and Home behavior is
preserved rather than rolled back to the original 0020 sketch. See the pack's
reconciliation table. Do not edit settled RFCs or historical acceptance packets.

All ten clips are implemented and exercised in this workstream. Landing-only is an
incremental milestone; completing it does not complete this RFC. A clip in a demo or
Settings gallery alone is not evidence of its ordinary product reachability.

## Product placement

### Home

Place Dif beside the existing `workbench_landing` guidance, at 96 logical pixels
normally, 72 at the compact breakpoint, with a reviewed 64-pixel minimum. Add no
separate dashboard, banner, mascot tab or new PR-open flow. Titles, paste-to-open,
sign-in/Settings actions, recently viewed items, queue, and Back to the review remain
primary. Below the space needed for usable controls, hide Dif before squeezing them.

Home can be visible over a still-open review. Eligibility depends on the actual
presented page; it does not clear the review or require `current PR` to be null.
Returning to Read/Discuss/Overview reclaims the illustration's Home layout space and
preserves file, selection, epoch pair, scroll anchors, comments, and focus.

In full motion, greet once on the first eligible Home exposure of each new workbench
lifetime, then calm idle. Ordinary activation, retheming, queue refresh, Home toggling,
or going back from Settings does not greet again. A newly created window may greet
once; an old window restored from deactivation does not. Greeting is decorative and
never means sign-in or an operation succeeded.

After four uninterrupted completed three-second calm loops, play one 0.6-second blink
at the loop boundary, then restart calm idle. No independent random/fidget timer. A
semantic one-shot, hide, deactivation or reduced motion resets this breath-cycle count.
The normalized calm clip contains no additional blink. Inactivity does not build a
catch-up blink/greeting debt.

### Explicit AI tools area

Add a small, noninteractive illustration slot associated with the existing Side Chat /
Findings tools in the conversations drawer, not an avatar on each message. Prefer a
single shared 64–72-pixel header/intro slot that remains stable while the user works
there. It must not cover comments, change the code pane's geometry per frame, force
open a drawer/tab, steal focus, or replace provider/model/effort/provenance labels.

Only an explicitly selected and actually visible AI tools section can use this slot.
A generic Conversations drawer showing shared PR comments is not permission to show
Dif. If the live UI places Side Chat/Findings differently, use that actual explicit
surface and record the narrow layout adaptation; do not create a parallel AI surface.
When its required controls would become cramped, the art disappears first.

In this slot, the baseline is static `rest`. **No continuous idle, blink, or thinking
loop while reading/writing code.** A valid cue plays once and returns to rest. The
runtime knows the return pose is the same neutral used by the asset's calm return;
it does not mutate the production clip manifest to encode domain state.

There is at most one sprite widget per workbench. Reuse it between eligible slots;
Home wins when it is the presented page, otherwise the explicit visible AI slot may
own it. The two must never be painted/animated simultaneously in one window. All other
surfaces hide it. No persistent toolbar/status mascot, tray animation, code overlay,
inline author image, or blocking mascot dialogue is added.

### Owner amendment, 2026-09-25: Dif leaves the conversations drawer

In chat, relayed by the coordinator, the owner directed, verbatim: "Dif living
in a tab that's likely hidden forever makes no sense, definitely doesn't belong
there. [...] Move Dif out of the right drawer entirely, he can live in the lower
left corner of the status bar by default just like AllTheThings unless he's got
a prime spot on a screen like on the first time experience home." It extends
to every surface the owner's direction of 2026-09-15, which RFC-BPR-0032
records for Home.

Hand-back 49 on 2026-09-26 further asks for a smaller corner presence.
BPR-1209 adopts 28 logical pixels under the owner's delegated choice; the
Home and workshop prime sizes remain unchanged.

This amends the placement above:

- **The drawer slot goes.** Dif no longer uses a slot in the conversations
  drawer.
- **The corner is his default place.** On every workbench surface without a
  prime spot, the same single widget rests at 28 logical pixels in the status bar's
  leading corner. There it has no pointer or focus path, and it steps aside
  while the status bar carries a transient message, as RFC-BPR-0032 records
  for Home.
- **Two screens give him a prime spot.** The signed-out first-time Home keeps
  the landing placement above. While a workshop proposal is shown on the
  Compose review page, and after it is accepted, Dif takes that page's
  workshop slot, as the owner chose on [[BPR-0966]] (decision `dba44cfd`).
- **Two statements no longer hold.** "All other surfaces hide it" and "No
  persistent toolbar/status mascot" give way to the corner.

The coordinator relayed that the owner delegated the resulting choices. Under
that delegation it was decided that every cue keeps its trigger, identity and
deduplication and plays wherever Dif is. That includes `compare_glance`, on the
reviewer's explicit selection of the tray's AI tab, so B7 is unchanged. The
one-sprite rule, the preferences, the lifecycle and the catalog are unchanged.

The acceptance criteria below keep their accepted text. B6's "explicit visible
AI tools slot" is read as the corner or a prime spot, and B5's courses that
assert Dif is hidden elsewhere follow the amendment. [[BPR-1209]] builds it and
carries [[BPR-0800]]. Until it lands, the product still shows the drawer slot.

### Owner amendment, 2026-09-13, restored 2026-10-02: Dif in the macOS menu bar

On 2026-09-13 the owner directed one static monochrome Dif in the macOS menu
bar (mandate revision 5 of the Dif pack). It was recorded only on the
`claude/bpr-dif-integration-v1-d932e2` lane, whose session stopped before the
work was built, and the integrated pack never carried it. On 2026-10-02 the
owner rejected `BPR-1250-help-v001` with: "the menubar icon needs to be a
monochrome/greyscale Dif avatar so it can be colored by macos -- I could have
sworn we did that already." [[BPR-1250]] restores the amendment and builds it.

This amends the placement above:

- **One status item, for the life of the process.** On macOS the desktop shows
  one menu bar status item from startup until Quit, with no window as well as
  with one. Every profile's notifications submit through it.
- **Dif's head, as a template image.** The item shows Dif's head, black with
  alpha at 18 points in 1x and 2x and marked as a mask, so the menu bar draws it
  in its own colour on a light, dark or highlighted bar. It is derived offline
  from the accepted neutral master and is not a catalog clip.
- **Static.** No tray animation, pose change or cue.
- **macOS only.** Linux and Windows keep a tray per profile with the
  application icon.

## Architecture

**The core decides. The host performs. Qt presents.**

| Region | Responsibility |
|---|---|
| Core appearance contract | Closed motion/visibility preferences, legality/defaults, deterministic resolution from explicit bounded platform observation. No clip, frame, timer, widget, resource, provider call or animation cursor. |
| Existing profile appearance preference host/store | Persistence/migration, accepted settings, per-profile fan-out, attachment-bound observations, labeled session-only fallback. No image/clip policy. |
| Existing review/agent hosts | Unchanged authority, revision binding, session/run lifecycle, accepted reply/finding/workshop/draft facts. No new mascot effects. |
| Qt presentation | Fixed catalog/cache handles, pure elapsed-time sequencer, widget painting, OS motion observation adapter, window/surface gating, presentation-only cue router and dedup latches. |
| `src/app/` composition | Inject one shared immutable art cache and observation facility, wire existing accepted facts to the correct window adapter, own lifetimes; no added domain decision about correctness. |

Do not place a mascot enum in a provider response, SQLite review record, or functional
review state. A Qt-local deterministic helper is not a reason to move clip arithmetic
into the functional core. Existing core-accepted facts remain the only review truth.
No new singleton, service locator, message broker, thread, animation framework, QML,
Qt Quick, QMovie, runtime SVG/GIF/APNG decoder or asset-directory service.

## Profile preferences and platform observation

Expose a small Appearance group using the real `settings_window` and accepted host
path: **Show Dif mascot** (default shown) and **Motion: System / Full / Reduced**
(default System). Proposed closed values are `mascot_visibility = shown | hidden`
and `motion_mode = system | full | reduced`. Their exact type placement follows the
live appearance contract; no shadow global `mascot_enabled` database/key.

The profile owns these preferences. Same-profile windows update together; another
profile is unaffected. Motion resolution is per attachment from the shared native
observation, never a stale setting write. Hidden means no art and no timers in either
slot. Reduced means static awake rest, no fades/bobs/pulses or one-shot playback.
System uses the reliable native reduction observation; unsupported/unknown/failure
resolves conservatively to reduced with an honest Settings explanation. Full is the
user's explicit override, including on an unsupported system.

Implement a narrowly scoped read-only native observation at the Qt/platform edge and
observe changes without polling. Inspect the current supported platform APIs during
implementation; do not claim support from a constant or fixture. If a platform cannot
reliably observe reduction, expose unsupported and allow explicit Full. Changing OS
preferences for validation requires an attended/native authorized course and restoring
the original setting; tests do not silently mutate them. This RFC does not add broad
accessibility inventory or a settings subprocess.

Migrate from the **actual current** store version (version 5 in the supplied source,
not the old manifest's version 3). Validate legacy values before transformation and
preserve palette, palette variant/stored flags, typography, syntax roles and unrelated
settings. Future/corrupt schemas are not silently repaired. Reserve the next version
at import to avoid parallel-work collisions. A failed write keeps the accepted state
and follows existing session-only/error behavior; all same-profile windows must agree
on its meaning. Reset reaches these fields through the existing scoped appearance reset.

Theme import/export is not a covert animation preference exchange: preserve its current
palette-only meaning unless the live format already explicitly includes these fields.
Changing motion/visibility must not invalidate lexical classification caches, rebuild
diff geometry, or apply a forced palette/QSS. Attach/detach and rejected/stale settings
completions obey the existing profile host's lifetime fences.

## Compiled resources and cache

Compile only RFC-0030's accepted generated strips and descriptor into the selected Qt
application target. Before exposure validate catalog schema/digest, resource identity,
exact dimensions/frame ranges/gutters and checked memory bounds. Decoder failure or
mismatch keeps the current surface functional without the illustration and emits one
bounded non-secret diagnostic per failure identity; do not substitute old art or crash.

One composition-owned, read-only decoded image catalog is shared across windows and
profiles for the process presentation lifetime. Initialize once when an eligible art
surface first needs it; retain until teardown. Art has no account data, so profile
switching does not decode duplicate copies. Per-window timers/cursors/cue state are not
shared. Inject the cache explicitly; do not hide it in a process-global singleton.

Retain one decoded representation, normally premultiplied RGBA suitable for the paint
engine, derived once from the declared straight-alpha source. Account for conversion
peak and retained memory. Do not retain full mirrored QImage and QPixmap catalogs by
accident. The complete pixel budget is 24 MiB; add and measure Qt/container/backend
costs separately. Set a 32-MiB art-owned retained CPU-memory ceiling for decoded
images, descriptor/cache records and their measured container overhead; measure the
transient decode/conversion peak separately and keep it below 40 MiB by decoding and
converting one strip at a time. Any backend-owned texture/cache duplication is measured
and reported separately, not hidden in the 24-MiB pixel claim. A target exceeding an
art-owned ceiling needs a reviewed budget change or a simpler cache, not an exemption.
Per-frame painting selects a source rectangle from that retained
image into a stable target rect. No per-frame image creation, slicing allocation,
resizing cache, decode, color conversion, metadata parsing or filesystem I/O.

Ordinary build/resource compilation never runs image generation or a model. Update
`verify_dif_asset_boundaries.py` to distinguish selected Qt app resources from forbidden
core-only assets/build tools, not to exempt everything with "dif" in its name. The
normal tests-disabled Release and tests-enabled app select the same catalog bytes.
Core-only retains no sprite bytes, Qt decoder, platform motion observer or generated
resource target. Review aids and raw input files are never bundled.

## Elapsed-time playback

Use one Qt-local sequencer with explicit catalog, current surface, motion eligibility,
clip start, and monotonic elapsed input. It returns a frame/source-rectangle identity
and optional next deadline. The widget owns the single one-shot Qt timer. Frame counts
and positive durations are checked; static rest has no deadline. Timing comes from the
catalog, not filenames or hardcoded handoff frame counts.

For loops select floor(elapsed/frame-duration) modulo the frame count. For one-shots,
select within the duration and then resolve directly to the surface's neutral baseline.
A delayed wake computes the current result once; it never walks all missed frames or
plays missed semantic cues. On a long delay, do not emit multiple expired one-shots or
synthesize queued fidgets. A new exposure restarts from a fresh neutral baseline.

Limit scheduled mascot timer wakeups to the declared rate, never over 12/second for the
active animated instance; coalesce repaint requests. Check duration/count overflow,
negative/nonmonotonic supplied time and invalid frame/catalog state. Failure stops
animation and selects validated rest (or hides art if the catalog itself is invalid).
No timer performs domain polling or fetches a new application snapshot.

Hidden page/drawer/tab, non-exposure, minimized/deactivated workbench, application
suspension, modal obstruction, hidden preference, reduced motion, profile/window
teardown and app shutdown cancel pending deadlines immediately. No greeting/cue backlog
survives. At most the already-dispatched event may drain; no next timer is armed and
no next frame change occurs. Disconnection and cancellation precede releasing widget,
projection bridge or cache dependencies.

Inactive windows may paint static rest without wakeups. One active top-level
presentation may animate; inactive workbench windows remain static. A Settings window
does not run a second continuously animated gallery. Platform/deactivation ordering
must be tested, including a modal that blocks an otherwise visible workbench.

## Truthful cue mapping

These are **presentation proposals**, not current host APIs. Implement through the
existing accepted callbacks/identities identified in the source map. Keep provider,
actual model/effort when known, source provenance and authoritative status in real UI.
Do not parse prose/logs, inspect tokens for "found", or infer success from a smile.

| Clip | Accepted trigger and eligible placement | Must not imply |
|---|---|---|
| calm_idle | Full-motion Home baseline | Loading, progress or AI analysis. |
| greet | First eligible Home exposure in this workbench lifetime | Authentication or operation success. |
| rest | Any eligible static baseline, reduced/inactive state | Sleeping, offline or disconnected. |
| blink | Every fourth uninterrupted completed Home breath cycle | A state or notification. |
| compare_glance | User explicitly selects/reveals the existing AI Findings section and its context is actually shown, with Dif's AI slot visible | The app/model compared or approved the code. Never per scroll, selection move or incoming update. |
| consider | The current explicitly requested invocation actually reaches the existing `start` projection for the visible AI context | A queued/refused click started a process; continuous analysis while merely waiting. |
| comment_ready | Validated reply content or a workshop proposal has actually been accepted and presented for the visible session | Publication, approval, or mere process-exit success. |
| attention | A delivered batch for that same visible request has at least one accepted presentable finding requiring the user's triage | A proven defect, one gesture per finding, or that Dif authored the finding. |
| acknowledge | An explicit local workshop/apply or draft promotion receives its existing accepted result and the accepted draft revision is presented in the originating AI context | Public comment submitted, code approved, or a save completed before its existing acceptance point. |
| blocked | Current explicit request has a terminal refusal/failure, or the visible session acquires a real continuation choice, and the matching explanation/action is already presented | Every background error, sadness, provider outage inferred from silence, or an old result belonging to another review. |

Every clip gets an ordinary-app path in the final verification. For empty/unavailable
findings, opening the actual Findings view may still produce the decorative comparison
glance, but never fabricates findings. Turning art off changes no status/actionability.

If the current draft path distinguishes optimistic editing from accepted/durable state,
use its accepted projection for acknowledge. Failed/stale/revision-conflicted apply
cannot produce that cue. An accepted explicitly labeled session-only edit is at most
an acknowledgment of the local edit and retains the nondurable label; never rename it
"saved". The mascot itself stores no draft and makes no provider mutation.

### Identity, deduplication and coalescing

Bind cue candidates to the originating BPR profile, window and generation, PR authority,
agent session/exact revision binding and invocation generation, or the existing draft
operation/revision identity for a local apply. Reuse strong IDs, not strings assembled
from display text. Unavailable/unknown identity yields no semantic gesture.

Only accepted live events for the currently visible context can trigger cues. Loading
existing history primes a baseline and does not replay old successes/failures/findings.
A new invocation replaces the bounded current invocation latch; late events for older
invocations are ignored. Session/PR/profile/window changes cancel candidates. Resuming a
hidden surface shows current text/rest; no retrospective animation. Continue-against-old-
revision remains bound to that explicitly selected old session and its labels, never
silently mapped to the head currently in the diff.

The existing invocation host delivers findings before finish. Accumulate only bounded
facts for the current admitted invocation until its terminal projection is accepted;
choose at most one outcome gesture: **blocked > attention > comment_ready > none**.
A successful exit without actual presented content yields none. This prevents a ready
wave immediately followed by a contradictory blocked/finding gesture. Workshop proposals
and local accepted apply are separate explicit operations, each keyed to its own result.

No generic event queue, timer-per-reply, unbounded seen-ID set or background ledger.
Maintain current invocation start/terminal latches, latest accepted draft operation/
revision, and view activation state. Replayed terminal/draft projection is a no-op even
after other UI updates. Records older than the currently admitted operation cannot
be made eligible by cache eviction; compare against the active identity, not just an
LRU of recently seen events. A window generation reset starts neutral and primes current
state rather than replaying past operations.

Use clip priorities in the design contract. Navigation/hide/reduced motion always
preempts instantly. Current failure/attention may replace a lower-priority gesture;
other arrivals while a gesture plays are coalesced to the current valid outcome or
dropped, never queued for later celebration. Semantic gesture starts are rate-limited
to at most one per two seconds in a given visible context; the comparison-view glance
has a five-second minimum interval. Suppressed animation never delays text, controls,
results or error recovery. Cooldowns have no wakeup when no other animation is active.
An urgent status can update immediately without any animation.

## Input, accessibility and appearance

Dif accepts no pointer/keyboard/drag/drop/context-menu interaction, has no shortcut,
and is absent from the tab chain. Keyboard and pointer behavior belongs to unchanged
adjacent controls. Expose it as decorative where supported; otherwise use a stable
name such as "Dif, BetterPullReview mascot", without frame announcements. No live-region
updates per frame and no new notification solely for a pose.

The brand colors stay in the accepted raster, surrounded by native/inherited palettes,
fonts and style. Test visible outline and faces on actual light/dark/high-contrast
surfaces and 1.0/1.25/2.0 device scale. A scheme/DPR change repaints only necessary
presentation and does not regenerate sprites, restart greeting or rerun syntax analysis.
Every meaningful condition already has text; reduced/hidden art is semantically equivalent.

## Verification and rollout

Build incrementally: accepted assets and runtime engine; Home/Settings; contextual cues;
then focused real-app integration. Independent engine/preference work can continue while
an art subject is awaiting a human verdict. Final shipped resources require that verdict.
No default live GitHub mutation or paid model run is necessary: existing deterministic
provider/agent fixtures drive the real application composition with the same widget and
assets. They prove wiring, not provider/native behavior outside the fixture's evidence.

Use one integrated app course on the primary available target for all ten states,
normal speed/size, interruption, duplicate events, settings restart, Home/retained review,
AI surface visibility, two windows/profiles and shutdown. Capture timestamped actual
rendered frames plus decode/timer/layout counters, including a static interval spanning
several nominal frame boundaries. A helper viewer/contact sheet or direct widget setter
cannot replace the ordinary gesture/accepted-projection path.

Run focused native/build smoke for supported Windows, macOS and Linux targets; repeat
only platform-specific observer, packaging/resource and lifecycle evidence, not every
creative subject. Preserve still-valid evidence. Missing target or unsupported observer
gets an explicit status/card; never fabricate cross-platform success from compilation.
One focused final art/UI review may reference RFC-0030's still-current visual verdicts
rather than asking to approve the same pixels again. Critical review concentrates on
migration, profile isolation and callback/timer lifetimes, not ritual per-file re-review.

## Acceptance criteria

- **B1 — Resource selection.** Normal tests-enabled and tests-disabled Qt apps select the
  accepted ten-clip catalog; raw inputs/review tools do not ship. Invalid assets fail to
  a functional art-free surface with bounded diagnostics. Core-only remains clean.

- **B2 — Playback.** Catalog-driven elapsed-time playback renders every clip at its declared
  rate, one-shots return correctly, idle seam/blink cadence/greeting-once behavior holds,
  and delayed/invalid time cannot burst, overflow or select gutters.

- **B3 — Preferences.** Real Appearance controls apply/migrate/persist/reset motion and
  visibility through the current profile authority; rejected/stale writes and session-only
  fallback are truthful, same-profile fan-out works and other profiles remain unchanged.

- **B4 — Observation.** Native System motion observation/change handling is proved per
  claimed target; unknown/unsupported resolves reduced, explicit Full is usable, and
  fixture/native claims and any changed OS setting are documented separately.

- **B5 — Home.** Dif is reachable through the existing signed-out and signed-in Home,
  including Home covering an open review. Open/back actions, Recent/queue controls,
  compact layout, focus and return-to-reading state are unchanged.

- **B6 — Context placement.** One explicit visible AI tools slot uses static baseline and
  short cues, never overlays code/shared comments or opens surfaces itself; one sprite
  per workbench, no duplicate active slot, and controls retain space/accessibility.

- **B7 — All-ten product paths.** Each clip is shown by its specified ordinary UI action
  or accepted projection. Actual replies/findings/draft acceptance drive ready/attention/
  acknowledgment; no process-only success, AI author substitution or autonomous effect.

- **B8 — Identity/dedup.** Duplicate, stale, replayed, switched-window/profile/PR/session,
  late invocation, rejected draft and superseded operation cases suppress incorrect cues;
  terminal coalescing and cooldowns never delay authoritative UI.

- **B9 — Lifecycle.** Hidden, inactive, reduced, modal-obstructed, minimized, suspended and
  destroyed presentations have no continuing mascot timers/repaints/late callbacks.
  Resume is neutral with no queued cues or repeated greet. Shutdown remains orderly.

- **B10 — Performance.** One decoded retained catalog is shared across windows/profiles;
  measured pixels/overhead/conversion peak are bounded, no per-tick image allocations or
  layout/classification changes, and a focused comparison shows no diff-work regression.

- **B11 — Rendered/accessibility evidence.** Actual app frames at shipping scales and
  supported appearance modes demonstrate readable alpha edges, calm motion, static reduced
  interval, noninterference with focus/status and approved visual integration.

- **B12 — Complete handoff.** Focused tests, resource packaging, platform claims, source/
  mutable docs, shared-skill consumer context and verification records agree. All ten clips
  are real and reachable; unavailable evidence/formal closeout remains honestly open, with
  no fabricated native/art verdict or forced rewriting of settled RFCs.
