---
schema: "implement-rfc.card.v1"
id: "BPR-1109"
title: "Windows: identify, read-only, the low-level keyboard hook that rewrites the Alt of every Alt chord on JaegAW"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"origin":"owner verdict on attended candidate BPR-1022-alt-chord-hook-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 7876410c-e657-44f8-be30-846db8115778, 2026-09-24","related_cards":["BPR-1022","BPR-1074"],"first_seen":"after 2026-09-23T22:47Z; seen at 9f144f1e (05:13Z-05:24Z) and 21332605 (06:46Z), 2026-09-24","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
---

# BPR-1109: Windows: identify, read-only, the low-level keyboard hook that rewrites the Alt of every Alt chord on JaegAW

## Why

[[BPR-1022]]'s real-key runs on bpr-main-win (`9f144f1e`, and again at
`21332605`) found that the Alt of every injected Alt chord reaches raw input as
a key with virtual-key code 0 (`vk=0x00`, `WM_KEYDOWN`), followed by the letter
as a plain `WM_KEYDOWN`. So Alt+Space, Alt+F, Alt+Up and Alt+F4 never reach any
application as chords. Alt alone and Alt with Ctrl pass through. Raw input runs
after the low-level keyboard hooks, so a `WH_KEYBOARD_LL` hook rewrites the Alt.
At 2026-09-23T22:47Z the same injection still opened the system menu on
Alt+Space, so the rewriting started after that.

Asked about it, the owner answered on 2026-09-24 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (candidate `BPR-1022-alt-chord-hook-v001`,
decision `7876410c`, 19:13:28Z), **select**, verbatim:

> I do not know what it is; have the Windows station identify it read-only,
> without stopping or changing it

The note is empty.

## Scope (read-only)

Name the process, and the module within it, that installed the hook, and when
it started, without stopping, unloading, reconfiguring or uninstalling
anything. Candidate sources, all read-only:

- The processes in the interactive session that started after 22:47Z on
  2026-09-23 and are still running: image path, signer, parent, command line
  and start time. Include input-related utilities, for example keyboard
  remappers, accessibility or automation tools, IME or language tools, remote
  desktop or streaming agents, game overlays, and the agents' own harnesses.
  This station's own automation is not ruled out in advance.
- Installed software and scheduled tasks or services changed since the last
  clean observation, from their install dates and event logs.
- The modules loaded into those processes that import `SetWindowsHookExW`.
  Where a supported read-only tool can enumerate the global hook chain, use it
  and name the tool.
- A reproduction of the symptom with the synthetic-only raw input sink
  BPR-1022 used, before and after the investigation, showing the hook is still
  in place and nothing changed.

## Acceptance

- AC1 The hook's owner is named: process, image path, signer, start time and
  the evidence that ties it to the rewriting. If it cannot be named
  read-only, the card names the remaining candidates and the evidence that
  rules out the rest.
- AC2 Nothing is stopped, unloaded, reconfigured or uninstalled, and no setting
  is changed. The symptom is observed unchanged at the start and at the end.
- AC3 The finding is reported to the owner. Only the owner decides what to do
  about the hook; this card does not.

## Boundaries

Read-only on the owner's interactive desktop. Only synthetic input from the
card's own harness, never the owner's typing, and no pointer movement. No
credential, keystroke content or private content goes into the record. When
chords reach applications again, BPR-1022's Alt+Space, Alt+F, Alt+Up and Alt+F4
rows are wanted, but they are that card's post-close runs, not this card's
acceptance.
