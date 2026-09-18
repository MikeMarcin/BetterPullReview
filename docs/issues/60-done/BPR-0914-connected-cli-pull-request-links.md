---
schema: "implement-rfc.card.v1"
id: "BPR-0914"
title: "Route pasted PR links through the connected CLI session"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1a74b36d","evidence":{"kind":"command","detail":"Composed GHES native+CLI account course clicks consent, accepts CLI session, pastes URL and renders PR #17 with zero vault stores/retrievals. application_setup_destination_public_path passed; rendered overview inspected."}},{"criterion":"AC2","outcome":"pass","revision":"1a74b36d","evidence":{"kind":"command","detail":"core_setup_destination and public-path other-host case pass; disconnected profiles do not confer authentication. Original core policy fails the new preference assertion as a negative control."}},{"criterion":"AC3","outcome":"pass","revision":"1a74b36d","evidence":{"kind":"command","detail":"Focused setup checks 3/3, documentation gate, 64 documentation tests, touched-file formatting and diff check pass. Core-only build passes; 143/146 tests pass with three unrelated nested-worktree source-scan failures explicitly recorded. No installed/release claim."}}]
---

# Connected CLI pull-request links

Owner report: after connecting a GHES CLI account, Home stays empty and a pasted
PR link asks for native credentials. Work directly without sub-agents.

## Bounded milestone

Reuse the accepted exact-host CLI session for pasted PR links, preferring it over
an inactive native profile. Discovery alone must not grant authentication.
Preserve signed-out native setup, anonymous GitHub.com preview, cancellation,
and host isolation. Verify policy and the composed public link path.

## Acceptance

- AC1: An accepted CLI session opens an exact-host pasted PR without native credentials.
- AC2: Inactive profiles and other hosts cannot borrow the accepted session.
- AC3: Focused regression and documentation checks pass.

## Diagnosis and boundary

Composition excludes CLI profiles from setup observations and requires a native
credential authority to open a destination. CLI sessions intentionally have no
native vault authority. Core resolution also chooses the first matching host,
which can be an inactive native profile ahead of an authenticated CLI profile.

Home personal PR discovery is separately unimplemented, tracked by BPR-0598 and
BPR-0599 under RFC-BPR-0026. The composed queue replays opened snapshots; this
repair cannot establish account-wide discovery or close RFC-BPR-0026.

Baseline: `e60ea0e0`. No other source writer or dirty source was present.

## Implementation and evidence

Source revision: `1a74b36d`. The composition projects authenticated exact-profile
sessions, including CLI leases, into setup resolution. Core resolution prefers
an authenticated same-host profile over the first inactive native profile.
Opening validates the accepted session against the selected configured host.
No native credential fallback or new publication authority is introduced.

The existing account-discovery fixture now supplies a bounded exact-host CLI
transport when its synthetic discovery inventory is present. Its composed course
clicks the real confirmation button (the old `done(Yes)` call merely closed the
message box), accepts a GHES CLI session beside an inactive native option, pastes
the PR URL, and reaches the recorded PR overview. Vault stores and retrievals
remain zero; an unrelated hostname still requires setup.

- Focused core/setup-host/public-path checks: 3/3 pass, including the new CLI
  course; `build/bpr0914-final-tests.log`.
- Original core policy compiled against the new test fails at authenticated CLI
  preference, exit 1; `build/bpr0914-negative/result.txt`.
- Rendered real workbench fixture inspected at
  `build/bpr0914-render/setup-connected-cli.png`; PR #17 overview is visible.
- Changed C++ files pass clang-format dry-run and `git diff --check`.
- Documentation gate reports `DOCUMENTATION_OK`; 64 verifier tests pass.
- Tests-disabled app target builds in `build/app-release`. A separate external
  fixture launch returned `STARTUP_FAILED` before account setup; this is not
  production-bundle or installed-app validation. An older `release-no-tests`
  cache also failed dependency regeneration with mixed Qt header errors.
- Core-only configure/build succeeded. Full test run finished 143/146 passing, recorded in
  `build/bpr0914-core-tests.log`; all three source-scan failures include a nested
  `.claude/worktrees/agent-a17cae88115dbbb4e` checkout as duplicate source.

## Remaining scope

No live account/PR was used, no release was published, and no user's installed
application was replaced. Home personal discovery remains open in BPR-0598/0599;
this issue fixes the connected-session URL regression only. RFC closeout is not
claimed. Direct mode was preserved without sub-agents.
