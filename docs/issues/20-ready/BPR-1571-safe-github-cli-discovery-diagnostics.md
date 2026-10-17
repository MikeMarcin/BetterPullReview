---
schema: "implement-rfc.card.v1"
id: "BPR-1571"
title: "Explain GitHub CLI discovery failures without exposing child output"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved Intel auth diagnosis and local-main integration 2026-10-17","governing_rfc":"RFC-BPR-0015","related_rfcs":["RFC-BPR-0078"],"reservation":"docs/issues/_reservations/BPR-1571--BPR-1571--a439bf305082.json"}
---

# BPR-1571: Safe GitHub CLI discovery diagnostics

Account discovery previously collapsed tool failures into a generic process
message and confused a completed inventory with no eligible account with an
empty inventory. The owner approved landing the missing diagnostics, actionable
messages and regression coverage on local main. No release or credential-access
redesign is authorized by this slice.

## Acceptance

- AC1: Discovery logs only fixed stage/failure categories, elapsed time,
  observed exit and stream byte counts; no child output, inventory, paths,
  arguments, account names or credential environment values.
- AC2: Missing tool, launch failure, timeout, nonzero exit, rejected output,
  malformed inventory and nonempty-but-ineligible inventory remain distinct;
  safe status text is selectable and discovery can be retried.
- AC3: Fixtures verify error content stays unavailable while counts survive,
  exit-zero inventory does not imply account eligibility, and the actual
  discovery completion keeps an inaccessible account signed out.
- AC4: Compatibility documentation distinguishes the observed macOS 15.1.1
  failure and post-update/reboot 15.8.1 success, with unchanged BPR/gh versions,
  from a proven Apple cause, universal qualification or a minimum fixed version.

## Scope

The unchanged sandbox and strict chosen-account credential path remain in force.
No credential, Keychain ACL, native broker, installed package or Intel developer
update changes. No generated binaries, private evidence or raw logs are committed.

## Verification

Pending focused M4-local process, parser, Qt and composed fixture checks,
format and documentation gates. Intel native observations are bounded context,
not a repeatable automated qualification test.
