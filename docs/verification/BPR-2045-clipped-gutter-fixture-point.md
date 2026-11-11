# BPR-2045: clipped gutter fixture point

## Exact scope and cause

Source baseline: `899056e7de7187edc0ee206a7dc499a1f5487af4`, frozen54.
Owned test source: `5a9a503f74bb72d53b0ff3a94acc252a84dc1dc7`.
TU SHA-256: `45c9781177cdb5e5e8647fc6ee75f9d9ae4901ede9d312c33ec3ec1aa66b235e`.
Only `tests/adapters_qt/diff_workbench_tests.cpp` changes as source.

The original full Windows Release100/200courses fail the visible native-row
pick. A one-shot failure-only original-flow diagnostic reproduces both: row1
has visible logical interval `[18,19)`, while the fixture clicks19. The native
cursor, production hit and selected source line all resolve row2, with scroll
unchanged1→1. At2x, the visible device interval is `[36,38)` and the click is38.
The diagnostic queries execute only after the original click and cached false
predicate; they cannot prime the successful click being assessed.

The correction derives the half-open visible interval from the existing native
bounds, chooses `min(visibleTop+1,visibleEnd-1)` and asserts the interval is
nonempty and the point inside. It retains every clipped row and all575original
named assertions, mouse gestures, exact native source-line pick,32RGB ink
threshold and one-device-pixel paint tolerance. Two pure fixture precondition
assertions are added. Frontend-design guidance was read; product geometry,
rendering and dispatch code are unchanged.

## Actual focused engineering proof

Native Windows11x64, Qt6.12.0, MSVC19.51/VS18, Release, offscreen; session3,
adminfalse, same qualified C temp and C32GiB/D128GiB reserves. A private full
frozen-source export overlays only the committed uninstrumented TU. Actual CL
and module-scan arguments equal the normal recipe after declared owned-path
rebinding. One direct link preserves all original libraries/resources and quiet
CRT object order. Compiler auxiliary cleanup is limited to exact owned
PID/start/image from the pinned compiler directory, using the existing guard.

| Original registered course | Native CTest exit | Seconds |
|---|---:|---:|
| `qt_diff_workbench_scale_100` |0|12.8275101|
| `qt_diff_workbench_scale_200` |0|19.2971174|

Each corrected full course runs once. Compile0/32.7796s and link0/11.0475s.
The private CTest graph preserves all runtime properties, scale, sandbox,
environment modifications, working directory, predicates and120s watchdog.
Only the private EXE and strictly output-only PNG destination namespace differ.
No golden/reference input is redirected. All1742 original producer inputs,
including original PNGs, normal source/cache/objects/images and toolchain,
remain exact; the candidate is unchanged and all owned forests empty.

External evidence root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-windows-diff-workbench-triage-r1`.
`diagnostic-handoff-r4.json` retains the old negative and operator-only
pre-link corrections. `point-positive-r1` binds the new source, strict read-only
normalization gate, actual native receipt/raw logs/XML and hashes. No raw logs,
images, build outputs or authenticated URLs are committed.

## Remaining scope

The original54full suite remains failed. These are focused engineering
projections, not native release qualification. The separate125paint assertion
predicts one fringe pixel at the clip edge and detects none; its cause remains
unresolved and is not accepted by this issue. No tolerance increase, skipped
row, Qt attribution or release waiver is supplied. Fresh common-source native
qualification remains the release coordinator's obligation.

Canonical source integration: `4baf8e0d86c997b412b39fb10b90e7e4eeee9802`,
byte-identical to the owned source above. Native MSVC `/W4 /WX` compilation,
LLVM22format, original-assertion/gesture preservation audit and documentation
checks pass. Generic checkpoint discovery retains a known unrelated legacy
`amendments` error; explicit current-plan binding resolves all3criteria/exits.
No tooling changes are included. Source/proof closeout is acceptance-driven;
separate125investigation retains its own coordinator-approved PC lease.
