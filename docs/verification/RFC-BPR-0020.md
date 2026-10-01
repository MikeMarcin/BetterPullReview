# RFC-BPR-0020 closeout ledger

Run `rfc0020-closeout-20260926`, segment `closeout-r1`, mandate r1: the owner
asked to close RFC-BPR-0020. Engineering owner `/root/rfc0020_closeout` is the
sole assigned writer on local `main`; no remote mutation is authorized.

The closeout-entry source is `a028dcee40729457066129c73edd34f1d1820bdf`. RFC-0020 had no live
cards or adopted plan. BPR-1220 and the [single-milestone plan](../rfcs/plans/rfc-bpr-0020-closeout/milestone-plan.json)
retain all original acceptance, existing successor amendments and the exact
prior native/human/independent evidence. Initial worktree was clean.

## Contract and evidence reconciliation

This is the closeout of the original presentation foundation, implemented and
accepted through RFC-0030/0031. It does not restart their implementation or
claim every original sentence remains literally true. The immutable
[reconciliation table](../rfcs/assets/bpr-dif-integration-v1/supporting/RECONCILIATION.md)
and live [RFC-0031](../rfcs/RFC-BPR-0031-dif-workbench-animation-integration.md)
explicitly preserve unsuperseded requirements while changing these subjects:

- The selected catalog is RFC-0030's accepted, prop-free ten-clip revision.
  The original three-clip/13.5-MiB restriction is superseded by the 24-MiB
  decoded-pixel, 32-MiB retained and 40-MiB conversion ceilings. Current pixels
  are 23,855,104 bytes. No runtime erasure or alternate source is used.
- Preferences belong to each BPR profile; one widget belongs to each
  workbench, one cache is shared across the presentation lifetime and only
  the active eligible presentation may animate. The original global
  preference and single-window counts are superseded.
- Home can cover a retained review. Greeting is once per workbench lifetime;
  ordinary Home toggling does not re-greet. The later accepted corner
  amendment puts the same sprite at 28 pixels on ordinary surfaces, with
  prime spots on first-time Home and the accepted Compose workshop. Literal
  absence from every open-review surface is superseded, not observed.
- RFC-0031 admits bounded cues from accepted presentation facts. Their
  authority, identity and platform follow-ups remain in their own records;
  RFC-0020 closure adds no semantic or autonomous review claim.
- The native macOS observer uses AppKit only to read and subscribe to the
  reduced-motion preference. RFC-0031's accepted observer edge supplies that
  API selection; playback remains Qt-owned and uses no AppKit animation.

All RFCs 0001–0019, including the listed 0005/0006/0013/0019 dependencies,
were Auto-Verified on entry. RFC-0014's eligibility condition is satisfied.
Neither the original implementation-frontier exceptions nor a successor's
status alone supplied acceptance. The earlier RFC-0030/0031 closure remains
historical; this transaction changes no other RFC's status.

## Original criteria and direct evidence

The numbered IDs below refer to the eleven original criteria, whose exact
words remain in RFC-0020 and [BPR-1220](../issues/60-done/BPR-1220-close-rfc-0020.md).
Only their list markers were normalized for the existing documentation gate.

| Criterion | Current interpretation, evidence and limits |
|---|---|
| AC1 | The closed System/Full/Reduced policy and conservative unsupported resolution are unchanged. `core_appearance_policy` passes at `2723138b`; core/host isolation and the core-only graph are also evidenced by the [RFC-0019 graph replay](../rfcs/plans/rfc-bpr-0019-closeout/graph-verification-r3.json). No frame or Qt state is introduced into core. |
| AC2 | Profile ownership replaces the old global assumption. `application_appearance_host`, `application_appearance_profile_consumers`, `sqlite_appearance_settings_storage` and `qt_appearance_settings_panel` pass at `2723138b`, covering migration, invalid/future data, stale/detached results, fallback, reset and fan-out. The freshly rebuilt composed multi-window course passes, including the real Settings control and durable profile behavior. [Runtime foundation](RFC-BPR-0031-runtime-foundation.md) retains the original `6f07c343` gesture/restart evidence. Later appearance migrations retain these fields. |
| AC3 | macOS is the only claimed reliable native observer. Its two source files are byte-identical to accepted `62354ed6`; exact native System change and restoration decision `822d32fc-483e-4085-a5c0-139167fa2a6e` remains valid. Fresh `qt_motion_observer` and core resolution checks pass. Windows/Linux remain explicitly unsupported for System observation, so System is static and Full is opt-in; no target-native success there is invented. |
| AC4 | The selected ten-clip prop-free catalog is the authorized successor, not the original three-clip raster. The asset tree is unchanged from `b03fc9fd`, whose fresh selected-graph/binary audit passes. Fresh `qt_dif_runtime` proves production decode, invalid-resource refusal and checked budgets. [Production art](RFC-BPR-0030-production-art.md) and RFC-0019's exact v008 acceptance retain art/provenance authority. |
| AC5 | Fresh `qt_dif_runtime` passes elapsed-time, malformed/backward/delayed-time, loop and one-shot, rest and lifecycle cases. The accepted sequencer is unchanged since `62354ed6`; the only later runtime implementation delta is guarded Windows resource registration. Greeting lifetime and the fourth-loop blink follow RFC-0031. |
| AC6 | Fresh runtime tests exercise hidden, inactive, reduced, obstructed, suspension and destruction cancellation. Historical [Home integration](RFC-BPR-0031-home-integration.md) and [closeout](RFC-BPR-0031-verification-closeout.md) retain the reviewed callback/timer lifetime fixes. The corner may remain statically visible during review under the placement amendment; continuous Home animation does not move into reading. |
| AC7 | The current three-scale workbench course verifies production frames, compact-first lazy decode, 96/72-pixel Home, disappearance before controls, focus/input exclusion and the 28-pixel corner. It includes negative controls for an old 40-pixel corner and forbidden drawer placement. Exact native appearance decision `e6d068b1-6ef2-4216-a86d-c3f8fa322315` and the later BPR-1209 corner verdict are reused for their bound subjects, not described as new native runs. Current scale replay is recorded below. |
| AC8 | The actual application path uses accepted Appearance controls and the shared production catalog. Prior `62354ed6` composed Full/Reduced intervals measured eight wakes/frames/paints in 1,800 ms and no changes in 1,200 ms Reduced; its exact native motion verdict is retained. Current multi-window composition proves same-profile adoption, cache sharing and retained review return. Static corner presence and per-window widget counts replace the obsolete zero-presence/single-window literals. The fresh replay does not claim it enabled the optional historical memory report. |
| AC9 | Current composed multi-window checks open independent reviews and preserve comparison, file, epoch, scroll and focus through Home. Historical measured Full/Reduced intervals leave diff geometry, publications, pane measurements and semantic adoptions unchanged. Source inspection confirms no new provider, cache, mutation, credential or effect path in this closeout. The test-only tray repair below changes no product behavior. |
| AC10 | Reuse `b03fc9fd`'s exact tests-enabled/tests-disabled/core-only graph and binary proof: ten selected strips, no generation in ordinary builds, no Dif/Qt implementation in core-only. All art/tool/resource ownership inputs remain unchanged; later CMake additions register unrelated tour, notes and logging sources. That historical Release binary is not relabelled as a fresh build at this closeout. No installed, signed, notarized or package qualification follows. |
| AC11 | The live [appearance guide](../ux/appearance-and-accessibility.md), [system architecture](../architecture/system-architecture.md#dif-asset-ownership-boundary), [build guide](../development/build-and-test.md), asset README and package inventory already document the implemented successor. This ledger, RFC note and index now reconcile the older foundation without deleting its original requirements. The documentation impact and final gates are below. |

The ten verification-matrix rows map, in order, to AC1, AC2, AC3, AC4/10,
AC5/6, AC7, AC8, AC8, AC9 and AC10. No matrix row is dropped. Long-run
resource/timer claims reuse the measured and explicitly bounded RFC-0031
closeout; default-zone sampling is not an enforced allocator maximum and the
native static-widget observation is not an animated-workbench allocation trace.

## Required attended and independent evidence

The four [sanitized native decisions](dif-verification-closeout-v001/human-verdict.json)
are bound to `62354ed613ac52756a5e908212adc052a60e33bb`, developer executable
SHA-256 `ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9`,
session `ef04771118304d90b3b73259e0604e58`, sequences 13–16, hand-back 17
with no pending questions. They separately accept native appearance,
actual-speed Full/Reduced behavior, placement/controls and the real macOS
System setting change with restoration. The motion, catalog pixels and
macOS observer have not changed; unrelated window additions do not turn
those decisions into a verdict on the whole current application.

Placement changes received their own decisions:

- [BPR-1209](../issues/60-done/BPR-1209-dif-leaves-the-right-drawer-and-rests-in-the-status-bars-corner.md)
  records the rejected 40-pixel candidate and accepted 28-pixel correction.
  Accept `c91d25c2-3b77-4e7c-a123-b545cedb4ad1`, session
  `0a3f0caf7998162c936e25e6d3f68672`, cursor 27/hand-back 32, binds source
  `1971a9ab82b965d57925023469ec9b14513de894` and executable SHA-256
  `b45b64204b0d34fac1e9e9c977905768408536ff1e42cbd8b03021c1ee512715`.
  Current Dif placement/size code retains that subject; later window changes
  add Tour/diagnostics and semantic anchors, not new Dif pixels or geometry.
- [BPR-0966](../issues/30-in-progress/BPR-0966-reconcile-dif-workshop-cues-with-composition-page.md)
  retains the exact Mac workshop Accept `3bcafec9-7811-46dd-83dd-f7bda4cdac1b`
  at source `388f814c`. Its deferred Windows cue course remains Backlog.
  That later semantic cue requirement is not a foundational RFC-0020 gate and
  is not completed here. BPR-1212 retains the owner's separate Compose rethink.

The runtime foundation, Home integration and final RFC-0031 closeout each
record consumed independent reviews. Their migration, profile isolation,
eager-decode, activation and callback/timer findings were corrected and
rechecked. The d15 review checked the native memory report and recorded no
blocking finding. These records satisfy the original capability-critical
review for the implementation. This closeout changes documentation and one
test assertion only; it neither launches a new reviewer nor calls self-review
independent acceptance. No new attended session or review server is needed.

## Current commands and the verification repair

The entry build refreshed the real composition at source `a028dcee` with
`cmake --build --preset dev --target test_profile_multi_window_public_path -j 4`.
`ctest --test-dir build/dev -R '^application_profile_multi_window_public_path$' --output-on-failure`
passed 1/1 in 82.18 seconds. Runtime production sources remain identical
through the documentation-only claim `2723138b` and test repair `c62c2fcb`.

The app plus seven focused test targets and the workbench-window target built
successfully. The [focused command record](../rfcs/plans/rfc-bpr-0020-closeout/focused-tests.json)
binds the seven core/host/store/panel/runtime/observer passes (4.15 seconds)
to `2723138b`.

The initial [three-scale run](../rfcs/plans/rfc-bpr-0020-closeout/scale-tests.json)
failed 0/3 before reaching Dif, on the old assertion that the tray contains
exactly Comments and Notes. BPR-0994's accepted native Tour page added a
third tab. The coordinator explicitly authorized the narrow verification
repair: `c62c2fcb` now requires exactly Comments, Notes and Tour, checks
Tour's accessible name and still requires Comments to be initially visible.
The other assertions and product code are untouched. The changed test passes
`clang-format --dry-run --Werror`; no test expectation was loosened.

The [repaired scale replay](../rfcs/plans/rfc-bpr-0020-closeout/scale-tests-repaired.json)
passes 3/3 at 1.0/1.25/2.0 (71.85 seconds) on `c62c2fcb`.
The [shipped appearance gesture](../rfcs/plans/rfc-bpr-0020-closeout/appearance-public-test.json)
passes 1/1 (81.43 seconds) on the unchanged production source. Together the
focused current result is 12/12; the initial failed scale run remains recorded.
The 1.25x Read capture was inspected: the small corner art remains outside
source panes and controls. This is an offscreen fixture observation, not a
new native or human acceptance.

## Documentation impact

| Surface | Disposition |
|---|---|
| Motion preference, migration and failure | No product change. Existing profile appearance documentation and tests describe the current store/host; this ledger records the superseded global assumption. |
| Platform motion and Qt resource/lifecycle ownership | No product change. Existing architecture/appearance documentation applies; macOS native and Windows/Linux unsupported limits remain explicit. |
| Workbench layout, focus/accessibility, scales, memory and timers | No product change. README and the appearance guide now name the already accepted corner/prime placement; exact evidence is linked here. The test correction reflects the already accepted Tour tab. |
| Core-only, tests-disabled resource selection, package inventory/notices | No product change. Existing selected-graph and accepted asset evidence is reused; no new package or release claim. |
| GitHub/GHES authority, Git/cache/SQLite review data, mutation safety | No impact: no runtime source or persisted review/schema change. |
| Diff identity/navigation, queue/notifications, agents | No impact: no new policy/effect. The original foundation is not extended into semantic cue qualification. |
| Credentials, network isolation, remote side effects | No impact: local fixture checks only, no live target or credential use, no remote mutation. |
| Agent instructions | Existing root/test/docs guides already name the owning boundaries and exact evidence rules; no instruction change is needed. |

## Durable checkpoint

All eleven criteria and the ten verification rows are accepted under the
recorded successor requirements. `DOCUMENTATION_OK`, all 64 documentation
contract cases, scoped board validation, touched-test format and whitespace
checks pass. The [evidence audit](../rfcs/plans/rfc-bpr-0020-closeout/evidence.json)
records exact binary hashes, unchanged accepted source objects and command
outcomes. The only source change after the entry baseline is the five-line
tray test diff; `src/` is identical.

The [binding](../rfcs/plans/rfc-bpr-0020-closeout/binding.json) fixes the
assignment and mandate r1. Its entry version is retained when final evidence
changes the card's execution table; no original criterion or exit is removed.
Historical asset/review packets are protected and untouched. There is no
newly frozen attended candidate, active review server or pending human decision.
All task-owned builds and tests have completed; the engineering writer stops
after the final checkpoint commit. RFC-0021 is the next canonical frontier;
this assignment does not start it or close later RFCs.

Guarded finish committed the accepted closeout as `3b1981c2`. The
[final handoff](../rfcs/plans/rfc-bpr-0020-closeout/handoff.json) and
[checked observation](../rfcs/plans/rfc-bpr-0020-closeout/check.json) bind that
transaction to the tested candidate and record the stop boundary.
