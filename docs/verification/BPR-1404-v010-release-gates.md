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

## Windows execution context

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
