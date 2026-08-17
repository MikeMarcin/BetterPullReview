---
id: BPR-0175
title: "Host: process launch, environment allowlist, and the credential boundary"
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Host: process launch, environment allowlist, and the credential boundary

The host resolves the executable, prepares the environment, launches the
process, and carries the context over its input channel.

## The two rules this card exists to hold

**No credential crosses.** The child's environment is an allowlist, not an
inheritance: a variable is present because the profile named it, never because
this process happened to have it.

**Nothing sensitive travels in argv.** Command lines are world-readable — `ps`
on POSIX, the process list on Windows — so a supplement in argv would be
visible to every other user of a shared machine.

## What this slice delivered

`application::agent_launch_request` and `materialize_agent_environment`, and
`qt::agent_process_launcher` which starts a real process from one. A canary
executable reports its own argv, its own environment, and everything it read
on standard input, so both rules are measured against what a child actually
receives.

### Why the request-level suite is not enough

The request could carry the boundary perfectly and the *call* could lose it
entirely. `QProcess` hands a child a copy of the parent's environment unless
told otherwise, and `QProcessEnvironment::systemEnvironment()` is the
convenient call. Changing one construction to it — a single word — leaks every
secret this process holds, and the request-level assertions do not move.

The canary catches it: `the child's entire environment is the profile's
allowlist - one name, read from the process itself`.

### The architecture scan refused my first placement, and was right

The launcher started in `src/adapters/qt/`. That whole directory is an
**effect-free region** the scan enforces, because presentation starts no
processes — and it refused the file. My first reaction was to reword the
comment that named the toolkit type, which was wrong: the mistake was the
placement, not the wording.

It lives in `src/adapters/agent/` now, following `src/adapters/git`'s
precedent.

The scan then refused it again, for a better reason: `QProcess` and
`QProcessEnvironment` each have an **ownership allowlist**, previously two
files. RFC-BPR-0011 introduces a third process this product starts and does
not audit, so a third owner is a real architectural act — and the gate forced
it into this commit rather than letting it arrive unremarked. The allowlist
now names the agent adapter, and the scan checks that the adapter's own
CMakeLists declares the target that owns it.

Three scans fired on this card in total. Two were my mistakes; the third was
bookkeeping the gate exists to compel.

### Read in full, never searched

The child's whole environment is compared against the whole allowlist, name
for name. A test grepping for `GITHUB_TOKEN` would pass against a child that
inherited `AWS_SECRET_ACCESS_KEY` instead — so the fixture plants **three**
secrets of different shapes, asserts the parent really holds each at the
moment the child runs, and asserts the child holds none.

argv is likewise compared argument for argument rather than searched for the
note.

### An allowlist that cannot become a denylist

`materialize_agent_environment` iterates the *allowlist* and asks the parent
for each name. It never enumerates the parent, so a variable the profile did
not name cannot be reached and therefore cannot be copied. A function that
started from the parent's environment and removed known-sensitive names would
be a denylist wearing an allowlist's name, and would pass every secret nobody
had thought to list. A profile allowing nothing gives the child nothing.

### Separators cannot be forged

The encoding is tab-and-newline delimited, and a reviewer's note may
legitimately contain both. They are escaped rather than rejected, and the test
counts records to prove a note's own newline did not become one.

## Non-vacuity

| Break | Failure |
|---|---|
| Let a credential-bearing variable cross | `the child's environment is exactly the profile's allowlist, name for name` |
| Carry the supplement in argv | `argv is exactly the protocol switch, argument for argument` |
| Inherit the system environment in the launcher | `the child's entire environment is the profile's allowlist - one name, read from the process itself` |
| Publish output from a cancelled run | `published nothing, because a partial answer from an agent is indistinguishable from a complete one that happened to be short` |

## What this card does NOT deliver

- **No workspace preparation.** The disposable review workspace is BPR-0176.
- **No streaming.** The launcher runs to completion and returns the output; a
  reviewer watching an agent think is RFC-BPR-0012's.
- **No Qt gesture.** Nothing in the product invokes an agent yet; BPR-0177 owns
  the explicit command.
- **No real agent tool.** The canary is the named surface. A smoke test against
  an installed Codex or Claude is recorded as optional extra validation and is
  not a gate.

## Required direct evidence

- A fixture process records its own argv and environment, and the test reads
  both in full rather than searching them for one known secret.
- The parent's own credential-bearing variables are shown to be present in the
  parent and absent in the child.
- The supplement arrives over the input channel, and appears nowhere in argv.
- Cancellation terminates the child and publishes nothing.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
