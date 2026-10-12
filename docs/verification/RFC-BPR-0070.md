# RFC-BPR-0070 verification

Mandate `merge-gates-r1`; milestone `keep-home-waiting`; immutable RFC base
`9857d596769468c0585c475467175e3998a33530`. Facts/persistence candidate
`393dfe602354b189a6e883c92dc66bdc6a66579f`; native product candidate
`b5b08b3660835a326f16aa62bb25d98c79d89f5c`; final bounded-codec candidate
`39ecdbe1b544e18c12275c95db46734ee3937ecc`; final partial-budget candidate
`551bce4e0d66bcc2b5743eba63a779da9088007e`. Frontend-design was loaded from the
Claude official marketplace before the native surface changes. The existing
native row, wrapped reason, keyboard opening and theme projection remain the
presentation mechanism.

## Acceptance

| Criterion | Direct evidence | Result |
|---|---|---|
| AC01 | The composed native and CLI courses save a repository, discover an unopened authored PR with three code-owner teams and Branch Freshness, select its exact Home identity, open accepted Overview and return Home with the same requests. Host courses exercise unread discussion and marking read without default-zero replacement. | pass |
| AC02 | Host courses bind exact accepted actor ownership for Follow-only and observer-only enrollment without a saved repository. Aggregate review-required with no roster remains waiting. Ownership is a projection fact and leaves local enrollment intent unchanged; Unfollow removes a Follow-only row. Numeric-account changes cannot reuse another viewer's ownership or facts. | pass |
| AC03 | The composed mixed example remains Waiting on others and names all three teams plus Branch Freshness. The native row wraps a long qualified team slug. Host courses exercise unread precedence and clearance, complete zero request removal with a remaining requirement, subsequent approval with a remaining failed check, and truthful blockers after reviews clear. Qt checks keep such a row visible under Hide quiet and expose Blocked in its accessible description. | pass |
| AC04 | Host/provider/storage courses cover dispatch order, stale earlier completion, omitted versus complete empty fields, head changes, late account/session results, failed optional persistence, restart and migration of old rows to unknown facts. Discovery still uses 100 search rows/30 detailed PRs, 30 retained PRs / 400 reviews and two in flight. Added discovery reads share 32 attempts/8MiB per answer; retained reads share 32 attempts/8MiB and refuse review continuation before a fifth review page once 400 records are retained. No full PR contents are loaded for Home. | pass |
| AC05 | Existing composed discovery exercises selection through a waiting-to-My-pull-requests transition, sibling-window projection, Group/State, exact opening, filters, saved-repository intent and Recent behavior. Native model checks cover My role, separate people/team counts, collapsed blocker totals and accessible text. Home slow-open/cancellation and all three Overview public scales pass. Existing notification categories and eligibility are unchanged. | pass |

## Commands and review

Focused dev build targets are `test_personal_discovery_public_path`,
`test_better_pull_review_github_paged_read_tests`,
`test_overview_summary_public_path` and `test_home_open_feedback_public_path`.
The final provider/Home/Overview run is recorded in
`/tmp/bpr-merge-gates-run/m3-candidate-tests.log` (6/6 pass); the earlier native/CLI discovery
clearance run passes 5/5 and the host/SQLite/Qt/Home slow-open integration passes 4/4.
The core-only dependency, architecture, attention, host and storage pass is 5/5.
Final AC04 self-review added provider and host negative controls for optional
page/byte budget exhaustion. Complete title/review/REST-request facts remain
usable while the cycle reports partial detail; the narrow correction is recorded
in `m3-limit-fix-tests.log`, with unchanged native/CLI waiting evidence reused.
Changed C++ passes the canonical clang-format 22 check. Focused clang-tidy on
the new codec and core policy passes with warnings as errors. The checker asked
for an explicit string-view length slice around an already bounded `from_chars`
call; that final mechanical correction has focused host/storage verification in
`m3-codec-tests.log`, reusing unchanged public and visual evidence. The documentation gate
returns `DOCUMENTATION_OK`, and all 67 documentation regression checks pass.

The initial final clearance regression failed all four discovery/Home public
courses, demonstrating that the terminal empty-roster assertions discriminate
failure. Two synthetic observations were corrected: the empty GraphQL connection
now has a null end cursor, and a viewer author has the viewer's exact actor ID.
Production also preserves a validated complete REST request count when richer
optional names are unavailable. A complete empty roster clears actor names;
unavailable observations retain last-known facts and cannot settle independent
review requirements or checks. The affected courses then pass.

A bounded self-review checked ownership versus enrollment, dispatch order,
optional read budgets, failed metadata writes, migration/account admission and
native Quiet wording. No collaboration mutation, remote mutation, owner desktop
capture or browser/profile inspection occurred. Every course uses disposable
private data and the network-isolated fixture transports.

Offscreen native pixels are retained in
`/tmp/bpr-merge-gates-run/milestone-3-renders/candidate`: light, dark and both high
contrast modes at requested 1100/520 widths. The native window's minimum width
makes the 520 request render at 900 pixels; it is not claimed as a 520-pixel layout.
The waiting identities and check failure are inspected in the actual pixels.

## Qualification and remaining scope

The final composed pass joins the check-inspection and review-requirement
milestones with Home waiting. The earlier RFC-BPR-0068/0069 ledgers remain the
source for their qualified protocol and detailed Overview claims. Supported
GitHub.com/GHES response contracts and both native/CLI routes are covered by
sanitized/provider fixtures; live private-organization, installed account and
native GHES parity remain unestablished. Current-head checks do not prove
unobserved test-merge evaluation. No local CODEOWNERS evaluator, team enrollment
expansion, automatic reminder, blocker notification or merge authority was added.

The milestone's five criteria and three exits are Auto-Verified. The explicit
mandate requires the coordinator to dispatch and consume an independent critical
review of the final whole pack. That separate pack obligation remains pending;
this ledger supplies no independent or human acceptance. No required human
criterion was authored, so there is no attended candidate or review server.
