# BPR-1404: v0.1.10 release gate repairs

## Scope and ownership

Mandate `release-v010-20261012-fix-and-publish-r4` is the owner's direct
instruction, "fix all the issues and go publish 0.1.10 prerelease afterwards".
The selected set is full macOS universal, Windows x64 and Ubuntu 26.04 x86_64.
The v0.1.9 timing exception does not apply. The published v0.1.9 source
`6cc7a99e8754ffe482a90dc012d5afe98b3a9da8` and its producer clone stay immutable.

The fresh RFC implementer owns all source/test/plan/card/evidence repairs.
The coordinator owns native producer dispatch, package/sign/trust/publication
operations. Unrelated MASK, ignore and RFC-draft changes are preserved.
This record remains open until the actual native results and affected exact
candidate checks close the milestone; no package or security qualification is
inferred from development checks.

## Committed repairs and measured public paths

Windows baseline `e004e1c2decd21387f835a5c16e925312970e196` failed compilation
before tests with MSVC C4389/C2220: `optional<uint32_t>` compared with signed
zero. `525993f6287ae30ddbe16f67a8e5bc2a2ff09724` makes the assertion's zero
`std::uint32_t`, preserving its behavior. The subsequent native build passed; final normal native acceptance remains pending.

Mac source/version repair
`8c02da1e94565db92d445cc39ab8a428198fdbc3` derives each byte-distinct path's
exact core file identity once per conversation adoption in the panel and cards.
The cache is local to one immutable input list; it owns no global state and
cannot carry an old path mapping into a refreshed list. Review identities,
selection, model signals, appearance and actions are unchanged. The source
version and preparation notes select 0.1.10 under the direct mandate.

Before the version-only bump, the uninstrumented repair tree passed all five
focused development courses: responsiveness, launch, conversation panel,
conversation cards and conversation model properties. The two PR-load input
gaps were **93.861583 ms** and **90.510250 ms**, both against the unchanged
**100 ms** limit. The new panel case proves repeated paths share file scope,
case-distinct and empty paths remain separate, and a replacement list's changed
path reaches both the model and cards. These are factual macOS arm64,
Qt 6.11.2, Debug/offscreen observations with the committed large fixture
(515 files, 1,000 threads and 1,000 comments), not a universal latency claim.
At source `f01b79c5a1568a4f25353f34cf61dcdd6627201e`, all 14 uninstrumented affected Mac courses passed in 154.09 s. The two exact-version load gaps were **91.358042 ms** and **91.199833 ms** against the same 100 ms limit. Final native and package checks remain required.

Original v0.1.9 misses remain recorded in
[BPR-1403's ledger](BPR-1403-v019-release-gates.md): normal 111.602/107.923 ms,
isolated 104.440/101.098 ms. Follow-up instrumentation localized conversation
adoption to about 35 ms: 8.5 ms retained indexing and 25 ms refresh, including
18.7 ms placement/cards and 6.2 ms model application. Both retained indexes
previously recomputed SHA-derived file identities for every node. Instrumented
passes were used only for diagnosis. Every source probe was restored from its
exact preimage before the actual repair and uninstrumented checks.

Local raw logs and measurements remain ignored:
`build/v0.1.10-path-cache-checks.{log,xml}` and
`.mask/sessions/release-v0.1.10/measurements/`. Focused clang-tidy, exact
clang-format 22 and the documentation gate plus 67 documentation tests pass.
The first boundary run failed on a coordinator-created extracted source tree
inside `build/release-workspaces`; after the coordinator moved that tree
outside the checkout, the unchanged boundary gate passed (28.16 seconds).

## Actual Ubuntu native frontier

Ubuntu 26.04 built baseline `e004e1c2decd21387f835a5c16e925312970e196`.
Its complete normal 673-row gate returned **661 pass, 11 fail, 1 skip**;
the Debug cache-bytes skip is configured. The full composition course passed
in 821.40 s. Nine failures were the three scales each of Overview summary,
workbench window and title row, with the two reproduced geometry causes below.
The remaining failures were `qt_appearance_profile` and the deleted course of
`application_reply_draft_contract`. The latter retained a drafting reply and
its note after confirmation; it failed in 2,688 ms with the 40,000 ms run budget
unexpired. The bounded native diagnostic isolated readiness false with no private operation pending; the restoration repair below awaits its native rerun. Coordinator log:
`build/v0.1.10-ubuntu-gate.log`. Actual final-source reruns remain required.

Diagnostic test commit `8b01bd7fc6c69674978f3b802fae960791ab707c` preserves
all assertions and reports each failing geometry/state/text fact separately.
A Mac offscreen negative control loaded the exact Ubuntu DejaVu Sans font
without installing it, at 96 DPI with the native Windows style. It reproduced
both causes: fractional glyph widths rounded down and elided `File` to `F…`,
and enlarged overview content needed 288 px inside a 284 px viewport.
Commit `f01b79c5a1568a4f25353f34cf61dcdd6627201e` rounds text widths outward
and lets the sidebar reserve its native non-overlay scrollbar extent. It
retains the ordinary 300 px minimum and native Qt palette/font/control styling.
All nine affected font-reproduction courses then passed unchanged assertions
in 102.32 s; the fixed enlarged offscreen render was inspected. Temporary font
probes were restored exactly before the final candidate build. The uninstrumented 14-course Mac pass above includes these nine courses. The actual Ubuntu a961 rerun passed all nine geometry courses.

The frontend-design skill was absent from the local deployments and was loaded
from its published upstream
[SKILL.md](https://github.com/anthropics/claude-code/blob/main/plugins/frontend-design/skills/frontend-design/SKILL.md).
Its digest-bound snapshot is ignored. The repair brief preserves the native Qt
palette, fonts, controls and existing layout intent; diagnosis uses offscreen
renders and never the owner's live desktop.

## Appearance contract and Windows composition repair

Commit `a961d802c1a43316967b887581180e9904f79a3f` corrects the appearance
test to the already accepted [BPR-1336 C4](../issues/60-done/BPR-1336-zoom-the-diff-panes-code-text.md)
contract: a visible reading line takes precedence over the synchronization
rail and stays within one row after remeasurement. The exact Ubuntu fixed-font
negative control preserved the comparison and all semantic identities but
moved the rail across a segment; the reading line moved from y274 to y288,
within its 20 px row. The repaired assertion checks initial visibility, final
visibility and the one-row bound, retaining every semantic and geometry check.
It passes both the exact-font reproduction and the ordinary Mac course.
Temporary font and reply-action probes were restored before this commit.

The Windows composition course separately failed with MSVC FileTracker
FTK1011 on a nested ECM try-compile path at the legacy path limit. Its parent
Release graph selected Ninja, but ECM's configure omitted the generator and
selected Visual Studio. The same commit forwards the parent generator,
platform, toolset and exact make program into ECM and invalidates older stages
once. A fresh Mac dependency stage/build and the integration check of both
nested projects' actual caches pass. The actual Windows composition rerun is in progress through the corrected local-token route.
The broader Mac dependency contract first encountered ENOSPC at 161 s and could not write its final XML. After task-owned disposable copies were removed, the unchanged contract passed in 97.15 s; both results are retained. The documentation gate also passes on the current ledger.

The final source scan exposed a separate recursive-glob bug: a nested other
writer's worktree was treated as another product target. Commit
`c83c397aca09a606d741e7e8bb26d2bc3cf19e7f` recurses only through `src` and
appends the one root CMakeLists explicitly. The new negative control fails
before repair; after repair it ignores nested worktrees/extracted build sources
while still rejecting duplicate targets in the root and nested product sources.
Both the normal architecture gate (18.19 s) and full boundary contract
(127.95 s) pass. No other worktree was changed or removed.

Commit `6673a3be33f620e6759039dfc963ba0286b6234b` addresses two focused tidy
findings in the touched UI translation units: a local constant's name and a
static QString initialization. The same sentinel is now a constexpr Latin1
view. Both translation units and the appearance test pass focused tidy and
format22; ordinary workbench100 and Overview1 pass (33.04 s).

The actual Ubuntu focused rerun at `a961d802c1a43316967b887581180e9904f79a3f`
passed 10/11 repaired rows; only deleted-reply discard remains failed. Its
same saved-drafting/note-present evidence is retained. Two earlier remote
wrappers ended before producing tests; their transport failures are separate
from this completed native result. The completed course used an owned limited
interactive-token scheduled task with local WSL output/receipt and was cleaned
up by its coordinator.

## Draft restoration lifecycle repair

The bounded native diagnostic at `8cf67d793dc4abadf245a5f3b13befa060a2f2cf`
recorded the confirmed discard gesture reaching an accepted draft while
`PrivateDraftsReady` was false and both private pending flags were false.
The store workflow was quiescent; no commit callback ran. The original
40-second run budget was unexpired, so increasing it would not address this
failure. Those temporary diagnostics are removed in source repair `f5e79380c5129cbd1067957dcc4b8e23d325f462`.

A comparison adoption increments the private workflow generation. If a draft
restore is already queued on either lane, its retired completion returns a
conflict after the last observation has skipped starting another restore.
The caller then remains unready indefinitely. Restoration now follows a
retired generation only while the same private context remains current and a
valid binding exists. A stable-generation store refusal, context retirement
or shutdown answers normally. Each real comparison change can cause another
read; a stable binding cannot cause a retry loop. Mutation commands keep their
existing exact revision/authority checks and are never automatically retried.

The deterministic regression uses the real private SQLite store and manual
effect/decision lanes. It fails with the old code after a comparison change,
then passes the repair for retirement both before the read runs and after the
read completes but before its decision lands. A shutdown case refuses once
and drains without retrying. Together with the unchanged seven-course public
reply-draft contract, the first Mac checks passed 2/2 in 14.95 seconds. After
focused hygiene fixes, all five restore, reply and private-review native/file/retry
courses passed in 32.10 seconds. Exact format 22, focused tidy and documentation
checks pass. Native repaired Ubuntu and Windows checks remain required.

Focused tidy also identified pre-existing hygiene in the touched private
workflow: its typed refusal carrier now derives from `std::exception` with a
fixed non-secret description, the polymorphic destructor declares `override`,
and an existing source list reserves its known bound. The reused core fixture
keeps parsed optional blob IDs directly instead of dereferencing and wrapping
them again. These preserve the same structured failure/result behavior.

The shared branch separately inherited BPR-1405 in merge `69a263cee`:
Home/Overview wording and projection updates, with 52/52 recorded related
checks and the owner's accepted frozen looks. Those changes are preserved;
final native affected checks must include Home, Overview, landing models and
personal-discovery paths in addition to this milestone's repairs.

The owner's subsequent instruction includes committed BPR-1406 through
`77905f2fe289320f03792f516896a4f4fc1c088a` in the next release candidate.
Its [completed card](../issues/60-done/BPR-1406-name-the-author-on-home-rows.md)
records author labels on every non-authored Home row, matching accessibility
text, 52/52 related checks, format/tidy/documentation and an inspected offscreen
render. The same native Home-related selectors cover this inherited change.

## Newly collected timing frontiers

The clean Mac normal gate at `99e96691` passed both unchanged 100 ms load
courses (responsiveness 12.14 s, launch 12.35 s), but failed history in
196.38 s. All 32 retained epochs and exact sources passed. Every refresh
completed within 12 seconds and every source selection within two seconds;
the failed bound is the separate 100 ms input interval. Refresh epochs 2–25
had maximum pulse gaps of 71–85 ms; epochs 26, 27 and 29–32 reached
110–173 ms. This actual failure remains open while its event-lane cost and
runtime conditions are diagnosed. No timing bound or assertion is relaxed.

The completed Mac `99e96691` normal gate returned 683 pass, six fail and
one configured cache-bytes skip in 59.98 minutes. Three failures concern the
contained-temporary helper and aggregate composition seals repaired above.
The updater transaction and native-staging failures had only 2.6 GiB free
against their existing 4 GiB prerequisites; those production guards remain
unchanged. After task-owned producer cleanup and Debug local-symbol omission
(`-g0`, executable linker `-Wl,-x`, assertions unchanged), 26 actual native
symbol/network/RFC0016/RFC0017/updater checks passed in 34.38 seconds. That
focused prerequisite repair does not replace the final normal gate.

A declared isolated history baseline at `8f68006e` failed in 145.14 seconds:
all 32 retained epochs and completion bounds passed, but refresh 24 reached
126.141 ms against 100 ms. A single no-content GUI-event diagnostic then
failed in 149.22 seconds. The failing serialized `QObject` MetaCall spent
99.859 ms wall time and 80.777 ms of GUI-thread CPU; ordinary corresponding
turns spent about 65 ms in both. Benchmark instrumentation was restored
byte-for-byte before further acceptance.

Commit `1dca2e7dc` avoids deriving Overview file identities when both private
receipt and note populations are empty. Its note-only regression passed, but
the clean native history course still failed in 191.91 seconds with refresh
gaps of 104–139 ms; it is not sufficient timing acceptance. Commit `8fa5ab470`
separately retains the receipt index only while the exact current ordered path
list is unchanged. Each adoption still rebuilds observation, progress and
navigation state. Changed, reordered, case-changed, renamed or removed paths
rebuild the index; clear retires both paths and authority. Commit `539f2cd07`
adds explicit same-path re-adoption after clear.

The Overview regression passes on the clean producer, and focused tidy of
both translation units and format 22 pass. Two separately compiled, contained
negative controls fail at the intended assertions: ignoring exact path equality
misapplies receipts after reordering; retaining paths after clear leaves a
stale empty index. Their temporary trees were removed without editing producer
source. A subsequent declared uninstrumented history course passes as recorded
below. A launched
course was interrupted after observing an unrelated 16-worker compiler build;
its context and exit 143 remain recorded, with no product result claimed and
no attribution of earlier failures to that later contention.


At `15671d48221496f5e3d66a42fe0f5d446b44dacd`, the declared isolated,
probe-free native Mac history course passes in **122.02 seconds**, retaining
all **32 exact epochs**, **961 recorded account reads** and **zero model
resets**. The largest refresh input gap is **72.578917 ms** against the
unchanged **100 ms** limit; the largest refresh completion is 1,562.876 ms.
A preceding two-epoch copied-source diagnostic (explicitly contended and not
acceptance) measured 7.8–8.2 ms GUI CPU reparsing the same accepted bodies in
each nonempty card forest. Endpoint placement repeatedly adopts 2,000, zero,
then 2,000 nodes. The repair retains one ordered exact accepted-body set across
those transitions, replacing it when nonempty body inputs change and releasing
it on authority clear. Its focused card regression passes in 1.39 seconds and
covers changed body, accepted reviewer prose, reordered nodes and clear.
Two independently compiled contained negative controls fail at their intended
assertions: retaining annotations after the body changes and reparsing them on
every placement change. All diagnostic copies and executables were removed;
producer source hashes remain unchanged. Raw report: `build/v0.1.10-history-body-annotations.json`.

The inherited BPR-1407 commits `8cc80c6ed` and `970cbf01a` add the requested
Home age, added/removed lines and comment counts. The card records 76/76 local
related checks. Its AC4 owner look remains pending in attended session
`7cf6171a73b084a2ae161dcd95702e61`, candidate `BPR-1407-home-facts`, frozen
root `/Users/jaeger/bpr1407-look.5jquigqu`. Final native selectors include the
provider read, discovery, queue facts storage and Home surfaces. Scoped review
of the inherited composition clock and this repair's fixture-only private
persistence hold reseals the full application and aggregate source pins; the
production mutation-branch digest stays unchanged. The pending-label course
now holds actual persistence while both public Publish gestures turn events,
then releases it and requires a new exact-body gesture. Assertions and product
publication guards remain unchanged.

The Windows 8cf normal course also exposed three empty tool-free writing
folders that Qt could not remove. They carry no read-only attribute; their
owner-only DACL lacks the package grant a lowbox access check requires. The
Windows launcher now creates each new request-owned folder with the existing
central owner-private descriptor, granting only its user and current package.
It changes no existing ACL. The native wrapper course additionally checks that
exact descriptor during the run and that each folder is gone afterwards.
The exact `004a9c8ef` Mac build and three focused cards/wrapper/label courses
pass in 69.34 seconds. Final native closure of this repair, the pending-label
course and the newly collected Windows title-row failure remains required.
Focused tidy finds no violations in cards or the wrapper test. Its full scan
of the large application TU reports 14 pre-existing naming findings outside
every changed range; no new fixture-hold finding is reported. Existing launcher
name/type and label-test copy/flush findings receive narrow hygiene corrections;
those two TUs are rechecked separately. Original failed tidy output is retained.

The Windows x64 Debug normal gate at `8cf67d793` separately failed
`application_diagnostic_subsystems` after 134.70 s: its enrollment child hit
the wrapper's hardcoded 120-second backstop. The independent enrollment row
had already passed in 172.96 s on that same native producer, Qt 6.11.2,
without another Windows/WSL course running. Mac and Ubuntu completed the
independent row in 33.48 s and 52.49 s respectively. This is a wrapper bound,
not an enrollment performance assertion or application run budget.

The wrapper now takes explicit per-child bounds. The other three remain
120 seconds; enrollment gets 540 seconds, three times the observed native
completion rounded upward according to the existing sizing guide. Their
900-second sum plus 30 seconds for process startup/teardown fits inside
three quarters of the 1260-second CTest allowance. Configuration checks that
relationship through the existing bound helper. Child exit and both-terminal
canary checks remain; timeout output names the child and bound without
printing unvalidated partial content. Five hermetic counterexamples pass,
including nonzero exit, either-stream canary, timeout and invalid bounds.
Actual native wrapper completion and margin remain pending.

## Windows execution context

The native title-row baseline failed its expectation that publishing review
files also makes the epoch selectors visible. The accepted
[BPR-1130 contract](../issues/60-done/BPR-1130-the-updates-line-folds-and-loses-its-permanent-caption.md)
starts a new window with Updates folded; the portable Qt course already
checks that the hidden timeline leaves no row. The Windows course now verifies
the visible review bar and default folded state, then publishes the explicit
core open-line delta before its existing full-width timeline and native-frame
geometry checks. No product behavior, geometry assertion or wait bound changes.
Actual repaired native Windows completion remains required.

The Windows agent-finding positive course read the original draft after offering
and accepting its revision. Its common path relied on the optional Dif sprite's
acknowledgement to delay that read; other variants already waited for accepted
text explicitly. A copied-test counterexample held the real private store lane
for two seconds and omitted the optional visual wait: the old course failed
the exact `draft-body-is-suggestion` term in 4.957 seconds, while a checked
accepted-text wait passed in 4.841 seconds. Producer source hashes were
unchanged and copied scratch was removed. The actual course now checks accepted
draft text independently of Dif, reserves the visual acknowledgement wait for
the Dif course, and holds the private lane in the positive course to verify
the old body remains until persistence completes. All existing terms and run
bounds remain. Exact-source Mac courses at `457a5bc29` pass 4/4 in 16.83
seconds: positive, full-agent, workshop-press and Dif-context. The focused
tidy run found one pre-existing local constant name in the included
`right_side_change.hpp`; its spelling is corrected without changing the
256-change bound. Actual repaired native Windows completion remains required.

The subsequent interactive-token Windows normal gate at diagnostic source
`8cf67d793` exposed a separate nested Release-build path failure: MSVC C1083
on the generated KSyntaxHighlighting indexer and Qt resource objects. The
course's original tree made the two object paths 264 and 282 characters.
Windows now keeps that same course-owned tree at `${PROJECT_BINARY_DIR}/cc`,
reducing those paths to 223 and 241 characters; other platforms retain their
existing path. The same tests-disabled configure/build and binary checks still
run. Native completion of that course on the repaired candidate is pending.

The stripped-image RFC0017 counterexample added in `99e96691` initially used
`tempfile.TemporaryDirectory`; the Mac normal gate correctly rejected that
Windows-incompatible root. It now uses the shared contained-temporary helper.
All 18 inventory counterexamples and six contained-temporary usage checks pass.

Scoped composition review covers every `src/app` commit after the last matching
pin at `3cc1b2fc3`: diagnostic `8cf67d793` and repair `f5e79380c`. The repair
removes all diagnostic changes from `github_application.cpp`; its full-source
and production mutation-branch digests match their existing pins. The only
remaining manifest changes are the private workflow's read-only restoration
retry, the existing vector reservation and explicit destructor override.
Review confirms that a retry retains the same current PR context, reads its
currently accepted head, and uses the existing serialized store/decision lanes.
Shutdown, retired contexts and stable-generation store failures return without
retry. No mutation, grant, revision check or publication path changes.
The aggregate application manifest is resealed to
`1d6432a7024143638ed6e4c75d12ac918d27b8b99895886d40615763048fc674`.
The real composition baseline and all its hostile-source/manifest counterexamples
pass after the recorded review; native full-course acceptance remains pending.

The first `f01b79c5a` Windows native build passed. Its SSH-launched suite then
had immediate DLL initialization/delay-load failures, empty architecture child
outputs and unavailable Credential Manager setup; the partial failed run was
preserved and stopped by its owner. A synthetic absent-target CredRead returned
1312 (`ERROR_NO_SUCH_LOGON_SESSION`) under SSH and 1168 (`ERROR_NOT_FOUND`)
under an owned, limited interactive-token task. No credential enumeration,
owner secret read, desktop automation, ACL change or network exemption was used.
Under that same local-token route and same source, six normal contained native
network/HTTPS/vault setup/lifecycle/cleanup rows passed, as did contained
Python-to-CMake startup. The Python real-location stderr remains observable
but its child returned zero. These focused observations resolve the execution
context diagnosis; they do not substitute for the required final full Windows
gate through the correct token route. Raw local logs retain both results.

The owner rejected BPR-1407's initial Home look and amended its presentation.
Selected commit `5081076e48efdd0f7f6be1eae4f11d54ce4913b4` supplies coloured
line counts separated by a slash, a short age, a comment icon/count, and a
reviewer count on authored rows. Its owning card retains the required revised
look and shorter-row acceptance; the original rejected look is not approval
for this revision.

Scoped composition review from `004a9c8ef` through `5081076e4` finds only one
additional `src/app` change: `github_public_path_fixture.cpp` introduces
`discovered_title` and uses it in the fixture's two existing read responses.
It gives pull request 4002 a long title. No request, credential, storage,
grant, mutation or source-inventory entry changes. This tests-only TU is
excluded from the production manifest, so all three existing pins remain
unchanged: full application `5aec6edb7ada80035e9019fa76fe9fd48a476db0f6aaf69abf2c808ce984be57`,
aggregate manifest `a525a2fc6658c8ff1ca24a7019ec5d379311b6ecfbfda4a0ba7d7bacfd473125`,
and mutation branch `e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.
The actual baseline and hostile-source/manifest controls pass in 2.012 seconds
on the clean revised source. There is no new hash to reseal. The final normal
tests-disabled composition course remains required on the selected source.

The clean `badb89b0a` producer (UI source `5081076e4`) builds the revised
application, landing model, Home list and discovery course successfully.
Eight normal focused rows pass serially in 94.47 seconds: landing model,
Home at 1.0/1.25/2.0, personal discovery and authored waiting through both
native and CLI paths. The owner look is still independent. Final affected
native Windows/Ubuntu and the authoritative Mac normal gate remain pending.

## Vulnerability and wider qualification frontier

The fresh v0.1.9 exact-SPDX Grype run remains failed: **32 matches**
(**2 Critical, 8 High, 19 Medium, 3 Low**). Its SPDX SHA-256 is
`c7df978a9e1198a01f96946dfaf7e1a68db844c1a6a6bc134c396a820d5a89f3`.
Scanner 0.120.1 used schema 6.1.10, built 2026-10-12T06:45:22Z, database
checksum `f1d61e7be5caa87b18ab7519f71982399f4d43d8f407edd24717903acef3d83d`.
Raw report `build/release-workspaces/v0.1.9-grype-final.json` remains local;
these older bytes are inventory input, not the final v0.1.10 scan.

The [source-backed proposals and primary references](BPR-1404-v010-security-dispositions.json)
retain each finding group and its pending independent acceptance. Known findings
require these separate dispositions:

| Group | Agent evidence and remaining obligation |
| --- | --- |
| Tika MIME XML attribution | The attributed resource is data, not Java Tika executable handlers. Preserve all upstream identifiers/notices and raw matches; independent Critical/High applicability acceptance is pending. |
| SQLite 3.53.4 | Upstream fixed versions for the three Bitnami matches precede the pinned version. Preserve unknown-range matches and exact source identity; independent High applicability acceptance is pending. |
| libdbus headers | Header attribution differs from the external runtime library. Final target-native runtime inventory must name its actual libdbus and disposition the Medium findings. |
| Apple system zlib 1.2.12 | Apple documents CVE-2022-37434 fixed at the declared Ventura minimum. ABI/version text alone does not certify OS patches; the newer CRC-combine finding needs its own call-path/OS disposition. |
| Gzip-write CVE-2026-85091 | The Windows Qt source predates the upstream fix. Canonical also lists Ubuntu 26.04 zlib as vulnerable with a deferred fix and warns about uncertain older-version applicability. Preserve the High blocker pending exact native linkage/source and independent affected-API reachability review. |
| PCRE2 10.47 and SLJIT | Existing exact-Qt source/API proposals remain unapproved. PCRE2 10.49 is the complete currently identified remediation target; 10.48 also has High CVE-2026-103111. First-party patterns are committed constants; packaged syntax dynamic captures are escaped. Candidate-native source/binary and independent applicability decisions remain pending. |
| New Qt XML patches | Current upstream patches CVE-2026-82292 and CVE-2026-16762 must be included in coverage even if absent from the scan feed. Exact Qt source already contains both 16762 fixes. QDom lacks 82292 checks but QtXml is absent from the v0.1.9 Mac runtime and first-party graph. Confirm final native module inventories and retain separate applicability decisions. |

[The Qt 6.11 archive](https://download.qt.io/archive/qt/6.11/) currently offers
6.11.2, so a nonexistent prebuilt 6.11.3 upgrade is not claimed. The
[PCRE2 advisory](https://github.com/PCRE2Project/pcre2/security/advisories/GHSA-r9hj-j2rw-4q3m)
requires attacker-controlled regex plus JIT and a growable stack and identifies
10.49 as fixed. A rebuilt and qualified pinned Qt producer is the alternative
if an independent owner rejects exact-version applicability proposals.

All final artifacts still need current scans and source binding. Under
[release policy](../security/release-policy.md), Critical/High matches block
until fixed or accepted by an independent named security owner with an
expiry/recheck condition; lower severities also require disposition. No agent
self-approval, feed suppression, metadata hiding or inherited v0.1.9 waiver is
used. Broader RFC0038 native/vault/trust/licensing/crash gaps stay explicit and
are not closed by this engineering milestone.

The security disposition proposals now include a separate proposed expiry of
2026-11-11T23:59:59Z and earlier rechecks on changed source/dependencies,
artifacts, selected modules, architecture, supported OS or relevant advisory.
No approval identity or date is filled. Every final binary, source asset, SPDX
and native runtime inventory still needs an exact binding before independent
acceptance. These are review terms, not an approved exception.

The CVE-2026-27171 system-zlib probe on macOS 26.6.2 arm64 reproduces hangs for
negative-length `crc32_combine` and `crc32_combine_gen`; owned subprocesses were
terminated and reaped after one second each. Positive CRC composition passes.
The exact retained QtBase 6.11.2 corresponding-source archive scan covers
8,572 C/C++ files with no non-third-party calls to these combine APIs or the
Windows advisory's gzwrite/gzprintf/gzvprintf APIs. Bundled zlib/Freetype
implementations remain present in source. First-party and pinned LibArchive
sources contain no combine calls. This is unused-API applicability evidence;
it is neither a system-library patch claim nor independent approval.
