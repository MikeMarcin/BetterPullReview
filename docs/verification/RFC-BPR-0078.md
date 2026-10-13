# RFC-BPR-0078 strict chosen CLI credential qualification

Mandate `strict-cli-publication-r2`; plan `strict-cli-account-publication-v1`;
M2 `strict-credential-qualification`, BPR-1427. Baseline
`aa3d71fe470627e1e78488eb51a9114a788f33bd`. The repaired execution candidate
`cb41d1a1609d69373871bc3dd9321f59dcd60cc0` has independent critical acceptance:
P1 durable recovery-before-acquisition and P2 tool-versus-named-refusal
classification no longer reproduce. Its existing exact-bound M1 evidence is
[retained](../issues/_checkpoints/strict-cli-account-publication-v1/m1-repair-evidence.json).
M2 adds explicit native qualification helpers and an actual-child watchdog
course; its final revision and current verification are recorded in
[the M2 checkpoint](../issues/_checkpoints/strict-cli-account-publication-v1/m2-handoff.md).
Bounded independent critical acceptance of final candidate
`2793606c5a7785d0b671b81a31f016b401cab1b5` is supplied in
`/tmp/bpr-strict-cli-run/critical-m2-review.md`: no blocking finding, independent
runner 1/1 pass, ten evidence-file hashes and probe/stock-gh hashes verified.
All five strict criteria and M2 exits are accepted under `strict-cli-publication-r2`
with the limitations below. Main integration owns matching terminal RFC/index
status confirmation; no lane-owned index or counter change is made.

| Criterion | Direct evidence and limits |
|---|---|
| AC01 | M1 strict fixture courses cover global switches before acquisition, after validation and during helper work; wrong login/numeric account refuses before writes. M2 stock gh named lookup refuses with five local probes/one sensitive lookup/zero API or network Git launches despite an available unnamed synthetic token. Selected live `/user` observations return both exact chosen logins while global active account remains MikeMarcin. Exact synthetic named item deletion after acquisition cannot replace held bytes during the native helper course. No real global auth switch is performed. |
| AC02 | M1 production adapter/runner/SQLite courses cover review, inline, tour, pagination, exact blob/batch/fallback and clone scope; unsupported and unchosen authority refuse. Whole-review/tour unknown and confirmed durable records survive acquisition refusal/tool failure, keep drafts, and reconcile GET-only without a second POST. M2 uses the production acquisition path and exact clone compiler with stock gh/Git; local help admission is supplemented by actual helper behavior. Full Qt gestures remain covered by the separately scoped composed fixture regressions, not claimed as native credential acquisition evidence. |
| AC03 | Generic arguments/environment/input/results are token-free. Native scans find no acquired canary in BPR-created private configuration or clone stores; the external synthetic Keychain setup is test input, not a BPR-created credential copy. M1 SQLite/recovery and complete generic-record scans remain relevant. Actual timeout/cancellation descendants verify inherited credentials and drain before owner release. M2 additionally forces a watchdog `reap_failure` after a real child has started: its credential-bearing descendant is alive when the caller returns, sensitive ownership remains held, then release drains the descendant before ownership expires. |
| AC04 | macOS arm64 Darwin 25.6.0, Qt 6.11.2, stock Homebrew/core gh 2.101.0 bottle, SHA-256 `db2578e119b888fa9ed294fbc9db7f25133954078529aed06e8c9d6ada11a8bc`, Apple Git 2.50.1. Three synthetic host classes each pass named absence, selected helper use and wrong-family negative. Uninstrumented production acquisition and API return MikeMarcin and MikeMarcinTest identities, with actual X-RateLimit-Remaining headers 4999 and 4997 on the final live run (initial observations were 4995 and 4999). Only `/user` is called; no remote repository is read or changed. Global active login and config/hosts inode/size/mtime/mode are unchanged; no global config bytes or broad Keychain inventory are read. |
| AC05 | Exact repaired M1 admission/recovery/identity and architecture evidence is reused for unchanged inputs. M2 focused integration, exact clang-format 22, targeted tidy and documentation results bind the final candidate in the checkpoint. Current architecture/operator/security/GitHub host descriptions and RFC-0015 adopted amendment state the narrow transient exception. Imported historical tokenless plans/evidence and BPR-1373 owner judgment remain unchanged. |

## Native helper proof

`qualify_strict_cli_native.py` creates only random `gh:<host>` entries for a
random chosen login and an unnamed synthetic credential. Stock go-keyring's
`security -i` stdin setup mechanism creates them without token arguments or an
update option. Exact item references authorize deletion and exact service/account
absence is checked afterwards. No production token, owner ACL, Keychain default,
search list or global trust setting is changed.

Each native positive course uses production `acquire_gh_cli_credential`, the
sensitive runner, `run_gh_cli`, private configuration and the exact clone compiler.
After acquisition, the driver deletes its chosen item. Stock gh clones a synthetic
repository through stock Git and its own `gh auth git-credential` helper. A local
HTTPS smart-Git server first challenges the unauthenticated request, then accepts
only `Basic x-access-token:<chosen canary>`; no Authorization bytes are retained.

For enterprise, github.localhost and tenant.ghe.com classes, the positive records
one metadata API call, one 401 challenge and two chosen helper requests. A
negative passes the held bytes in the wrong primary token-variable family. The
metadata API deliberately permits it to reach Git. The challenge then elicits
the still-usable unnamed fallback: one rejected fallback request, zero selected
requests and failed clone. This would fail if helper inheritance or stock-gh
hostname classification changed; a public clone alone is not used as proof.

Test routing is explicit: the gh repository-metadata API uses a test-owned Unix
socket added to its private configuration, and Git uses a test-only
`http.sslCAInfo` clone option plus loopback CONNECT/TLS. This instrumentation
avoids changing macOS system trust; production continues to reject supplied CA
facts on macOS. The selected live API observation uses ordinary production
configuration, system trust, private pin and sensitive acquisition without those
routing additions. The helper course is stock-tool behavior under instrumentation,
not public GHES/tenant reachability or an installed-package claim.

## Lifetime and security limits

The new post-start seam is absent from production factories. Its injected stall
reproduces the runner's real completion-watchdog `reap_failure` with an actual
credential-bearing leader/descendant, unlike the earlier pre-execution queue-only
quarantine course. This satisfies the observable failure/ownership obligation.
A kernel-uninterruptible child or failed SIGKILL itself is not forced. Descendants
that escape the initial process group, numeric process identity reuse, malicious
configured executables/helpers and precise erasure of Qt/OS/stock-child copies
remain residual risks. No swap or crash-dump prevention is claimed. The existing
native-vault, crash, vulnerability and release gaps retain their original scope.

Missing and Keychain-denied named lookup share one honest
`credential_unavailable` outcome: stock `TokenForUser` reports no usable token
for either. No native denial prompt is forced on the owner. Process/capability
failure remains tool failure; a possibly transmitted publication remains
indeterminate with its journal/draft retained.

## Reproduce deliberately

From any working directory, build the explicit probe target in this checkout,
then run the local-only course. `--live-api` additionally retrieves both named
existing accounts through the product and records only identity/rate facts.
Output must name an absolute private evidence path. This is not a routine test
or permission to mutate other GitHub repositories.

```sh
cmake --build /Users/jaeger/.codex/worktrees/22e7/BetterPullReview/build/dev --target test_better_pull_review_strict_cli_native_probe
python3 /Users/jaeger/.codex/worktrees/22e7/BetterPullReview/tests/github_contract/qualify_strict_cli_native.py --probe /Users/jaeger/.codex/worktrees/22e7/BetterPullReview/build/dev/tests/github_contract/better_pull_review_strict_cli_native_probe --gh /opt/homebrew/bin/gh --output /tmp/bpr-strict-cli-run/m2-native-live.json --live-api
```

The initial native setup experiments are recorded as failures, not silently
substituted: isolated HOME hid the Keychain search list, a Python-created ACL
waited for reader access, macOS correctly rejected supplied CA facts, and an
initial wrong-family negative still had the chosen named item present. The final
course uses typed actual HOME/private GH_CONFIG_DIR, stock stdin setup, explicit
local routing and exact post-acquisition deletion. Every created item was cleaned;
no raw process/HTTP logs, credentials, generated source/object/certificate material
or private repository content are committed.
