# v0.3.0 release engineering

## Assignment and source

Mandate `release-v030-coordinated-r1` requests full coordinated stable 0.3.0
for macOS universal, Windows x64 and Ubuntu 26.04 x86_64, retaining agent
integrations and local automation. Entry source is clean main
`a426db419d393895020ab961509df46aba94ee46`; the change summary is grounded in
the committed delta from v0.2.2. [BPR-1623](../issues/60-done/BPR-1623-prepare-coordinated-v030-release-source.md)
owns version, notes and exact-source readiness under the
[engineering plan](../rfcs/plans/release-v030-engineering/milestone-plan.json).

The source-preparation engineering owner was `/root/release_engineering`. The coordinator
owns native producer dispatch, normal native gates, packaging, signing and
publication. No full build/test command or remote production is part of this
source-preparation card. Later gate repairs require an explicit bounded
coordinator handoff. The durable source/command checkpoint is
`.mask/sessions/release-v0.3.0/engineering-handoff.json`.

## Qualification and evidence limits

The [RFC-BPR-0038 ledger](RFC-BPR-0038.md) and blocked BPR-0958–0962 remain
authoritative. Historical M8/M9 and licensing approvals are candidate-specific;
they do not qualify 0.3.0 or another platform. Native clean-account/Intel,
Windows and Ubuntu desktop/authentication/vault, accessibility, crash/privacy,
independent final-artifact trust, licensing/source access and retention,
vulnerability and complete release-security obligations remain open. Historical
raw Grype failures are retained as failures, not current-candidate scans or
approved applicability dispositions. RFC0038 remains Proposed.

Every native gate must record actual failures and configured skips. The earlier
Debug cache-bytes skip and separate optimized Release pass keep their original
source and scope; neither establishes this candidate's result. One shared
source revision must bind final packages, SPDX, notices, separate versioned
corresponding-source assets and checksums. WSL production is distinct from clean
Ubuntu desktop/Secret Service acceptance. Profile purge excludes shared updater
and registry state and is not uninstall.

## Source preparation verification

The project version changes only from 0.2.2 to 0.3.0. The
[release notes](../releases/v0.3.0.md) summarize shipped changes, selected
platforms, stable release intent and the qualification limits above. Product
semantics, dependency pins, thresholds, test selectors and skip policy are
unchanged. No new human or independent acceptance criterion is introduced by
this bounded source-preparation slice. Final native results and package trust
remain coordinator obligations.

Source preparation passes `python3 scripts/verify_documentation.py --root .`
(`DOCUMENTATION_OK`), all 67 cases in
`python3 tests/scripts/verify_documentation_tests.py`, and `git diff --check`.
A direct version/notes check reads 0.3.0 from the CMake project and confirms
the selected platforms and qualification limitations. Self-review compared the
notes against the committed v0.2.2 delta and completed cards; the sole build
change is the project version. No native gate, new configured skip, package,
signature or publication result is claimed. Both documentation commands finished;
there are no task-owned background commands or review server.

## Bounded gate repairs

The coordinator assigned `/root/release_repairs` the release prerequisite and
gate repair milestone while normal native gates retain frozen source
`f6ba8f43e4477f60941dd24cdac4450ca68de911`.
[BPR-1624](../issues/60-done/BPR-1624-validate-insights-budget-fixture-security.md)
repairs the fixture-security gate's missing `insights-budget-v1` entry under
the [repair plan](../rfcs/plans/release-v030-repairs/milestone-plan.json).
Its manifest and ten responses are the committed `overview-labels-v1`
recording copied by `build_insights_budget_fixture.py`; the exact synthetic
identity and populated thread/review/comment shapes now traverse all existing
manifest, byte, decoded-JSON and field/artifact checks. No fixture byte,
security validator, inventory assertion or skip changes.

The focused fixture-security script reproduced the eleven-file inventory
failure before the repair, then passed all 18 tests. In a disposable copy,
adding an `environment` field to the Insights viewer response fails its
field/artifact test while the raw-byte scan still passes; a separate synthetic
forbidden-byte canary fails its per-file byte check. Both controls reject the
changed Insights response and the disposable tree is removed.

The coordinator also authorized correcting the release policy's stale direct
fuzz source references. It now names the diagnostic parser and the production
SHA-256 stream/backend plus pinned HACL* composition. Iteration count, seed,
input sizes, timeout and sanitizer requirements are unchanged. Actual sanitizer
builds/runs and matching native gate reruns remain coordinator work. These
test/documentation repairs have no product architecture or runtime behavior
impact and supply no additional RFC0038 qualification or release approval.
The documentation gate reports `DOCUMENTATION_OK`, all 67 documentation tests
pass, and `git diff --check` passes. Self-review confirms the only executable
change is the fixture table entry; existing validators and their assertions
are intact.

[BPR-1625](../issues/60-done/BPR-1625-make-mcp-terminal-audit-environment-exact.md)
owns the terminal privacy audit's macOS input correction. The exact frozen
helper (`f063cfca5159527a9fb1c8e781f5bde0cb0f537d8267b07f6ce9d53288c45205`)
reports 258 received names and zero retained/unexpected names for 257 supplied
synthetic names: its linked CoreFoundation inserts `__CF_USER_TEXT_ENCODING`
before `main`. A disposable CoreFoundation-linked probe confirms 257 becomes
258, while explicitly supplying a synthetic value for that name keeps 258
supplied equal to 258 received. The audit now supplies it and derives the exact
expected count from its constructed input. No minimum-count comparison or
product environment/diagnostic change is introduced.

All terminal audit checks pass against the unchanged frozen helper, including
content exclusion, fixed records, blocked/closed pipes, inherited flags and
protocol output. Three in-memory negative controls independently alter received,
retained and unexpected-retained totals; each produces exit 1 solely for the
complete-count check. The disposable probe is removed, and the ignored repair
handoff binds the diagnostic and control receipts. Native full-suite reruns
remain with the coordinator.

[BPR-1626](../issues/60-done/BPR-1626-supply-linux-verification-key-in-v030-notes.md)
adds the approved Linux verification key to the committed 0.3.0 notes for
delivery through the same private release channel. The fenced block is
byte-identical to the approved 404-byte public export, SHA-256
`c271732f02b5f3c709ad1a61d7f7816200e96226c490fd77d88f6583437f9798`,
fingerprint `ED85BAA84A3B6D83FCD5B1F107CEC96F2688E4A8`. A public-only
import into a disposable GPG home succeeds with auto-start disabled; listing
that home yields exactly the approved fingerprint and the home is removed.
The notes explain saving, checking, importing and verifying every detached
signature before use, including the DEB's separate verification requirement.
No package asset, manifest, publisher guard, private material or signing
configuration changes; final signatures and actual publication remain pending.

[BPR-1627](../issues/60-done/BPR-1627-isolate-agent-canary-from-product-libraries.md)
removes the agent canary fixture's inherited product-diff link. The frozen
fixture unexpectedly links xdiff and CoreFoundation; with exactly its two
synthetic profile names supplied, it reports those two plus
`__CF_USER_TEXT_ENCODING`. This is the test directory's inherited application
dependency, not an extra launcher grant. The canary source uses standard and
platform APIs only. Its target now clears inherited links before adding the
existing Windows `winmm`/`ntdll` probe libraries. Production environment policy
and the exact two-name assertion are unchanged.

A disposable native compile of the unchanged fixture source has no xdiff or
CoreFoundation loader dependency, completes its protocol and reports exactly
the two supplied names. Supplying a third synthetic name produces a set that
fails the unchanged exact comparison. The disposable binary/tree is removed.
This verifies the fixture's actual dependency requirement; the matching real
CMake target rebuild and `qt_agent_launch_canary` integration rerun remain
coordinator-owned after the frozen baseline gate finishes.

[BPR-1628](../issues/60-done/BPR-1628-repair-windows-normal-test-target-dependencies.md)
owns four failures from the captured Windows incremental build at frozen
`f6ba8f43`: CNG stub `windows.h` shadows the SDK for the injected quiet-CRT
unit; SQLite/Git Insights tests lack the platform include and `advapi32`
dependency their private scratch helper uses; the workbench test exceeds the
ordinary MSVC COFF section bound. The diagnostic build exits 1 with complete
native output retained outside its transcript. No test ran in that baseline.
The repair confines CNG stub includes to an object library, retains the real
SDK for the executable's CRT unit, adds the two tests' Windows dependencies
and enables `/bigobj` only on the oversized MSVC target. Quiet-CRT behavior,
scratch security, CNG stub logic and workbench assertions are unchanged.
At committed `9b901a0a83d1016d5e2da3795b1e7fa5249c4b85`, all seven
affected targets compile. Five CNG modes, the Git Insights contract, Help menu,
agent canary and workbench placement checks pass. The SQLite Insights course
fails contained at private-store creation but passes directly. A tiny probe
uses the actual scratch descriptor and native admission header: canonical root
succeeds, while the ancestor open with `FILE_READ_ATTRIBUTES | READ_CONTROL`
receives Win32 5 only inside the AppContainer and the retained chain reports
`invalid_permissions`. Both courses clean their owned scratch directory.
Only this row receives the established Windows-uncontained registration and
exact `ancestor-control-denied` verifier pin. The Git row remains contained;
all SQLite assertions and macOS/Linux containment remain unchanged. The
corrected native row passes (0.34 seconds); the mandatory manifest subsequently
passes on `613f33524f92e844e8996126f86c5f64e54b1103` after BPR-1631 supplies
the previously missing measurements. The original failures remain retained.

[BPR-1629](../issues/60-done/BPR-1629-include-insights-in-desktop-help-contract.md)
repairs the resident Help test's stale six-action expectation. The accepted
Insights implementation puts seven entries in both Help menus: Keyboard
Shortcuts, separator, Insights, Diagnostics, separator and the two exports.
The test now asserts that exact order, every non-separator disabled without
a window, and parity with the window's actual command labels and keys.
Its disabled-gesture course includes both exports. Product UI, callbacks and
shortcuts are unchanged; this is a test correction with no new visual or
human acceptance. The touched C++ is formatted with clang-format 22. Matching
native Windows `qt_profile_management_window` passes at committed candidate
`9b901a0a83d1016d5e2da3795b1e7fa5249c4b85` (5.43 seconds), including the
complete-order, disabled-state and actual-window parity checks. The coordinator
retains the macOS/Linux normal integration reruns.

[BPR-1630](../issues/60-done/BPR-1630-expose-linux-libc-endian-api-only-to-hacl.md)
repairs the Ubuntu 26.04 normal build's strict-C11 HACL generated-C compile.
The pinned Karamel Linux branch includes libc `endian.h`; glibc 2.43 hides its
endian helpers under strict C11 without `_DEFAULT_SOURCE`. The provider now
sets that definition privately on its generated C target only on Linux.
Archive digest, upstream bytes, strict C11, hardening and consumers are unchanged.
At `98488e2ef845541aa5f0a20cf49b878a897848ec`, the actual provider and hash
target build. Generated commands confirm `-std=c11`, both hardening flags,
`-D_DEFAULT_SOURCE` on HACL, none on 138 core consumer commands, and empty
`CMAKE_C_FLAGS`. Native isolated `core_sha256` passes (0.06 seconds).
The first focused wrapper's diagnostic filename typo and the next attempt's
unbuilt sandbox launcher are retained as failed/not-run evidence; after building
the existing launcher, the unchanged hash test passes. No full suite or signing
result is implied.

[BPR-1631](../issues/60-done/BPR-1631-measure-fourteen-windows-containment-registrations.md)
owns the fourteen earlier Windows exemptions still marked unmeasured. The
native mandatory manifest failure at `18dff0d1` is retained. The exact current
direct and contained commands must be measured before any finite cause is
recorded or an unnecessary exemption retired; no shared-helper assumption
replaces an actual row. The actual fourteen direct and fourteen contained observations are now retained.
The markdown fixture passes contained and loses its exemption; thirteen finite
pins describe the measured admission failures. Three direct fixture defects
are repaired without changing product policy: initial Windows lifetime-lock
setup before exact inventory, native file identity for reset replacement, and
private-DACL plus owned process-tree cleanup for recovery. Eleven original
direct passes, two corrected reset passes and the actual recovery pass supply
the complete direct set. At `613f33524f92e844e8996126f86c5f64e54b1103`,
recovery passes in 10.08 seconds and the mandatory native manifest passes in
1.24 seconds. All 72 verifier tests pass. All owned native commands are complete;
nine completed repair tasks, five probes/overlays and nineteen exact owned
fixture roots are removed after receipts and process-absence checks. The shared
PC slot returns to the coordinator. Focused clang-tidy22 with warnings as errors passes on the three changed
test C++ units against the clean Mac `4b73e7046` compilation database; their
verified source is unchanged. New Mac repairs and whole release integration
remain pending under the same milestone.

[BPR-1632](../issues/30-in-progress/BPR-1632-diagnose-large-review-history-input-gap.md)
owns the frozen Mac history failure. The sole violating refresh is
`epoch_refresh_4`: attributed GUI pulse interval 142.3115 ms exceeds the unchanged
100 ms limit; refresh completion 440.256 ms is below 12,000 ms. Its wall gap is
145.307833 ms, GUI processor time 1.433292 ms and measured runnable wait
2.996333 ms. The attribution therefore retains about 140.878208 ms spent away
from processor execution that was not observed as runnable wait. This is a
retained non-CPU interval, not proof of a particular lock, I/O operation,
unobserved scheduler wait or transient load. The report lacks a call stack for that interval. Read-only
source/report diagnosis continues; no timing formula, threshold or application
code changes, and native reproduction awaits baseline completion.


The initial frozen Mac baseline finishes with 800 passes, nine failures and one
configured skip. Its additional native-menu failure retains two Insights items
in the Apple menu after the window closes; agent diagnosis will use source and
offscreen or isolated evidence without operating the owner's live desktop.
The coordinator's two 100,000-run ASan/UBSan fuzz courses pass after the direct
diagnostic link also includes the production `release_maintenance.cpp` option
validator; the policy recipe now records that concrete input. These are focused
results, not final release qualification or signing approval.


[BPR-1634](../issues/60-done/BPR-1634-bind-workbench-command-loop-entries-by-reference.md)
repairs the two actual GCC15 copied-range-binding warnings. Both loops now
bind entry/id pairs by const reference while callbacks still capture id by
value. The coordinator's native Ubuntu r3 and complete r5 builds compile the
workbench with all warnings as errors. Mac focused `qt_change_end_keys` passes
(0.52 seconds), and focused clang-tidy22 with warnings as errors passes on the
workbench's exact compilation database. No command behavior or warning flag
changes. BPR1635 and BPR1636 separately retain the later native test compile
failures and their verification obligations.


[BPR-1635](../issues/60-done/BPR-1635-select-native-nanosecond-metadata-timestamp.md)
selects Apple's `st_mtimespec` and Linux's `st_mtim` without losing the
nanosecond snapshot field. The actual Ubuntu complete r5 build compiles it,
and its runtime retains 45 unchanged-checkout observations, zero writes and
a passing canary; the separate C5 question-type failure is owned by BPR1637.
The actual Mac metadata harness and scoped clang-tidy22 pass.

[BPR-1636](../issues/60-done/BPR-1636-make-final-gcc-test-helper-lifetimes-explicit.md)
confines three macOS Option-key helpers to their existing Apple caller, and
names the expected membership vector without altering complete equality or
request counts. Ubuntu's complete r5 build passes all original warnings as
errors. Mac Option typing passes in 1.14 seconds and the actual GitHub provider
course passes in 3.36 seconds; both changed test units pass focused tidy22.

[BPR-1633](../issues/30-in-progress/BPR-1633-keep-one-native-insights-application-menu-item.md)
loads the canonical frontend-design skill and preserves the approved native
Insights label, location, shortcut and profile action. One application-owned
Cocoa item selects the existing QAction belonging to the actual current menu
bar; QPointer donors prevent dispatch after destruction. Removing the donor
from its QMenu preserves its enabled state; hiding it was rejected because Qt
disables hidden actions. A background process with activation prohibited shows
no windows and reads only its own AppKit menu. Its completed-arbitration input
retains Qt's shared application item without focusing a window. Actual checks
pass for one item, distinct donor dispatch, exact once-only native shortcut,
sibling destruction, disabled resident menu and reopening. All new C++ and
Objective-C++ pass focused tidy22 with warnings as errors. The original native
foreground desktop course is not rerun and remains exact-candidate isolated or
human qualification; this focused course does not claim its activation result.

BPR1632 retains the original failed report and an unchanged unprofiled passing
rerun (149.25 seconds). One authorized diagnostic course on `adb1e7bf6` samples
only its owned synthetic child at 10 ms and passes with maximum refresh gap
67.224 ms. Those perturbed timings are diagnostic, not acceptance. Aggregated
stacks show actual GUI allocation waiting on the native malloc fork lock and
a worker in fork; they do not identify the original 142.3115 ms gap. No source,
threshold, attribution or retry policy changes. All diagnostic processes end;
the original failure's disposition remains pending with the coordinator.

[BPR-1637](../issues/60-done/BPR-1637-preserve-git-oracle-directory-questions.md)
retains the Ubuntu C5 failure, the missing-directory negative and the rejected
blanket-slash counterexamples. Git infers path type from its filesystem, while
K receives an explicit directory flag. The faithful fixture now prepares those
declared directories before queries or snapshots, leaving original bare Git
queries and all classifications unchanged. The complete Mac harness passes
in 15.16 seconds, with scoped tidy22 passing. Native Linux rerun remains pending
after its current suite; the failed serializer candidate is not accepted.


The remaining Ubuntu failures are tracked as
[BPR-1638](../issues/60-done/BPR-1638-diagnose-ubuntu-folder-ink-raster-failure.md)
and [BPR-1639](../issues/60-done/BPR-1639-diagnose-ubuntu-shortcut-column-overflow.md).
Their original raster/column predicates remain intact; failure-only diagnostics
record the missing palette/pixel or geometry/font observations. Neither a
tolerance nor a product layout change is justified or claimed yet. Native
measurements wait for the current Ubuntu suite to finish.

The Mac command slot is quiescent after focused work. An ignored exact manifest
partition identifies four existing Cocoa courses that display native desktop
items, request notification behavior or show/activate windows. They remain
required isolated or attended qualification. A selected unattended run can
exercise the other 807 of 811 rows with unchanged properties, but must report
that selected completeness separately; it is not a complete normal native
gate. No test skip, selector registration or runner completeness guard changes.


[BPR-1640](../issues/60-done/BPR-1640-diagnose-ubuntu-disclosure-negative-controls.md)
retains the later two Ubuntu disclosure scale failures. The selected Windows
style renders the supposed old native-bezel control using theme surface fills,
not its supplied grey button palette. Dark contrast and both high-contrast
negative controls fail; current disclosure assertions report no failure in the
retained output. The actual style/fixture cause and native focused verification
remain pending. No assertion, palette tolerance, product style or threshold is
changed before those observations.


[BPR-1641](../issues/60-done/BPR-1641-decide-excerpt-visibility-before-moving-input.md)
retains the Ubuntu public composition course's nested Release build failure:
GCC15 at -O3 diagnoses the optional excerpt's stored vector after move
assignment. The bounded source repair computes the same guarded visibility
predicate on the initialized input before transfer, preserving stored content,
visibility and widget update order. No compiler flag or UI rule changes.
Source format/diff and documentation checks pass; exact optimized native compile,
public composition/tour and focused tidy evidence remain pending while both
native slots are coordinator-owned.


[BPR-1642](../issues/60-done/BPR-1642-run-diagnostic-controls-in-public-path-test-environment.md)
retains the Ubuntu diagnostic-controls startup abort before any assertions.
This automated row omitted the shared public-path environment that adjacent
composition courses already use. Its registration now binds that existing
offscreen environment, including the established headless Linux style.
Isolation, backstops and all control/persistence assertions remain unchanged.
Native generated-environment and complete-course verification stay pending;
offscreen automation supplies no real-desktop qualification.


[BPR-1643](../issues/60-done/BPR-1643-diagnose-ubuntu-identifier-foreground-raster.md)
retains the later Ubuntu identifier appearance failure after all710 contrast
pairs pass in four variants. Exact pane-layout foreground checks also precede
the failed pixel assertion. Failure-only context now names its token, variant,
font, geometry and closest actual raster color without changing the original
RGB predicate or contrast requirements. Native observation and a source-backed
cause remain pending; no raster tolerance or product color changes.


At native Linux sourcef8b8f3383, the complete metadata harness passes in3.23
seconds. This supplies BPR1637's remaining native evidence for faithful explicit
directory preparation. The generated diagnostic-controls environment contains
both existing offscreen and Linux Windows-style entries, and that complete
public course passes in6.60 seconds; the mandatory manifest passes in0.42
seconds. BPR1642's original display abort is retained. Neither result supplies
real-desktop, signing or complete release qualification.

The same twelve-row observation pass retains all eight visual/locale failures
and their exact diagnostics. The complete public composition course passes its
nested tests-disabled GCC15 -O3 build and public checks; its source-bound compile
database and Ninja completion record retain all original warnings/hardening.
BPR1641's remaining scoped tidy verification stays explicit. An initial repair
wrapper used the wrong cache path without the final release component and
stopped after a successful focused build; its failed receipt is retained.

## Native repair verification checkpoint — 2026-10-19

The later native Linux focused courses retain the original eleven Ubuntu
failures rather than overwriting the baseline. Atf8b8, the metadata harness
passes3.23s and diagnostic-controls public course passes6.60s. The complete
public composition passes407.88s with the affected excerpt TU compiled by
GCC15.2 at Release -O3, unchanged warnings-as-errors and hardening. The actual
compile database, Ninja completion record and raw diagnostics are retained.

The shortcut minimum is now measured from native key labels, the existing
title floor and view/layout margins. Original240px minimum left30px beside
195px keys where73px of title was required. Linux scales100/125/200 now pass
and the optimized object compiles; Mac4e0a scales pass0.66/0.59/0.63s. The
canonical frontend-design skill applies to this bounded established layout.
No native labels, search, accessibility, preferred size or resize behavior
changes.

Native thin nine-point glyphs lack literal foreground at DPR1 despite correct
layout formats and visible geometry. The unchanged complete courses pass at
appDPR2, establishing a coverage control. The agreed assessment-only repaint
uses minimumDPR2 on the actual viewport without changing appDPR, font or
logical geometry. Original RGB-at-most3, folder counts,710contrast pairs, four
variants, per-character formats and fresh-process assertions remain. Actual
wrong folder/token paint must fail the identical reader, and restoration must
pass. Linuxc3bc complete identifier appearance passes2.79s and Macc3bc/4e0a
passes with all controls. Final foldere40 passes nativeLinux and Mac1.18s.
This supplies paint/palette evidence, not DPR1 user-legibility qualification.

The old-bezel disclosure fixture now supplies fixed Window grey only to the
old checked button that reads it on Windows style. Its host theme stays
unchanged. The rendered bezel stays fixed across themes, allowing actual
Fusion shading; every old rejection, Dark contrast and shared current-widget
reading remains. NativeLinux and Mac both scale100/200 courses pass.

Insights' two original C-locale failures used comma literals for exact1240,
3348 and1700 values. Expectations now use the same established QLocale
formatter, preserving exact counts/text anchors and every budget, latency,
privacy, persistence and later course assertion. Nativec3bc complete defaultC
and en_US courses pass1.19/1.16s. Mac4e0a public course passes1.57s and its
positive marker binds actual offscreen role1 plus Help-menu association. The
original enum-assumption failure remains. The safe native Prohibited unit
passes0.20s; original foreground qualification remains pending.

All seven changed C++ translation units pass focused clang-tidy22 with
warnings-as-errors against the actual Mac dev compilation database at4e0a.
The final folder change passes its updated-TU repeat at e40. Format22, current
architecture-boundary and documentation checks pass. Eligible cards1638–1644
are Done;1632 original history cause/disposition and1633 foreground native
qualification remain separate pending obligations. The configured Debug
cachebytes skip is retained; the separate optimized tests-on supplement passes
in102.44s at e40cd8026, with normal Release configuration and original assertions.
The task-owned optimized tree is397MiB, dev18GiB and free space20.87GiB at
completion. All Mac implementer commands are quiescent before slot handoff.

The coordinator's earlier Mac selected composite is805pass/1fail/1Debugskip,
with four explicit native foreground courses pending. Its sole failure was
the now-verified Insights fixture association. This is honest multi-source
selected evidence, not one final-source full normal native pass. Windows full
normal gate remains next on the serialized PC. No signing, credential recovery
or publication is performed by the repair implementer.

## Relocated smoke safety repair

[BPR-1645](../issues/60-done/BPR-1645-keep-relocated-release-smoke-offscreen.md)
records the coordinator stopping the old release driver before smoke because
it removed QT_QPA_PLATFORM and the tests-disabled self-test shows its window.
The helper now explicitly selects existing offscreen Qt and removes ambient
style only. Isolated HOME, exact self-test/budget arguments, accepted0/1 status,
private-profile and production bundle-identity predicates remain unchanged.
Real shell-helper tests launch only a synthetic executable and observe actual
environment/arguments; missing private marker, wrong identity and exit2 refuse.
Trust, signatures, notary and native foreground qualification remain unchanged.
Actual coordinator producer smoke on the committed source is still pending.

The first Windows fullnormal wrapper at6e610 passed configuration but stopped
before compilation: Ninja's -k0 was passed to the VisualStudio/MSBuild preset
and MSBuild refused that switch. Its logs and failed receipt are preserved;
this is a wrapper error, not a product compile/test failure. The next wrapper
uses the existing generator-supported normal build with no trailing -k0.

The actual source93897 signed Mac producer r2 passes bundle signature checks
but its forced offscreen smoke aborts134 because the package intentionally
ships Cocoa only. Raw failure and the partial source ZIP remain retained;
no notarization or completed distribution output is claimed. BPR1645's bounded
follow-up binds the offscreen dependency to the release cache's exact absolute
Qt6_DIR. It verifies universal slices, allows only packaged QtCore/QtGui and
system dependencies, and redirects the staging copy's RPATH to the packaged
frameworks. Existing selected signing seals only that assessment copy, with
the app team required under hardened library validation; prepare-only uses
ad-hoc. Source/copy digests, architecture and team are logged. The copy stays
outside the bundle/assets, the shared Qt cache stays untouched, and all existing
package/signature/identity guards remain. All20 script tests, Bash syntax,
documentation and diff checks pass with synthetic executable/tool fixtures.
Coordinator actual signed smoke and separate native Cocoa qualification remain
pending on the resulting clean source. Recovery adopts the already-running
Windows r3 normal gate at93897 without dispatching competing heavy work.

The df7e environment-path candidate's actual signed producer r3 also aborts134
before Apple: distribution startup intentionally removes external plugin paths
and restricts Qt to its own PlugIns. This refusal is retained and the approach
is superseded. The revised BPR1645 assessment copies the complete sealed app
and verifies file/link byte equality before adding the pinned offscreen plugin
only inside the clone. It reseals that clone, acknowledging changed signature
bytes, and runs it with external plugin paths cleared. The original delivered
app/executable fingerprints must remain equal before/after and its signature
must still verify. All21 script tests pass, including escaped/corrupted clone
and original-mutation refusals. Tests-disabled source schedules no native
notification course and no device probe; the offscreen platform has no native
tray surface. Neither product runtime restrictions nor delivered plugin policy
changes. Coordinator actual clone smoke remains pending and cannot qualify
exact shipped Cocoa launch, native desktop or independent trust.

## Preserved Windows gate and final repair continuation

The complete native Windows normal r3 gate at clean93897e9c2cfce8b0ad674e6d311a3ed84eb70cde
configures/builds successfully and selects all780 rows:767 pass,12 fail,
1 configured Debug cache-bytes Skip; CTest exits8 after10239.96s. The full
configure/build/test logs,780-row manifest/JUnit/timings and wrapper receipts
are preserved in the ignored complete-evidence ZIP, SHA-256
`5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`.
The completed task is removed and producer/build/test processes are absent
before focused diagnostics. No failed row is converted to a pass by this
archive or by later unchanged courses.

The12 failures group into native metrics self-child pipes, native HTTPS
OpenSSL startup, epoch maintenance root privacy,2x thread-move settling,
three workbench scales plus current-file unread badge, three Markdown scales,
and UIforETW selected-diff capture. New BPR-1646 through BPR-1652 own the seven
grounded repairs. Original direct/contained observations and temporary
synthetic diagnostic evidence remain under the ignored Windows diagnosis
root. Both temporary UIforETW privacy sources are restored exactly and the
original app rebuilt; neither diagnostic binary supplies acceptance credit.
The call stack uses only own-app RVAs and the existing linker map, with no
process memory or personal profile inspection. Focused repaired rows and
separate optimized Windows/Ubuntu cache-bytes supplements remain pending.

BPR-1645's actual coordinator signed Mac r4 assessment clone smoke passes
at clean76974f7d0f89317a28f512a8cab3c2a3e1e4c5a2. The original complete
bundle fingerprint is `4612b2eeda70246808413f250f7699fd733a9d8ffcc187f5363bd7caebf7c3df`
and executable SHA-256 is `8ef495711f4db619ffc3eed02eddb50a133b677408100b4af2a36bc696ca7e61`,
both unchanged before/after; original strict/deep signature remains valid.
The8 final Mac assets pass package/checksum audits, and original app/DMG
pass actual publisher/team/runtime/universal/stapler/Gatekeeper checks.
The private clone remains an assessment dependency, with native Cocoa launch
and four foreground courses separately pending. Root owns any later
source/platform/artifact reconciliation and security disposition; no blanket
qualification transfers to later candidates.

## Measured Windows repairs and cross-platform closeout

BPR-1646 through BPR-1652 are Done after native focused verification. The
original 780-row Windows gate remains 767 pass, 12 fail and one configured
Debug Skip at 93897; its complete evidence and each original refusal remain
retained. A later focused 16-row native course passes at 1cdaf43b3, repairing
all 12 failed rows without a full normal rerun claim. Final metrics and the
reviewed composition-seal follow-ups pass at f164d6214. Changes after that
source are tracker/ledger upkeep and require explicit producer source binding.

The native metrics fixture forwards only unused standard channels, allowing
its canonical self child to run under the existing AppContainer. CPU work,
exact retained process handle, lifetime and reap predicates remain. The
contained final course passes in 0.240959s. HTTPS is the sole additional finite
uncontained registration: the exact MSYS OpenSSL runtime refuses
NtCreateDirectoryObject under BaseNamedObjects with 0xC0000022 and exit
3221225794, while the full 26-scenario direct course passes. Its existing
90-second timeout, loopback port count one and assertions remain. The actual
registered course passes in 8.44005s and mandatory native manifest in 1.41546s;
all 72 verifier unit checks pass. No shared object ACL or product launch policy
changes. The measured tools are Git's openssl.exe SHA-256
`78340fb01dea2df5caab73dcf28833b31329226a4c9e02f717f1d399592265b9`
and msys-2.0.dll SHA-256
`2ea49553e4c03055dcf1c4a2bef54668081a07663fba283f4b34cf70f2157191`.

The epoch fixture applies its existing owner-private helper to both Git and
catalog roots. The complete Windows maintenance course passes in 8.86113s;
its two recovery consumers pass in 23.6378/36.5195s. The label-bearing tray
fixture explicitly supplies the wide host layout; compact product behavior
and all label/focus/privacy/accessibility predicates remain. Full Windows
workbench scales 100/125/200 pass in 187.012/196.604/216.219s under their
original budgets, and the unread-badge course passes in 4.89385s. Timed thread
frames buffer raw pixmaps and serialize PNG after the original animation
course; event boundaries always pump. The 1350ms animation, all capture times
and final settled predicate remain. All three thread scales pass; the six
retained 200% light frames contain five distinct hashes, with arrival/held
equal and dissolve/settled distinct. Original stalled frames remain retained.

The footnote fixture clicks inside the full selected reference span, retaining
real press/release, keyboard/back-reference and authored-link negative controls.
All three complete Markdown scale courses pass. The Windows-only tests-on
UIforETW observation now reads the caption action it already asserted at
lead+16, preserving the complete row, rendered public composition and wire-cut
control. The full native course passes in 50.0452s. Both bounded synthetic
fatal/RVA diagnostic sources are restored exactly and the original privacy
source rebuilt before acceptance; no diagnostic binary becomes a candidate.
The exact app-source review since 7697 contains only that guarded index change.
The stale composition seal first refuses, then its two full-scope pins are
renewed for the reviewed bytes while the production mutation digest remains
`16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1`.
The complete native round-trip composition contract passes in 8.44589s.

Cross-platform offscreen checks cover every changed shared fixture. Mac passes
13 selected rows plus final rebuilt metrics and both recovery consumers;
Ubuntu passes all 15 corresponding rows at f164d6214. The four shared fixture
translation units pass focused clang-tidy 22 with warnings-as-errors against
the actual Mac compilation database; its original two static-query warnings
are retained and corrected without changing their calls. All five changed C++
files pass clang-format 22. The canonical frontend-design skill guided the
bounded Qt fixture work, preserving the established product style and layout.
These offscreen checks provide no native desktop or Cocoa launch qualification.

Separate tests-on optimized cache-bytes supplements preserve the configured
normal Debug Skip: Mac passes in 102.44s at e40cd8026 and Windows Release in
262.874s at f164d6214 under its original Windows 3420-second timeout. The
first Ubuntu wrapper preserves a successful 350.403s optimized build but exits
before cache execution on its incorrect timeout assumption. The original
Linux registration is TIMEOUT900, RUN_SERIAL true and SKIP_RETURN_CODE77;
its corrected operational wrapper builds only the existing network-namespace
launcher omitted by the fresh target-only build, then runs that unrun course.
The actual first Ubuntu Release course fails in49.6288s at its unchanged
216MiB footprint-growth limit (229601280 measured bytes versus226492416).
The exact-binary bounded counter observation reproduces it in49.9305s, with
zero swap; both raw reports, full sample series and process provenance remain.
BPR-1653 owns grounded allocator/live/mapped-page diagnosis before any repair.
The coordinator permits a reviewable producer source freeze with its failed
gate explicitly retained. No passing supplement or unchanged-budget acceptance
is claimed. No unsigned Linux
distribution, credential operation or package publication is performed here.

Frozen Mac source input equality remains false for github_application.cpp.
The exact sole index replacement is under both BPR_BUILD_TESTING and _WIN32;
transparent excluded-branch source projections match for distributions and
Mac, rather than claiming compiled identity. All other required/governing
inputs, the fixture tree and all four frozen executable hashes are unchanged.
The coordinator's bounded adoption at 1cdaf43 does not automatically transfer
to the final closeout source; fresh comparison and explicit adoption are
required. Likewise the prior 7697 signed assets/security dispositions require
final source/platform/artifact reconciliation and reproduction by the release
coordinator.

BPR-1632's original 142.3115ms Mac history input-gap cause remains unproved;
later unchanged passes do not close it. BPR-1633 and the four session-bound Mac
native courses remain pending: menu/status-item Cocoa, native notification,
empty-desktop native menu bar and hide-drawers native menu key. The supported
Linux signing unlock recheck still stops at missing credential 1168 before
secret retrieval or signing. Existing M8/M9, vulnerability coverage and
corresponding-source access limitations remain explicit. The new BPR-1653 memory failure remains an agent-owned obligation. Together,
these obligations keep the overall milestone/release open despite the seven
completed Windows repair cards.

## Ubuntu memory diagnosis closeout with strict failure retained

The original Release course and unchanged-binary repetitions remain failures
under the216MiB/900s serial contract. The raw failed sample is bound separately
because the benchmark prints it before appending to its JSON resource series.
No memory limit, baseline, formula, skip, workload or allocator setting is
changed. Every temporary source is restored and original binary SHA-256
`67f292c90c8ccb089cc70893360597a11dd6ce2cdb8b5cd55a3b9435f623d777`
is rebuilt before handoff. Current source/fixture/runtime inputs remain equal
to the focused-tested f164d6214 source; final tracker/notes/plan changes require
fresh explicit source and frozen-input comparison.

The a661 fixture-move proposal preserves all32 heads/992 encoded exchanges and
rejects altered status/path controls, but fails the same limit. Matched live
heap is effectively unchanged and resident differences change sign. Its header
is intentionally reverted at c01e12c1e, with full equality/causal evidence
retained. No composition seal changes or frozen rebuild are adopted solely for
this rejected proposal.

R9 reproduces the original failure with187 own-descendant map observations.
File-backed application/Qt/system/font categories plateau while anonymous
residency grows. R10's scratch-only Heaptrack course reaches only the first
large selection before observer overhead triggers failure. Its existing trace
is analyzed without another native run: QtSvg supplies zero allocation-stack
events, xdiff's largest request is607456 bytes and none remains at global peak
or end, while preparation splitting, C++ refinement and Qt publication expose
source-grounded transient/cohort allocations. Overlapping category peaks,
aggregate traffic and equal-size/stack cohorts do not prove individual
lifetimes or a causal allocator threshold. The separate bounded independent
review establishes no new leak, metric defect or justified correction.

R12 latches/logs the two original footprint predicates, completes the original
20-selection/eviction/revisit/supersession/close/shutdown/destruction workload,
suppresses the acceptance marker and always fails. All completed non-footprint
checks hold, but eight footprint observations fail. Live heap returns from
159909840 bytes before close to2371136 after destruction with cache/jobs zero;
resident bytes remain206852096. This distinguishes released live ownership
from residency without qualifying the failed course. Glibc free arena bytes
cannot be subtracted as if they were measured resident pages.

All four later temporary controls fail unchanged numeric predicates and retain
matched-original failures: equivalent Dif conversion R11 at selection18,
preparation disabled R14 at18, C++ refinement bypass R15 at20, and direct Qt
publication R16 at18. R14 deliberately omits preparation and R15 changes
refinement semantics, so neither can qualify the original contract. R16's
paired final cache/live bytes are effectively equal, with only565248 resident
bytes difference. No permanent feature removal, staging-threshold increase or
speculative storage repair is retained. The frontend-design skill was applied
before the bounded visual-runtime controls; accepted Qt styling is unchanged.

The unmodified historical e257b236a source is reconstructed in owned scratch
with its declared pins/current native toolchain. Its original course passes
in32.396s with BudgetsEnforced false, a similar sampled live peak and lower
resident high-water. All14 workload payloads match current; source/static/font
closure and application image differ. QtSvg/xdiff are current-only ELF dependencies; other compared dynamic
dependency files match. This does not isolate a causal source owner.

R17's first category-observer wrapper refuses before launching the course on
an incorrect expected skip property. Its corrected R17-r2 preserves historical
TIMEOUT900/RUN_SERIAL true with SKIP_RETURN_CODE absent, runs the unchanged
historical binary in30.9226s and collects119 samples. At each run's actual
maximum concurrent mapping sample, current R9 totals262025216 bytes versus
historical229085184: application image29863936 versus21483520, anonymous/kernel
191332352 versus167112704. Qt/system/font pages are nearly stable. The points
occur at different phases/timings; they are not a matched attribution and no
independent maxima are summed. Both image and anonymous residency differ.
The corrected archive SHA-256 is
`7d072893165f296772ff3bd0e6f1370393f54930014e24a225ef1518c027d07e`.
Both exact R17 tasks are retired after preserving the refusal and completion;
native build/test/producer image inventory is empty.

The coordinator's separately allocated R18 observes copied matcher-document
ownership against begin/end timestamps around the original native resource
snapshot, with later diagnostic draining/TID/maps observations timestamped
separately. The one unchanged-predicate Release course fails in 49.3444s at
selection 18: 227815424 footprint-growth bytes against 226492416, excess 1323008.
There are 21 original sample brackets, 490 numeric events and 49 complete
large-copy lifetimes, with zero overflow. All tracked owners retire before
every relevant sample bracket; the nearest lead is 469.558ms and the failing
sample's lead is 655.952ms. Live retention of these copied documents at the
checkpoints is falsified using event intervals, rather than later counters.

Delayed recorded-TID checks find none present and stack-VMA RSS is at most
167936 bytes; these are not atomic sample-time native-exit/residency proofs.
Former-clone-intersecting VMAs reach 66699264 resident bytes, but stale buffer
addresses cannot separate other live allocations and allocator pages. No
production leak, metric defect or justified product/input correction is
established; phase-specific live allocation/page ownership remains missing.
The original 216MiB/900s predicates, fixture, scheduling and allocator settings
are unchanged, and no diagnostic result supplies acceptance.

The three temporary sources are guarded-restored exactly at unchanged clean
WSL c01e12c1ec959091905e7f3dafc004d933901786; six governing source hashes stay
equal and the original 67f292 benchmark SHA-256 recorded above is rebuilt
exactly. Both target builds pass, the one course fails its original predicate,
the wrapper exits 0, the owned task is retired and no native build/test jobs
remain at lease return. The evidence archive SHA-256 is
`cb9de9df03b9c01a825e572392c2c551cc7c3476dcdd806a8db0be570f034d07`;
receipts, full failed samples and interval analysis remain under ignored
`.mask/sessions/release-v0.3.0/memory-owner-discriminator-r18/`.
The coordinator accepts the bounded falsification, stops further experiments
and authorizes only this card/ledger continuation before refreshed source
bindings and candidate reproduction. Original criteria and all release gaps
remain intact; no owner waiver decision is supplied or inferred.

BPR-1653 remains In Progress: causal ownership, a justified correction and the
original optimized native pass remain unsatisfied. The coordinator's explicit
stopping boundary permits final source/card/notes/plan upkeep and reviewable
producer reproduction with this strict failure recorded. Further native
diagnosis needs a newly allocated lease and a named phase-matched ownership/
residency question. This boundary does not amend card criteria, certify release
readiness or close the milestone. Mac history, four native courses, missing
Linux signing credential, M8/M9/security coverage and corresponding-source
access obligations remain as recorded above; later changes require explicit
candidate and security/frozen-input reconciliation.


## Windows final package audit continuation

The signed Windows b914 producer completes in the approved interactive context
at 2026-10-19T13:47:22Z. Its seven final files transfer unchanged to the Mac;
MSI SHA-256 is
`f5c36f76b1bb7109339a73b2fc30baa16f92fd20bf60ebc603216b67bb64c932`,
SPDX `0b257c0ae72ebae5165d96888955cafe91cd2d8f1bdac4a925e7ece690c3cde9`.
Native/offline manifest preflight, fresh administrative extraction, exact
29-file MSI/runtime-ZIP equivalence and all five first-party/MSI publisher,
chain and timestamp checks pass. Administrative extraction is not product
installation. Complete compiler/configuration, raw imports/exports for 21 PEs,
four exact producer images/full maps, corresponding source and native receipts
remain in ignored evidence. No source, signing configuration or installed app
is changed by these checks.

The actual native package/security audit fails with
pe-map-symbol-address-mismatch. Only the MCP helper has one rejected static
SDK import-name marker at RVA zero; all producer bytes and map timestamp/base/
entry identities match. The full failed attempt is retained. BPR-1654 owns
only a subsequently authorized verifier correction and its negative controls.
The source freeze is explicitly invalidated; no b914 artifact is relabelled.

The initial positive-import premise is falsified: this helper has no bcrypt
import or ASCII/wide DLL-name bytes. Independent byte parsing finds a static,
value-zero/type-zero .idata$6 symbol in the SDK specimen, naming 12 DLL-name
bytes. RC SDK path correlation also does not establish LINK library selection.
The coordinator openly amends the scope to a pinned independently reviewed
metadata specimen, with no historical selected-library claim. Admission still
requires the exact static/no-flags/same-owner/null-descriptor/image-base/
non-executable/exact-wrap shape and actual import/name absence; every name and
all ordinary identity/address/fixture/composition assertions remain. Missing
or wrong specimen bytes refuse. Independent final code/evidence review and the
complete native audit remain pending at this implementation checkpoint.


The BPR-1654 verifier at aaad passes 17 security, 7 package and 8 dependency
unit cases; independent source review finds no blocking changed-surface issue.
Actual native R2 retains the no-specimen refusal, then advances beyond MCP
metadata and fails undeclared-pe-producer. Separate R3 passes the actual MCP
positive plus 23 hostile controls, retains all symbol/anchor scans and original
image/map bytes, and separately reproduces upstream coverage refusal for
Qt6Svg.dll and qwebp.dll. Both exact module SPDX documents exist in the native
producer; the interactive configured cache resolves to its existing canonical
C:/code/git/BetterPullReview provider. No library-selection claim is inferred.

BPR-1655 corrects shared deployed-module SPDX selection and the Windows Svg
basename. Its generated provenance, notices and final inventory now use the
same required module closure; source offers and the separate corresponding
source asset continue to bind QtBase, QtSvg and QtImageFormats. Portable controls
cover all three native naming families, required missing documents, unknown/
ambiguous/changed/escaped producers and old omitted installed provenance.
An existing release-prefix lexical fixture selected the assessment helper's
first PREPARE_ONLY branch; it now binds the unique main ASSET_BASE branch,
retaining all nonpublishing assertions. No release.sh or runtime bytes change.

The packaging-source change invalidates old b914 audit build bindings. Preserve
its actual package-build-source-drift refusal and never patch old metadata to
make it pass. BPR-1654/1655 complete-public-audit criteria remain pending fresh
coordinator-owned common-source Mac/Windows production and exact native audits.
Card closeout and publication/download observations remain external after the
freeze until postrelease administrative upkeep. All earlier strict memory,
history, native/vault/crash, security-coverage and source-access gaps remain.


Native R4 at 9579 passes exact producer/map/export/payload/dependency/hardening
checks for all 21 PEs and complete separate source-asset/installed-reference
verification. The corrected Qt closure has 11 files. Both full public audit
commands preserve the expected old-source drift refusal; old installed
provenance preserves its omitted-Svg refusal. No old artifact changes.
Independent review then proves a case-only Windows Svg rename could still
repeat that omission. Windows provenance identity now folds filename case,
with uppercase Svg/WebP/core names exercised through actual platform-specific
notices/provenance/SPDX generation. Darwin/Linux identity remains exact.
The evidence for9579 is retained with that counterexample; final acceptance
binds the corrected common-source candidate and fresh producer artifacts.


Native R5 at 1a71 passes the uppercase actual 29-file private-copy course: its
11-file Qt closure equals the canonical closure, QtSvg/WebP exact producer
checks pass and changed-copy bytes refuse. The temporary copy is removed and
all original runtime/asset hashes remain unchanged. Native package-runtime 23
pass/2 platform skips (Linux baseline and POSIX ldd fixture), and security 18
pass. All four verifier tasks are retired
at 2026-10-19T14:33:58Z; no owned producer/build/test process remains. Producer
and source leases return to the coordinator after final preproduction upkeep.
Full native package/security acceptance still requires reproduced common-source
artifacts. Keep those receipts outside source after freeze; terminal card
administration follows publication and postdownload verification, preserving
the publishing commit and immutable tag. No earlier gap is converted to a pass.


Independent changed-surface review completes at 1a71 with no remaining blocker:
canonical and uppercase actual inventories yield the same 11-file/65-package
closure; native controls, source/notices/relink and the unchanged-assertion
release-prefix fixture correction reconcile. Local Mac package-runtime has
24 pass/1 Linux-native skip and covers the POSIX fixture separately from
Windows's 23 pass/2 platform skips. Source review completion supplies no final
artifact audit, new SPDX vulnerability disposition or publication acceptance.


## Standalone xdiff corresponding-source repair

[BPR-1656](../issues/30-in-progress/BPR-1656-ship-a-standalone-xdiff-recipient-rebuild-recipe.md)
repairs an actual practical source-material gap discovered in both retained
DFB source ZIPs. Their declared three build inputs are byte-correct, but the
provider requires absent acquisition/hardening helpers and an application tree.
A recipient could not use it as a standalone entry point. Exact inventory
success did not establish practical rebuilding; original assets remain intact.

The source asset now additionally pins a standalone recipient CMake project,
its instructions and the unchanged canonical hardening helper. With the
already supplied Git archive and shims it builds the original seven C11 sources
as a separately replaceable shared library, with matching output/export settings.
It needs no BetterPullReview checkout, CPM, shared cache or network acquisition,
and permits edits to recipient-extracted source. Installed offers, the package
guide and provenance identify this entry point. The production provider,
compiled C/C++ inputs, dependency pins and hardening policy are unchanged.

The fresh component-only Mac assessment builds both architectures, verifies
all seven compile commands and canonical flags, matches all 30 defined arm64
exports against the retained signed DFB library, and directly links/runs the
public C API with exact diff output. An edited-source marker executes after
rebuild/relink with original exports/API preserved. Corrupt-archive,
missing-helper/header/source and wrong-result controls refuse; disposable roots
are removed and delivered bytes are unchanged. Receipts retain actual input
hashes and distinguish an assessment from a produced release asset.
Focused package-runtime tests pass 24 with one native-Linux configured skip on
Mac; source-asset tests pass 21. Canonical criterion binding, documentation and
independent changed-surface review accompany the final handoff. No full runtime
matrix is repeated for this build-material-only correction.

The native Windows course at clean `1d62aadb33bf5e3e094fc75c2098afcc143713cd`
subsequently passes in 8.082 seconds: the exact seven-source build, all 33 defined
DLL exports, actual import-library consumer link/public-API result and modified
source marker/API/refusal controls. Own task/source cleanup and PC lease return
are explicit. This establishes that bounded Windows recipient rebuild, while
keeping DFB comparison artifacts distinct from the new assessment.

The Ubuntu 26.04 x86_64 WSL2 course also passes (91.923 seconds), matching
all 30 ELF exports of the unchanged retained Release library and passing the
original/modified API and refusal controls. That library is an unpackaged
baseline, not a final release. Linux Git reports the mounted Windows source
dirty; that raw mismatch remains. All nine actual assessment input hashes on
each platform equal committed 1d62 bytes, while Windows native Git remains
clean. The existing WSL application source/build/benchmark is untouched, and
the own task retires before PC/WSL lease return.

The regenerated final common-source assets,
installed notices/provenance/SPDX, native public audits and fresh exact security
bindings remain coordinator-owned acceptance. The issue stays In Progress at
source freeze. Later operational receipts remain outside source; administrative
closeout does not move the publishing candidate. Existing strict test failures,
unavailable native clean-account contexts and all other release obligations
are preserved.
