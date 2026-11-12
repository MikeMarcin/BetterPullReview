# BPR-2129: warning-clean non-Mac ownership upgrade

Build59's native Linux Release compile failed with GCC15.2:
`upgrade_version_one` did not use `aLive` outside its macOS branch, and
`-Werror=unused-parameter` rejected the translation unit.

The correction adds only `static_cast<void>(aLive);` inside the existing
non-Mac branch. The macOS checks, non-Mac version loop, migration protocol,
identity/authority rules and warning options are unchanged. Windows shares
that branch; native MSVC verification remains part of release qualification.

## Source and contribution

- Baseline private source: `15e3f61ae2ed99d83d2fba4a028f6c61f8c5994f`.
- Native Build59 source: `bd7cfab8fff8ac6952cae5971144999a15c1d9e8`.
- Both baseline engine files have SHA256
  `55e81e8617ad19eafe0a4bf7c8c428d4f1198e19d6b964a223f2c9e4e188404a`.
- Corrected private commit: `35e5cc19f3adfab33e3b22ce4ac51ef4bd47d63d`;
  canonical contribution: `c42dc83e10f14ed3ea7e154e5f4bc76034b31974`.
- Corrected engine SHA256:
  `5c7892ada6beb6a96d90d21753fb415f200d303ae51c6cae4ee019975c1c7486`.

Removing that one inserted line recovers the complete original file exactly.
The canonical contribution was cherry-picked over current clean main
`7c2a6fa31961f6aed48d9ee264fa1440bcc2d189`, whose engine baseline matched.
External main work and its issue counter were preserved. No whole-main intake
into the release is implied.

## Actual native proof

An ordinary UID1000 private Linux root compiled the complete corrected engine
with `/usr/bin/c++`, Ubuntu GCC15.2.0, using the original failed Release command.
Its `-O3`, C++23, hardening and every warning option, including `-Werror`,
were unchanged. Only source, object, dependency and module-map paths relocated
to the private root. The module map retained its original `$root .` contents,
with the working directory and temporary/module outputs private.

The actual full translation-unit compile exited0 with empty stdout/stderr.
It emitted a975672-byte object with SHA256
`354567df377b08579f37d3db608618c669df4f281d7a42284f89d99fc9b539ee`.
All893 enumerated source/header inputs matched before and after. Protected
Build59 source/build paths matched their seals, the producer stayed clean at
its original commit, and the compiler's owned process group was empty at return.

A separate input-enumeration step is not counted as the compile proof. Its
auxiliary output and initial diagnostic assertion are retained separately.
The first transport attempt exceeded argument limits before any remote launch;
the successful attempt streamed its payload through a small PowerShell/WSL
bootstrap. Original Build59 failure and all diagnostic receipts remain in the
external release-run evidence.

## Source gates and limits

Private documentation, clang-format22, conditional identifiers, the actual
composition source-inventory/adversarial method and the owned MASK965 plan pass.
No app input/pin or test hook changed. Global plan discovery retains the existing
unrelated BPR-2120 metadata limitation; no runtime upgrade was performed.

This proves the one-line compiler correction. It supplies no link/runtime,
MSVC, full native graph, signing, package or release approval. Build59's rejected
source and partial build remained untouched; the next release cohort must run
its normal native qualification. No cohort or publication was created here.
