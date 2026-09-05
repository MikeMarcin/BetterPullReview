---
schema: "implement-rfc.card.v1"
id: "BPR-0267"
title: "Use libproc for the sandboxed macOS runtime audit"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0264"]
---

# Use libproc for the sandboxed macOS runtime audit

The default-deny network wrapper correctly contains every automated test, but
macOS Seatbelt refuses `/bin/ps` even under an otherwise allow-default profile.
`verify_runtime_surface.py` currently launches `ps -eo` to discover children,
so its audit fails before it can establish that the shipped application has no
unexpected descendant process. A direct read-only `libproc` child query works
inside the same sandbox without broadening its authority.

## Required final state

- The Darwin runtime audit discovers direct child PIDs through a bounded native
  `libproc` query and never launches `ps` or a shell.
- Native return values, buffer sizing, PID bounds, races with exiting children,
  and unsupported/error states are handled deterministically and fail closed.
- Linux `/proc` and Windows process-query behavior remain unchanged.
- The real application passes child-process and network-socket inspection
  inside the existing default-deny wrapper; negative unit tests still detect
  an actual child.
- No sandbox profile gains process-wide observation or any new network rule.

## Non-goals

- Do not change application process behavior or declare the broader BPR-0235
  Linux/Windows matrix complete.
- Do not repair the independent Cocoa notification activation course.
- Do not execute a credentialed/live target.

## Verification

- Run runtime-surface verifier unit tests including Darwin API error, empty,
  bounded, and child-present cases.
- Run `runtime_process_surface` under the checked-in macOS network wrapper and
  the network-isolation manifest/structure counterexamples.
- Run Python compilation, documentation, diff, and relevant architecture gates.
- Review the final security verifier change independently.

## Closeout

Darwin child discovery now calls `proc_listchildpids` through its exact signed
ABI, validates parent and returned PIDs, sorts and deduplicates the point-in-
time result, and fails closed on library, API, invocation, native error,
invalid-result, or bounded-capacity violations. The fixed buffer admits up to
1,024 direct children plus one overflow slot. Darwin dispatch launches neither
`ps` nor a shell; Linux and Windows behavior is unchanged, as are the sandbox
profile and application.

Thirteen unit tests, including a real native direct child and hostile ABI/error/
capacity cases, passed. `runtime_process_surface` passed inside the unchanged
default-deny network wrapper, and the runtime-audit, network structure/manifest,
and architecture selector passed 6/6. Documentation and diff gates passed.
Independent security review returned `ACCEPT` with no blocking or high-risk
finding.
