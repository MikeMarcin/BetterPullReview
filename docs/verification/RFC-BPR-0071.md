# Remembered CLI connection qualification

Mandate `remember-cli-r1` adopts RFC-BPR-0071 and the accompanying proposed
CLI amendment together. Baseline: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.
The exact source candidate and command results are recorded in
[the M3 ledger](../issues/_checkpoints/remember-cli-connections-v1/m3-verification.json).
AC01–AC06 are accepted on source
`62bd6a5a0aefbca78a8678a6884ccd6b7575e885`: the combined runtime-source
pass on `c27eb1356` is 15/15 in 156.84 seconds, followed by the 2/2 final
test-style recheck. Production bytes are unchanged by that follow-up.
BPR-1410 records the closeout decision.

## Acceptance map

| Criterion | Direct evidence |
|---|---|
| AC01 | `application_remembered_connection_restart`: explicit inactive named account and first read trust; OS process exit/recreation on one private root; no inventory or repeat consent; stable host/login/numeric identity; explicit closure of every window and profile shutdown followed by another process. Selection is saved before layout or Quit. |
| AC02 | The same course selects account 77 beside globally active inventory account 78, explicitly replaces it with 78, then reconstructs it with no inventory. Distinct GitHub.com/GHES records keep stable identities; exact-host links validate the saved connection and reach a private fixture PR, including after a different startup host refuses. Siblings share one account factory call. The public-path course adds native and CLI identities on the same server. Existing `gh_cli_git_smart_transport` checks private per-call configuration against a changed fixture global active account. |
| AC03 | Fresh processes refuse viewer/numeric mismatch and an unavailable CLI capability, present a diagnostic, expose no private Recent titles or accepted snapshot and preserve the complete choice; a later matching process succeeds. The public-path course also mutates only the saved numeric ID while preserving login. Adapter configuration/provider contracts cover missing executable/account and current process checks. Windows/Linux remain unsupported for CLI execution; no target-native qualification or expansion is claimed. |
| AC04 | Public Sign out disables reuse through process restart, including beside an older durable device grant and a pending native attempt allowed time to complete late. GHES Forget retires a revalidation and sibling authorities, removes its row and retains the other server; a new process cannot resume the forgotten destination. Cancelled and offline native replacements preserve the previous durable choice. Successful native replacement allocates a fresh ID, settles its existing device-binding durability and becomes the next lifetime's choice. The profile-isolation course and real store/profile contracts preserve another BPR profile. |
| AC05 | Fresh-process courses hold a real SQLite rival write transaction: failed selection is visibly session-only, while failed disable signs this session out and warns automatic sign-in may return. Newer-schema load refuses visibly and preserves database bytes. Removing the essential metadata demonstrates that prior state cannot manufacture CLI consent. Real storage contracts cover complete reopen, malformed/unreadable/newer data, stale writers, cancellation rollback and child interruption before commit. Maintenance inventory/purge includes the database and sidecars. |
| AC06 | Existing CLI wire/Git contracts exercise token and environment canaries, pin exact accounts, keep fixture global configuration unchanged and remove private per-call configuration. The inline-publication course reconstructs the saved CLI connection without inventory or repeat read consent, then independently requires publication confirmation for success, lost-answer retry and Cancel; Cancel sends nothing. Metadata schemas carry no credential or publication grant. |

## Regression and baseline discrimination

At the baseline, CLI profiles and their numeric pin were runtime-local; startup
chose native credentials. The scoped final diff adds pure selected-connection
policy, a private complete-snapshot store, a serialized writer, profile startup
and navigation revalidation, accepted-session persistence and retirement.
The missing-metadata negative control recreates the baseline's absent selection:
no account transport, vault or device acquisition is launched implicitly.

M3's composed courses failed on the preceding runtime and required three repairs:

- Identity invalidation was presented before the old accepted model retired.
  Unconditional observation remembered that old model under the new generation,
  obstructing replacement and overwriting the visible disable-save failure.
  Remembering now requires a delivered accepted-session projection.
- A saved exact-host link chose the right profile but stopped at generic Sign in.
  An explicit revalidation fact now lets core select the authenticating stage;
  the host performs validation with cancellation/reentrant admission guards.
- Fresh native replacement identity was rejected by the device capability bound
  to the retired connection. Composition validates the same registered server
  before guarded rebinding between flows; refresh enrollment follows that ID.

Focused core/host regressions reject foreign/stale/empty identities, active-flow
rebinding, unregistered capability manufacture and shutdown ingress. A metadata
record continues to authorize only validation, and current exact-account session
acceptance continues to gate private reading.

## Scope and ownership

Evidence is macOS arm64, Qt 6.11.2, offscreen, network-denied fixtures. There are
no live GitHub/GHES calls, owner desktop actions, personal browser reads, global
`gh auth switch`, installed-package claims or new Windows/Linux support. Native
vault and release/security qualifications retain their existing scope. The
separately parked BPR-1373 owner judgment and historical CLI-profile plan remain
unchanged. No human or independent acceptance is required by this RFC or inferred
from fixture results. No attended review session or server is created.

The broad composition tidy scan reports 14 inherited identifier-naming errors.
Every diagnostic line is unchanged from the immutable RFC baseline, as recorded
in `m3-inherited-tidy.json`. Changed core/host, fixture and qualification course
checks pass; this closeout does not claim a globally clean tidy gate.
