---
schema: "implement-rfc.card.v1"
id: "BPR-0494"
title: "The platform course's symbol prohibitions are blind on Windows"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# The platform course's symbol prohibitions are blind on Windows

## Why

`check_platform_surfaces()` proves two things about the shipped artifact: that
it links this platform's credential vault, and that it links neither a foreign
vault nor any browser engine. Five of those six checks are negative —
`require(symbol not in table)` — so they are only as good as the table.

On Windows the table was empty. `symbol_table()` shelled `dumpbin /symbols` at
the artifact, and an MSVC-**linked** image has no COFF symbol table at all; the
linker consumed it. Measured target-native by `bpr-win-jaegaw` against a real
tests-disabled Release artifact (`better_pull_review.exe`, 7,517,184 bytes,
sha256 `f901bf9e620f4426caf0d0aada60058464848bda879d9f7b05e52a5b7edabf0e`):
`dumpbin /symbols` produced 340 bytes, `dumpbin /headers` reported
"0 number of symbols", and `/exports` showed none.

So on Windows:

- `require(vault in table)` fails hard — `platform-vault-absent:windows_credential_manager`
  — which is loud but blames the product for a defect in the inspector.
- the five prohibitions all pass, because the table is empty rather than
  because the artifact is clean. **A Windows build that genuinely linked
  QtWebEngine would satisfy the check whose entire purpose is to forbid it.**

Only the two `PROHIBITED_STRINGS` payload checks kept working, because they
scan the file's bytes instead of the table.

The repository had already solved this and this course was the consumer that
missed it. `src/app/CMakeLists.txt:301-307` emits `link /MAP` under `if(MSVC)`
precisely because "an MSVC-linked image carries no symbol table", and its
comment ends "Nothing else consumes it."
`scripts/verify_composed_hosts.py` carries the careful reader — publics and
statics headers, `undname` demangling from the same toolchain, and the 32,767
character command-line bound — established by [[BPR-0364]].

## Required final state

- The course reads the linked image's real symbols on every platform, reusing
  the composed-host gate's reader rather than a second implementation.
- An unreadable or empty symbol table is a refusal, never an answer. No
  negative check may be satisfied by the absence of data.
- macOS and Linux results are unchanged, proven against the real artifact.
- The Windows vault, foreign-vault, and browser-engine claims are proven
  target-native against a real tests-disabled Release artifact.

## Repair and what proves it — 2026-08-29

`symbol_table()` now imports `verify_composed_hosts.inspect_symbols`, which
prefers the `<name>.map` beside the executable and falls back to
llvm-nm / nm+c++filt / objdump elsewhere. An empty result raises
`symbol-table-empty:<inspector>` instead of returning a string that every
prohibition would pass against.

Verified on this host against the real macOS Release artifact
`build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`:

| Claim | Result |
|---|---|
| The reader returns real symbols | 13,630 symbol lines, 5,403,638 chars |
| This host's vault is present | `macos_keychain` PRESENT |
| Foreign vaults are absent | `linux_secret_service`, `windows_credential_manager` ABSENT |
| Browser engines are absent | `QWebEngine`, `QWebView`, `QtWebEngine` ABSENT |
| The full gate still passes | `check_platform_surfaces(artifact, "macos")` passes |
| A false platform claim is refused | `check_platform_surfaces(artifact, "windows")` → `platform-vault-absent:windows_credential_manager` |
| An unreadable table refuses rather than answers | `symbol-inspector-unavailable:...` |

`bpr-win-jaegaw` separately read the Windows linker map by hand and reports the
claims are true there — `windows_credential_manager` present with 494 hits,
both foreign vaults and all three browser-engine symbols absent. That is
evidence the artifact is clean; it is not yet evidence this repaired code path
produces that answer, which is what the target-native row below needs.

| Target-native Windows verification | Recorded below — the repaired reader answers on Windows |
|---|---|

## Notes

Found 2026-08-29 by `bpr-win-jaegaw` while producing [[BPR-0311]]'s pre-flip
Release-artifact evidence, and confirmed here against the repository's own
`src/app/CMakeLists.txt` comment and [[BPR-0364]]'s reader before any change.
Distinct from [[BPR-0492]], which was about build configuration rather than
symbol readability; both were latent for the same reason, that no Windows
station had ever run this course.

## Target-native results — 2026-09-01, station `bpr-win` at `86659977`

The repaired reader was exercised by the course itself, not only by hand.
`check_platform_surfaces` is step six of `verify_rfc0016_platform_course.main()`
and it **passed** on this host during the exact ledger run; the run went on to
fail at step nine for an unrelated launch-environment reason recorded on
[[BPR-0492]] and [[BPR-0311]]. Passing step six is the row this card needed:
the negative checks were evaluated against a real table rather than an empty
one.

Artifact under test — a real tests-disabled Release build from the committed
`windows-app-release` preset, produced by that same course run:

```text
build/rfc0016-windows-enablement/release/src/app/better_pull_review.exe
7,517,184 bytes
sha256 0112e65e7ac6bf1563506b8f99cf66748f62803102f1648ec9639be1ea207190
```

Measured directly against it, inside the Visual Studio developer environment
that `inspect_symbols` requires for `undname`:

| Claim | Result |
|---|---|
| The reader used is the composed-host gate's, not a second implementation | `inspector = "link /MAP \| undname"`, reading `better_pull_review.map` beside the executable |
| The reader returns real symbols | **32,906 symbol lines, 7,570,076 chars** — the table `dumpbin /symbols` reported as 340 bytes and "0 number of symbols" |
| This platform's vault is present | `windows_credential_manager` PRESENT, 301 occurrences |
| Foreign vaults are absent | `macos_keychain` ABSENT (0), `linux_secret_service` ABSENT (0) |
| Browser engines are absent | `QWebEngine`, `QWebView`, `QtWebEngine` all ABSENT |
| The payload string checks still hold | `client_secret`, `QtWebEngineCore` ABSENT from the file's bytes |
| The full gate passes for this platform | `check_platform_surfaces(artifact, "windows")` passes |
| A false platform claim is still refused | `check_platform_surfaces(artifact, "macos")` → `platform-vault-absent:macos_keychain` |

The last row matters as much as the vault row. On the old `dumpbin /symbols`
path an empty table made every negative check pass, so a false claim about a
*foreign* platform could only ever have been caught by the one positive check.
Here the reader has 32,906 lines to answer from, the positive check refuses the
wrong platform, and the five prohibitions are answered from data rather than
from its absence.

### Two numbers that differ from the earlier hand read, and why

The 2026-08-29 hand read reported 494 occurrences of
`windows_credential_manager`; this reads 301. The counts are not comparable and
neither is wrong: that read was a raw `findstr` over the map text, this is
`parse_map_defined_symbols` plus `undname` demangling, which keeps defined
symbols only and rewrites each one. The claim both support — present, and
present in quantity — is unchanged.

The artifact is the same 7,517,184 bytes as the earlier build but a different
sha256 (`0112e65e…` here, `f901bf9e…` on 2026-08-29). MSVC embeds a build
timestamp and a fresh PDB signature, so two builds of the same source do not
produce identical bytes. This card does not claim a reproducible build, and
the earlier hash is left standing as the record of that earlier artifact rather
than silently replaced.

The card is terminal: the reader answers on Windows, from the linked image's
own map, and the answers are the ones the artifact should give.
