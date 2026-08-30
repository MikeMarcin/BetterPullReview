---
id: BPR-0493
title: Smart App Control and the ledger's --fresh requirement are mutually exclusive
status: Ready
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
