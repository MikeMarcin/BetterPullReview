---
schema: "implement-rfc.card.v1"
id: "BPR-0650"
title: "Home action list"
status: "Done"
rfcs: ["RFC-BPR-0028","RFC-BPR-0026"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"RFC-BPR-0028 AC-06 with the mockup's groups and columns","outcome":"pass","revision":"57c6824c + this finish","evidence":{"kind":"observation","detail":"qt_review_landing_model pins the bucket-to-group mapping, the headings, and the title, author, role, group and unseen roles; qt_composed_github_scale_100 pins Home carrying the open bar and the list with room, the delegate drawing through the list, each group's first row carrying its heading and the extra height, and the title and unread dot reaching the row. Home's funnel, the strip's Home-only chrome and the Home button are BPR-0646's and stay pinned."}},{"criterion":"Opening a row lands on that pull request's Overview","outcome":"pass","revision":"57c6824c + this finish","evidence":{"kind":"observation","detail":"qt_composed_github_scale_100: a click on a row sends the open intent for its repository and number and the pull request that arrives is shown on Overview; Return in the open bar sends the locator's intent for a link and clears the bar, and text naming no pull request sends nothing."}},{"criterion":"Focused model tests and a composed course over the queue fixture","outcome":"pass","revision":"57c6824c + this finish","evidence":{"kind":"observation","detail":"qt_review_landing_model and application_queue_composition_contract pass with qt_workbench_window_scale_{100,125,200}, qt_repositories_drawer, check_boundaries, strict tidy, clang-format --Werror and verify_documentation.py."}}]
---

# Home action list

RFC-BPR-0028 Home, replacing RFC-BPR-0026's desk presentation: a grouped
action list (needs you, waiting on others, following) with pull request,
title, why the row is here, my progress, author, role and updated time; open
by URL or owner/repo#number from the bar. Rows come from the queue and ledger
projections; discovery and observer rules remain RFC-BPR-0026's host work and
are not reimplemented here.

## Acceptance

- RFC-BPR-0028 AC-06 with the mockup's groups and columns.
- Opening a row lands on that pull request's Overview.
- Focused model tests and a composed course over the queue fixture.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- Slice, 2026-09-07. Home's list is the decided queue drawn as the action
  list: `review_landing_delegate` (in `review_landing_model.{hpp,cpp}`)
  draws one row per pull request with "repository #n", the title, the
  one-word reason (amber when it needs me), the author and my role, the
  unread dot before a row with unseen comments, and a heading above the
  first row of each group; `home_group_of` maps the seven buckets to Needs
  me (the four that need me, the repositories drawer's amber set), Waiting
  on others (authored and waiting) and Following (observed activity and
  quiet), so the priority sort gives the groups in the design's order and
  a repository or number sort interleaves them under repeated headings.
  The model gained `title_role`, `author_role`, `role_role`, `group_role`
  and `unseen_role`; its display text, filters and ordering are unchanged,
  so the queue courses and the earlier pins hold. A click or Return on a
  row opens the pull request through the same GitHub intent as a row of
  the repositories tree, and it arrives on Overview; restoring a selection
  after a projection opens nothing. The bar above the list opens by link
  or owner/name#number: `parse_pull_request_locator` reads "owner/name#n",
  "owner/name/pull/n" and a pull request link on any host into the
  locator's intent, and a bare number or anything else opens nothing and
  stays for correction. The list no longer carries the drawer's 84-pixel
  bound on Home.
- Not carried: my progress and the updated time (no projection names
  them; the ledger's receipts are per open review) and the mockup's
  explicit column headings, which the delegate's row order stands in for.
- Verification at the finish: `qt_review_landing_model` (groups, columns,
  the bar's grammar), `qt_composed_github_scale_100` (the bar and the list
  on Home with room, headings and heights per group, a click opening on
  Overview, Return in the bar sending the locator's intent),
  `qt_workbench_window_scale_{100,125,200}`, `qt_repositories_drawer`,
  `application_queue_composition_contract` (the composed course over the
  queue fixture), `check_boundaries` (the wording avoids the transport
  terms the Qt boundary refuses), strict tidy on the touched translation
  units, `clang-format --Werror` and `verify_documentation.py` pass.
