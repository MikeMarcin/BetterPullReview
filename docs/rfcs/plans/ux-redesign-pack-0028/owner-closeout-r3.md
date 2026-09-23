# RFC-0024 owner closeout amendment r3 — 2026-09-22

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0024-claude`, milestone
`close-rfc-0024`; sole non-delegating `rfc-implementer` engineering and
bookkeeping owner in `/Users/jaeger/.codex/worktrees/0152/BetterPullReview`,
branch `codex/rfc-0018-0026-closeout`. The resolved coordinator-implementer
strategy and immutable RFC base `e851eb8a851ab605cf5a85260a1317ecdf66773b`
are unchanged. This continues the ordered pack after RFC-0018's amendment r2
and RFC-0024's planning reconciliation; it does not reopen either.

After about eight hours with no verdict on the one served candidate, the
owner directed, verbatim: "Wtf I dont care about the windows stuff, defer
that. Get to work". This **changes acceptance**; it is not an observation,
not a passing native verdict on `BPR-0979-windows-activation-v001`, and not
a Windows release approval. It supersedes the r2-era requirement that the
native Windows desktop observation block this milestone. No other criterion
is weakened or removed; no native or human observation is self-certified.

## Exact transfer

- [[BPR-0980]] retains the native Windows desktop observation of repeated
  ordinary launch activating the owner profile's MRU window and explicit
  `--new-window` opening another window without a second owner. It is
  standalone (`rfcs: []`) with explicit RFC-0024 origin metadata and Blocked
  on `native-desktop-observation`. It preserves, unchanged, the exact
  attended candidate `BPR-0979-windows-activation-v001` (fingerprint
  `bc3d75f62152e8f2a6cb18e6c67222d4b20878bddfebeb00b8b681fd7a39c1e9`,
  revision `windows-dev-674c68dc-1cd7f2f1-2026-09-22`), the station build
  `C:/Users/mikem/bpr0979-044e8779/source/build/windows-dev/src/app/Debug/better_pull_review.exe`
  (sha256 `674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef`)
  and the passing automated Windows evidence at `1cd7f2f1` on station
  `bpr-win` (`sqlite_profile_activation` 5.83 s,
  `application_profile_activation_public_path` 12.98 s,
  `build/bpr0979-windows/ctest-final.log`). That automated pass is evidence
  for the bounds/isolation and test rows, not for the deferred observation.
- [[BPR-0979]] finishes Done on its remaining accepted rows: the Windows
  named-pipe transport's bounds and isolation contract and public course pass
  on the station at `1cd7f2f1`, and the macOS activation and window-layout
  regressions pass at the same source. Its first acceptance row records the
  amended scope and names BPR-0980 as the holder of the transferred
  observation; it does not call that observation passed.

BPR-0980 is not a dependency of [[BPR-0595]], exactly as BPR-0974/0975 are
not dependencies of BPR-0593. Its local dependency is the Done BPR-0979,
whose transport it observes. RFC-0018's BPR-0974/0975 remain untouched and
independent. The original AC-06 native wording remains in the RFC.

All other RFC-0024 criteria retain their existing scope and evidence: the
owner accepts on `BPR-0715-signed-out-open-v004` (decision `b1eb8efb`) and
`BPR-0977-native-geometry-v001` (decision `ec0c5993`) bind only to bundle
`6c830b41…` built from `d5a2426c`; the two-account, multi-window, startup
and activation courses keep their exact producing commits. No live
GitHub/GHES mutation, credential, push, merge to `main`, or change to a
protected station checkout is authorized.

## Completion and handoff

Record BPR-0980 and finish BPR-0979 through the shared board helpers, then
promote, claim and finish BPR-0595, set RFC-BPR-0024 to the terminal status
for the r3 accepted scope and reconcile `docs/rfcs/README.md` row 158. Run the
documentation gate, `issue_board validate --rfc RFC-BPR-0024`, the touched
macOS regressions and `git diff --check`; refresh the milestone checkpoint
binding/check. Release the remaining candidate protection and stop the
attended server (pid 49234, port 55108) while leaving session
`8d2c2e32f7131447c24c2fd522f18fe6`'s durable records intact. Finish at
RFC-0024; the coordinator owns starting `plan-rfc-0025` and eventual `main`
integration.

The configured main integrator allocated exactly BPR-0980 with
`python3 scripts/allocate_issue_id.py` in the registered project root;
reservation commit `dabcdaec` is incorporated on this branch as `4e58ab78`.
No MASK repair, alternate execution topology or live provider mutation was
introduced.
