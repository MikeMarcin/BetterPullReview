---
schema: "implement-rfc.card.v1"
id: "BPR-0836"
title: "The designated-live capsule image build can leave a write-once device attached"
status: "In Progress"
rfcs: ["RFC-BPR-0016"]
depends_on: []
priority: "P3"
platforms: ["mac"]
metadata: {"found_by":"dsp-003 BPR-0814 slice","found_at":"c1a795c3ed724eb0086aa4661082793f9643a679","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0836: The designated-live capsule image build can leave a write-once device attached

## Defect

[[BPR-0814]] found that `hdiutil create`, while other image creates run on
the machine, sometimes exits 0 with its temporary write-once device still
attached, and the device stays attached (one was still there after 9.5
minutes). The image bytes are final. A later attach of that image fails at
once, with empty stderr under `-quiet`
(`build/r2-logs/dsp3-0814-probe-unfixed.log`,
`dsp3-0814-leftover-anatomy.log`, `dsp3-0814-leak-then-attach.log`).

BPR-0814 made the test fixture detach its own image after each create. The
production image command in `scripts/designated_live_macos_capsule.py`
runs the same `hdiutil create` and has the same exposure: it can print a
correct digest, exit 0, and leave a device attached, after which stage zero's
attach fails closed. That script's bytes are pinned by the structural gates.

## Scope

- Decide whether the production build should verify that its own image is
  not attached after create, and detach it (never forced, matched by exact
  image path), or report the leak as a refusal.
- Treat the change as security-relevant: update every pin the gates require
  and keep it minimal.

## Acceptance

- After a production image build, `hdiutil info` lists no attachment of
  that image, including when concurrent creates run.
- The capsule suite and the structural gates pass.
