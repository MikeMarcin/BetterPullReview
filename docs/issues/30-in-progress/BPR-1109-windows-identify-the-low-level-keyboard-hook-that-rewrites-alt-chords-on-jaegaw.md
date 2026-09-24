---
schema: "implement-rfc.card.v1"
id: "BPR-1109"
title: "Windows: identify, read-only, the low-level keyboard hook that rewrites the Alt of every Alt chord on JaegAW"
status: "In Progress"
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

## Station record, 2026-09-24 (bpr-main-win): the read-only survey names candidates, not yet the owner

Run `bpr-main-win-20260923`, main at `657ca3bb`.

- No input was injected and no pointer moved.
- No process was stopped, unloaded or reconfigured, and no setting changed.
- Logs and scripts are under `build/logs/bpr-1109-main-win/` on the station
  and are not committed.
- The two steps that need synthetic keys on the desktop are prepared. They
  wait for a foreground window (below).

### What the survey shows (facts)

**Nothing still running started in the onset window.** The whole session-1
process inventory (`Win32_Process`: name, pid, parent, creation time, image
path; no command lines) shows no process started between the last good
observation (2026-09-23T22:47Z) and the first bad one (2026-09-24T05:13Z)
that is still running. The only later starts are:

- the OneDrive sync service, 09:50Z;
- Codex's `node`/`node_repl` children, 18:50Z;
- the Claude desktop app, 19:55:56Z. It was updated at 19:45Z.

So the rewriting comes from one of two kinds of process:

- one that was already running at 22:47Z and changed state after it;
- one that has exited since. In that case the symptom may be gone, which
  the probe below would show.

**No supported read-only tool lists the global hook chain.** WinDbg's
`!hooks` needs kernel debugging, and turning that on is a boot
configuration change, so it is outside this card.

**Hook capability, by import scan.** A Python PE reader went over every
non-system module loaded in the session's processes, delay-loads included.
These import `SetWindowsHookExW`, and so can install a low-level keyboard
hook:

| Process (pid, started UTC) | Module | Signer | Also imports |
|---|---|---|---|
| `msrdc.exe` (37336, 2026-09-18 03:16:27; parent `wslhost.exe`), WSLg's Remote Desktop client, 1.2.7214.0 | `C:\Program Files\WSL\rdclientax.dll` | Microsoft Corporation | `keybd_event`, `SendInput`, `RegisterRawInputDevices`, `RegisterHotKey` |
| `codex-computer-use-swift.exe` (21016, 2026-09-21 05:19:16), OpenAI's computer-use runtime under the ChatGPT/Codex app | the executable | OpenAI OpCo | `SendInput`, `SetWinEventHook` |
| `NVIDIA Overlay.exe` (10740, 2026-09-17 02:58:14), 128.4.13.31 | the executable | NVIDIA Corporation | `RegisterRawInputDevices` |
| `Everything.exe` (28156, 2026-09-17 02:59:51), 1.5.0.1422b | the executable | voidtools | `RegisterHotKey` |
| `Slack.exe` (28624, 2026-09-17 02:59:54) | `slackdesktoputils.node` | Slack Technologies | `SendInput` |
| Chromium-based apps: Chrome, ChatGPT, Discord, Claude | `chrome.dll`, `Discord.exe`, `claude.exe` (delay-loaded) | their vendors | `SendInput`, `RegisterHotKey` |
| `claude.exe` desktop (44372, 2026-09-24 19:55:56) | `claude-native-binding.node` | Anthropic | `SendInput` |
| Windows and Visual Studio | `explorer.exe`, `tiptsf.dll`, `msenv.dll`, `VA_X64.dll` and others | Microsoft, Whole Tomato | — |

**The station's own automation is ruled out.**

- The station launchers (python 31160, 46792 and 32596) load no `_ctypes`,
  so they cannot call the hook API.
- The owner's terminals (powershell 43632 and 26420) carry no `Add-Type`
  assembly.
- The Claude Code CLI (`claude.exe` 32668 and 36124) imports no hook API.
- The BPR-1022 harnesses only call `keybd_event`, the same way at 22:47Z,
  when chords worked, as at 05:13Z and later, when they did not.

**What changed in the onset window.** From the System, Application,
AppX-deployment, Windows Update and RDP-client logs, 22:00Z–07:00Z:

- The display went off at 22:57:30Z (Kernel-Power 566, reason 12, idle). It
  came back on at 05:13:36Z (reason 31), woken by the station's own injected
  keys.
- An unannounced WSLg window was on this desktop from 23:14:59Z to about
  23:17Z ([[BPR-1022]], the integrator's report). `msrdc.exe` draws every
  WSLg window.
- The Python Manager package updated at 04:55:56Z.
- Two Defender signature updates.
- WER re-queued the same batch of GPU live-kernel reports at 00:52Z and
  04:34Z.
- No software install, driver change, service change or RDP-client event is
  logged.

**The symptom's shape.** BPR-1022's r7 sink shows it; nothing new was
recorded here. In a chord, the Alt arrives as `vk=0x00 make=0x00` down
before the chord key and `vk=0x00` up after it. Alt alone, and Alt held with
Ctrl, pass through. A hook that swallows the Alt and injects a key with
virtual-key code 0 in its place would produce exactly this.

### The most likely owner (inference)

**`msrdc.exe`, pid 37336** (`C:\Program Files\WSL\msrdc.exe`, WSL's Remote
Desktop client, signed by Microsoft, running since 2026-09-18T03:16:27Z).
Confidence: **low to moderate.**

For it:

- Its `rdclientax.dll` can both hook the keyboard and inject keys.
- Remote Desktop clients hook the keyboard to route key combinations to the
  remote session. That is general knowledge, not verified here.
- It is the only candidate tied to a recorded desktop event inside the onset
  window: the 23:14–23:17Z WSLg window.

Against it:

- It was already running at 22:47Z, and its RDP settings (`wslg.rdp`) set no
  `keyboardhook` value.
- The link is a correlation plus a capability, not an observation of the
  hook.

Next in likelihood:

1. OpenAI's computer-use runtime: an input-automation process that can hook
   and inject, and is active today.
2. The NVIDIA overlay, whose own hotkeys are Alt chords.
3. An earlier Claude desktop instance, if one ran in the window. That is not
   recorded.

The rest have hook capability but no link to the window.

### What remains, and needs the foreground

`C:\Users\mikem\bpr-main-helpers\bpr1109\fg-1109-hookprobe.ps1` is a station
helper. It is syntax-checked, has not been run, and is not committed.

- It injects only synthetic keys: 100 taps of the unassigned virtual key
  `0x97`, then five `Alt+0x97` chords, into whatever window is in front. An
  unassigned key is ignored there, and the chords open no menu.
- It moves no pointer and activates no window.
- It reads every thread's context-switch count before and after the taps,
  against an equal idle interval. Low-level hook threads are woken for every
  key event, so each shows about 200 extra wake-ups. That names the process
  and thread holding each low-level keyboard hook without touching them.
- A synthetic-only raw input sink shows whether the Alt of each chord still
  arrives as `vk=0x00`.

It takes about 20 seconds. Run it once to observe the symptom and attribute
the hook, then once more at the end (AC2). Each thread-counter read takes
about 5.5 s (6,753 threads), which is already counted in the 20 seconds.

### Where the criteria stand

- **AC1 pending.** The candidates and the evidence are named above. The
  owner is not pinned. The probe would pin it, or rule out the rest.
- **AC2 pending.** The symptom has not been observed at the start or the end.
  That needs the probe's foreground window. Nothing has been stopped or
  changed.
- **AC3.** This record is the report to the owner. What to do about the hook
  is the owner's decision.
