---
id: BPR-0513
title: "Qt: place the side-chat panel and finding queue in the workbench drawer"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0504]
---

# Qt: place the side-chat panel and finding queue in the workbench drawer

## Why

[[BPR-0504]] composes `qt::side_chat_panel` and `qt::finding_queue_view` in
the shipped application, drives them through the composed gesture path, and
cut-tests each, but does not lay them into `workbench_window`'s drawer: that
is an edit to `src/adapters/qt/`, which was outside that slice's file surface.
A reviewer today can reach a finding through the composed gestures and cannot
see the panel that presents it.

## Required final state

- The side-chat panel and the finding queue are placed in the workbench
  drawer beside the existing agent launch panel, under the drawer's existing
  collapse and scale behaviour, and the workshop view opens from a finding's
  action inside the draft editor already in the drawer.
- Placement is presentation only: the panels keep their intent-out contract,
  and no new member or edge appears in the composed hosts.
- `qt_composed_github_scale_100`, `_125`, and `_200` stay green, and the
  composed window's capture shows the panels at each scale.

## Non-goals

- A chat session host — [[BPR-0514]].
- Any new action or field on either panel — [[BPR-0502]], [[BPR-0503]].

## Required direct evidence

- `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|application_agent_finding_composition_contract\|composed_application_hosts)$' --output-on-failure`.
- A composed-window case that opens the drawer and finds both panels by
  their accessible names; removing either placement fails it by name.
