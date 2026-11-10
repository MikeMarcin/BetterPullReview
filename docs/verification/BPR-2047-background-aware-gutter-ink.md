# BPR-2047 background-aware gutter ink

Mandate: release54-background-aware-ink-r1, RFC-BPR-0038. Frozen producer
899056e7de7187edc0ee206a7dc499a1f5487af4 remains unchanged. The only code
change is the existing Qt fixture TU; BPR-2045's clipped-point correction stays.

## Failure and cause

The original Windows54 125% course failed the painted-number geometry check.
One failure-only diagnostic retained its already captured reference/current
frames and reproduced the unchanged predicate. The visible fringe is present
at the same device pixel, but its background changed with selection. PenRGB
91/101/112; reference118/130/142 on208/224/242 has maxRGBdistance30, while
current126/134/143 on240/242/245 has distance35. The fixed32classifier drops
the current fringe. Adjacent row spans translate exactly10devicepixels and
native/production row/baseline measurements agree. This is a fixture color
classification defect; no product or Qt defect is inferred.

## Correction and controls

Private source c4c6ebcc793d08ce178b1643888d14ad59980a69, main equivalent
3783652920a81b04e62d6a7afbbed004193d9c01, TU SHA256
`e3544c853f376b79f85271dc8f582aa50ed50471036cabf0e8bdf47fa625718b`.

Compare the pixel's distance from the pen after normalizing to the reference
background contrast. Integer products are at most65025. Sampling is one
logical pixel inside each column's left padding: production reserves two
GutterPadding widths (4logicalpixels each) and right-aligns the number. The
reference nominal row centre maps by the existing native displacement; image
bounds and the same original row are asserted, never clamped. Invalid or
low-contrast samples fail closed. On the original reference background, the
mask is exactly the original per-channel32mask. The geometry predicate remains
exactly +/-1devicepixel. All575original named assertions remain ordered, all
capture/gesture counts are unchanged, and clipped rows remain required.

Same-code controls recognize the observed RGB pair, reject background-only,
invalid/low-contrast samples, missing visible one- and multi-pixel ink, and
+/-2pixel displacement while allowing +/-1. Exhaustive scalar checks cover
all256pixel distances and every valid33..255contrast for reference-mask
identity. Observed current4550<=4768passes; blankbackground19370rejects.

## Native engineering proof

Task BPR2047-background-positive-r1 dispatched once at2026-11-10T20:32:56Z.
Actual PID5336/session3/adminfalse, InteractiveToken/Limited. Pinned native
MSVC18/14.51.36231, SDK10.0.26100.0, original Qt6.12.0 and network containment.
One private TU compile0/32.876s, one link0/11.244s. Actual compile and module
arguments normalize identically to the normal graph; original link input and
resource order is retained. Normal properties, working directory, environment,
assertions and120sCTest watchdog remain exact except the declared private EXE
and strictly output-only PNG namespace. Qualified Ctemp andC32/D128GiB reserves
remain enforced.

Combined uninstrumented original courses, each once:

- qt_diff_workbench_scale_100: nativeCTest0/12.876989s.
- qt_diff_workbench_scale_125: nativeCTest0/14.556052s.
- qt_diff_workbench_scale_200: nativeCTest0/19.312627s.

Each raw stdout contains diff-workbench-tests:pass and100%tests passed; JUnit
and normal/projected rows are retained. All1742producer inputs (source, normal
images/objects/cache/tools/PNGs) remain exact; candidate unchanged; source899
clean; owned forests empty. TaskReady/LastResult0 and idle12 empty process list
were observed. PC lease returned root; no further native operations are planned.

This is bounded engineering evidence for the two fixture corrections, not
release qualification. All original54failures remain failed and retained.
A fresh common-source all-native qualification remains root's obligation.

## Source gates and evidence

LLVM22format, native/WX compilation, original-message/gesture and numeric
static checks, explicit authored-plan binding, documentation and diff checks
cover this slice. Scoped full-TU LLVM22tidy passes0 with the established explicit
macOS SDK sysroot. Initial analysis-only include-discovery failures are retained;
no source correction or native rerun was needed.

External evidence root (no transient logs or images committed):
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1`.
BPR2047-source.patch and BPR2047-static-numeric-audit.json bind the source;
paint125-richer-r1 retains the original diagnostic frames/numeric failure;
background-positive-r1/raw retains terminal receipt, three raw courses/JUnit,
actual rows, held seals and process forests. Operator packet SHA256
`68e1aeb61870f82039da32cdf0af35116400968d150e9d122e67090f6e8fddf1`.

Canonical main retains foreign2046 and all counter/reservation provenance;
NEXT remains BPR-2048. Only exact owned commits enter root's release clone.
No product, dependency, policy, timeout or reference threshold changed.
