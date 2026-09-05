---
schema: "implement-rfc.card.v1"
id: "BPR-0112"
title: "Reconcile RFC-BPR-0006 documentation impact"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0111"]
---

# Reconcile RFC-BPR-0006 documentation impact

Update the appearance and accessibility document, the workbench specification, the dependency inventory and build guide for the new acquisition, the fixture catalog, and the RFC-BPR-0006 ledger.

Record the acquisition decision and its reasoning, not just its outcome, so a later reader knows why caller-installed discovery was not used.

## Changed-path classification

The RFC-BPR-0006 range touches 40 non-tracker paths: six core, four
application, four Qt adapter, three syntax adapter, two CMake provider files,
one provenance document, and the tests and fixtures for each. Every path routes
to an owning instruction file.

## Dispositions

| Document | Disposition |
|---|---|
| Appearance and accessibility | Updated. Appearance modes are now described as implemented rather than planned, with the recommend-versus-override rule, the complete-blob projection and why it must not become a per-hunk call, the theme-absent cache key, and the measured contrast thresholds and enumerated pairs |
| System architecture | Updated. The technology baseline names the pinned KSyntaxHighlighting and ECM versions and the Qt 6.9 floor; a new section allocates appearance across core, host, syntax adapter, and Qt, and records that the Qt-free preset acquires neither |
| Build and test guide | Updated. The dependency boundary records the acquisition route and why caller-installed discovery was unavailable, the digest scheme, and the provenance location; focused commands are added for all five new suites |
| Fixture catalog | Updated. The three core suites, the syntax contract and its six language fixtures, and what each pins |
| RFC-BPR-0006 ledger | Updated. Every claim row carries its own result; only the owner-attended scheme toggle stays deferred |
| Third-party provenance | Written during BPR-0109 |

## A repeated mistake, caught and corrected

The claim matrix was first updated by replacing every occurrence of a
`Planned, BPR-XXXX` marker with one result string. Three rows shared the
`BPR-0110` marker, so all three received the definition-selection evidence: the
hunk-projection row and the theme-reparse row silently acquired another row's
proof.

This is the same error made in BPR-0098, and it recurred because the fix there
was applied to that document rather than to the method. A claim matrix whose
cells are individually plausible but collectively misattributed is materially
harder to catch than an empty one — every row reads as evidenced. It was found
by extracting the result column alone and reading the ten values in sequence,
which makes duplicates obvious in a way that reading the wide table does not.
Each row now states the evidence that belongs to it.

## What the documentation now records that it did not

The acquisition decision is recorded with its reasoning, not just its outcome:
caller-installed discovery was investigated first and is unavailable on this
platform, so verified CPM source is the only route that satisfies the RFC
rather than deferring it. A later reader asking "why not find_package" has the
answer.

The complete-blob projection is documented with an explicit instruction not to
reduce it to a per-hunk call, because that reduction passes casual testing and
fails exactly the criterion it exists for.

The span cache is documented as keyed without the theme, so the guarantee is
visible as structural rather than as a behaviour someone must remember to
preserve.

## Direct verification

- `DOCUMENTATION_OK`; every claim row resolved; the deferred row names its
  owner-attended card.

## Result

Every RFC-BPR-0006 documentation obligation is discharged, with the acquisition
reasoning, the projection constraint, and the cache-key guarantee recorded as
reasons rather than conclusions. BPR-0112 is complete.
