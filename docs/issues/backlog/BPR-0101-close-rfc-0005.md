---
id: BPR-0101
title: Verify and close RFC-BPR-0005
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0090, BPR-0091, BPR-0092, BPR-0093, BPR-0094, BPR-0095, BPR-0096, BPR-0097, BPR-0098, BPR-0099, BPR-0100, BPR-0189, BPR-0190, BPR-0191]
---

# Verify and close RFC-BPR-0005

Replay the complete RFC-BPR-0005 claim matrix from clean state, resolve every documentation-ledger row, perform a skeptical final diff review, and obtain an independent review pass.

This card cannot mark RFC-BPR-0005 `Auto-Verified`. Closure is strictly ordered and RFC-BPR-0004, RFC-BPR-0003, and RFC-BPR-0002 are all still `Proposed` pending real Windows x64 evidence in BPR-0036 and BPR-0053. It also depends on the owner-attended BPR-0100.

It stays in `backlog/` and makes no `Auto-Verified` claim.

## Three composition cards were added after this card was written

BPR-0188 measured that the application does not contain `workbench_window`,
`workbench_drawer`, or `workbench_application_host`, and that neither
`workbench_settings_port` nor `workbench_observation_port` has an adapter. The
shell this RFC is named for has never been on screen.

When this card is eventually prepared, replay against the **composed
application**. Replaying the component suites would reproduce exactly the gap
BPR-0188 found: they all pass today, against a window no user can open.

Ask the linker as part of the replay. `nm` on the shipped binary reporting
symbols for every component this RFC specifies is a second's work and is the
check that no amount of green component testing substitutes for.
