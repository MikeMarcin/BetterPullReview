---
schema: "implement-rfc.card.v1"
id: "BPR-1483"
title: "The Colours list course counts the namespace and enum-member rows"
status: "Ready"
rfcs: ["RFC-BPR-0034", "RFC-BPR-0072"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"kind":"defect","found_by":"qt_colour_role_list on main in a Release tests-on build at 6bb137d15, 2026-10-13","introduced_by":"c58bdebda","reservation":"docs/issues/_reservations/BPR-1483--BPR-1483--8833b8751efa.json","related_cards":["BPR-0986","BPR-1083","BPR-1085","BPR-1411","BPR-1412"]}
---

# The Colours list course counts the namespace and enum-member rows

`qt_colour_role_list` fails on main every run: "58 of the 60 roles the core
declares are listed" at `tests/adapters_qt/colour_role_list_tests.cpp:139`.
c58bdebda (RFC-BPR-0072, BPR-1412) appended `syntax_namespace` and
`syntax_enum_member` to the core's vocabulary, raising
`core::appearance_role_count` from 60 to 62, and gave both a Syntax row in
`appearance_role_display.cpp`. It updated the role-display and settings-panel
courses to 60 offered roles and 28 in Syntax, but not this one, which still
asserts 58 listed of 60 declared and a Syntax count of 26.

## Decision

The list offers both roles, as it offers every other syntax role. The
RFC-BPR-0034 additive syntax amendment of 2026-10-12 says so: Namespaces and
Enum members join Syntax, the vocabulary is 62 roles, the card offers 60, and
Syntax holds 28. RFC-BPR-0072 asks for both as independent syntax colours
through the existing Colours controls, and the
`cpp-identifier-context-v1` checkpoint records "62 total, 60 offered, 28 in
Syntax". The owner confirmed on 2026-10-13: "it should offer those roles same
as others". The list is already right, so the course changes, not the list.

## Acceptance

- AC1: The Colours list offers Namespaces and Enum members in Syntax like the
  other syntax roles. It lists 60 of the 62 declared roles, and its Syntax
  header counts 28. Each new row has its name in words, a keyboard-reachable
  swatch and the theme's value. The two hunk-header roles are still not
  offered.
- AC2: `qt_colour_role_list` asserts that inventory, names both new roles, and
  passes. No stale 58-role count remains in the list's own documentation.
- AC3: The focused appearance Qt courses pass at 1.0x, 1.25x and 2.0x.
