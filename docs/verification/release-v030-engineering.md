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
