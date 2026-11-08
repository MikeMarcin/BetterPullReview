---
schema: "implement-rfc.card.v1"
id: "BPR-1946"
title: "Re-arm the live agent clock at actual elapsed second boundaries"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"bug","found_by":"rejected candidate42 native Windows precise-timer failure","related_cards":["BPR-1945","BPR-1941"],"milestone_plan":{"id":"bpr-1946-elapsed-clock-boundaries","path":"docs/rfcs/plans/bpr-1946-elapsed-clock-boundaries/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1283e1fba1836f38d45261ab8145e93e5e0dc646","evidence":{"kind":"observation","detail":"One existing single-shot precise timer; same actual elapsed sample drives unchanged floor formatting and delay1..1000. Same-clock refresh, settle/lifetime, formatter and original composed deadlines mechanically reviewed unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"1283e1fba1836f38d45261ab8145e93e5e0dc646","evidence":{"kind":"command","detail":"Original rejected42+10/32 contained precise negative retained; same-context corrected32/32 positive and uninstrumented originalQt4 Mac+Windows PASS, extended8phase through two boundaries/activity refresh; no deadline/predicate/isolation waiver.","path":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate42/implementer-windows-diagnosis/verification1946-results.json"}},{"criterion":"AC3","outcome":"pass","revision":"1283e1fba1836f38d45261ab8145e93e5e0dc646","evidence":{"kind":"observation","detail":"Verified rejected42 source/binaries/flags archive before ordinary Windows development. Full native/src/tests trees match main1283; normalpreflight/unchangedBPR+SDK+ClCompile settings, format22/tidy22/docs/self-review pass. Exact source intentionally pushed latestmain, nativeidle/sourceclean and scoped handback; full release obligations pending."}}]
---

# BPR-1946: Actual elapsed clock boundaries

## Acceptance

- AC1. The shared run line uses one owned single-shot precise timer and re-arms
  to the next actual elapsed whole-second boundary (1–1000 ms). Preserve floor
  formatting, nominal one-second visible cadence, status/start/settle/lifetime
  and same-clock activity-refresh continuity. No elapsed rounding or appearance
  change. Remove the redundant present-time timer start.
- AC2. Retain candidate42 and the exact-source contained Windows negative.
  Extend the existing eight-phase semantic regression through a second whole
  second and same-clock activity refresh. Focused local/native chat and Agent
  controls pass with original first1300ms Release and per-boundary allowances;
  no assertion/budget waiver or broad matrix.
- AC3. Focused format22/tidy22/docs/board and latest authoritative main landing
  pass. Before native development, verify the rejected42 source/cache/VSproject/
  flags and original app/chat binary snapshot; preserve original negatives.
  Hand back exact source/proof/runtime and quiescent ownership. Full frozen
  native qualification, trust and publication remain coordinator work.

## Native counterexample and mandate

Candidate42's actual Windows Release graph builds, then31/33 focused courses
pass. qt_tour_side_chat fails its new first-second phase assertion in1.38s,
still0:00 despite the BPR-1945 precision choice. The frozen source and original
binaries remain immutable until archived development handoff. This follow-up
does not rewrite BPR-1945's actual macOS acceptance or its negative evidence.

The one external diagnostic copies exact owning source/header, adds only
bounded primitive timestamp output after the original elapsed expression,
and uses MSVC /utf-8 and the same configured zero-capability AppContainer,
ENVIRONMENT and ENVIRONMENT_MODIFICATION. Actual runtime Qt6.12.0 is loaded
from the expected hash-sealed prefix.10/32 lines still say0:00 at1300ms;
recorded internal precise callbacks occur at991.7–999.9ms. Actual elapsed
floor formatting is correct. Waiting another whole timer period loses the
visible first second. A documented precision choice alone is insufficient
on this observed native runtime.

The coordinator approves re-arming to the next actual boundary, with the
original elapsed truth/deadlines. No UI design or policy change. The loaded
frontend-design desktop guidance remains applicable; existing accepted
1941/1944 appearance remains unchanged and needs no new visual decision.
The explicit Windows development handoff requires a verified rejected42
archive before any checkout/build changes. Linux and frozen Mac42 are outside
this slice; no signing, packaging, release allocation or publication is authorized.

A separate original private_review_native failure occurs before any agent
invocation, after its5s modality step latches failure with2requests outstanding.
One unchanged normal registered control using the already approved short C
TEMP/TMP passes10.96s, retaining all5s/90s/400s predicates and budgets. All
source/cache/binary/Qt DLL hashes and configured node properties are unchanged.
No private-review source repair is indicated. The coordinator aligns its next
early-check operator with the existing final qualifier runtime; this focused
control is not full-release acceptance. Raw original failures and both actual
controls are retained externally under candidate42/implementer-windows-diagnosis.
An initial operator UTF8 write refusal happened before either control ran;
its receipt remains distinct and is not counted as a native course run.


## Correction, self-review and exact proof

One existing owned timer is now single-shot. The same nonnegative monotonic
elapsed sample supplies floor formatting and re-arm delay1000-(elapsed%1000),
so the delay is1–1000ms. A callback arriving before the whole second rechecks
its remaining fraction, and a later callback counts actual elapsed time. The
redundant present-time start is removed; present still updates immediately.
Same-clock activity changes retain the boundary, settle still stops and clears
status, and QObject ownership/lifetime remains unchanged. All four formatting
functions, settle and the original composed deadline/stage/redraw block are
byte-identical. No registration, environment, isolation or visible design
changes. The extended eight-phase regression observes actual whole seconds1
and2 with a writing refresh in between; original Release first1300ms and the
same300ms allowance at the next boundary remain enforced (Debug retains its
existing larger semantic window, without Debug performance credit).

Before touching Windows development, the exact rejected42 source is archived:
415306442bytes,12774entries, verified PAXcommitfdc4add1df8b15a82cb0298659e7af0606401b8a,
SHA256410a09b616d71247428fc897dbae5af87f9b2f67d0bde792e74e82a58922ccd1.
Original app/chat executables, Qt static library, caches, VSprojects/actual
ClCompile flags, command tlogs and negative logs are copied and hash-verified.
The archive and original source/binary seals are checked before the approved
private development switch. No frozen Mac42 source/package or Linux9c checkout
was touched. The ordinary Windows graph keeps empty cohort/producer, and all
selected BPR/cache/SDK keys and owning Release ClCompile settings are identical
across normal configure. No guard or compiler warning is disabled.

Local macOS arm64 Qt6.12.0 Release: affected target buildPASS, four originals
PASS33.88s (sidechat4.59s, Agentwindow27.53s, session0.66s, composedchat1.07s).
Format22, warnings-as-errors tidy22 on both changed TUs, docs and self-review
pass. The ordinary cache has CLI OFF; no shipped CLI qualification is claimed.

Windows private development source3fb50358ed848eb1cd2177b4ee3d20943fe744b2
builds app/Qt/fourtargets PASS69.33s after unchanged-flags configure and the
normal three source preflight rows. Exact same-source contained diagnostic
harness with the original Dtemp context now passes32/32 first-second probes;
its raw early callback measurements remain, showing re-arm rather than elapsed
rounding. The diagnostic binary is explicitly marked and is not original-course
acceptance. The uninstrumented original registered courses then pass:
qt_tour_side_chat4.28s, qt_workbench_window_scale_10035.84s,
qt_tour_agent_session_view0.40s, qt_tour_chat_in_tab0.95s. Their exact configured
properties, environment modifications, predicates and timeouts are unchanged;
the runtime temp is the already approved Croot as in the final qualifier.
No extra private-review or Linux course ran for this source correction.

The complete src treec11d28241af386c71f14842bb5b6630d835cbc60 and tests tree
190764a8d39e91fb582981df52030b7a33f8a0a2 are IDENTICAL between native3fb and
main source1283e1fba1836f38d45261ab8145e93e5e0dc646. Main1283 is intentionally
integrated/pushed, including latest accepted mainac75; no other new functional
main delta was observed. Source hashes, original/final binaries, actual XML,
flags, runtime and negative/probe provenance are external under candidate42/
implementer-windows-diagnosis/verification1946-results.json. All original42
failures and prelaunch operator refusal are retained. The task is Ready0,
maintained nativeBuildProcesses is empty/idletrue, and the development source is
clean. Final closeout changes this card only. Native ownership returns to the
coordinator for the next exact frozen candidate; no full release, native trust,
security or publication acceptance is inferred from this focused engineering.
