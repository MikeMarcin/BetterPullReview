---
schema: "implement-rfc.card.v1"
id: "BPR-1742"
title: "Dispose of PCRE2's JIT-stack advisory for the Qt 6.12.0 releases"
status: "Inbox"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1742--BPR-1743--04a7cd1de032.json","found_by":"RFC-0038 security-audit port, 2026-10-22","found_at":"4e318b625","related_cards":["BPR-0962"]}
---

# BPR-1742: Dispose of PCRE2's JIT-stack advisory for the Qt 6.12.0 releases

## Finding

PCRE2 [GHSA-r9hj-j2rw-4q3m](https://github.com/PCRE2Project/pcre2/security/advisories/GHSA-r9hj-j2rw-4q3m)
(`CVE-2026-103111`) is fixed in PCRE2 10.49. Grype's feed does not report it,
so it never appears as a scanner row.

The pinned Qt 6.12.0 source (`qtbase-everywhere-src-6.12.0.tar.xz`,
`a951bd16…1307`) bundles PCRE2 10.48. Its JIT stack-growth stub in
`pcre2_jit_compile.c` is the one the 2026-10-16 follow-up found unfixed.
Qt's x86_64 and Windows paths assign a growable 32–512 KiB JIT stack and
retry, which is the mechanism the advisory affects.

On Qt 6.11.2, v0.3.0's independent `dd6` adoption disposed of it as
"reviewed product attacker-pattern exclusion, no arbitrary extension
immunity". No v0.3.1 or v0.3.2 record carries that to Qt 6.12.0. The v0.3.1
reviews covered the raw rows, and the PCRE2 rows had dropped out with 10.48.
See the [RFC-BPR-0038 ledger](../../verification/RFC-BPR-0038.md#which-02x-conclusions-still-describe-current-rows).

The owner's 2026-10-16 known-issues deferral named this advisory, but only
for Qt 6.11.2 and the 0.2.x packages. Its recheck trigger fired when Qt
6.12.0 was selected
([release policy](../../security/release-policy.md#owner-decision-on-known-upstream-qt-issues-2026-10-16)).

## Acceptance

- AC1: An independent reviewer records a decision on this advisory for each
  current exact artifact: v0.3.1 Mac, Windows and Ubuntu, the v0.3.2 Mac
  prerelease, and any later Qt 6.12.x candidate. The decision states each
  architecture's JIT status and the pattern-origin bound it relies on, and
  carries a recheck trigger. Insufficient evidence stays pending.
- AC2: The owner answers one Accept/Reject question in a short attended
  session: whether the 2026-10-16 known-issue deferral extends to this
  advisory and to Windows bundled zlib `CVE-2026-85091` on Qt 6.12.0.
- AC3: The next release's security record names this advisory explicitly,
  rather than relying on the raw scanner rows.

## Boundary

No Qt fork or private patch. A fix arrives through an official Qt release.
This card grants no release, trust or security-owner approval.

## Owner decision, 2026-10-23

AC2 is answered in chat: "Yes that decision carries over to 6.12.0 and
continues to carry over until Qt releases the fix." The 2026-10-16 known-issue
deferral covers this advisory and Windows bundled zlib `CVE-2026-85091` on Qt
6.12.0 and on every later Qt release that still carries them, until an
official Qt release ships the fix. The [release policy](../../security/release-policy.md#owner-decision-on-known-upstream-qt-issues-2026-10-16)
records it. AC1 (an independent reviewer's per-artifact decision) and AC3 (the
next release's security record naming the advisory) remain open; this grants
no release or security-owner approval.
