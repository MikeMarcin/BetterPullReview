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
