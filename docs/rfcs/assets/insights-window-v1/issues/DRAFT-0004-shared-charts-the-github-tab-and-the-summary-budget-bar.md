---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "Shared charts, the GitHub tab and the Summary budget bar"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0003"]
platforms: ["mac", "win", "linux"]
---

# Shared charts, the GitHub tab and the Summary budget bar

## Scope and acceptance

Implement RFC-DRAFT-0024 "Charts" (AC02) as a shared Qt chart component,
then the GitHub tab (AC04) and the Summary budget bar and Problems tile (AC03).

- Lanes, maximum labels, the foreground/background stack, gaps, hover with
  hysteresis, edge flipping, theme roles and accessible descriptions. The
  calendar mode is added by whichever of DRAFT-0006 and DRAFT-0008 lands
  first.
- Load `frontend-design:frontend-design` first. The owner compares the charts
  with AllTheThings' Energy chart in an attended review.
