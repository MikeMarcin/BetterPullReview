---
id: RFC-BPR-0020
title: Animated Dif Mascot Qt Presentation and Workbench Composition
status: Deferred
area: BPR
milestone: Post-Pack v2
depends_on: [RFC-BPR-0005, RFC-BPR-0006, RFC-BPR-0013, RFC-BPR-0019]
pack_version: post-v2
---

# RFC-BPR-0020: Animated Dif Mascot Qt Presentation and Workbench Composition

## Status and scope

This is the second of two ordered deferred post-pack RFCs for Dif. It consumes
only the Auto-Verified RFC-BPR-0019 asset contract and composes one animated,
nonessential mascot into BetterPullReview's real Qt workbench landing surface.

The governing rule remains **The core decides. The host performs. Qt
presents.** Semantic motion preference and durable acceptance use the existing
appearance core and host; Qt owns platform motion observation, resource decode,
frame progression, painting, focus/accessibility mechanics, visibility, and
animation lifetime. The mascot performs no provider, Git, SQLite, agent,
filesystem, network, or process effect.

This RFC does not make Dif a review actor. The first public integration offers
only a calm idle loop, one landing acknowledgement, and a static rest pose.
Reviewing, thinking, issue-found, approving, commenting, and other semantic
states remain outside scope until a later RFC defines truthful typed cues.

## Problem

BetterPullReview has no mascot presentation or motion preference. Simply
placing a timer and pixmap label in `workbench_window` would leave several
material questions unanswered:

- whether animation is allowed under the user's motion preference;
- when the timer starts, pauses, resumes, and dies;
- whether delayed ticks can burst or outlive the widget;
- whether a loaded pull request loses useful review space to decorative art;
- whether Light, Dark, high contrast, high DPI, focus, and accessibility remain
  correct;
- whether the real executable selected and decoded the reviewed RFC-BPR-0019
  resources rather than a fixture substitute; and
- whether a screenshot of one frame was mistaken for proof of animation.

The mascot must therefore enter through the same typed appearance,
presentation, composition, build, and verification surfaces as the rest of the
native workbench.

## Product contract

BetterPullReview shows one Dif instance in the existing empty/landing guidance
region when no pull request review is active. The mascot is visually secondary
to the connection and open-pull-request controls. It never overlays source,
connectors, comments, drawers, dialogs, menus, notifications, or selected-diff
detail, and it returns all of its layout space when the active review surface
replaces the landing region.

When full motion is allowed, the first visible landing presentation plays the
bounded `greet` clip once and returns to `calm_idle`. Re-showing or activating a
window does not repeatedly greet unless a new landing-surface generation is
created. When motion is reduced, animation is suspended, or the widget is not
actually exposed, Dif shows RFC-BPR-0019's `rest` frame.

Dif is decorative. It accepts no pointer, keyboard, drag, drop, clipboard, or
context-menu input; is absent from the tab chain; and cannot be the only source
of application status. Existing landing title, guidance, connection state,
diagnostics, and accessible status remain authoritative.

## Goals

- Add a typed, durable application-wide motion preference to the existing
  appearance contract, defaulting to `system` and supporting explicit `full`
  and `reduced` choices.
- Observe platform reduced-motion capability/value at the Qt/platform edge and
  resolve it with the accepted preference without hidden globals.
- Implement one bounded Qt sprite catalog, animation sequencer, and custom
  mascot widget using the exact RFC-BPR-0019 resources.
- Compose Dif into the real `workbench_window` landing path selected by
  `app_better_pull_review`, not a parallel sample or test-only window.
- Make frame progression elapsed-time based, deterministic under explicit test
  input, coalesced after delays, and quiescent across hide, deactivation,
  landing replacement, destruction, and application shutdown.
- Preserve native palette/style/font inheritance, high-DPI layout, focus,
  accessibility, workbench identity, and bounded incremental updates.
- Directly prove motion with time-separated rendered frames and prove reduced
  motion with a static interval through both the Qt component and real
  application.

## Non-goals

- Adding review-result, agent, GitHub, defect, approval, notification, comment,
  queue, or loading semantics to mascot poses.
- Showing Dif over an active source comparison, as an inline comment avatar,
  in a notification, or as a persistent toolbar/status icon.
- Making Dif clickable, focusable, draggable, dismissible, or separately
  resizable.
- Creating a mascot-specific settings database, service, process, thread,
  network endpoint, or runtime asset directory.
- Loading images, clip metadata, or animation instructions from a repository,
  provider, plugin, theme package, environment variable, current directory, or
  user-controlled path.
- Adding QML, Qt Quick, Qt WebEngine, SVG runtime rendering, GIF/APNG decoding,
  Core Animation, AppKit, or a foreign animation framework.
- Claiming final package installation, signing, notarization, update behavior,
  or cross-platform package readiness before RFC-BPR-0014 owns those surfaces.

## Architectural allocation

| Region | Responsibility |
|---|---|
| Functional core | Closed `motion_mode` vocabulary, default, legality, resolution from a bounded platform observation, persistence value, reset semantics, typed accepted/rejected outcome, and presentation projection. No timer, frame, image, Qt, platform API, or mascot clip state |
| Appearance application host | Load/save/migration sequencing through the existing appearance settings authority, generation and stale-completion handling, session-only fallback, and accepted motion projection. No platform probing, resource decode, or animation timing |
| Qt/platform presentation adapter | Platform reduced-motion observation, catalog/resource validation, one decoded asset cache, elapsed-time clip sequencing, visibility/activation/suspension, custom painting, layout, scale, palette, accessibility, and event translation |
| `workbench_window` | Owns the one mascot widget in the landing region and changes its exposure with the existing landing/review presentation. It owns no frame policy, persistence, or semantic review mapping |
| Composition root | Selects the platform motion observer, routes the existing typed appearance preference and projection, supplies the fixed Qt resources, constructs one mascot, and destroys it before presentation dependencies disappear |

Animation progress, current frame, elapsed clip time, decoded images, device
pixel ratio, paint cache, exposure state, and greeting-generation state are
Qt-local. They never enter core state, the application host, SQLite, logs, or a
provider projection.

## Motion preference and platform observation

The appearance core adds this closed preference:

```text
motion_mode = system | full | reduced
```

- `system` is the fresh-install and reset default.
- `reduced` always selects the static `rest` behavior.
- `full` permits the reviewed clip motion even when the platform preference is
  unavailable. It is an explicit user choice, not the default.
- `system` resolves to reduced when the platform reports reduced motion.
- If the selected platform exposes no reliable reduced-motion value, `system`
  resolves conservatively to reduced and the presentation explains that full
  motion remains an explicit choice.

The platform observation is a bounded value with `supported` and
`reduce_motion` facts plus an observation generation. It contains no native
handle or provider type. The Qt/platform edge obtains it through a documented,
target-appropriate, read-only API and emits changes on the serialized
application/GUI lane. The implementation records which supported operating
systems expose a reliable observation; it does not simulate support or shell
out to a settings command.

The existing appearance store advances through a versioned transactional
migration. Older valid records acquire `system`; future, malformed, or
out-of-range values fail through existing appearance fallback. Store failure
may leave the control session-only only where the appearance contract already
permits that fallback. A mascot widget never writes the store directly.

The existing appearance settings surface exposes `System`, `Full`, and
`Reduced` with one plain-language consequence and the current platform support
fact. RFC-BPR-0018 may later move that control into the Settings center; it
must route the same typed intent and cannot create a second preference.

Hermetic tests may inject platform observations to exhaust resolution and
change-delivery behavior. Such an injection is not evidence that a production
observer was selected or that an operating system exposes the claimed value.
Each platform support claim requires a target-native run through the real
selected observer, an exact before/change/after observation, and restoration of
the owner's original setting. When that controlled change is unavailable, the
platform is recorded as unsupported or its claim remains on an explicit live
card; a fixture constant cannot close it.

## Qt sprite catalog

The Qt adapter compiles RFC-BPR-0019's generated strips and catalog as fixed
resources. Construction validates the catalog schema/digest, every declared
resource, dimensions, frame ranges, return target, and decoded budget before a
mascot is exposed. A missing, corrupt, excessive, or mismatched resource fails
closed to the existing text-only landing surface with one non-secret
diagnostic; it does not crash, scan a directory, or substitute foreign art.

The catalog is decoded at most once per application presentation lifetime and
shared read-only by the one admitted widget. Runtime painting selects a source
rectangle from retained image data. It performs no filesystem I/O, JSON parse,
image decode, slicing allocation, offscreen resampling, or color conversion per
frame. The paint engine may sample that retained source rectangle directly into
the target rectangle; doing so must not create or retain a new image for each
tick. The cache stays within RFC-BPR-0019's complete 13.5 MiB decoded-strip
pixel budget plus separately measured and bounded Qt/container overhead and is
released at presentation teardown.

Qt resource binding belongs to the selected application graph. `core-only`
contains neither the raster bytes nor their decoder/renderer. Tests-disabled
Release contains the exact same reviewed Dif resources and no test substitute.

## Sequencing and lifecycle

The sequencer accepts an explicit clip catalog, motion allowance, exposure
facts, landing generation, and elapsed monotonic duration. It returns a frame
index and the next wake deadline. This deterministic Qt-local component is
tested by supplying elapsed durations directly; it has no wall-clock read or
timer of its own.

The widget owns the Qt timer and monotonic elapsed source. Its rules are:

- at most one timer is armed and at most one repaint is pending;
- delayed delivery advances directly to the correct frame for elapsed time and
  never emits a catch-up burst;
- a zero/negative/overflowing duration, unknown clip, or invalid frame stops
  animation and selects `rest`;
- `greet` plays once per landing generation and returns through the manifest's
  declared `calm_idle` target;
- reduced motion disables all frame changes, fades, bobs, pulses, procedural
  motion, and one-shot playback;
- hide, non-exposure, window deactivation, landing replacement, application
  suspension, and shutdown stop the timer and select `rest`;
- palette, font, style, device-pixel-ratio, screen, or geometry change
  invalidates only derived presentation and never restarts a greeting;
- a resumed, exposed, full-motion landing continues from a new calm baseline,
  not from elapsed time accumulated while suspended;
- destruction disconnects observations and cancels pending delivery before
  catalog/cache release; no callback captures a dead widget or application.

Window deactivation pauses decorative work. A platform may keep a visible but
inactive window painted; it still displays `rest` without consuming timer
wakeups.

## Rendering and layout

`dif_mascot_widget` is one custom-painted Qt widget with a stable size hint and
height-for-width policy derived from the admitted landing layout. It paints the
current frame into an aspect-preserving target rect using the device-pixel-ratio
appropriate retained source. It does not create one child widget per frame and
does not install QSS, a forced palette, or a forced style.

The landing layout reserves 72-96 logical pixels for Dif at ordinary supported
window sizes and may use the reviewed 64-pixel presentation at the compact
boundary. Below the minimum usable landing width, the mascot disappears before
connection controls, labels, diagnostics, or keyboard access are compressed.
No review-mode geometry includes mascot space.

Dif's stable brand colors remain in the raster, while any focus/background
plate, diagnostic text, and surrounding surface derive from the active Qt
palette and existing appearance profile. The widget repaints on application
palette, font, style, color-scheme, and screen/DPR changes. It does not recolor
source pixels into a second theme system.

## Accessibility and input

- The widget uses `Qt::NoFocus`, accepts no input, has no shortcut or action,
  and keyboard traversal skips it in both directions.
- It is hidden from accessibility as a decorative image when the platform
  accessibility bridge supports that distinction. If exposed as an image, its
  stable name is `Dif, BetterPullReview mascot` and its description is
  `Decorative panda companion`; frame names and animation progress are never
  announced.
- Greeting, looping, pausing, and frame changes emit no live-region event.
- Existing text communicates every landing, connection, loading, empty, and
  error fact. Turning animation off changes no accessible meaning or command.
- High contrast, large fonts, 1.0x/1.25x/2.0x scale, long translations, and
  keyboard-only use retain the existing landing controls and status surfaces.

## Public composition

The only accepted public path is:

```text
app_better_pull_review
  -> QApplication and fixed application resources
  -> github_application composition
  -> accepted appearance/motion projection
  -> workbench_window landing presentation
  -> dif_mascot_widget
```

The normal executable and its production-shaped fixture construct the same
widget and resource catalog. A mascot demo, isolated render helper, linked
symbol, or direct widget test does not prove product reachability.

The public proof changes `Full` and `Reduced` through the real appearance
gesture and accepted host/store projection. A command-line flag, environment
variable, direct widget setter, injected platform observation, or fixture-only
motion override cannot satisfy that row. The composition course also counts
live mascot widgets, decoded catalogs, and armed mascot timers so one visible
Dif cannot conceal a second hidden instance doing work.

Opening a pull request replaces the landing presentation and proves the Dif
widget is hidden, its timer is stopped, and the comparison's file selection,
focus, drawers, geometry, and selected-diff identity are unchanged. Returning
to a new landing generation may greet once. Refresh, appearance changes, queue
updates, notifications, comments, and agent activity do not create mascot
semantic states.

## Failure handling

- Resource/catalog failure keeps the text-only landing functional and reports
  one bounded non-secret diagnostic through the existing application status
  path.
- Appearance-store failure follows the existing labelled session-only/default
  behavior and cannot disable GitHub, diff, or review functionality.
- Platform motion observation failure resolves `system` to reduced without
  retry loops or status spam.
- Timer or paint failure is contained to the mascot presentation and cannot
  veto an accepted pull-request transition, durable appearance commit, window
  close, or orderly shutdown.
- No failure path downloads an asset, reads the current working directory, or
  falls back to Nib, Mel, the concept sheet, the application icon, or an
  arbitrary file.

## Performance and resource limits

- Exactly one visible mascot and one decoded catalog are admitted.
- Decoded pixels, clips, frames, frame rate, and source dimensions remain
  within the RFC-BPR-0019 manifest bounds; construction rejects excess.
- Full motion schedules no more than the declared clip frame rate and never
  exceeds 12 timer wakeups per second for Dif.
- Hidden, inactive, review-mode, reduced-motion, suspended, and closing states
  produce zero mascot timer wakeups after the current decision turn drains.
- A frame change invalidates only the mascot rect. It does not relayout the
  window, reset a model, rebuild diff geometry, rerun syntax classification, or
  repaint the selected workbench.
- One instrumented long-run course records timer wakes, paints, decodes,
  allocations, layout requests, and retained bytes; delay and activation churn
  remain bounded without drift or bursts.

## Security and privacy

- Mascot resources are immutable compiled application inputs with checked
  digests; no remote or repository content selects a resource or frame.
- Motion preference records only the closed enum. Platform observation records
  only support and reduction facts; no broad accessibility inventory is
  persisted or logged.
- Frame data, diagnostics, tests, and evidence contain no credentials, private
  repository content, comments, source, prompts, agent output, or user paths.
- The widget creates no process, network request, file read, database access,
  clipboard content, drag payload, telemetry, notification, or external
  accessibility action.
- Resource/license/SBOM handling extends RFC-BPR-0014's existing package rules
  without weakening them or claiming package completion.

## Prohibited designs

- A widget timer that also decides review state, polls an application object,
  or queries GitHub, Git, SQLite, an agent, files, or settings.
- Core or application-host state containing a frame index, elapsed animation
  time, `QPixmap`, `QImage`, `QTimer`, device pixels, visibility, or native
  motion API types.
- A process-wide mascot singleton or service locator.
- `QMovie`, GIF/APNG, runtime SVG, QML, Qt Quick, AppKit/Core Animation, or a
  third-party animation runtime for three bounded reviewed clips.
- One pixmap allocation, decode, scale, slice, palette conversion, or layout
  pass per tick.
- Random idle fidgets, hover/click reactions, repeated activation greetings,
  or animation while hidden/inactive.
- A persisted `mascot_enabled` key outside the typed appearance store or an
  untyped global settings map.
- A screenshot, symbol scan, fixture-only widget, or generated text report as
  the sole proof that Dif moves or that reduced motion is static in the real
  application.

## Acceptance criteria

1. The core defines and directly tests the closed `system/full/reduced` motion
   preference and resolution rules without importing Qt, platform, clock,
   filesystem, thread, or mascot frame types.
2. The existing appearance host/store accepts, persists, migrates, restores,
   resets, rejects stale completions, and labels fallback for motion mode
   through its typed public path; old valid data defaults to `system` and future
   or malformed data fails closed.
3. The selected Qt/platform adapter reports a bounded reduced-motion
   observation on each supported platform, a target-native before/change/after
   course proves the real observer and restores the original setting, change
   delivery is serialized, and unsupported or failed observation makes
   `system` static rather than silently animated.
4. The Qt catalog validates and decodes the exact RFC-BPR-0019 digest-bound
   resources once, stays within the declared limits, and fails to a text-only
   landing when any resource or descriptor is wrong.
5. Deterministic sequencer tests prove `greet -> calm_idle`, a closed calm
   looping, elapsed-time frame selection, delayed-tick coalescing, one greeting
   per landing generation, and static reduced/suspended behavior.
6. Widget lifecycle tests prove no timer wake or repaint after hide,
   deactivation, review replacement, suspension, close, or destruction, and no
   callback accesses released presentation state.
7. Qt rendering at 1.0x, 1.25x, and 2.0x across System, Light, Dark, and both
   high-contrast variants preserves reviewed Dif bounds, native surrounding
   presentation, text/control priority, focus order, accessibility, and the
   compact-width disappearance rule.
8. The real `app_better_pull_review` public course selects `Full` and `Reduced`
   through the real appearance gesture and accepted host/store projection,
   counts exactly one mascot/catalog and at most one armed mascot timer,
   captures two time-separated mascot-region rasters that differ during full
   motion, captures a reduced-motion interval whose region remains identical,
   observes the greet-to-idle transition, and proves zero mascot presence/timer
   work after a pull request opens.
9. Opening and reviewing a pull request with Dif compiled in leaves selected
   diff identity, navigation, drawers, focus, palette, syntax, model-reset
   counts, network behavior, and orderly shutdown unchanged.
10. `core-only` contains no Dif resource or Qt/platform motion implementation;
    tests-disabled Release contains the same reviewed resource digests and no
    test-only clip, timer, provider, or filesystem seam.
11. Documentation and agent instructions describe ownership, preference and
    migration, platform support, resource selection, lifecycle, accessibility,
    performance bounds, public placement, failures, and exact verification
    without claiming semantic review behavior or package readiness.

## Verification matrix

| Claim | Direct surface | Command or interaction | What proves the claim |
|---|---|---|---|
| Motion policy is deterministic and Qt-free | Appearance core public transition/resolution API and `core-only` graph | Table-test every preference/observation combination; scan core dependency boundary | Wrong defaults/resolution, platform/Qt types, hidden globals, or mascot frames in core fail |
| Motion persists through the existing authority | Appearance host/store gesture, restart, migration, stale completion, and failed-store courses | Change mode through the real typed control, restart, inject old/future/malformed records, reorder completions | A parallel key, optimistic control, lost value, stale overwrite, or false durability label fails |
| Platform reduction is selected honestly | Target-native production-selected Qt/platform observer | Through an authorized native course, record the original setting, change it through the documented OS surface, inspect before/change/after observations, and restore it; use injection only in separate hermetic tests | A fixture constant, injected selected-surface result, unsupported claim, shell-out, missed change/restoration, or full motion under observed reduction fails |
| Catalog is exact and bounded | Tests-enabled and tests-disabled selected Qt resource graph | Corrupt each catalog/resource fact in component fixtures; inspect Release resource digests and decode counters | Substitute/missing art, excess resources, repeated decode, directory scan, or mismatched digest fails |
| Sequencing and lifecycle are correct | Deterministic sequencer and live Qt widget | Advance explicit elapsed time, delay ticks, churn exposure/activation/scale, replace landing, close and destroy | Wrong frames, burst, repeated greet, post-hide wake, post-destruction access, or leaked cache fails |
| Dif is accessible and nonintrusive | Real `workbench_window` at supported sizes/themes/scales | Traverse keyboard/accessibility tree, inspect layout and status equivalence with full/reduced/no mascot | New focus stop, frame announcements, hidden text/control, status conveyed only by pose, or active-review space fails |
| Dif moves in the real product | `app_better_pull_review` production-shaped public fixture | Select `Full` through the real appearance control, count widget/catalog/timer instances, capture timestamped full-window and mascot crops before/after a declared frame boundary, then open a PR | A direct setter/flag/env bypass, duplicate hidden instance, unchanged full-motion crop, changes outside mascot rect, helper-only construction, or mascot remaining over review fails |
| Reduced motion is truly static | Same real executable and accepted appearance preference path | Select `Reduced` through the real appearance control, hold over multiple nominal frame boundaries, and compare exact mascot-region pixels plus instance/timer counters | Any direct override, duplicate worker, pixel/procedural motion, fade, wakeup, or fixture-only bypass fails |
| Existing review behavior is unchanged | Composed selected-diff and workbench interaction course | Open/select/navigate/retheme/close with resource and timer instrumentation | Diff identity/focus/model/network/shutdown changes or non-mascot repaint/layout work fails |
| Build/package boundary stays honest | `core-only`, developer, tests-disabled Release, resource/dependency/SBOM scans | Clean configure/build/test selected graphs and inspect resources/symbols/dependencies | Core leakage, test seam in Release, runtime loader, unrecorded notice, or package-readiness overclaim fails |

Every row is seeded into the RFC closeout card before implementation and gains
the exact command, expected failing condition, evidence, result, and proving
commit as slices land. Static render fixtures prove layout only. Motion and
lifecycle require time-separated or instrumented runtime evidence.

## Attended verification

After unattended component, public-path, scale, appearance, accessibility,
performance, lifecycle, and build evidence passes, one digest-bound attended
batch presents:

- the actual workbench landing at supported sizes in System, Light, Dark, and
  high contrast;
- `greet`, the handoff to `calm_idle`, steady idle, and the static `rest` state
  at shipping speed;
- full versus reduced motion in the same composition;
- compact-width disappearance and transition into an active selected diff;
- focus and text/control prominence before and after Dif is present.

The verdict decides whether motion is calm, non-distracting, visually coherent,
and subordinate to review controls. It does not substitute for timer,
selection, accessibility, or public-composition automation. Changed asset,
timing, placement, palette behavior, or layout bytes reopen the bound subjects.

## Eligibility gates

Implementation planning may begin only when:

- RFC-BPR-0019 is Auto-Verified with approved asset and provenance evidence;
- RFC-BPR-0014 is closed or the product owner records an explicit post-pack
  implementation-frontier exception;
- the implementation run inventories the live appearance schema, host/store,
  platform support, workbench landing surface, resource graph, and selected
  public fixture rather than assuming the paths in this RFC remain current;
- the issue plan isolates any unavailable target-native motion observation as
  an explicit card rather than weakening the cross-platform claim; and
- no concurrent card owns the same appearance schema, `workbench_window`, Qt
  resource, or application-composition paths.

## Suggested implementation decomposition

1. Plan the RFC from the live tree, record the immutable base, allocate cards,
   settle target-native platform observation, and seed the claim matrix and
   documentation ledger.
2. Add the Qt-free motion preference/resolution contract and focused core
   tests.
3. Migrate the existing appearance store and compose typed host/control
   persistence, restart, fallback, and stale-result evidence.
4. Implement the fixed Qt resource catalog, deterministic sequencer, bounded
   cache, and mascot widget with focused rendering/lifecycle tests.
5. Compose one Dif into the live `workbench_window` landing path and prove
   landing/review replacement without review-state regression.
6. Add public executable full/reduced-motion capture, scale/theme/high-contrast,
   accessibility/focus, performance, shutdown, Release/core-only, and package
   boundary evidence.
7. Complete the digest-bound attended review, product/architecture/developer/
   agent documentation, and the documentation-impact gate.
8. Run a behavior-preserving simplification pass and a capability-critical
   independent review because the RFC changes durable schema, platform
   observation, timer lifetime, and public composition.

## Documentation impact

Required updates cover the appearance preference and migration, platform
motion observation and unsupported behavior, Qt resource/catalog ownership,
sequencer and widget lifecycle, workbench landing composition, accessibility,
appearance/high-DPI behavior, resource and timer limits, failure fallback,
tests-disabled/core-only selection, package notice/SBOM implications, runtime
probe commands, and target-native evidence boundaries.

The documentation ledger must explicitly record no-impact or updates for
GitHub/GHES authority, Git/cache/SQLite review data, review mutation safety,
diff identity/navigation, queue/notifications, agents, credentials, network
isolation, and remote side effects. Dif must not change any of those surfaces.
