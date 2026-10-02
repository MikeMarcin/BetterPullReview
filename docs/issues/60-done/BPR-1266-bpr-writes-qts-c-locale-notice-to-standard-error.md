---
schema: "implement-rfc.card.v1"
id: "BPR-1266"
title: "bpr writes Qt's C-locale notice to standard error, breaking the CLI's stderr contract in a C locale"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1167's Linux run, 2026-10-02","reservation":"docs/issues/_reservations/BPR-1266--BPR-1267--82b4691d051e.json","related_cards":["BPR-1167","BPR-1267"]}
acceptance: [{"criterion":"cycle_cli_public_path passes on Linux with the course's empty environment","outcome":"pass","revision":"821d512f3","evidence":{"kind":"command","detail":"ctest --test-dir build/dev-tour-cli -R '^cycle_cli_public_path$' passes (4.87 s, 4.78 s); env -i ... bpr cycle capabilities exits 0 with 0 bytes on stderr."}},{"criterion":"Negative control: without the fix, the course fails on the notice","outcome":"pass","revision":"821d512f3","evidence":{"kind":"command","detail":"With the previous tour_main.cpp rebuilt (not committed), cycle_cli_public_path fails with AssertionError: success writes nothing to standard error: 'Detected locale \"C\" ... has switched to \"C.UTF-8\"'."}},{"criterion":"macOS is unaffected, or the change is run there","outcome":"pass","revision":"821d512f3","evidence":{"kind":"freeform","detail":"Portable Qt API only. macOS emits no such notice, so the handler forwards everything there. Not run on macOS; the integrator offered to run it."}}]
---

# BPR-1266: bpr writes Qt's C-locale notice to standard error, breaking the CLI's stderr contract in a C locale

Found by bpr-main-linux on 2026-10-02, at `91f25b215` (see [[BPR-1167]]'s Linux note).

## Observation

`cycle_cli_public_path` runs `bpr` with an empty environment (no `LANG`).
On Linux its first command, `bpr cycle capabilities`, fails the course's
"success writes nothing to standard error":

```
Detected locale "C" with character encoding "ANSI_X3.4-1968", which is not UTF-8.
Qt depends on a UTF-8 locale, and has switched to "C.UTF-8" instead.
If this causes problems, reconfigure your locale. See the locale(1) manual
for more information.
```

`QCoreApplication`'s construction applies the environment's locale. Finding
LC_CTYPE not UTF-8, it switches LC_CTYPE to a UTF-8 locale itself and prints
this notice with `qWarning`. So every successful `bpr` command run in a C
locale writes to standard error, against the CLI's contract. macOS prints no
such notice.

## Scope

- Fix it in the product (`src/app/src/tour_main.cpp`), not in the course.
  The course keeps its empty environment.
- Do no blanket filtering of standard error. Qt's other locale message, that
  it *failed* to switch to UTF-8, still reaches standard error.

## Acceptance

- `cycle_cli_public_path` passes on Linux with the course's empty
  environment.
- Negative control: without the fix, the course fails on the notice.
- macOS is unaffected, or the change is run there.

## Fix (bpr-main-linux, 2026-10-02, `821d512f3`)

`src/app/src/tour_main.cpp` installs a message handler only for the
`QCoreApplication` construction and restores the previous one right after.
It drops exactly Qt's notice, a `QtWarningMsg` starting `Detected locale "`
and containing `Qt depends on a UTF-8 locale, and has switched to`. Every
other message goes to the previous handler, including Qt's warning that it
*failed* to switch, which asks the user to act. The locale itself is still
Qt's own switch to UTF-8. Only the announcement is gone.

**Rejected: switching the locale before Qt does.** Qt 6's locale
initialization calls `setlocale(LC_ALL, "")`, re-reading the environment, and
only then repairs LC_CTYPE. A `setlocale` before construction is overwritten,
so the only switch Qt would honour is rewriting `LC_CTYPE` (and expanding
`LC_ALL`) in the process environment. That would leak into every child
process `bpr` starts, `git` among them. Qt's own switch is in-process only.

The pinned round-trip digests are unchanged by this edit
(`BPR_PRINT_PINNED_DIGESTS`: full_application `3c20aedd…`, production
manifest `e42a57ab…`, mutation `e067befe…`; `tour_main.cpp` is in the
tree inventory only), so no reseal was needed.

| Check (Linux, GCC 15.2, `build/dev-tour-cli`) | Result |
|---|---|
| `cycle_cli_public_path`, with the course's empty environment unchanged | Passes, 4.87 s and 4.78 s |
| `env -i PATH=/usr/bin:/bin QT_QPA_PLATFORM=bpr-no-display bpr cycle capabilities` | Exit 0, 0 bytes on standard error |
| Negative control: the previous `tour_main.cpp`, rebuilt (not committed) | Fails with `AssertionError: success writes nothing to standard error: 'Detected locale "C" … has switched to "C.UTF-8" …'` |
| clang-format 22, `git diff --check`, `tidy.py --paths` (tour-cli tree) | Clean, `tidy=0` |

macOS prints no such notice, so the handler never drops anything there. It
was not run on macOS.
