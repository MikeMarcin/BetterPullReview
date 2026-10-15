# BPR-1525: Windows normal release gate repairs

Mandate `authorized-release-continuation-native-gates-r2`; milestone
`bpr-1525-windows-release-gates/native-gate-contracts`.
Original release source: `8f0d25418cd4278065d5a043dabaa728feb202f1`.
Engineering owner `/root/bpr1525_implementer`, native `rfc-implementer`, configured
`gpt-6.1-sol` / `xhigh`. No differing runtime route was exposed.
The initial generic worker's WIP is `a8abc5451`; no historical native route
or acceptance is inferred from that work.

## Original failures and current disposition

The original Windows normal gate is still running. These are retained failures,
not a complete final inventory, and no corrected native pass is claimed yet.

| Contract | Original observation | Repair or remaining diagnosis |
|---|---|---|
| `github_recorded_fixture_contract` | Source Find/PR Search construction bytes and later commit differ | Explicit UTF-8/LF writes in both generators; portable full contract and CRLF negative control pass at `a8abc5451`; Windows pending |
| `test_network_isolation_manifest` | Nine Windows registrations are intentionally unmeasured | Actual contained measurements still pending; no pin is guessed |
| `test_network_isolation_child` | Grandchild `CreateProcess` returns WinError 5 | Selected interpreter is the per-user Python runtime, whose image has no package/container RX; configure probe and conditional named-container runtime grant prepared; native control pending |
| `application_diff_host` | Matcher stop 16.2308 ms exceeds unchanged 10 ms budget | Replace timer polling with synchronized stop notification in the application layer; native public-host and race checks pending |
| `review_cycle_git_readers` | Generic fixture Git command failure after ignore-case row | The literal-prefix pattern's oracle returns nonzero on Git 2.55; probe both known Git behaviors independently and retain capture's conservative exclusion |
| `git_metadata_harness` | 89 unclassified differences, zero checkout writes | 85 C4 findings come from the Windows oracle returning no file identity; four C5/capture findings are the exact literal-prefix compatibility difference below |
| `gh_cli_transport` | First strict acquisition assertion expects credential-unavailable | Test composition expected a successful POSIX private-account seam on Windows; preserve production refusal and add exact zero-effect Windows coverage |
| `github_provider` | 30.04 s timeout | Cause unproven; focused native diagnosis pending |
| `qt_diff_workbench_scale_100` | 120.01 s timeout at original row 418/705 | Cause unproven; only offscreen size-hint warnings observed; no UI source or timeout changed |

Root preserved and stopped the original failed partial gate (426/705,
11 failures/timeouts) and confirmed its process tree absent. Original external
log `C:/Users/mikem/AppData/Local/BetterPullReviewRelease/v021-windows-production.log`
and partial reports remain retained. Root authorized reuse of
`C:/Users/mikem/bpr-v021` on `codex/bpr1525-native-repair-r2` after preserving the
clean baseline as `bpr1525-preserved-original-8f0d2541`. The separate station
`C:/code/git/BetterPullReview` is outside this ownership.

## Git compatibility policy under existing RFC83 H1

RFC-BPR-0083 H1 permits a documented deliberate choice and keeps unknown
differences fatal. No owner waiver is added. Capture continues to exclude
`config/local/a/key.pem` under `config/local**/*.pem`, with either
`core.ignoreCase` value, as the previously qualified Git 2.50.1 behavior does.
Changing installed Git must not silently include material that this deterministic
capture policy previously withheld.

The actual local Apple Git 2.50.1 oracle ignores the path. The original Windows
Git 2.55 oracle keeps it. Upstream's
[2.50.1 `match_pathname`](https://github.com/git/git/blob/v2.50.1/dir.c)
removes the whole literal prefix before matching; the
[2.55.0 implementation](https://github.com/git/git/blob/v2.55.0/dir.c)
retains one prefix character so the matcher can distinguish a directory-component
boundary. The matcher source itself has the same relevant double-star rule.

The reader course independently requires either the exact ignored output with
exit zero or empty output with exit one; every other oracle result fails.
Its capture exclusion assertion remains intact. The harness's deliberate-choice
entry binds K, C5 or C5-capture, the two named ignore-case corpus cases, the exact
path, oracle `kept`, and candidate `ignored` or `excluded`. Unknown paths, cases,
components and reversed answers remain unclassified and fail. Native corrected
passes and a negative control are still pending.

## Selected interpreter containment

The original CTest manifest names
`C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe`.
Read-only ACL inspection shows its image lacks the test-container/package RX
that the Program Files copy has. Checking the Program Files image alone did not
diagnose the selected interpreter. The uncontained launcher can create the first
process; that process's contained descendant must open the interpreter itself.

The prepared configure change probes an actual descendant using the canonical
selected interpreter, in its runtime directory. Only a failed probe grants RX
on that directory to the one named test-container SID, followed by the same
probe. An already accessible machine installation needs no ACL mutation.
The source/build/Qt grants, firewall requirement and zero network capabilities
remain. The existing child counterexample must still prove its grandchild's
AppContainer token and WSAEACCES 10013; a process-creation refusal is no pass.

## Verification and release boundary

The current source is still a repair candidate. Required focused native checks,
nine contained measurements, format/tidy and the provider-timeout diagnosis
remain open. Documentation verification and all 67 documentation verifier tests
pass with the installed
implement-rfc script directory on `PYTHONPATH`; the worktree has no managed
`.agents` link. Formatter 22.1.8 and `git diff --check` pass on the new C++ edits.

These repairs invalidate baseline `8f0d25418` as final release source.
Every release platform must rerun its required gate and packages from the same
corrected final source. This milestone grants no authentication, signing,
publication, native trust or release approval.


## Focused native evidence and further exact causes

Windows candidate `0d5f9cc2c90ecd99571a8dd32aba4347e585596a`, Interactive
Limited session 1, configures and builds successfully. The selected-runtime
probe fails before the named-container RX grant and succeeds after it.
`test_network_isolation_child` passes (0.19 s), as do external (0.15 s), loopback
(0.13 s) and concurrent launch (0.97 s). `application_diff_host` passes (6.06 s)
with its original internal 10 ms stop limit; readers pass (10.01 s), CLI passes
and provider passes (23.22 s) under its unchanged 30 s CTest limit. The original
provider timeout remains unexplained; final-source/full-gate repetition remains
required, and no cause or timeout repair is invented from this focused pass.

The native construction check exposed a second independent byte cause after
LF JSON writes: both JSON files had zero CRLF and equal lengths, but ambient
`core.autocrlf=true` changed the intentionally mixed-ending source blobs and
therefore their Git OIDs. Native regenerated head was `28a3fe88ff0de41b687d4fa750f9fdd247fee055`
instead of committed `755992e699b42939e3bff7abbdd1d66827ce439f`. The shared fixture
pack runner now disables that conversion per command; no global Git setting
changes. The new ambient-autocrlf regression rejects the former implementation
and accepts the corrected exact fixture. The complete portable fixture course
passes. Native corrected repetition is pending.

The harness drops from 89 to one unclassified difference on that native source.
The remaining C4 `format-compat-object-format` case sets repository format 1
and `extensions.compatObjectFormat=sha256`. Apple Git 2.50.1 prints the Git
directory; Windows Git 2.55.0.windows.3 exits 128 with exactly `fatal:
compatibility hash algorithm support requires Rust`. Upstream
[`repo_set_compat_hash_algo`](https://github.com/git/git/blob/v2.55.0/repository.c)
contains that compiled-without-Rust refusal. The existing deliberate discovery
policy opens directories without reading config; all config-reading candidate
components continue to refuse this extension. A new classification binds only
that exact case, component, candidate, observed refusal text and `answers`.
The oracle now retains that exact semantic refusal; every other error keeps its
previous value and cannot match the new entry. The source-backed rule extends
the existing C4 discovery distinction under H1, with no production change or
blanket suppression. Portable readers and harness pass after the change;
corrected Windows and negative controls remain pending.

BPR-1526's separately tracked [Mac action repair](BPR-1526-macos-workshop-action.md)
is part of the coordinator-authorized combined milestone. Its original press
and neighboring public courses now pass on the same application source.
