---
id: BPR-0077
title: Implement shared comparison scroll controller
status: Done
rfc: RFC-BPR-0004
depends_on: [BPR-0074, BPR-0076]
---

# Implement shared comparison scroll controller

Own one retained subpixel comparison position, synchronization rail, and
shared vertical scrollbar. Route pixel/angle wheel and trackpad input over
either pane or the center divider, Page Up/Down, Home/End, scrollbar actions,
and programmatic reveal through that controller. Suppress recursive pane
feedback and keep horizontal scrolling pane-local.

The pane receiving input is not a leader. Never copy pane scrollbar values or
round-trip authority through integer scrollbar projection.

## Required direct evidence

Compare identical left/right input traces; prove right insertion and left
deletion pinning, 4:7/7:4 proportional motion, repeated forward/reverse without
drift, no handoff jump, smooth subpixel accumulation/inversion, no dead travel,
all navigation paths, feedback suppression, and horizontal isolation.

## Implementation record

- `bpr::qt::comparison_scroll_controller` owns the only retained vertical
  comparison position. Separate live `QWidget` bindings cover each text
  viewport's wheel delivery and its focused editor's key delivery. The filters
  translate pixel-preferred wheel/trackpad input, angle fallback, phased zero-
  delta events, and Page/Home/End from the left pane, right pane, or link-map
  surface without assigning a leader.
- Each accepted position projects both documents through the immutable
  measured axis, derives independent clamped pane origins around the 0.43
  synchronization rail, and publishes one guarded two-pane frame. Reentrant
  controller input is suppressed; reentrant geometry replacement/clear is
  serialized and coalesced into a coherent following frame. Frame and repaint
  callbacks are exception- and destruction-safe, while repaint requests are
  queued and coalesced.
- The shared integer `QScrollBar` is a bounded projection of retained `q`, not
  an authority round-trip. Page Up/Down, Home/End, single/page/minimum/maximum
  scrollbar actions, drag values, and stable-segment reveal all enter the same
  controller path. Discrete scrollbar actions retain exact `qreal` targets;
  only a user drag maps an integer target into comparison space.
- A same-comparison axis replacement preserves stable segment identity and
  fractional progress, then recomputes pane origins from the new font, DPI,
  viewport, or layout geometry. A different comparison begins at its legal
  start.
- Horizontal pixel and Shift-wheel input changes only the receiving pane's
  horizontal scrollbar. Qt-delivered inverted deltas are consumed as already
  inverted, avoiding a second platform-dependent sign change. Inward
  horizontal subpixel residual survives either scrollbar boundary.

## Verification record

- `cmake --build --preset dev`
- `ctest --test-dir build/dev --output-on-failure -R '^(architecture_boundaries|architecture_boundary_contract|core_diff_model|core_diff_compute|core_diff_semantics|core_diff_transition|application_diff_host|qt_diff_axis_scale_(100|125|200)|qt_comparison_scroll_controller_scale_(100|125|200))$'` — 13/13 pass
- `ctest --test-dir build/dev --output-on-failure -R '^qt_comparison_scroll_controller_scale_(100|125|200)$'` — 3/3 pass
- A dedicated `build/bpr0077-asan` configuration using
  `-fsanitize=address,undefined -fno-omit-frame-pointer`, its focused target
  build, and the same three-scale CTest filter — 3/3 pass
- `python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --paths src/adapters/qt/src/comparison_scroll_controller.cpp tests/adapters_qt/comparison_scroll_controller_tests.cpp --warnings-as-errors` — pass
- `cmake --build --preset dev --target coding-format-check` — pass
- `python3 scripts/verify_documentation.py --root .` — pass
- `git diff --check` — pass

The selected Qt composition fixture directly installs the controller on real
left, right, and link-map `QWidget` event surfaces and uses a real shared
`QScrollBar` at 1.0x, 1.25x, and 2.0x. It proves identical receiver traces,
insertion/deletion pinning, 4:7 and 7:4 motion, 1,000 forward/reverse subpixel
cycles without drift, fine-angle retention, no receiver handoff jump, phased
and inverted wheel semantics, continuous positive-axis sampling, every
navigation entry point, non-commensurate exact page/single reversals,
same-comparison remeasurement retention, serialized reentrant axis replace and
clear, callback exception/destruction safety, recursive-feedback suppression,
coalesced repaint, and pane-local horizontal isolation. A live
`QPlainTextEdit` composition additionally proves wheel input is consumed from
real text viewports while focused-widget Page/Home/End delivery enters the
same controller without changing either pane's native vertical scrollbar, and
supplies the selected scale's measured DPR. The sanitizer replay includes
synchronous controller destruction from a shared-scrollbar action callback.

This controller slice paints no product surface, so it makes no raster,
ribbon, selected-workbench, system-theme, or presentation-polish claim.
BPR-0078 owns custom painting, BPR-0079 owns the selected rendered composition,
and RFC-BPR-0005 retains the planned presentation-polish and native visual
acceptance pass.

## Documentation impact

- `docs/verification/RFC-BPR-0004.md`: records the Qt input/controller seam,
  exact direct evidence, and the deliberately open rendered-composition
  boundary.
- Product authority, GitHub/GHES, authentication, persistence, mutation,
  package, and launch behavior: `no-impact`; this is Qt-local transient
  interaction state and performs no host or remote effect.
- Target-native Windows execution remains open without blocking this
  available-platform slice.

## Simplification review

The focused code-simplifier pass removed an unused surface lookup, used typed
horizontal-bar pointers, routed late frame-sink installation through the
guarded application path, and kept event translation, retained authority,
semantic geometry anchoring, pane projection, exact action handling, serialized
axis replacement, and repaint delivery as separate readable responsibilities.
No broader public API or architecture change was justified.
