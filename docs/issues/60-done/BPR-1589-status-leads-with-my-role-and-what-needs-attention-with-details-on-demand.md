---
schema: "implement-rfc.card.v1"
id: "BPR-1589"
title: "Status leads with my role and what needs attention, with details on demand"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1588", "BPR-1576", "BPR-1583"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "overview-status", "criteria": ["AC03"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC03","outcome":"pass","revision":"88d5f086c","evidence":{"kind":"observation","detail":"Owner Accept of the frozen build frozen-39afb7162-v002 (executable 10709d45…30bd) in attended session 82f52ad4245bb64879f3bf2680814dea, candidate BPR-1589-status-content-v002 (fingerprint e36ac89e…), decision 71bc675f-6911-4d51-a034-74a8c97c1a63, actor human, no note; v001 rejected (decision 7e7d36fc: verbose role line, may-be-a-member hedge). Automated on the tree merged with main 57f335411: core_review_role (every role, several at once, team member, not a member, unknown with reason, mentions), application_pull_request_overview (roles, attention, membership answers), github_paged_read and github_provider (team membership read: member, not a member, similar slug, INSUFFICIENT_SCOPES, 403/404, hidden organization, GHES, signed out; asked once a session), qt_pull_request_overview_page (role line, attention named once, Go to them, disclosure memory, no empty rows with a negative control), qt_drawer_parts_scale_*, qt_workbench_window_scale_100 (Go to them opens Threads with Only new), application_status_attention_public_path, application_overview_summary_1, every application composition contract, architecture_boundaries, coding_format_check (32 rows). Live: the membership query answered correctly for a member and a non-member; BPR-Test has no teams."}}]
---
# BPR-1589: Status leads with my role and what needs attention, with details on demand

RFC-BPR-0085's Status content (D7): my role, Needs attention, My review with
unread threads and Go to them (Threads with Only new; Discuss for pathless
comments), Checks as one bar, People as faces, every section a disclosure to
its details, no empty rows, each failed check named once.

## Acceptance

- RFC-BPR-0085 AC03, with core courses for every role, several at once, and
  unobserved team membership.
- Owner, attended: "Status leads with my role and what needs me, and I can dig
  into each part."

## Boundaries

Core decides role, attention and unread facts from accepted observations;
the host extends the overview projection; Qt presents. Reuses [[BPR-1576]]'s
loading pattern and [[BPR-1583]]'s avatars, so it starts after both land.
