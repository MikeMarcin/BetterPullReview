---
id: BPR-0113
title: Simplify the RFC-BPR-0006 change set
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0112]
---

# Simplify the RFC-BPR-0006 change set

Run a conservative simplification pass over the RFC-BPR-0006 change set, preserving behaviour and public semantics. Record an explicit no-change result if nothing is safe to improve.

## One real duplication found and removed

The Qt appearance adapter carried its own complete copy of the sRGB
linearisation, the three luminance coefficients, and the contrast offset —
nineteen lines reimplementing exactly what the core's contrast module owns.

The cause is ordering rather than carelessness. BPR-0106 landed the Qt profiles
before BPR-0111 added contrast to the core, so when the Qt side needed a
readability comparison the core version did not yet exist. Once it did, nothing
went back to remove the copy. That is precisely the residue a simplification
pass exists to find.

Leaving it would have been more than untidy. The Qt copy feeds a readability
heuristic that chooses between a text colour and its inverse, and the core copy
feeds the audited accessibility checks. A corrected formula or an adjusted
offset in one would leave the two silently disagreeing, so the shipped
readability decision could diverge from the measurements said to validate it.

`contrast_ratio` now delegates to `core::contrast_ratio_hundredths` and the
duplicated constants and helper are deleted, with the reason recorded at the
call site. The now-unused `relative_luminance` declaration is removed from the
public header rather than left as a second entry point into the same idea.

## Behaviour preserved

- The full Qt suite passes 25/25 and the Qt-free core suite 39/39, including
  the appearance-profile cases that measure contrast through the delegated
  path. Those cases pass unchanged, which is what makes this a simplification
  rather than a behaviour change.
- Both architecture boundary suites, the format gate, and strict `clang-tidy`
  with `--warnings-as-errors` pass.

## Other smells checked and absent

Broad service access: no `QNetworkAccessManager`, `QProcess`, or
`QSqlDatabase` reference in the syntax adapter or the appearance profile.
Whole-state refreshes: no `beginResetModel` in the appearance host or profile.
Unnecessary serialisation: none introduced; every boundary is a typed in-process
call.

Duplicated policy elsewhere: the Qt adapter's three `bounds_of` calls are *uses*
of the core's published bounds rather than a restatement, which is the intended
direction. The role-vocabulary references in the contrast module and the syntax
adapter are consumers of the closed enumeration, not second copies of it.

## Result

The one genuine duplication in the RFC-BPR-0006 change set — a second
implementation of the contrast formula that could silently drift from the
audited one — is removed, with behaviour proven unchanged by the existing
suites. BPR-0113 is complete.
