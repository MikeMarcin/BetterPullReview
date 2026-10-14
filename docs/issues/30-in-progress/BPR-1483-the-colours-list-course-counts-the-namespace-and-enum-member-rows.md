---
schema: "implement-rfc.card.v1"
id: "BPR-1483"
title: "The Colours list course counts the namespace and enum-member rows"
status: "In Progress"
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
RFC-BPR-0034 additive syntax amendment of 2026-10-12 says so: the namespace
and enum-member colours join Syntax, the vocabulary is 62 roles, the card
offers 60, and Syntax holds 28. RFC-BPR-0072 asks for both as independent
syntax colours through the existing Colours controls, and the
`cpp-identifier-context-v1` checkpoint records "62 total, 60 offered, 28 in
Syntax". The owner confirmed on 2026-10-13: "it should offer those roles same
as others". The list already offered them, so the course changes to match.

The amendment worded the two rows "Namespaces" and "Enum members", while the
other 26 Syntax rows begin "Syntax", and the filter matches a row's words, so
a filter for "syntax" listed 26 of the 28. Asked whether to keep that wording
or rename the rows, the owner chose to rename them (2026-10-13). They now read
"Syntax namespace" and "Syntax enum member", and RFC-BPR-0034 records the
direction. Stable names, counts and order are unchanged.

## Acceptance

- AC1: The Colours list offers the namespace and enum-member colours in Syntax
  like the other syntax roles. It lists 60 of the 62 declared roles, and its
  Syntax header counts 28. Each new row has its name in words, a
  keyboard-reachable swatch and the theme's value, and the two rows end the
  Syntax group. The two hunk-header roles are still not offered.
- AC2: `qt_colour_role_list` asserts that inventory, names both new roles, and
  passes. No stale 58-role count remains in the list's own documentation.
- AC3: The two rows read "Syntax namespace" and "Syntax enum member", a filter
  for "syntax" lists all 28 Syntax rows and nothing else, and RFC-BPR-0034
  records the owner's direction.
- AC4: The focused appearance Qt courses pass as registered, and the two
  courses this card changes also pass at 1.0x, 1.25x and 2.0x.

## Evidence

Product `a5d8afec4`, merged with main as `b5faae22b`, macOS arm64,
2026-10-13, dev preset (Debug), targeted build.

```sh
ctest --test-dir build/dev -j1 -R '^(qt_(colour_role_list|appearance_role_display|appearance_settings_panel)|application_cpp_identifier_appearance)$' --output-on-failure
```

All four pass on `b5faae22b`. The course before this change, rebuilt on the
same tree, fails with the reported "58 of the 60 roles the core declares are
listed". The changed course passes, and asserts 60 of 62 listed, Syntax
28, both rows in Syntax under their new words, below Syntax other and above
Comment labels, and a "syntax" filter listing exactly the 28 Syntax rows with
namespace and enum member last.
`application_cpp_identifier_appearance` edits both roles through their
swatches on the composed panel and passes.

Each course binary was also run with `QT_QPA_PLATFORM=offscreen` and
`QT_SCALE_FACTOR` 1.0, 1.25 and 2.0. `qt_colour_role_list` and
`qt_appearance_role_display` pass at all three. `qt_appearance_settings_panel`
passes at 1.0 and 1.25. At 2.0 it fails at BPR-1354's "the popover sits
directly below the glyph, left edges aligned", because the panel is larger than
the offscreen screen at that scale and the popover is clamped. The same binary
built with the label file from before this change fails at the same line. That
course is registered only at the default scale.

`_coding-standard/scripts/format_check.py` passes the six changed C++ files
with clang-format 22.1.8. `_coding-standard/scripts/tidy.py` passes the four
changed translation units with no warnings, and `scripts/verify_documentation.py`
reports `DOCUMENTATION_OK`.

## Limits

- Run on macOS only. The change is two label strings, comments and test
  assertions, with no platform branch.
- No attended look: the Colours list's words come from the owner's direction
  in this session, and the course checks them as rendered labels.
