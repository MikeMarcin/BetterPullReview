---
schema: "implement-rfc.card.v1"
id: "BPR-0886"
title: "Windows: the agent transport writes the whole request before draining any child output, so a chatty child stalls to timeout"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable agent partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0886: Windows: the agent transport writes the whole request before draining any child output, so a chatty child stalls to timeout

## Defect

`src/adapters/agent/src/agent_protocol_loop.cpp:37` writes `Request.Input`
first. On Windows, `write_all`
(`src/adapters/agent/src/windows_agent_process.cpp:329-392`) blocks in
synchronous `WriteFile` until every byte is accepted or the deadline fires,
and **nothing reads stdout or stderr while it does**. `make_pipe` passes 0 for
the buffer size (`:299`), so the anonymous pipe takes the default, commonly
4 KiB.

A child that writes more than that before it finishes reading stdin therefore
fills its output pipe, stops, and never drains the input the parent is still
writing. Both sides block and the exchange ends at `timeout` — the classic
pipe deadlock, and it presents as an agent that is simply slow and then fails.

The Unix path does not have this shape: `QProcess::write` buffers and
`waitForReadyRead` services both directions
(`src/adapters/agent/src/agent_process_launcher.cpp:218-227, 252-271`).

Whether the shipped agents trigger it is **not established** — the reviewer
was explicit about that, and it is the first thing to find out. A chatty agent
is not unusual, though, and a child that greets on stderr before reading its
request would be enough.

## Scope

- Establish whether any shipped agent reaches the threshold, before changing
  the transport. A contract with a child that writes 8 KiB to stderr before
  reading stdin settles both the question and the fix.
- Then make the Windows path service both directions while writing:
  overlapped I/O, a reader thread, or interleaved partial writes with polled
  reads. Match the Unix path's behaviour rather than inventing a second
  model.
- Consider whether an explicit pipe buffer size is worth setting as well; it
  raises the threshold but does not remove the deadlock, so it is mitigation,
  not the fix.

## Acceptance

- A child that emits more than the pipe buffer before reading its input
  completes, proven by a contract row on Windows.
- The card records whether any shipped agent was affected.
- macOS and Linux agent behaviour is unchanged.

## Reproduced, fixed and proven on Windows (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2), at
`585d8846` plus this change, with the row contained as it is registered. The
logs are station records under `build/logs/bpr-0886-main-win/` and are not
committed.

**The contract row.** `qt_agent_launch_canary` gains
`test_output_before_input_does_not_stall_the_request`. The canary's new
`--stderr-before-input` writes 64 KiB to stderr before it reads anything. The
request carries a 12 KiB draft, the one part of a request a reviewer can make
large, so it is 13,048 bytes: more than the input pipe holds. It runs for
`codex_jsonl_exec`, `claude_stream_json` and `external_terminal`, with a
20 s timeout, and asserts four things:

- the run succeeds, exit 0;
- it finishes well inside the timeout (under 10 s);
- the child's stderr is kept whole and in order;
- the child received the request byte for byte, by its length and an FNV-1a
  digest the canary reports.

The canary reports a digest rather than echoing the request. Its paced writer
sends three bytes a millisecond, so echoing 13 KB and its hex would reach the
timeout by itself. The first version of this test echoed it, and it timed out
even with the fix. A trace showed the write itself completing: every parked
byte was drained, and the rest was the echo.

**Before the fix, the row is the deadlock the card describes.** With the
transport as it was at `585d8846`, the row ends at the timeout:
`BPR-0886 protocol 1: request 13048 bytes, outcome 6, 20012 ms`, then
`FAILED: a child that writes before it reads its request completes`. The
child blocks on its full stderr pipe. The parent blocks in `WriteFile` on its
full stdin pipe.

**The fix.** It matches the POSIX path, where `QProcess` reads while it
writes. While `write_all` waits on its writer thread, it now services the
child's output between polls. `park_available` moves whatever stdout and
stderr hold into order-keeping buffers without blocking, and `read_parked`
serves those bytes to the protocol loop before anything the pipe still holds.
So the loop reads exactly the bytes the child wrote, in order, only later.

Two limits keep the loop's own rules. A batch the loop has already begun on a
pipe is left for the loop to finish, so no byte is read twice. Each buffer
stops one byte past the loop's own ceiling, so a child that floods before
reading still meets the loop's `output_too_large` refusal. No explicit pipe
size was set: that would only raise the threshold, as the card says.

**After the fix** the row passes contained: `qt_agent_launch_canary`, every
case, 108.12 s. A trace at the same fix showed the 64 KiB parked during the
write, and the write completing.

**Whether a shipped agent was affected is not established.** `codex.exe` and
`claude.exe` are installed on this station, but running either would be a
live agent run, which this station does not make. It no longer decides
anything for the transport: a child may now write any amount up to the loop's
ceilings before reading, and the request still goes through.

**macOS and Linux are unchanged.** The transport is
`windows_agent_process.cpp`, which only Windows compiles. The canary's new
switch and the new test run on every platform. There the POSIX transport
already reads while it writes, so the test is expected to pass as it stands.
That still needs a run on bpr-main-mac and bpr-main-linux:
`ctest --preset dev -R '^qt_agent_launch_canary$' --output-on-failure`.

`clang-tidy` on the three files reports six findings, none on a changed line:
`windows_agent_process.cpp:119` and `:308` (the handle list and `make_pipe`),
and three in the canary's handle probe. They are pre-existing Windows-only code
that this station's tidy runs had not reached.
