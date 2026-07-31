---
id: BPR-0053
title: Run RFC-BPR-0003 Windows x64 target-native verification
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0052]
---

# Run RFC-BPR-0003 Windows x64 target-native verification

On a designated real Windows 10/11 x64 machine, run the checked-in
`windows-dev` configure, build, and complete CTest matrix against the
documented supported installed-Git version.

Directly exercise Git image/exec-path selection, empty configuration and
environment, native-HTTPS smart transport, Credential Manager authority,
process-tree/Job Object cancellation and reap, DACLs, junction and long-path
rejection, bare cache create/reuse, force-push retention, catalog recovery,
exact prune/delete cleanup, public GitHub.com/GHES fixtures, epoch selectors,
native system Light/Dark rendering, runtime process/network scope, PE imports,
and secret/private-content absence.

This open real-machine task does not block implementation, tests,
documentation, security review, simplification, or later RFC work. It blocks
only affected Windows claims and RFC-BPR-0003 Auto-Verified closeout.
Cross-compilation, mocks, another operating system, and GitHub Actions are not
substitutes.
