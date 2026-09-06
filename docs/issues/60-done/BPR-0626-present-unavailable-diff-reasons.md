---
schema: "implement-rfc.card.v1"
id: "BPR-0626"
title: "Present unavailable diff reasons instead of zero changes"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0625"]
acceptance: [{"criterion":"Truthful content-state presentation","outcome":"pass","revision":"945043fd240e8fba8ee9116181e4ff5bea3515fe","evidence":{"kind":"observation","detail":"Qt uses the validated typed reason for unavailable/binary results; text alone gets a semantic count. Unit checks cover real compute refusal after changed text, cached refusal, binary content, equal text, cleared panes and selectable/accessibility metadata."}},{"criterion":"Composed failure and recovery","outcome":"pass","revision":"945043fd240e8fba8ee9116181e4ff5bea3515fe","evidence":{"kind":"observation","detail":"application_unavailable_diff_public_path passed 15.72s: all stored pages and installed-Git path, actual compute limit with default matrix ceiling and test-only disabled index, rendered visible message, binary result and exact supported-file recovery. Full-window PNG inspected."}},{"criterion":"Regression, scope and cleanup","outcome":"pass","revision":"945043fd240e8fba8ee9116181e4ff5bea3515fe","evidence":{"kind":"observation","detail":"Five local checks passed 5/5 in 98.80s: Qt workbench, unavailable course, default large-review course, isolation manifest and reviewed composition contract. Documentation and git diff --check pass. No production limits/credentials, GitHub Actions, push or paid CI changed. Task reports and renders consumed and removed."}}]
---

# Present unavailable diff reasons instead of zero changes

BPR-0624's first composed large-file run returned a typed `compute_limit`
result, but the application displayed “Diff ready — 0 semantic changes”. The
bounded matching fix in BPR-0625 makes that particular input available; other
resource-limited inputs still need truthful presentation.

## Bounded acceptance

- Project unavailable/nontext result state and its typed reason through the
  existing application/Qt presentation path. An unavailable comparison must
  not imply a successful comparison with no changes.
- Keep genuinely equal text, binary content and unavailable content distinct;
  use useful, copyable user-facing wording without exposing source content.
- Exercise a real composed application selection that exceeds a retained
  compute limit; render and verify its unavailable message, then select a
  supported file and verify the normal state returns. Reuse focused core reason
  evidence and add only missing presentation coverage.
- Inspect existing ownership before implementation. Expected scope is the
  application projection, Qt status presentation and focused public-path test.
  No new algorithm, settings system or diagnostic subsystem is required.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Use synthetic fixtures and local Mac checks. No GitHub Actions, push, paid CI,
credential access or production application launch.

## Implementation and acceptance

Source `945043fd` fixes the Qt publication path. The core/host already supplied
the typed content state and reason; Qt now uses that reason for nontext results,
keeps binary comparison distinct, and reports a semantic change count only for
text. Cached unavailable results retain their reason. The existing selectable
status label and accessible description remain in use. Production limits and
publication policy are unchanged.

A test-only composition option passes limits into the ordinary diff host.
`application_unavailable_diff_public_path` retains the default matrix ceiling
and disables the optional ordered index: the stored 20,000-line file really
exceeds that limit. No fixture-generated diff or failure publication is used.
The test traverses paged provider reads, installed Git, normal file selection,
core/host completion and Qt projection. It verifies the typed unavailable
result, exact visible reason, empty panes/selection, keyboard/mouse selectable
message, then a distinct binary result and exact supported-file recovery.

Agent inspection of the full 1600×1000 window confirms the selected large file,
visible “Comparison unavailable — file exceeds the comparison work limit”
message and cleared source panes. Qt unit coverage also exercises a genuine
equal-text result and cached/noncached unavailable results after changed text.
The test-only override is unavailable in a production build and is ignored
when no fixture is selected. No new settings or logging system is introduced.

Five focused dev checks pass 5/5 in 98.80s on this Mac: Qt workbench scale 100
(1.94s), new unavailable public path (15.72s), unchanged default large-review
interaction course (27.92s), isolation manifest (4.23s) and reviewed application
composition contract (48.98s). Documentation and diff checks pass. The app
contract's source digest was refreshed for the inspected test-only limit seam;
the production mutation boundary is unchanged. Relevant prior core/architecture
evidence is reused because no core/host/dependency boundary changed.

All checks used local synthetic fixtures with denied network access. No GitHub
Actions, workflow dispatch, push, paid CI, credential operation or production
app launch occurred. Temporary run reports/renders were consumed and removed.
BPR-0627 remains open for measured UI stalls; this card closes only truthful
unavailable/nontext presentation and recovery.
