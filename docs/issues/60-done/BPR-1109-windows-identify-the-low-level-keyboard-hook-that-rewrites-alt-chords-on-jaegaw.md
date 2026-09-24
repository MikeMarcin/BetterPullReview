---
schema: "implement-rfc.card.v1"
id: "BPR-1109"
title: "Windows: identify, read-only, the low-level keyboard hook that rewrites the Alt of every Alt chord on JaegAW"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"origin":"owner verdict on attended candidate BPR-1022-alt-chord-hook-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 7876410c-e657-44f8-be30-846db8115778, 2026-09-24","related_cards":["BPR-1022","BPR-1074"],"first_seen":"after 2026-09-23T22:47Z; seen at 9f144f1e (05:13Z-05:24Z) and 21332605 (06:46Z), 2026-09-24","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
acceptance: [{"criterion":"AC1 The hook's owner is named; if it cannot be named read-only, the card names the remaining candidates and the evidence that rules out the rest","outcome":"pass","revision":"38507ec5","evidence":{"kind":"observation","detail":"Second branch. The hook rewrote back-to-back Alt chords at 05:13Z-06:46Z (BPR-1022 r4-r7) and 21:52Z-21:56Z (r8, r9). A WH_KEYBOARD_LL hook lives only with its installing thread on its desktop, so the owner is an interactive-session process running at both times. No still-running process started between the last clean observation (2026-09-23T22:47Z) and 05:13Z. Of those running, only code in msrdc 37336 (rdclientax.dll), ChatGPT 17592 and codex-computer-use-swift 21016, NVIDIA Overlay 10740, Everything 28156, Slack/Discord/Chrome, Visual Studio 20452 and OneDrive 18292, and the Windows shell imports SetWindowsHookExW (PE import scan of every loaded non-system module). Ruled out: station automation, exited processes, processes without the import, and the Claude desktop restarted at 19:55Z (started after 05:13Z; its predecessor only if the successor re-installed the same hook). r9 shows the hook keys on timing (same key 0x97, same application: back to back rewritten, 47-57 ms gap intact), not on the chord key.","path":"docs/issues/60-done/BPR-1109-windows-identify-the-low-level-keyboard-hook-that-rewrites-alt-chords-on-jaegaw.md#who-owns-it-ac1-not-nameable-read-only-candidates-narrowed"}},{"criterion":"AC2 Nothing is stopped, unloaded, reconfigured or uninstalled, and no setting is changed; the symptom is observed unchanged at the start and at the end","outcome":"pass","revision":"38507ec5","evidence":{"kind":"command","detail":"Read-only survey (process/module lists, PE import scan, event logs, signers); cleared foreground runs sent synthetic keys only: 21:48Z/21:49Z probes into the foreground Terminal (unassigned VK 0x97, 30 ms gaps), 21:52Z r8 BPR-1022 rows and 21:56Z r9 Alt+0x97 into pid-guarded test instances on fresh private data roots, closed with WM_CLOSE exit 0. A back-to-back Alt chord's Alt reaches raw input as vk=0x00 identically at the start baseline (BPR-1022 r7, 06:46Z) and at the end (r8 Alt+Space/F/Up/F4, r9 Alt+0x97). The 21:48Z gapped chords are explained by r9's timing contrast. Nothing stopped or changed.","path":"docs/issues/60-done/BPR-1109-windows-identify-the-low-level-keyboard-hook-that-rewrites-alt-chords-on-jaegaw.md#where-the-criteria-stand-2"}},{"criterion":"AC3 The finding is reported to the owner; only the owner decides what to do about the hook","outcome":"pass","revision":"38507ec5","evidence":{"kind":"observation","detail":"'For the owner (AC3)' states what the hook does, whom it affects (scripted and very fast chords; human-paced chords mostly pass), which applications' global-shortcut settings to check, and how the owner can identify it (quit one candidate at a time, then the station reruns the 1-minute check). The card changes nothing; the owner decides.","path":"docs/issues/60-done/BPR-1109-windows-identify-the-low-level-keyboard-hook-that-rewrites-alt-chords-on-jaegaw.md#for-the-owner-ac3"}}]
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

## The probe, 2026-09-24T21:48Z and 21:49Z (bpr-main-win, cleared foreground): the rewriting is not seen

The coordinator announced and cleared the probe, twice, with synthetic keys
only. This was after the Linux station's WSLg probe had ended at 20:44:34Z.

- **Before each run,** the foreground was Windows Terminal (pid 23164, class
  `CASCADIA_HOSTING_WINDOW_CLASS`). No `msrdc.exe` had a window.
- **After each run,** the foreground was unchanged.
- No pointer moved, and no window was activated.
- Nothing was stopped, unloaded or reconfigured.

Logs, not committed: `build/logs/bpr-1109-main-win/probe-{1,2}.txt` and the
`*-foreground.txt` files.

### The symptom, observed (facts)

Both runs sent five `Alt`+`0x97` chords (`0x97` is an unassigned virtual
key). Every one reached raw input as a normal Alt chord:

```
vk=0x12 make=0x38 down msg=0x104
vk=0x97 make=0x00 down msg=0x104
vk=0x97 make=0x00 up   msg=0x105
vk=0x12 make=0x38 up   msg=0x101
```

The Alt arrives as `VK_MENU`, and the chord key as `WM_SYSKEYDOWN`. There is
no `vk=0x00` key. The BPR-1022 r4–r7 sinks, at 05:20Z–06:46Z, recorded
`vk=0x00` in place of the Alt, and the letter as a plain `WM_KEYDOWN`. The
same sink, the same `keybd_event` injection and the same Alt scan code
(`0x38`) now show no rewriting.

So the symptom is **not** unchanged, and the card's premise has moved. Two
readings fit, and these runs cannot tell them apart:

1. **The hook is gone.** Whatever installed it has exited or stopped
   rewriting since 06:46Z.
2. **The hook is key-selective.** It rewrites only chords with keys it acts
   on. BPR-1022 tried Space, F, Up and F4, and never an unassigned key.

Reading 2 needs a hook that holds the Alt until the next key arrives. The r7
timings allow that. They do not prove it.

The process that restarted in between is the Claude desktop app, restarted
by its update at 19:55:56Z. Its native binding imports `SetWindowsHookExW`.
Every other hook-capable candidate in the survey table was still running
with the same pid and start time:

- `msrdc` 37336;
- `codex-computer-use-swift` 21016;
- `NVIDIA Overlay` 10740;
- `Everything` 28156.

So under reading 1, the likeliest owner is the earlier Claude desktop
instance, not `msrdc`. That is inference: its exit time and hooks are not
recorded.

### The thread attribution (inconclusive)

Each run tapped `0x97` 100 times into the foreground Terminal. That woke
hundreds of threads well beyond one wake-up per event:

- the kernel's `System` threads;
- the Terminal, `dwm` and `explorer`;
- WSL's `vmmemWSL`;
- Defender.

So a count of about 200 does not single out a low-level hook thread. Among
the hook-capable candidates, several rose again in both runs by far more than
their idle count, and raw input or window traffic could account for any of
them:

| Thread | Probe 1 | Probe 2 |
|---|---|---|
| NVIDIA Overlay 10740/9516 | +2327 | +1854 |
| `msrdc` 37336/36784 | +1099 | +795 |
| `codex-computer-use-swift` 21016/38168 | +472 | +349 |

Everything 28156 rose by about +220 on two threads in probe 1 only.

The method cannot separate a hook thread from a raw-input sink or a window
thread. It names no owner.

### Where the criteria stand

- **AC1: pending.** No owner can be named with confidence. The remaining
  candidates are:
  - the earlier Claude desktop instance, which exited at about 19:55Z (if
    the hook is gone);
  - `msrdc` 37336, the NVIDIA overlay, OpenAI's computer-use runtime, or
    Everything (if the hook is key-selective).
- **AC2: not met as written.** Nothing was stopped or changed. But the
  symptom was not observed unchanged. It is absent for `Alt`+`0x97` at both
  the start and the end of this run.
- **AC3.** Reported here. The owner decides.

**One run would settle it.** Drive [[BPR-1022]]'s real-key rows (Alt+Space,
Alt+F, Alt+Up) with the application in front, as that card's post-close run.
That needs its own foreground window.

- If those chords now reach the application, the hook is gone. BPR-1022's
  wanted rows pass, and this card can close as "no longer present;
  last-seen window 06:46Z–21:48Z".
- If they still fail, the hook is key-selective. The same sink with those
  keys then gives the next attribution step.

Those keys were not sent here: into the owner's Terminal they would open its
system menu, type into a shell or close it.

## BPR-1022's real-key rows with the application in front, 2026-09-24T21:52Z (cleared): the hook is still there

The coordinator announced and cleared this run.

- **The harness:** `fg-1022-realkeys.ps1`, tag `r8-bpr1109`, driving a test
  instance of `build/dev` at `38507ec5`. The executable's SHA-256 is
  `e7f0e336…`. The instance was pid 44652, on a fresh private data root that
  was removed afterwards.
- **Guards:**
  - Before the run, the foreground was Windows Terminal, and no `msrdc`
    window was showing.
  - Before every gesture, the harness confirmed by pid that the foreground
    was the application's own window, and would have aborted otherwise. It
    was the application's window at every step, and no key went elsewhere.
- **Afterwards:** the window was closed with `WM_CLOSE`, exit code 0, and
  no `better_pull_review` process was left. The foreground went back to
  Windows Terminal.
- **Recorded:** the synthetic-only raw input sink ran alongside. The logs
  are `build/logs/bpr-1109-main-win/realkeys-r8-*` and
  `build/logs/bpr-1022-main-win/realkeys-r8-bpr1109.json`. They are not
  committed.

### Which keys fail (facts)

| Chord | Raw input | At the application |
|---|---|---|
| Alt+Space | `vk=0x00` down, Space `WM_KEYDOWN`, Space up, `vk=0x00` up | no system menu (FAIL) |
| Alt+F | `vk=0x00` in place of the Alt, F `WM_KEYDOWN` | nothing opens (the row passes, but only because the Alt never arrived) |
| Alt+Up | `vk=0x00` in place of the Alt, Up `WM_KEYDOWN` | nothing opens (the same) |
| Alt+F4 | `vk=0x00` in place of the Alt, F4 `WM_KEYDOWN` | the window stays open (FAIL) |
| Alt alone, left or right | `VK_MENU`, `WM_SYSKEYDOWN` | the application menu opens |
| Ctrl with right Alt (AltGr), alone or with Q | `VK_CONTROL`, then `VK_MENU` E0 | nothing opens (pass) |

This is the r4–r7 pattern of 05:20Z–06:46Z, unchanged. So at the end of
this investigation the symptom is exactly what it was at the start.

The "File highlighted" rows fail in the same way r6 recorded, and those
failures are not this card's: the harness lists the application menu's
items without File.

### What it settles, and what it does not

- **The hook is not gone.** So the Claude desktop instance that exited at
  about 19:55Z is not the owner, unless its restarted successor installed
  the same hook again.
- **The rewriting is selective.**
  - Alt+`0x97` passed at 21:48Z and 21:49Z, with Windows Terminal in front.
  - Alt+Space, F, Up and F4 were rewritten at 21:52Z, with the application
    in front.
  - Two things changed between those runs: the chord key, and the
    foreground process. These runs cannot say which one the hook keys on.
- **AC1: still pending.** The candidates are the long-running hook-capable
  processes in the survey table:
  - `msrdc` 37336;
  - `ChatGPT.exe` and its computer-use runtime (21016);
  - the NVIDIA overlay 10740;
  - Everything 28156;
  - the restarted Claude desktop app.
  None of them was stopped to test it, and none will be without the owner.
- **AC2: now observed.** The rewriting of Alt+Space, F, Up and F4 is the same
  at the start (BPR-1022 r7, 06:46Z) and at the end (r8, 21:52Z). Nothing was
  stopped or changed.

### The next attribution step

One more cleared run, of about 1 minute, with the same guards and the
application in front:

- Send Alt+`0x97`, which is harmless in the application, and read the raw
  input sink.
  - If it passes there too, the hook keys on the chord key, not on the
    foreground process.
  - If it is rewritten, the hook keys on the foreground process.
- Either answer narrows the list:
  - A per-key hook acting on Alt+Space in every application behaves like a
    launcher or quick-entry shortcut. Inference: that points at the chat
    and launcher apps' global shortcuts.
  - A per-application hook points at a tool with per-window rules.

After that, the owner can check those applications' shortcut settings. Only
the owner should change them.

## Alt+0x97 with the application in front, 2026-09-24T21:56Z (cleared, the last run): the hook keys on timing

The coordinator announced and cleared this run, the last one of this
segment.

- **The harness:** `fg-1109-alt97.ps1`, BPR-1022's harness with its gestures
  replaced by Alt+`0x97` only. Tag `r9-alt97`.
- **The instance:** a test instance of `build/dev` at `38507ec5` (pid 16936),
  on a fresh private data root that was removed afterwards.
- **Guards:**
  - Before the run, Windows Terminal was in front, and no `msrdc` window
    was showing.
  - Before every gesture, the harness confirmed by pid that the application's
    window was in front. It was, every time.
  - No key went to the Terminal.
- **Afterwards:** the window was closed with `WM_CLOSE`, exit code 0, and
  no `better_pull_review` process was left. The foreground went back to the
  Terminal.
- **Logs:** `build/logs/bpr-1109-main-win/alt97-r9-*`.

| Gesture, same key, same application in front | Raw input |
|---|---|
| Alt+`0x97`, back to back (as r8's chords), 3 of 3 | `vk=0x00` down, `0x97` `WM_KEYDOWN`, `0x97` up, `vk=0x00` up: **rewritten** |
| Alt, 30 ms, `0x97`, 30 ms, Alt up, 3 of 3 | `VK_MENU` `WM_SYSKEYDOWN`, then `0x97` `WM_SYSKEYDOWN` 47–57 ms later: **intact** |

### What the hook keys on (facts, then inference)

- **Not on the chord key.** An unassigned key is rewritten like Space, F, Up
  and F4 when it follows the Alt at once.
- **Timing matters.** The same key, into the same application, is rewritten
  back to back and passes when it follows the Alt by about 47–57 ms. That
  explains the 21:48Z probe, whose chords had the same gaps.
- **The foreground application is not shown to matter.** Chords sent into
  the Terminal all had gaps, so they cannot test it.
- **Not timing alone, at least earlier in the day.** BPR-1022's paced run r3
  (05:18Z, 50 ms between events, no raw sink) also delivered Up and F4
  without Alt. The Qt log shows `Key_Up` and `Key_F4` with no modifier. So
  either the rule includes more than the gap, or it changed between 05:18Z
  and 21:56Z. That stays open.

**Inference:** a low-level hook that holds each Alt press briefly. If another
key follows within its window, it swallows the Alt and puts a
virtual-key-0 key in its place. Otherwise it lets the Alt through. Launcher
and overlay tools do this to tell an Alt tap or a shortcut of their own from
a chord. The owner's own typing, which rarely puts less than about 30 ms
between Alt and the next key, would mostly not notice it. Scripted input and
fast chords do.

### Who owns it (AC1): not nameable read-only, candidates narrowed

A `WH_KEYBOARD_LL` hook lives only as long as the thread that installed it,
and only on its own desktop. It rewrote chords at 05:13Z–06:46Z, and again at
21:52Z–21:56Z. So the owner is an interactive-session process that was
running at both times. No such process started between the last clean
observation (2026-09-23T22:47Z) and 05:13Z. So the owner was already running
at 22:47Z and began rewriting after that.

Among those processes, only these load code that imports `SetWindowsHookExW`
(from the import scan above):

- `msrdc.exe` 37336, WSLg's Remote Desktop client (`rdclientax.dll`), since
  2026-09-18;
- `ChatGPT.exe` (OpenAI Codex app) 17592 and its `codex-computer-use-swift.exe`
  21016, since 2026-09-21;
- `NVIDIA Overlay.exe` 10740, since 2026-09-17;
- `Everything.exe` 28156, since 2026-09-17;
- Slack 28624 (`slackdesktoputils.node`), Discord and Chrome, since
  2026-09-17;
- Visual Studio 20452 (`msenv.dll`, Visual Assist `VA_X64.dll`) and OneDrive
  18292;
- the Windows shell: `explorer.exe`, the taskbar and the text input host.

Ruled out:

- The station's own automation. The launchers load no `ctypes`, and the
  harnesses only inject.
- Every process that has exited.
- Every process without the hook import.
- The Claude desktop app restarted at 19:55Z. It started after the 05:13Z
  observation. Its earlier instance could have owned the morning's hook only
  if the restarted one installed the same hook again, and nothing records
  that.

Which of the candidates holds the hook cannot be told without stopping one.
Nothing was stopped, by the card's rule.

### For the owner (AC3)

**What it is.** Something on JaegAW intercepts an Alt press that is followed
at once by another key. It replaces the Alt with a key code of 0, so the
application sees the letter without Alt. It does not change Alt pressed
alone, AltGr, or chords typed with a normal human gap, as far as measured
today.

**What to check.** Only the owner changes any of these:

- the global shortcuts of the ChatGPT or Codex desktop app (its launcher is
  commonly Alt+Space) and of its computer-use feature;
- the NVIDIA overlay's hotkeys (Alt+Z and others);
- Everything's global hotkeys;
- Slack's and Discord's global shortcuts;
- WSLg's Remote Desktop client, which exists only while WSL runs.

**How to find the owner.** Quit one candidate at a time, then run the
1-minute check again (BPR-1022's harness with a back-to-back Alt+Space). The
check is the station's; stopping the applications is the owner's.

### Where the criteria stand

- **AC1: met by its second branch.** The owner cannot be named read-only.
  The remaining candidates are listed above, with the evidence that rules out
  the rest.
- **AC2: met.** Nothing was stopped, unloaded, reconfigured or uninstalled,
  and no setting was changed. A back-to-back Alt chord was rewritten, in the
  same way, at the start (BPR-1022 r7, 06:46Z, the baseline this card cites)
  and at the end (r8 21:52Z and r9 21:56Z). The gapped chords of 21:48Z are
  explained by the timing rule above, so they are not a change.
- **AC3: reported here.** What to do about the hook is the owner's decision.
- **Open, not a criterion:** why r3's 50 ms paced chords were rewritten at
  05:18Z while 47–57 ms gaps pass at 21:56Z.
