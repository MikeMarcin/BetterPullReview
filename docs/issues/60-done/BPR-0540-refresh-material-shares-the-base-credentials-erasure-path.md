---
schema: "implement-rfc.card.v1"
id: "BPR-0540"
title: "Refresh material shares the base credential's erasure path, and the resume never spends an orphan"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0536"]
---

# Refresh material shares the base credential's erasure path, and the resume never spends an orphan

## Why

The second independent review of RFC-BPR-0016 at `0bd39349` ([[BPR-0285]],
2026-09-04) found two blocking defects in the refresh composition
[[BPR-0536]] delivered. No product code calls `delete_refresh`: the
orderly-shutdown policy deletes the base credential and leaves the refresh
token in the vault under the same authority, so a grant followed by a quit
orphans a six-month refresh token. On the next launch a pasted token for the
same account reaches the same authority key, the host's one-time probe finds
the orphan, composition takes the "renewable" branch, `resume_refresh`
spends the orphan at GitHub, and the resulting device token supersedes and
deletes the pasted-token session with nothing on screen. The fixture's
retained-secret check reads only the base slot, so the refresh course passes
with the refresh token left behind after the shutdown that deleted the
access token. No session survives a restart today, so the resume from slot
presence is reachable only through that wrong case.

## Required final state

- Wherever the base credential is exact-deleted under an authority —
  orderly shutdown, sign-out, a superseding enrollment from a manual source —
  the refresh slot is deleted under the same authority through the same
  vault port and completion discipline; host tests cover each site.
- The resume answers true only for a credential whose provenance is a
  device grant this host enrolled or resumed; a pasted token never resumes,
  even with material in the slot.
- The fixture reports the refresh slot (`DEVICE_FLOW_REFRESH_RETAINED`,
  `DEVICE_FLOW_REFRESH_STORES`); the refresh and registered courses pin
  `REFRESH_RETAINED:false` after their orderly shutdown; and an orphan
  course — refresh slot pre-seeded, a pasted credential connected — proves
  `REFRESH_RESUMES:0` and the pasted session intact, failing before the fix.
- The ledger's refresh row, the RFC's token-handling disposition, and the
  three documents describe the erasure and the provenance rule.

## Non-goals

- Persisting a session across a restart ([[BPR-0541]]).

## Lane — 2026-09-04

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its dependency terminal, as the blocking fix the
review named. The slice runs on `bpr-0540-work` in a worktree; the
documents and the ledger row are the integrator's at close.

## Landed, and the card closes — 2026-09-03

At `fa91490c` on `main`, verified on the integrator host after the two-pass
repin. Three parts, plus one correction the platform cards forced.

**The erasure.** One site in code, three occasions in behaviour. Every exact
delete of a base credential under an authority funnels through the host's
`start_delete`, so a single `start_refresh_delete(operation)` there covers
the orderly-shutdown `delete_retained_on_shutdown` path, sign-out
(`invalidate`), and a superseding enrollment: same operation context, same
authority, same `github_credential_vault_port`.
`OutstandingRefreshDeletes` joins `is_quiescent()`, so composition holds the
host and the vault alive until the vault has answered for the second slot
too. Only a failure crosses the decision lane, as a `vault_unavailable`
diagnostic plus `shutdown_cleanup_failed()` when closing.

**The provenance.** `github_credential_provenance { manual, device_grant }`,
defaulted to `manual` everywhere; the device-authorization enrollment
boundary is the one caller that says `device_grant`. It travels
`NextEnrollmentProvenance` → `SessionEnrollmentProvenance` →
`CredentialProvenance` and is remembered per binding in the catalog, so an
in-process resume carries it. `accepted_credential_renewable()` is true only
when the vault answered *and* the accepted credential's own provenance is a
device grant. A pasted token for an account a past grant enrolled is never
renewed from that grant's material, whatever the slot holds.

**The correction the platform cards forced.** The first draft treated the
vault's typed `unavailable` as benign, reasoning that a platform without a
second slot answers that way. Reading that rule against the adapters found
[[BPR-0542]] and [[BPR-0543]]: two shipped platforms had no slot at all.
With both now real, `unavailable` means only an unreachable store, so
`erasure_left_material` treats every error as a failure, the host test's
fourth case became "an unreachable refresh store fails shutdown cleanup",
and the fixture vault has a refresh slot on every course rather than only
the renewal one.

**The evidence.** The fixture reports the slot at evidence time
(`DEVICE_FLOW_REFRESH_RETAINED`, `DEVICE_FLOW_REFRESH_STORES`,
`DEVICE_FLOW_REFRESH_RETRIEVES`, and a `LastEnrollmentDigest` comparable to
the stored digest). The refresh course pins `REFRESH_RETAINED:false` and
`REFRESH_STORES:2` after its orderly shutdown; the registered course pins
`REFRESH_RETAINED:false` inside its `identityReported` block so the panel
and browser rows keep their adjacent-line pins. The new
`application_device_authorization_orphan_course`
(`--device-authorization-orphan-course`, tests-enabled, implying the refresh
and registered composition) seeds `fixture-refresh-token-0276` into the slot
at construction, connects with the fixture's pasted credential, waits for
the host's one-time probe, and prints `REFRESH_RESUMES:0`, `ROTATIONS:0`,
`VAULT_STORES:1`, `REFRESH_STORES:0`, `REFRESH_RETAINED:false`,
`STORED_IS_PASTED:true`, `DEVICE_FLOW_ORPHAN_OK`.

**Negative controls, both recorded.** Against the pre-fix rule the host
test `a refresh entry under the accepted authority is renewable` — written
for a *pasted* credential — failed by name, which is the review's B1
asserted as a requirement; it is retired and replaced by a device-grant
positive and an orphan case. And with the provenance gate neutralized the
orphan course printed `REFRESH_RESUMES:1`, `ROTATIONS:1`,
`VAULT_STORES:2`, `STORED_IS_PASTED:false`, `DEVICE_FLOW_ORPHAN_FAILED` —
the defect end to end: the orphan spent, a rotation following, and the
pasted session superseded by the grant's.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The graph builds clean and the pins bind it | `cmake --build --preset dev`; the round-trip contract, two repins (`ef8d2fb9…`, `83a5c74c…`) | exit 0, zero `error:`/`warning:` lines; pass 3 green | this commit |
| Every device row including the orphan course, the host, the vault, the composed-host audit, both inventory gates, and both composition contracts | `ctest --preset dev -j 2 -R '^(application_device_authorization_(host\|refresh\|refresh_course\|orphan_course\|registered_course\|unavailable\|release_contract\|public_path\|ghes_refusal\|browser_handoff\|browser_absent\|panel_course\|panel_absent\|browser_address)\|application_github_host\|application_github_credential_vault\|application_public_path\|composed_application_hosts\|application_composition_contract\|rfc0016_test_inventory\|test_network_isolation_manifest)$' --output-on-failure` | 21/21 in 137 s | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Strict tidy and format on the touched sources | `tidy.py --warnings-as-errors --paths`; `clang-format --dry-run --Werror` | exit 0; exit 0 | this commit |
| The orphan course does not flake | `--repeat until-fail:4` in the worktree | 4/4 | worktree run before integration |

**Left open, recorded rather than claimed.** The orphan course's terminal is
a bounded settle window after the probe rather than a positive observation
of a resume that did not happen; it can only report a false pass, never a
false failure, and a very slow station could in principle shut down before a
resume landed. [[BPR-0544]] fixed the sibling refresh course's analogous
race by waiting on the counter the row asserts; the same treatment here is
worth a follow-up if the row ever flakes. Nothing was run on Windows or
Linux; the final-head replays cover that.

