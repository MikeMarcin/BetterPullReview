#!/usr/bin/env python3
"""Ask the linker which application hosts the shipped binary actually contains.

BPR-0180 and BPR-0188 found six RFCs' worth of components that were built,
tested, non-vacuously proven, and connected to nothing. Every component suite
passed throughout. The check that would have caught it costs a second: ask the
linker whether the shipped binary contains the thing.

This makes that check standing evidence rather than a measurement somebody took
once.

What it does NOT prove: that a composed host is ever *constructed*. `nm` reads
the link, and the code of a constructor that no path reaches is still compiled
and still present. Disabling the workbench host's construction outright leaves
this check reporting success, which was measured rather than assumed. The
runtime half is the application's own `WORKBENCH_HOST_COMPOSED_OK` marker,
asserted by the fixture contract. Neither check substitutes for the other:
linkage without construction is dead weight, and this file cannot see it.

Both directions are enforced. A name under COMPOSED must be present, so
composition cannot silently regress. A name under NOT_COMPOSED must be absent,
so the day a card composes one, this file has to be edited in the same commit
and the verification ledgers cannot drift away from what the product contains.
The second rule is the load-bearing one: a list of things that must exist is
satisfied by adding more, while a list of things that must not exist forces the
bookkeeping to stay honest.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys

# Composed into the application: symbols must appear in the binary.
COMPOSED = (
    "github_application_host",       # RFC-BPR-0002
    "epoch_application_host",        # RFC-BPR-0003
    "diff_application_host",         # RFC-BPR-0004
    "workbench_application_host",    # RFC-BPR-0005, composed by BPR-0190
    "appearance_application_host",   # RFC-BPR-0006, composed by BPR-0182
    "packaged_syntax_classifier",    # RFC-BPR-0006, composed by BPR-0185
    # The decision's own free functions rather than its translation unit: a
    # file name is not a symbol, and the entry that used to read
    # "review_queue_notification" could never have matched anything.
    "notification_title",            # RFC-BPR-0010, composed by BPR-0193
    "warrants_notification",         # RFC-BPR-0010, composed by BPR-0193
    "review_queue_notifier",         # RFC-BPR-0010, composed by BPR-0193
    "system_tray_notification_sink", # RFC-BPR-0010, composed by BPR-0193
    "workbench_window",              # RFC-BPR-0005, composed by BPR-0191
)

# Not composed yet: symbols must be absent. Each names the card that will
# compose it. Moving a name up to COMPOSED is part of landing that card.
NOT_COMPOSED = {
    "review_ledger_application_host": "RFC-BPR-0007 composition",
    "review_mutation_coordinator": "RFC-BPR-0008 composition",
    "live_review_mutation_provider": "BPR-0142 live mutation composition (RFC-BPR-0008)",
    "review_synchronization": "RFC-BPR-0008 composition",
    "defect_correlation": "RFC-BPR-0009 composition",
    "review_queue_refresh": "RFC-BPR-0010 composition",
    "review_queue_enrollment": "RFC-BPR-0010 composition",
}


def symbols(binary: str) -> str:
    nm = shutil.which("nm")
    if nm is None:
        print("COMPOSED_HOSTS_SKIPPED: nm is unavailable", flush=True)
        sys.exit(0)
    listed = subprocess.run(
        [nm, binary], capture_output=True, text=True, check=False
    )
    if listed.returncode != 0:
        print(f"COMPOSED_HOSTS_FAILED: nm could not read {binary}", flush=True)
        sys.exit(1)
    demangler = shutil.which("c++filt")
    if demangler is None:
        return listed.stdout
    demangled = subprocess.run(
        [demangler], input=listed.stdout, capture_output=True, text=True,
        check=False,
    )
    return demangled.stdout if demangled.returncode == 0 else listed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    arguments = parser.parse_args()

    table = symbols(arguments.binary)
    failures: list[str] = []

    for name in COMPOSED:
        if name not in table:
            failures.append(
                f"{name} is declared composed but has no symbols in the "
                f"binary - it is not merely unreachable, it is not there"
            )

    for name, card in sorted(NOT_COMPOSED.items()):
        if name in table:
            failures.append(
                f"{name} now appears in the binary. If {card} composed it, "
                f"move it to COMPOSED here and update its RFC verification "
                f"ledger in the same commit"
            )

    if failures:
        for failure in failures:
            print(f"COMPOSED_HOSTS_FAILED: {failure}", flush=True)
        return 1

    print(
        f"COMPOSED_HOSTS_OK: {len(COMPOSED)} composed, "
        f"{len(NOT_COMPOSED)} still absent",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
