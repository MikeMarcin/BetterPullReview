---
schema: "implement-rfc.card.v1"
id: "BPR-1514"
title: "Prevent Add CLI reentry while Home loads a CLI connection"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Home while loading cli shows the new user add buttons and gets into a weird state if you try to add cli while it's loading cli"}
---

# BPR-1514: Prevent Add CLI reentry while Home loads a CLI connection

While Home is loading a CLI connection, it currently displays the new-user Add controls. Trying Add CLI during that load can leave Home in an inconsistent state. Project an accurate loading state and prevent conflicting connection enrollment from racing the in-flight load.

## Acceptance

- AC1: Reproduce the reported sequence with controlled delayed CLI connection loading and record the actual conflicting transitions.
- AC2: Home represents CLI connection loading accurately, without presenting new-user Add controls as though initial connection loading had finished.
- AC3: Repeated Add CLI gestures or queued connection intents during loading cannot corrupt connection selection, account identity or Home state. Core policy and host admission enforce this beyond widget disabling.
- AC4: Successful, failed and cancelled loads reach coherent recoverable states; retry/add works when appropriate. Late completions cannot replace a newer authority.
- AC5: Verify a deterministic delayed composed startup/CLI path, reentry, failure and recovery, plus an offscreen rendering of the loading state. Do not switch global gh accounts or mutate remote data.

## Coordination

Read remembered-connection and CLI authority contracts and relevant RFCs before edits. Coordinate Home layout overlap with BPR-1505. Load frontend-design:frontend-design before any UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Required human judgement uses short attended review and an exact frozen candidate.
