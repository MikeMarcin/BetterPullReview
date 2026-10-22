---
schema: "implement-rfc.card.v1"
id: "BPR-1694"
title: "Prepare and qualify the coordinated v0.3.2 release"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"release-v032-owner-request-20261021","baseline":"111235987772e808f4fae26383925759985b3041","execution_strategy":"direct","milestone_plan":{"id":"release-v032","path":"docs/rfcs/plans/release-v032/milestone-plan.json"},"external_state":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032"}
---

# Prepare and qualify v0.3.2

The owner requested `$publish-release v0.3.2 if there are fixes to be made make us an implementer`.
This card owns source preparation, reproduced blocker repairs and exact-candidate
qualification for the existing full macOS universal, Windows x64 and Ubuntu
26.04 WSL producer route. Broad changes since v0.3.1 require comprehensive
native Release and Debug. Release preparation supplies no inferred legal,
security, native installation or trust acceptance.

## Acceptance

- AC1: Prepare grounded user-facing notes and version 0.3.2; commit one clean common candidate and preserve unrelated work.
- AC2: Reconcile native jobs before dispatch; qualify the exact committed candidate with comprehensive native Release and Debug, retaining failures and configured skips. Repair reproduced blockers in explicit committed slices without weakening gates.
- AC3: Prepare and audit complete binary, corresponding-source, SPDX, checksum, provenance and native-signature sets through the existing release tools and approved producer configuration. Keep required human/security/legal decisions explicit and use short attended review or wizard handoffs when needed.
- AC4: Publish only a qualified, authorized exact candidate through the existing publisher; verify remote state and downloaded bytes/native trust. Preserve unfinished obligations and external continuation state if prerequisites prevent publication.

## Initial reconciliation

Clean main at the baseline above. CMake still selects 0.3.1 and no v0.3.2
notes exist. Early composition inventory and documentation checks passed in
the assessing chat; neither is native/artifact qualification. BPR-1653 is Done
under its separately approved 240 MiB RSS and 216 MiB live-heap criteria;
v0.3.1's exception is not extended. BPR-1633's foreground native-menu course,
BPR-1371's resource investigation and older BPR-1551/BPR-1404 qualification
frontiers require current evidence reconciliation, not an assumption of a
current product failure. External phase ledger preserves request timing,
producer observations and subsequent results.

## First rejected candidate and repair

Candidate `b15be685a51d01053cf48a1d550bff0da2c7baa8` fails the macOS
Debug build: `gh_cli_transport_tests.cpp` omits the new
`list_home_pull_requests` operation from its exhaustive semantic-request
helper. Warnings-as-errors stop compilation. The same exact-invocation test
also needs the operation's repository authority parent rather than a
pull-request parent. The repair supplies the two requested PR numbers and
repository parent through the existing all-operation course; no product
policy, warning, assertion or limit changes. Raw failure is retained externally.
The remote candidate rejection sentinel blocks all unstarted downstream
stages while its already-running Release build retains its own result.

Target-only milestone discovery initially encountered an invalid unrelated
legacy plan (missing boundary fields). This bounded release plan links the
card directly, preserving the complete owner mandate and all four criteria;
no historical plan is rewritten.

The repaired Debug CLI target builds and `gh_cli_transport` passes in 0.70 s.
The existing closed-invocation catalog now exercises the batched Home read
with both server profiles, exact child argv/environment/body/bounds, and the
validated repository parent. Focused clang-format 22 and clang-tidy 22
warnings-as-errors pass, as do the documentation and whitespace gates. This
fix changes only the contract fixture. Full native gates remain pending.

## Windows fixture portability blocker

The original candidate's native Windows Release build completed with exit 1
in 1004.75 s. Its sole collected compiler error is C4996 at
`commit_filter_view_tests.cpp:355`: `std::getenv` is deprecated under MSVC's
warnings-as-errors policy, which the test instructions explicitly prohibit.
No Windows suite ran. The macOS producer for `ef5939561` was then stopped by
its exact owned process IDs during Release compilation, before tests, signing
or packaging. Its interruption and child identities remain in external evidence;
neither candidate is qualified.

The fixture now uses Qt's environment and path APIs for the optional generated
row render directory, preserving unset/set handling and Unicode paths. No
product code, warning policy, test assertion or budget changes. Both Mac
100%/200% commit-filter view courses pass and write their requested PNGs into
a temporary Unicode directory; the directory is removed after observation.

## Native Home fixture lifetime failure

Candidate `6bc681a005b1b894242a0d303205597f7a965266` builds successfully
in Windows Release, then its eight affected public courses return seven
passes and one failure: Home's open-feedback course loses its expected queue
selection when returning from Recently viewed. Unchanged native observations
also crash. The simulated #4001 network error printed by the course is expected.
A passing instrumented run and a passing debugger run are retained as diagnostic
observations, not accepted retries.

A process-local exception trace preserves the failing original gesture sequence
and captures access violation `c0000005` in `QSortFilterProxyModel::parent`,
through `QListView::rectForIndex` and `QListView::visualRect`. The fixture retains
plain proxy indexes across mouse helpers that process events and across renders.
Its later projection adoption can replace the proxy mapping. Temporary diagnostic
sources and binaries are restored after every native observation.

The repair retains only row identity across those boundaries: mouse helpers
resolve the target immediately before dispatch and release the index before
processing events; colour sampling resolves the row after grabbing the image;
selection checks reacquire current indexes. All original gestures, four themes,
selection/colour/accessibility assertions, network waits, deadlines and Home
preemption checks remain. No product code changes.

## Historical snapshot contamination

The Mac comprehensive Release attempt also fails two network-isolation source
checks because ignored historical scan snapshots under `.mask/sessions` are
counted as extra CMake registrations. After stopping only the owned producer
process tree, both snapshots are moved outside the checkout, every file's size
and SHA-256 is verified, and their original paths remain directory symlinks for
historical receipts. The checker is unchanged and does not follow directory
links. Both focused source checks pass in 1.58 s. The failing/incomplete suite
is retained; final comprehensive qualification is still required.

The repaired exact fixture passes the full native Windows Release course in
64.26 s and Mac Release in 64.24 s. Focused clang-format 22 and clang-tidy 22
warnings-as-errors pass; documentation and whitespace checks pass. The native
engineering observation is built with only the repaired fixture substituted;
its original source and binary are restored afterward. Final committed-source
comprehensive gates remain due.

## Expanded candidate and pack regeneration

The owner explicitly included the features that landed before publication. The
isolated release branch now includes RFC-BPR-0089 M3 and BPR-1693; the release
notes describe split per-commit diffs, force-push rematch offers and opening a
set-aside generated file. The original development checkout remains untouched.

Windows Git encodes the regenerated fixture packs differently from the committed
packs. The contract now imports each with Git's strict pack validator and
compares the complete typed object contents in object-ID order. Every other
fixture file remains byte-exact. All three variants, including M3's split
fixture, pass on Mac and native Windows Release. Controls reject a truncated
pack and a valid pack containing different objects. This follows the existing
recorded-pack builder's cross-Git-version contract; no fixture data or product
assertion is weakened. Expanded-source comprehensive qualification remains due.

## Measured Windows containment boundaries

Native Interactive Limited measurements on expanded source `a023d1d81` retain
both streams, exact registered argv/environment, child exit and source restoration
receipts outside source. All three commit-filter fixture builders fail contained
at Git init: `cannot lock ref 'HEAD': unable to create directory for .../.git/HEAD`.
The application fails at its private course root; a temporary, scoped probe of
that same QTemporaryDir confirms `CreateFileW(WRITE_DAC)` returns Win32 5.
The direct expanded application course passes all five scenarios in 23.26 s;
the repaired fixture-object contract passes all four tests in 7.83 s.

The two existing uncontained registrations now name these measured causes:
`git-reference-parent-denied` and `owner-private-acl-denied`. No capability,
ACL, registration, timeout or assertion changes. Temporary diagnostic source
and binaries are restored and rebuilt afterward. Final committed-source
manifest and comprehensive native qualification remain required.

## Reading restoration ordering repair

Windows exposed two ordering failures: immediate Quit could overtake posted tab
completion and reading observation, and a pending tab driver could restore an
older selection after an accepted line gesture on its exact displayed diff.
The host now exposes explicit decision-lane entry points through the unchanged
core transitions. The composed gesture completes only the pending load whose
request, generation, pair, file and displayed publication are current, before
recording the newer reading. Arbitrary-thread entry points retain posting.

The host regression requires persistence before immediate shutdown and refusal
after shutdown; substituting the posted entry points makes it fail. A composed
course queues an actual line gesture while the tab is pending: it reproduces
the durable-reading failure before the pending-load repair and passes afterward.
Exact reading, immediate Quit and durable equality assertions remain. Final
reading-restoration courses pass on Mac and Windows. Their tab-menu course
exposed a separate stale fixture assumption: M3 adds generated-file actions.
The fixture now requires one enabled, correctly labelled action with the
existing Open in New Tab identity and triggers that action. It retains all
background-tab, comparison, modality, reading and restart checks. Final common
committed-source comprehensive qualification remains pending.

## Command ownership renewal

Candidate `5ea09f27aaf5737ba203f229915399f258619320` is rejected: the
comprehensive Mac suite's command-correctness registry contract finds the old
application, restoration fixture and Python driver hashes. Its raw failure and
interrupted owned producer tree are retained. The Windows rejection sentinel
blocks unstarted downstream stages while the running early course retains its
result. No signing or publication occurred.

The early gesture phase uses another child of the existing private temporary
root and the same sequential, bounded subprocess lifetime. The lane and menu
repairs add no shared output, external process, timing budget or resource.
After reviewing those exact changes and the native passing reading/tab courses,
only this family's source/input hashes and ownership explanation are renewed.
Its commands, configuration set, environment, private output and resource locks
remain unchanged. All nine registry tests pass, including the driver, binary,
configuration, output and capture refusal controls. Other stale families retain
their conservative serial scheduling. Full native qualification remains due.

## Complete Home fixture index lifetimes

The committed-source Windows early course again fails Home's return-to-queue
selection assertion. Its full failure is retained; passing later diagnostic
attempts are not a release verdict. Review finds more ordinary proxy indexes
held across event processing outside the earlier Recently viewed repair: the
opening row and the row used by Return. The fixture now resolves the stable row
identity again before each gesture and assertion, and releases the Return
index before focus and key delivery. The window-system click also requires its
point to be inside the visible viewport before dispatch. All four themes,
selection/colour/accessibility, slow-network, retry and preemption checks remain.

The full repaired Home course passes on Mac in 64.66 s and native Windows in
64.49 s. The Windows run rebuilds the normal application, checks its exact
UTF-8 normalized source digest, and refuses images containing the temporary
reading-reset diagnostic. It substitutes only the explicitly recorded fixtures
for this engineering proof. Focused clang-format 22, clang-tidy 22 with warnings
as errors and whitespace checks pass. Final common-source comprehensive gates
remain pending; the separate Windows durable-reading and epoch-readiness
failures still require causal diagnosis.

## Adopt shared Git policy without rewriting it

The retained Windows epoch-readiness failure occurs during cache opening,
where constructing an installed-Git adapter can fail before repository
initialization. Bootstrap was reopening every existing isolation policy file
for exclusive writing, even after verifying its exact contents. Another Git
command's read handle can refuse that write on Windows.

A deterministic native regression holds a read-sharing handle over the exact
global configuration and constructs a second adapter. Unchanged production
code fails its adoption assertion. Bootstrap now creates only missing policy
files, using the existing exclusive-create helper, and reads existing files
without rewriting them. Exact contents, private-file checks and single-link
identities remain required; the identity is checked explicitly when adopting
so POSIX retains the hard-link refusal previously enforced by the write path.

The Git process, cache and sparse-cache contracts pass on macOS and Windows.
They include the new Windows read-sharing and portable read-only-file cases,
plus existing foreign-content, hard-link, symlink and hostile-process controls.
Focused clang-format 22 and clang-tidy 22 with warnings as errors pass, as do
the documentation gate and its 67 regression tests.
This proves the controlled sharing defect and its repair. The original
application readiness failure still needs the repaired composed course, and
the separate Windows durable line-position failure remains open. No final
qualification, signing or publication is claimed.

## Fence reentrant outbound tab loads

A deterministic host counterexample reproduces a newer displayed line being
reset in the same file and exact comparison: projection feedback refreshes a
pending load, the newer load reaches the view and the reviewer chooses line 1,
then the outer delivery sends the superseded initial-position load afterward.
Core stale-completion checks cannot undo an obsolete effect already sent to
the view. Unchanged production host code fails the line-preservation assertion.

The host now dispatches a load only while open and while that exact load is
still the core's pending request. The core remains the policy authority; no
command or completion rules change. Reentrant projection and persistence
feedback, reset and shutdown cases pass. The same line-loss counterfactual
fails with old host code and passes after the fence. The full host suite passes
on macOS and Windows, with focused clang-format 22 and clang-tidy 22 checks.
Fresh bounded ASan/UBSan maintenance and diagnostic fuzz courses each complete
100,000 runs; they do not fuzz document tabs or qualify the whole application.

The separate controlled desktop-effect barrier reached the public Quit/catalog
save interval and preserved line 1 through queued tab settling and the original
durable assertions. That case did not reproduce the Windows failure. Its child
passed; the outer wrapper's wrong stdout-marker guard is retained separately
and reconciled read-only against the marker in stderr, without rerunning it.
The reentrant host defect is proven, but linkage to the original composed
failure and final native qualification remain to be established.

## Normal composed integration after both repairs

The original Windows restoration course passes in 291.322 seconds with both
production repairs and failure-only fixture diagnostics. The executable excludes
the temporary admission, selection, store, factory and Quit-barrier probes.
Original gestures, immediate public Quit, deadlines and exact durable position
and active-tab equalities remain intact. Both production files match the common
committed candidate. All temporary native source substitutions were restored.

The deterministic host counterfactual proves the same line-reset symptom, but
the exact callback sequence from the earlier composed failure was not captured.
Those failures remain retained. This composed pass supplies integration evidence;
full native Release and Debug qualification, artifact audits, signing and
publication remain pending.

## Rejected common candidate and combined-release hold

The full Windows Release gate rejects common candidate
`4f8f12f4f72316e9b15fc0ccfaba1fe05f633549`: the original restoration course
fails after 285.85 seconds, during a later authenticated seed. Window 2 keeps
the exact comparison and file but stores line 0 instead of the selected line 1;
its active tab has no saved selection. The earlier focused normal pass and
the proven host fence do not close this composed failure.

The owned Windows suite was stopped and its downstream stages blocked. The
owned macOS producer was stopped with exit -15 before Debug or packaging.
Partial functional results, the original failure and observed foreign-work
overlap remain retained; no complete native qualification is claimed. No
signing, notarization submission, tag or publication occurred.

One already-started diagnostic covered both authenticated seed and advance
lifetimes, sampling application/recent-reading admission only on an interactive
line choice and constructing the full snapshot only for a mismatch. It passes
in 294.487 seconds without reproducing the mismatch. All nine temporary native
source substitutions are restored. This is diagnostic evidence, not acceptance
or permission to rerun unchanged courses.

The owner then directed publication to wait for the in-flight changes and a
combined release. Shared main contains additional completed owner work; an
About-page follow-up remains uncommitted at the recorded observation. That
work has not been merged into this rejected candidate. Identify the intended
batch and its landing gates, reconcile the retained reading blocker with the
landed source, then freeze one clean combined revision before fresh native
Release/Debug qualification and the remaining artifact, source, security,
legal and native-trust obligations.

## Combined release resumed

The owner confirmed that the in-flight work has landed and instructed the
release process to resume. Integration takes shared main
`b9a74b63a2ef7ae4edbf997a7291592214673b2a` in full, including RFC-BPR-0090,
and retains the release repairs through `0f52c515e`. The app-source merge was
reviewed against that main: its only added differences are the two explicit
on-lane tab calls and the guarded pending-tab reading gesture already reviewed
above. Main's feature and publication reviews remain recorded in the contract;
the mutation digest stays unchanged. The restoration fixture preserves both
the unique enabled menu-action check and its first position. Only its reviewed
ownership family is renewed; other stale families retain exclusive scheduling.

The cheap composition inventory and hostile-bypass control, nine ownership
controls, documentation and whitespace checks pass. The prior Windows reading
failure remains an open integration obligation until the combined source is
validated; no old-candidate pass or exception qualifies the new bundle.

## Atomic completion and reading acceptance

Review over matching `e031a961a` found a remaining split transition: completing
a pending view publishes its old reading before the following observation. A
reentrant projection or persistence refresh starts a newer request carrying that
old reading, correctly rejecting the late observation as stale. An external
counterexample linked to the real old production libraries fails the exact
line-preservation assertion. Both callback variants pass with the repair.

The core now composes completion and reading acceptance on a bounded state copy,
committing only when both accept. Stale and invalid selections leave the entire
state unchanged. The host performs this single transition on its decision lane;
the app uses it only inside the existing exact pending-material gesture guard.
Ordinary completions, current-request/generation checks, shutdown and publication
authority are unchanged. Core and host suites and clang-format 22 pass.
Only the app full/manifest tripwires and the reviewed restoration family's app
input hash are renewed; the mutation tripwire and scheduling remain unchanged.

The prior composed Windows failure's precise callback sequence remains uncaptured.
The normal combined-source restoration and Home courses pass, but its Windows
commit-filter course fails while reading the restoration store. That separate
failure and final native qualification remain open; no publication is implied.

## Native path in the commit-filter restoration fixture

The normal Windows combined-source commit-filter course fails at its direct
restoration-store read. Failure-only diagnostics retain its synthetic root and
show that flushing deferred Qt deletion does not change the refusal. The retained
database passes SQLite integrity and has the expected owner-private ACLs.
A fresh-process probe using the same executable and exact database deterministically
refuses Qt's forward-slash root as `invalid_path`, then accepts the same root
with native separators and loads the store.

The fixture constructed its recursive iterator from `QTemporaryDir::path()`
through `toStdString()`. It now uses the existing `native_filesystem_path` Qt
boundary conversion before deriving the storage root. The production validator
still refuses non-native paths; no product policy, gesture, deadline or assertion
changes. The original failed courses and probe-image hash remain retained.
The repaired atomic-reading source's full Qt-free core-only suite passes all
314 tests, including both architecture contracts, with no skips (788.57 seconds).

## R40 rejection and genuine legacy fixture

The combined candidate `7d4db926a` fails full native Release qualification.
macOS completes with three failures: Qt and native empty-desktop menu parity,
and authenticated Recent legacy migration. Windows also reports Qt review-bar,
suggestion-conversion, legacy fixture, start-review and startup-timing failures.
These are retained; no Debug, package, signing, notarization submission or
publication follows this candidate. Earlier tail-only progress monitoring missed
failed rows; full accumulated logs are now checked and the owner was told the
corrected status.

The legacy course stamped a current database as version 4 after dropping newer
columns but left the version-7 `review_filters` table. Production correctly
requires five tables for version 4 and refuses its six-table shape before writing.
A synthetic shape discriminator reproduces six tables before and five after
removing that newer table. The fixture now drops it too. Production admission,
migration, account isolation and source-retention checks are unchanged.

The original authenticated Recent course passes in 36.13 seconds using the
unchanged production helper and corrected fixture. Original exact account,
destination, reading and retained-object equalities remain. The temporary
source was restored byte-for-byte before applying this intentional fixture
repair. Native Windows confirmation and corrected final qualification remain
pending; no previous failure is converted into a pass.

## Windowless menu parity

The full macOS Qt parity and native last-window-close courses both expose the
same missing command: the window has BPR-1699's Mark All Comments as Read,
but the resident desktop bar omitted it. The desktop now retains that exact
command and separator in the window's order, disabled while no window is open.
It has no dispatch or shortcut. Existing composition and tour keys remain exact.

Applied the frontend-design skill for this presentation repair, preserving the
approved native vocabulary, palette, fonts and geometry. This restores the
existing command contract without introducing a new design. The unit's exact
order, key and disabled-action checks include the inserted command; all shifted
action references were updated. Native menu parity passes in 6.65 seconds and
the complete Qt profile-management unit passes in 0.81 seconds. Focused
clang-format 22 and clang-tidy 22 with warnings as errors pass. App composition
pins and mutation policy are unchanged. Final combined native qualification
and the separate Windows failures remain outstanding.

## Contained About fixture setup

The Windows full Release About course cannot write its first scratch document:
Qt's temporary directory lacks the AppContainer grant. The fixture now uses the
existing owner-private scratch helper, including the running container's grant.
This exposes another setup flaw: the original test ignored symlink creation
failure, so its claimed link rejection could test an absent file instead.

The native configure process now creates a bounded generated legal fixture and
refuses a missing or unexpected link. The contained course verifies that its
counterexamples really are a symlink and a directory before requiring both to
be rejected. The positive scratch documents remain runtime-created inside the
original network-denial boundary. No product legal-file policy or containment
exception changes. The revised normal Mac course passes; the native contained
Windows course passes in 0.17 seconds. External R41/R42 records retain both
setup failures, exact substituted source bindings and byte-for-byte restoration.
These focused observations do not qualify the complete release candidate.

## Native font layout fixture corrections

Failure-only Windows observations show that the whole Approve or comment…
wording fits at 1280 (bar hint 1258), with the exact PR title, dates and file
position preserved. The fixture had pinned macOS's shortened wording. It now
allows either approved wording at 1280 while requiring the whole bar to fit;
1460 still requires the full wording. The existing narrowing course still
requires ordered full/brief/icon yields with no overflow or lost commands.

The held suggestion converts to the exact expected text, ends the code edit
and releases its hold on Windows. Its 311-pixel card correctly restores Start
a review plus the compact More menu instead of the separate Add single comment
button. The fixture now checks the actual send action in either approved
layout, including opening the compact menu and requiring its enabled named
entry. It also explicitly requires the former Convert control to disappear.
No command is sent by opening that menu. Production presentation is unchanged.

Applied frontend-design guidance to preserve the existing native vocabulary and
font-dependent layout. All three complete default Qt courses pass on Windows
(32.00, 34.89 and 42.26 seconds); the complete line-draft course passes in
11.27 seconds. Mac's three default courses pass in 76.51 seconds total, and
line-draft passes in 9.97 seconds. The reviewed family's source seal is renewed;
occurrence counts, commands and ownership are unchanged. These scoped positives
do not clear the separate intermittent Home/start-review or startup-time failures.

Focused clang-tidy 22 exposed two pre-existing `std::endl` warnings in this
fixture's optional case tracing. They now spell the same newline and explicit
flush separately; trace timing and pipe delivery remain unchanged. Focused tidy
passes with warnings as errors, and both inventory source seals are renewed.

## A draft after reselecting the retained line

The native start-review failure has matching application and surface selection
on line 3, no editable draft and no pending work after its publication reread.
Choosing that unchanged line reports no semantic selection delta, so it never
retried the missing draft. Failure-only observations and original predicates
are retained externally.

A separate Qt callback now reports explicit same-line choices, with the exact
selection copied before feedback and discarded if feedback changed it. The
existing semantic-selection sink and reading gestures keep their contracts.
The application invokes its existing draft presenter only for the exact line
both layers still hold, with no draft, publication, draft operation or tab load
pending. Scrolling and restoration do not request that retry. No widget gains
an effect or draft policy. The Qt unit checks the exact gesture and preserves
the existing assertion that the semantic sink receives no duplicate delta.

A broader reading-gesture retry was rejected: although start-review passed,
Windows restoration lost the new selected line. That experiment is retained
externally and was removed before this slice. With the separate callback,
normal Mac unit, start-review, line-draft and full restoration courses pass
(8.69, 15.68, 8.62 and 194.25 seconds). Native Windows counterparts pass in
12.34, 25.72, 14.77 and 295.76 seconds. The normal application image excludes
the temporary Home geometry trace. App composition source pins are renewed;
the mutation inventory is unchanged, and its direct-bypass counterexamples
pass on both hosts. Only the revalidated restoration family's application
input seal is renewed; other stale families remain exclusive.

This slice does not close Home's separate native selection-colour failure or
the retained startup timing failure. Complete common-source Release and Debug
qualification and artifact, trust and publication obligations remain open.
