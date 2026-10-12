---
schema: "implement-rfc.card.v1"
id: "BPR-1403"
title: "Repair and verify the v0.1.9 release gates"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"release-v019-20261012-macos-r3","reservation":"docs/issues/_reservations/BPR-1403--BPR-1403--d8f83f6e0ba0.json","milestone_plan":{"id":"release-v019-gate-repairs","path":"docs/rfcs/plans/release-v019-gate-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0829baad5c75c9d5aeac4dd4c3d311558d85e9bc","evidence":{"kind":"observation","artifact":"docs/verification/BPR-1403-v019-release-gates.md","detail":"Original normal gate retained666pass/22fail/1skip; every failure classified. Fixture/procedure/pin causes repaired, no assertion disabled or weakened; timing failures reproduced and retained under explicit r3 owner release exception."}},{"criterion":"AC2","outcome":"pass","revision":"0829baad5c75c9d5aeac4dd4c3d311558d85e9bc","evidence":{"kind":"observation","artifact":"docs/verification/BPR-1403-v019-release-gates.md","detail":"All non-timing failed/affected Mac paths pass:21rows of26 at7d88970dd plus repaired account isolation41.08s and full32head history155.23s at0829. Strict32head replay/provider/contained-temp/architecture/anonymous/native contracts pass. Responsiveness/launch timing failures remain failed104.440/101.098ms; owner accepts only forv019. Debugcachebytes remainsconfiguredskip."}},{"criterion":"AC3","outcome":"pass","revision":"0829baad5c75c9d5aeac4dd4c3d311558d85e9bc","evidence":{"kind":"observation","artifact":"docs/releases/v0.1.9.md","detail":"Direct owner r3 amendment makesv019 Mac-only publication and explicitly accepts current timing misses. Windows/Ubuntu gates/packages and timing repairs preserved onBPR-1404 with original plan exits retained in followup milestone. Native gates not supplied or claimedpassed. One committed source to coordinator; widerRFC0038 obligations unchanged."}},{"criterion":"AC4","outcome":"pass","revision":"0829baad5c75c9d5aeac4dd4c3d311558d85e9bc","evidence":{"kind":"observation","artifact":"docs/verification/BPR-1403-v019-release-gates.md","detail":"Boundary and contained-temp tests pass; ninechangedC++ files format22.1.8 passes; focusedreplaytidy warnings-as-errors passes; compositionpositive/hostile controls pass; documentation gate and67contract tests pass, finalr3docs gatepasses. TemporaryGUI source restored exactly; all owned builds/tests/sampling complete; no reviewserver; explicitpathrepaircommits preserved."}}]
---

# Repair and verify the v0.1.9 release gates

The owner requests a new prerelease patch build, full packages for macOS,
Windows x64 and Ubuntu 26.04, and amends execution: "if there's actual fixed
to be done use an rfc implementer dont do them yourself". The coordinator
assigns this bounded repair milestone to `/root/release_gate_repairs` on main.
The release source starts at `0a35a94f263599d84da871aa7129a625cafd5b57`;
entry candidate is `c9a4164756bcd8ce3601115c0912845cb3ef5bcd`.
Existing repair commits are adopted and checked, preserving failed-run evidence.

## Acceptance

- AC1: Collect the running macOS normal gate and disposition every failed row against current source; fix actual causes without weakened assertions, disabled rows, broad ignores or bypasses.
- AC2: Rebuild repaired targets and pass non-timing failed plus materially affected Mac public-path checks, including provider/replay, large review, rate/scaling, offline account isolation and attended replay. Preserve actual timing failures under the explicit owner release acceptance, unchanged passing evidence and configured skips.
- AC3: Bind one clean committed Mac candidate for the owner-authorized macOS-only v0.1.9 release. Transfer Windows/Ubuntu normal gates, package obligations and timing repairs to BPR-1404 without claiming them passed.
- AC4: Pass relevant boundary, format and documentation checks, commit explicit repair paths and upkeep, and leave a durable checkpoint with candidate identities, commands, verification and pending scope.

## Ownership and limits

Engineering and board/plan/evidence ownership belongs to the nondelegating
implementer; coordinator owns producer dispatch, packaging and source syncing.
Preserve unrelated .gitignore, .mask.json, .mask.lock.json and RFC draft work.
No signing, notarization, tag, push, publication, credential change, owner
desktop/browser inspection or live GitHub mutation is authorized. RFC0038 and
BPR0958–0962/BPR0973 remain open with their wider native, vault, crash/privacy,
trust, licensing and vulnerability obligations. This card accepts only repairs
and their release gate evidence.

Durable continuation: `.mask/sessions/release-v0.1.9/repairs-handoff.json`.

## Composition pin review

The last matching reseal is `493d08433`. Every intervening production-manifest
change is in `f170654e651cb57e262d6c4e4696378c5464c365` and
`393dfe602354b189a6e883c92dc66bdc6a66579f`; only `main.cpp` and
`github_application.cpp` change in that manifest. Scoped engineering self-review
checked their full diffs. Draft metadata is optional, accepted under the exact
numeric account/session/host before updating an existing Recent destination;
unknown metadata preserves prior truth. Waiting facts come from the accepted
snapshot, with author actor equality and host dispatch revision, through the
existing queue host. Both retain the current notification baseline and grant
no enrollment, credential or publication authority. Main's changes are inside
testing guards and update Home fixture composition/counting only.

Prior acceptance and review remain in the RFC0067 ledger/BPR-1389 and RFC0070
ledger/merge-gates milestone-3 handoff, with independent pack review consumed
by the coordinator. This scoped self-review additionally accounts for the stale
release tripwire; it is not new independent release qualification. No source
inventory entry or mutation path changes. The production mutation-branch digest
remains `e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.
Full/application-manifest pins are resealed with unchanged semantic bypass
checks; their positive and hostile-source controls must pass before acceptance.


## Owner amendment: macOS-only v0.1.9

Mandate revision `release-v019-20261012-macos-r3`, 2026-10-12: "Just get the
current mac build out, timing misses be damned, and fix everything else
including windows and ubuntu for a follow-up patch". This supersedes the
selected three-platform release boundary for v0.1.9. Root owns Mac packaging,
signing/notarization/publication. This implementer freezes source and stops
before touching that release candidate again. No follow-up version is chosen.

The owner accepts the current timing misses for v0.1.9; this is a release
exception, not passing timing evidence or a relaxed assertion. Initial load
misses 111.602/107.923 ms reproduce in isolation at 104.440/101.098 ms against
100 ms. The Debug cache-bytes skip remains explicit. Profiling and temporary
event instrumentation are stopped and exact pre-instrument source restored.
Original normal gate and focused failed logs remain intact.

BPR-1404 retains timing diagnosis, Windows/Ubuntu gates and packages and any
other remaining actual repairs for the follow-up patch. Original r2 mandate,
plan source receipt and failed observations remain history; current AC2/AC3
are revised only under this direct owner amendment. Wider RFC0038 qualification
criteria and its standalone blocked cards are unchanged.
