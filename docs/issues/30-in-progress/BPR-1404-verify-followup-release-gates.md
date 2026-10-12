---
schema: "implement-rfc.card.v1"
id: "BPR-1404"
title: "Repair timing and finish Windows and Ubuntu follow-up release gates"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"release-v010-20261012-fix-and-publish-r4","reservation":"docs/issues/_reservations/BPR-1404--BPR-1404--ef22c2dc4a83.json","milestone_plan":{"id":"release-v019-gate-repairs","path":"docs/rfcs/plans/release-v019-gate-repairs/milestone-plan.json"}}
---

# Follow-up release gates

The owner's 2026-10-12 amendment makes v0.1.9 macOS-only and accepts its
current timing misses. This card preserves the original outstanding work for
a separately selected 0.1.10 follow-up patch. A fresh RFC implementer owns
this milestone after the root coordinator freezes the v0.1.9 source. Keep its
clean release clone and protected candidate immutable. The coordinator selected
0.1.10 for full macOS, Windows and Ubuntu 26.04. The subsequent direct owner
mandate, "fix all the issues and go publish 0.1.10 prerelease afterwards",
authorizes the version bump and publication after the known release issues
are repaired and verified. The v0.1.9 timing exception does not carry over.

## Acceptance

- AC1: Diagnose and repair the unchanged 100 ms Mac PR-load input-gap failures without weakened budgets or assertions. Preserve original 111.602/107.923 ms and isolated 104.440/101.098 ms failures and bounded profiling evidence from BPR-1403.
- AC2: Synchronize one clean committed follow-up candidate to the native Windows and Ubuntu 26.04 producers, pass relevant normal native gates and affected contracts, and account for configured platform skips and all actual remaining failures before package handoff.
- AC3: Preserve intended full Windows/Ubuntu package/source/SPDX/digest/trust obligations and broader RFC0038 limits. Complete agent-owned repairs and upkeep; delegate no source work outside the assigned implementer. The coordinator owns authorized signing and publication after verified source handoff. Preserve source-backed vulnerability dispositions and independent security-owner acceptance where policy requires it.

## Frontier

The Windows baseline at `e004e1c2decd21387f835a5c16e925312970e196`
failed compilation before tests; typed-zero repair `525993f6287ae30ddbe16f67a8e5bc2a2ff09724`
subsequently built natively. Its partial SSH-launched suite had logon-context
startup/vault failures; the same source passed six focused native rows under
an owned limited interactive-token task, without ACL changes or exemptions.
The final full Windows gate remains required. Ubuntu's complete baseline gate
returned 661 pass, 11 fail and one configured skip. Nine failures have measured
font/scrollbar repairs; appearance follows the accepted visible-reading-line
contract, and the deleted-reply restoration race now has a deterministic failing old-code control and a passing Mac repair. Actual final-source native reruns remain required. Builds use at most eight jobs and Windows/WSL
performance courses do not overlap.

Prior source repairs and Mac passes are in BPR-1403 and its ledger.
`build/v0.1.9-repairs-timing-repro.log` preserves the isolated misses;
`build/v0.1.9-repairs-gui-profile-test.log` identifies adjacent GUI MetaCall
work at about 90 and 42 ms during load. Follow-up probes localized about 34 ms
to conversation adoption, 19 ms to overview publication and 21 ms to epoch
handoff. Instrumented passes are diagnostic, not repair acceptance. Every
probe was restored before the actual cache repair and uninstrumented checks.
Commit `8c02da1e94565db92d445cc39ab8a428198fdbc3` selects 0.1.10 and its
measured repair; exact-version/native acceptance remains in progress. The v0.1.9 frozen source and its
published assets remain immutable.

The [follow-up gate ledger](../../verification/BPR-1404-v010-release-gates.md)
records committed repairs, actual measurements, the nine collected Ubuntu
usability failures and the vulnerability disposition frontier. Commit
`f01b79c5a1568a4f25353f34cf61dcdd6627201e` repairs the two reproduced font
and scrollbar causes; all nine offscreen exact-Ubuntu-font reproductions pass.
Actual final-candidate native reruns remain pending.

Commit `a961d802c1a43316967b887581180e9904f79a3f` propagates the parent
Ninja generator into ECM, repairing the distinct Windows FileTracker frontier,
and corrects the appearance test to accepted BPR-1336 C4. Focused Mac checks
pass; the wider dependency gate passed in 97.15 s after its initial local disk-exhaustion failure. The ledger preserves actual failures and context separately.

Commit `f5e79380c5129cbd1067957dcc4b8e23d325f462` removes temporary reply
probes and follows a retired private draft restore into its current comparison.
The deterministic old-code control fails; repaired restore, full reply and
private-review native/file/retry Mac courses pass 5/5 in 32.10 s. True store
failures, retired contexts and shutdown are not retried. Format 22, focused
tidy and documentation pass. The shared BPR-1405 Home/Overview merge is
preserved, with its recorded 52/52 checks and owner acceptance; final native
affected runs include those inherited surfaces. Native full/affected results
and independent exact-artifact security acceptance remain pending.

The current Windows normal run also exposed nested Release-build object paths
over MSVC's limit. Its course now uses the shorter owned build-tree child `cc`
on Windows, preserving every nested-build and binary check. The RFC0017
stripped-image counterexample uses the shared contained-temporary helper;
18 inventory and six usage checks pass. Scoped review of all `src/app` changes
since the last matching pin (`3cc1b2fc3`) covers `8cf67d793` and `f5e79380c`:
temporary diagnostics are removed, private restore retries only the current
read-only binding, and existing mutation/authority/shutdown guards remain.
The aggregate manifest reseal and the passing hostile composition controls are
recorded in the ledger. Actual final-source native full-course checks remain
pending.
