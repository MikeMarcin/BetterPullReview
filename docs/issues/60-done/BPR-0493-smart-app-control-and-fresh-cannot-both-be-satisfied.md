---
id: BPR-0493
title: Smart App Control and the ledger's --fresh requirement are mutually exclusive
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# Smart App Control and the ledger's `--fresh` requirement are mutually exclusive

## Why

`docs/verification/RFC-BPR-0016.md` requires [[BPR-0311]] to run the platform
course with `--fresh`, twice — once before the Windows selection flip and once
after — and [[BPR-0285]]'s final-head replay requires `--fresh` again. On the
`bpr-win-jaegaw` host that requirement cannot be met in a single pass.

Smart App Control is on and enforcing:

```text
HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 1
Win32_DeviceGuard.UsermodeCodeIntegrityPolicyEnforcementStatus            = 2
```

Configure fails at 6.5 minutes because the syntax-highlighting dependency
generates and then executes `katehighlightingindexer.exe`, and a freshly linked
unsigned executable is refused until its reputation verdict lands — historically
15–40 minutes, sometimes over an hour. This is the [[BPR-0424]] quirk.

The structural part is `--fresh` itself. Every fresh build root relinks that
tool, which restarts the reputation clock, and the course fails closed at
`configure-failed:windows-dev` with no retry. So the flag that makes the
evidence trustworthy is the same flag that guarantees the run cannot complete.
Running without `--fresh` completes but no longer proves the isolated-clean-tree
property the course exists to establish.

The same condition also failed `native_https_device_authorization_contract`
with `BPR_NETWORK_ISOLATION_UNAVAILABLE:launch:win32=4551` — a refused freshly
linked test image, not a product defect.

## Required final state

- The RFC's Windows rows are satisfiable on a Smart-App-Control host without
  weakening what `--fresh` is there to prove, or the ledger records an exact,
  accepted narrowing of that requirement rather than a silent one.
- The course distinguishes a Device Guard refusal from an ordinary configure or
  build failure, so an operator reading the output knows to wait rather than to
  debug the build.
- Whatever the resolution — bounded retry with backoff, an operator-attended
  two-phase run, a signed or pre-warmed dependency tool, or a recorded host
  exception — it is written down as the supported Windows procedure and not
  rediscovered by the next session.
- A run that waited out a reputation verdict is distinguishable in the evidence
  from one that never hit the condition.

## Notes

Found 2026-08-29 by `bpr-win-jaegaw` while running [[BPR-0311]]'s exact ledger
command at HEAD `6941a91b`, after [[BPR-0492]]'s multi-config repair. The
station is working it as a two-phase run — fresh configure, wait out the
verdict, resume without `--fresh` — and will report which phase produced which
row; that report is the input this card decides from, not its conclusion.

This is a host condition, not a repository defect, but it blocks a repository
requirement, so it is tracked rather than left as an operator anecdote. Turning
Smart App Control off is a real option and an operator decision, not one this
card takes unilaterally.

## There is no exception mechanism — 2026-08-29

The station investigated whether this can be narrowed rather than resolved, and
the answer is no. Recorded so nobody spends the same hour again:

- Smart App Control has no allowlist. Its UI is On/Off only, and the active
  policies under `CodeIntegrity\CiPolicies\Active` are Microsoft-signed WDAC
  policies (8 active).
- "Run anyway" belongs to SmartScreen, a different subsystem, and does not
  affect code-integrity blocking.
- Defender's `-ExclusionPath` is also a different subsystem and does not affect
  it either.
- Signing is not a practical out. The refused image is
  `katehighlightingindexer.exe` — a third-party dependency's build tool,
  relinked in every fresh tree, not a BetterPullReview artifact we could sign.

So the options really are two: wait out each reputation verdict, or turn Smart
App Control off. **Turning it off is irreversible** — restoring it requires a
Windows reset. That is an operator decision. The station raised it with the
operator explicitly and correctly declined to treat it as its own; it remains
open.

## Two-phase does not satisfy the ledger — 2026-08-29

Asked for its judgment, the station's answer, which [[BPR-0311]] has adopted:

> the moment the split is permitted, "which phase produced this row" becomes a
> thing a reader has to reconstruct

For the *unavailable* rows the risk is low, because a wrong answer fails loudly
rather than passing quietly. For the *enabled* flip course it is not
acceptable: that course exists to prove a one-time selection change against a
clean artifact, and a half-fresh tree is precisely where a stale object could
still carry the pre-flip constant.

That is why this card is now a `depends_on` of [[BPR-0311]] rather than an
advisory note beside it. BPR-0311 cannot close on this host while Smart App
Control enforces, whoever holds write authority — so this card, not the write
authority, is that card's real gate.

Three rows are currently held here, with their exact codes on the BPR-0311
card: the `check_device_selection` zero-request probe
(`1073751882`), `native_https_device_authorization_contract`
(`win32=4551`), and `verify_runtime_surface.py` (`WinError 4551`, which names
the policy in plain language and is the attribution for the other two).

## Dev Drive hypothesis — open, 2026-08-29

The operator created a Windows Dev Drive at `D:\` and checked the repository
out to `D:\git\BetterPullReview` to see whether the block does not apply there.

Recorded because it is worth settling, but the mechanism argues against it. A
Dev Drive's benefit is Microsoft Defender **antivirus** performance mode — ReFS
plus filter-attach control — and Smart App Control is the **code integrity**
subsystem, enforced by the kernel's CI policy independently of which volume an
image lives on. That is the same subsystem boundary that already made Defender
`-ExclusionPath` useless here. If that reading is right, `D:\` relinks
`katehighlightingindexer.exe` into its own dependency cache and it is refused
exactly as on `C:\`.

The test is cheap and decisive, and it should be run before the irreversible
option is considered:

```powershell
cd D:\git\BetterPullReview
cmake --preset windows-dev
```

Getting past the `bpr_syntax_execute` step means the hypothesis holds and this
card can close as host-configuration guidance. Failing again at
`MSB8066` / `1073751882` means it does not, and the card's two real options
stand unchanged.

Two things to check in the same run, because they cost nothing extra and one of
them could dissolve this card without touching Smart App Control at all:

- Whether the refused tool is rebuilt per build root at all. It lives under a
  `_bpr\ksyntaxhighlighting\...` dependency cache rather than inside the course's
  `--build-root`, so if that cache survives `--fresh`, the tool is linked once
  and cleared once, and `--fresh` never restarts the clock. The Windows run saw
  it refused on a first build, which does not distinguish the two cases.
- Whether a second `--fresh` configure against an already-cleared cache
  proceeds. That is the same question from the other direction.

`D:\git\BetterPullReview` has no `.remote-station.json`, so by the identity
guard a session opened there is not a station, holds no lease, and must not
accept a `remote-task` envelope addressed to `bpr-win-jaegaw` — see
[[BPR-0491]]. It is a third checkout on that machine and should be launched or
driven deliberately, not by pointing the existing station session at it.

## Decision: Smart App Control is being turned off — 2026-08-29

The product owner decided to disable Smart App Control on the JaegAW Windows
host outright, declining both the Dev Drive test and the dependency-cache
question. The decision was taken with the irreversibility stated in advance:
restoring Smart App Control requires a Windows reset, and this is a permanent
reduction in that machine's protection.

The operator performs that change themselves. It is a system security setting,
so no agent makes it and none was asked to — not this session, and not the
station, which would be the same act at one remove.

### What this settles, and what it does not

Settled: the three held rows on [[BPR-0311]] — the `check_device_selection`
zero-request probe, `native_https_device_authorization_contract`, and
`verify_runtime_surface.py` — should all run once the policy stops enforcing,
and the ledger's `--fresh` requirement stops conflicting with the host. That
must be confirmed by re-running rather than assumed; a policy that has stopped
blocking is not the same as a row that has passed.

Not settled, and this card stays open for it: **the constraint itself is not
host-specific.** Any Windows machine with Smart App Control enforcing — which
is the default on a clean Windows 11 install — hits exactly this wall, because
the refused image is a third-party dependency's build tool relinked from
source, not anything this project could sign. Turning it off here fixes this
machine and teaches the project nothing unless it is written down.

So this card's remaining requirement is documentation, not a code change:
`docs/development/remote-stations.md` should state that a Windows station host
cannot run the platform courses while Smart App Control enforces, that there is
no allowlist or exception, and that disabling it is irreversible — so the next
person provisioning a Windows host learns it from the document instead of from
a six-and-a-half-minute configure failure with the cause swallowed.

The [[BPR-0492]] repair that made `configure-failed` carry its output tail is
what would have made that failure self-explanatory the first time, and stands
regardless of this decision.

## Claimed on JaegAW, and Smart App Control is off — 2026-08-29

The operator performed the decided change. Read back on this host at 23:08
local, before any build ran:

```text
HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 0
Win32_DeviceGuard.UsermodeCodeIntegrityPolicyEnforcementStatus            = 0
Win32_DeviceGuard.CodeIntegrityPolicyEnforcementStatus                    = 2
CodeIntegrity\CiPolicies\Active                                           = 8 files
```

`0`/`0` is the not-enforcing reading of the same two values this card recorded
as `1`/`2` when it was written. The eight signed policies still sit under
`CiPolicies\Active` — those are the base Windows policies and were never the
Smart App Control verdict — and `CodeIntegrityPolicyEnforcementStatus = 2` is
kernel-mode HVCI, a different policy from the user-mode one that did the
blocking. The user-mode number is the one this card is about, and it is `0`.

A policy that has stopped enforcing is not a row that has passed, exactly as
the decision section says. The confirmation this card needs is a real
`--fresh` course completing on this host, which is [[BPR-0311]]'s exact ledger
command, so that run is this card's evidence as well as that one's.

Claimed by the session driving `C:\code\git\BetterPullReview` on `main`, which
is where the work has to land: the `bpr-win-jaegaw` station is
`read-only-proof` and cannot commit, and this is a native Windows host, so the
project integrator and the target-native platform are the same session here.
Both writer leases read free before the claim.

## Closed on `main` by the integrator — 2026-09-02

The hand-started session that claimed this card on 2026-08-29 did not return
to it; its later work landed as [[BPR-0509]]. The remaining requirement was
documentation, and the input this card said it would decide from has arrived:
`bpr-win` completed BPR-0311's exact `--fresh` ledger command twice on the
JaegAW host with Smart App Control off (`VerifiedAndReputablePolicyState=0`,
`UsermodeCodeIntegrityPolicyEnforcementStatus=0`), in one pass each, with no
two-phase resume — 13 m 32 s from a plain shell, failing at the release
configure, and 12 m 04 s under `vcvars64`, reaching step nine of ten. That
run is [[BPR-0495]]'s closing evidence as well.

Against the required final state:

- *Satisfiable without weakening `--fresh`*: yes, on the supported host state,
  which is Smart App Control off. The ledger's requirement is unchanged; the
  narrowing is of the supported host, recorded in
  `docs/development/remote-stations.md`, not of the evidence.
- *The course distinguishes a Device Guard refusal*: not implemented. The
  owner's decision made the condition non-occurring on the supported host,
  and the operator document now says an enforcing host is unsupported rather
  than detected. [[BPR-0492]]'s output tail on `configure-failed` is what
  makes such a failure readable if it ever recurs.
- *Written down as the supported Windows procedure*: the new section
  "Windows hosts: the supported platform-course procedure" in
  `docs/development/remote-stations.md`, carrying the two further findings
  from the same run — the Visual Studio developer environment that both the
  Ninja release preset and the `undname` symbol reader require, and the
  `Duplicate preset` collision from a stale `CMakeUserPresets.json` — and the
  `_bpr/` dependency cache that `--fresh` does not clear.
- *A run that waited out a verdict is distinguishable*: moot on the supported
  host; the station's envelope reply records the policy readings before the
  run, which is the distinction that remains meaningful.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The procedure is written down where a Windows operator reads | `docs/development/remote-stations.md`, section "Windows hosts: the supported platform-course procedure" | present, five numbered steps | this commit |
| The documentation gate | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| The requirement is met on the supported host | BPR-0311's ledger command on `bpr-win`, 2026-09-02, twice, single pass | steps 1–8 pass; step 9 is [[BPR-0511]] | `f2e39891` on `windows-lane`, merged at `c2d7b84f` |
