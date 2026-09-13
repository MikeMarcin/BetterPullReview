---
schema: "implement-rfc.card.v1"
id: "BPR-0784"
title: "Expose an honest opt-in GitHub CLI account profile"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
acceptance: [{"criterion":"Explicit CLI row names hostname and observed login without replacing native profile","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Tests-disabled public fixture selected GitHub.com CLI, consented and connected octo-reviewer on github.com; native profile remains a separate selection. build/app-release/rfc0015-evidence/public-path.json"}},{"criterion":"Account row discloses configured-CLI trust limitations","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Qt account text and qt_composed_github_scale_100 assert executable, configuration, socket, CA, redirect, child-process, account-switch and no-exact-account warnings, plus version/size qualification."}},{"criterion":"Read connection does not authorize publishing; identity drift fails visibly","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Read-only lease and mutation denial remain separate; focused provider/account contracts cover mismatch/revocation; CLI warning says read opt-in does not authorize publishing. No live PR mutation."}},{"criterion":"Rendered app and UI checks do not expose or persist token/private response","outcome":"pass","revision":"211e82f8","evidence":{"kind":"observation","detail":"Visible public fixture read and negative refusals used synthetic content; source and canary probes check no token extraction, arguments or output. build/app-release/rfc0015-evidence/public-path.json; build/rfc0015-r2-d3-hostile-git-config.json"}}]
---

# Expose an honest opt-in GitHub CLI account profile

RFC baseline: `6c9ef203` (after issue-ID reservation, before RFC-0015 source changes).

- [ ] A CLI account row requires explicit opt-in and shows exact hostname and observed `/user` login without replacing a native profile.
- [ ] The row names configured-CLI executable/config/CA/redirect/account-switch/process limitations as trust risks, not contained guarantees.
- [ ] Read connection does not grant publishing; a changed or unavailable identity fails visibly.
- [ ] The rendered app and account UI tests exercise the selection and warning; no token or private response is displayed or persisted.

The original deferred RFC contract remains preserved as historical evidence. No
live PR write is authorized. This card joins BPR-0785 in the first milestone.
