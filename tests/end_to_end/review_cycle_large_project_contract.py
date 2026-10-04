"""[[BPR-1184]]. Large-project responsiveness of Review Cycles, through the real desktop and `bpr cycle`.

RFC-BPR-0054 RC54-08 and RFC-BPR-0061 RC61-10, against the LIMITS contract
(`docs/rfcs/assets/bpr-review-cycles-v1/specs/LIMITS.md`): cold/warm open,
browsing, capture, typing and navigation while a capture runs, comparison,
freeze/export and peak memory on named synthetic corpora, with the limits a
reviewer meets reported rather than silently applied.

The reviewer is the application's own profile desktop, the production
composition the [[BPR-1166]] course drives one window-system gesture at a time
(`review_cycle_two_round_public_path_tests.cpp`), which times each gesture
inside the process and reports the GUI thread's longest turn while it ran. The
terminal is the shipped `bpr cycle`, timed as a person runs it: one process per
command, start to exit, with that process's peak memory.

Corpora are generated here, deterministically, in a scratch root the course
makes and removes (`tests/AGENTS.md`):

- `small-files-1m-lines-v1`: 20,000 files of 50 lines, 1,000,000 lines.
- `tree-100k-files-v1`: 100,000 files of 10 lines in 100 x 10 directories,
  1,000,000 lines, with one 129 MiB file over the 128 MiB a capture keeps.
- `stress-round-v1`: a round of 512 notes and 2 suggestions on the first
  corpus, frozen, exported and read, inside every round limit.

Percentiles are nearest-rank over the samples taken; a phase with few samples
says so by its count. The report is a JSON document, written to `--report`,
condensed and deterministic in its keys so a station can return it byte for
byte. Timing targets are the LIMITS acceptance targets: p95 under 100 ms for
note typing and navigation while a capture runs; an operation over 250 ms needs
progress and cancellation. They are checked only where `--enforce 1` (an
optimized build on a platform whose measurements justify them); elsewhere they
are reported with `enforced: false`. Correctness (every key typed reaches the
note, comparisons show the exact changed rows, every page covers every path,
limits are stated) fails the run everywhere.

This course does not purge the operating system's file cache: "cold" is a
first open into a new profile with no retained material, "warm" a reopen of an
existing cycle after a restart. It measures one machine; it is not a
frame-rate guarantee and claims no token or model saving.
"""
import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402
from review_cycle_two_round_contract import Desktop, span  # noqa: E402

POSIX = os.name != "nt"
SCHEMA = "bpr.cycle.large-project-report.v1"
MIB = 1024 * 1024
TYPING_TARGET_MS = 100.0
NAVIGATION_TARGET_MS = 100.0
PROGRESS_THRESHOLD_MS = 250.0
CAPTURED_FILE_LIMIT = 128 * MIB

CORPORA = {
    "small-files-1m-lines-v1": {"files": 20000, "lines": 50, "oversized": False},
    "tree-100k-files-v1": {"files": 100000, "lines": 10, "oversized": True},
}
STRESS_NOTES = 512


def nearest_rank(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return round(ordered[max(0, math.ceil(fraction * len(ordered)) - 1)], 3)


def summary(values):
    """Count, p50, p95 and max of a phase's samples, in milliseconds."""
    return {"count": len(values), "p50": nearest_rank(values, 0.50), "p95": nearest_rank(values, 0.95), "max": round(max(values), 3) if values else None}


def corpus_path(index):
    return f"d{index // 1000:03d}/s{(index // 100) % 10}/f{index:06d}.txt"


def corpus_text(index, lines, tag):
    return "".join(f"line {line} of file {index} {tag}\n" for line in range(lines)).encode("utf-8")


def changed_indices(files):
    """One file in a hundred, spread through the tree, as the scale bench edits."""
    count = max(1, files // 100)
    return sorted({(step * 97) % files for step in range(count)})


class LargeDesktop(Desktop):
    """The [[BPR-1166]] reviewer, with clocks long enough for a large project."""

    def ask(self, verb, timeout=1800, **arguments):
        identity = self.next_id
        self.next_id += 1
        pending = Path(str(self.signal) + ".command.tmp")
        pending.write_text(json.dumps(dict(arguments, id=identity, verb=verb)), encoding="utf-8")
        os.replace(pending, str(self.signal) + ".command")
        line = self.await_line(f"REPLY {identity} ", timeout)
        reply = json.loads(line.split(" ", 2)[2])
        assert "error" not in reply, f"{verb} {arguments}: {reply['error']} {json.dumps(reply.get('state'))[:2000]}"
        return reply


def child_peak_bytes(process):
    """The finished child's own peak memory: resident set on POSIX, peak commit on Windows."""
    if POSIX:
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
        return usage.ru_maxrss if sys.platform == "darwin" else usage.ru_maxrss * 1024
    process.wait()

    class Counters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_size_t) for name in ("cb", "PageFaultCount", "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage",
                                                          "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
    counters = Counters()
    counters.cb = ctypes.sizeof(Counters)
    if ctypes.windll.psapi.GetProcessMemoryInfo(ctypes.c_void_p(int(process._handle)), ctypes.byref(counters), counters.cb):
        return counters.PeakPagefileUsage
    return None


def environment_facts(args, qt, qpa):
    facts = {"platform": platform.platform(), "machine": platform.machine(), "python": platform.python_version(), "build_type": args.build_type, "qt": qt, "qpa": qpa,
             "logical_cpus": os.cpu_count()}
    try:
        if sys.platform == "darwin":
            facts["cpu"] = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True).stdout.strip()
            facts["memory_bytes"] = int(subprocess.run(["sysctl", "-n", "hw.memsize"], capture_output=True, text=True).stdout.strip())
        elif sys.platform.startswith("linux"):
            info = Path("/proc/cpuinfo").read_text(encoding="utf-8", errors="replace")
            facts["cpu"] = next((line.split(":", 1)[1].strip() for line in info.splitlines() if line.startswith("model name")), platform.processor())
            meminfo = Path("/proc/meminfo").read_text(encoding="utf-8")
            facts["memory_bytes"] = int(next(line.split()[1] for line in meminfo.splitlines() if line.startswith("MemTotal"))) * 1024
            facts["wsl"] = "microsoft" in platform.release().lower()
        else:
            facts["cpu"] = platform.processor()

            class Status(ctypes.Structure):
                _fields_ = [("dwLength", ctypes.c_uint32), ("dwMemoryLoad", ctypes.c_uint32), ("ullTotalPhys", ctypes.c_uint64), ("ullAvailPhys", ctypes.c_uint64),
                            ("ullTotalPageFile", ctypes.c_uint64), ("ullAvailPageFile", ctypes.c_uint64), ("ullTotalVirtual", ctypes.c_uint64), ("ullAvailVirtual", ctypes.c_uint64),
                            ("ullAvailExtendedVirtual", ctypes.c_uint64)]
            status = Status()
            status.dwLength = ctypes.sizeof(Status)
            ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status))
            facts["memory_bytes"] = status.ullTotalPhys
    except (OSError, ValueError, StopIteration) as error:
        facts["host_error"] = str(error)
    return facts


def source_revision(root):
    """The checkout the course ran from, read only; unknown where Git is not installed."""
    if not root:
        return {"revision": None}
    try:
        revision = subprocess.run(["git", "-C", root, "rev-parse", "HEAD"], capture_output=True, text=True)
        status = subprocess.run(["git", "-C", root, "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True)
    except OSError as error:
        return {"revision": None, "error": str(error)}
    return {"revision": revision.stdout.strip() or None, "tracked_changes": len(status.stdout.splitlines()) if status.returncode == 0 else None}


class Course:
    def __init__(self, args, workspace, environment):
        self.args = args
        self.workspace = workspace
        self.environment = environment
        self.report = {"schema": SCHEMA, "corpora": {}, "targets": [], "failures": [],
                       "run": {"named_corpora": not args.files, "files_override": args.files, "stress_round": not args.no_stress, "limit_probe": bool(args.limit_probe),
                               "enforced": bool(args.enforce)}}
        self.qt = None
        self.qpa = None
        # Read before anything runs: the checkout can move on while the course does.
        self.report["source"] = source_revision(args.source_root)

    # ------------------------------------------------------------------ corpora
    def generate(self, name, spec):
        root = self.workspace / name
        started = time.perf_counter()
        total = 0
        digest = hashlib.sha256()
        for index in range(spec["files"]):
            path = root / corpus_path(index)
            if index % 100 == 0:
                path.parent.mkdir(parents=True, exist_ok=True)
            content = corpus_text(index, spec["lines"], "base")
            path.write_bytes(content)
            total += len(content)
            digest.update(content)
        oversized = None
        if spec["oversized"]:
            oversized = "aaa-limits/oversized.bin"
            (root / oversized).parent.mkdir(parents=True)
            with (root / oversized).open("wb") as handle:
                handle.truncate(CAPTURED_FILE_LIMIT + MIB)
        facts = {"files": spec["files"] + (1 if oversized else 0), "text_files": spec["files"], "lines": spec["files"] * spec["lines"], "text_bytes": total,
                 "text_sha256": digest.hexdigest(), "oversized_file_bytes": CAPTURED_FILE_LIMIT + MIB if oversized else None,
                 "generated_s": round(time.perf_counter() - started, 3)}
        return root, facts, oversized

    def edit(self, root, spec, tag):
        indices = changed_indices(spec["files"])
        for index in indices:
            (root / corpus_path(index)).write_bytes(corpus_text(index, spec["lines"], tag))
        return indices

    # --------------------------------------------------------------- the terminal
    def bpr(self, data, *arguments, stdin=b"", cwd=None):
        command = [self.args.cycle_application, "cycle", *arguments, "--json", "--profile", "1", "--data-root", str(data)]
        started = time.perf_counter()
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd or self.workspace, env=self.environment)
        out, err = b"", b""
        process.stdin.write(stdin)
        process.stdin.close()
        out = process.stdout.read()
        err = process.stderr.read()
        peak = child_peak_bytes(process)
        elapsed = (time.perf_counter() - started) * 1000.0
        assert err == b"", f"{arguments}: JSON calls keep standard error empty: {err[:400]!r}"
        value = json.loads(out)
        return value, process.returncode, elapsed, peak

    def ok(self, data, *arguments, **options):
        value, code, elapsed, peak = self.bpr(data, *arguments, **options)
        assert code == 0 and value["outcome"] == "ok", f"{arguments}: {code} {json.dumps(value)[:1200]}"
        return value, elapsed, peak

    def terminal(self, name, spec, root, data, indices, oversized):
        phases = {}
        peaks = {}

        def timed(phase, *arguments, **options):
            value, elapsed, peak = self.ok(data, *arguments, **options)
            phases.setdefault(phase, []).append(elapsed)
            if peak is not None:
                peaks[phase] = max(peaks.get(phase, 0), peak)
            return value["data"], value

        opened, _ = timed("open_cold", "open", str(root))
        cycle = opened["cycle"]
        assert int(opened["latest_entries"]) >= spec["files"], f"{name}: the first capture holds every file ({opened['latest_entries']})"
        for _ in range(3):
            captured, _ = timed("capture_unchanged", "capture", cycle)
            assert not captured["recorded"], "identical bytes record nothing"
        before = captured["latest"]
        self.edit(root, spec, "terminal-edit")
        captured, _ = timed("capture_changed", "capture", cycle)
        after = captured["latest"]
        assert captured["recorded"] and after != before, captured
        # Every changed path, a page at a time, and nothing else.
        listed, cursor, unknown = [], None, []
        while True:
            page_data, value = timed("compare_changed_page", "compare", cycle, "--changed-only", "--from", before, "--to", after, *(["--cursor", cursor] if cursor else []))
            for entry in page_data["entries"]:
                # Bytes no capture kept cannot be said unchanged: they are listed, as unknown.
                (unknown if entry["change"] == "unknown" else listed).append(entry["path"])
            cursor = page_data.get("next")
            if not cursor:
                break
        assert unknown == ([oversized] if oversized else []), f"{name}: only the file over the capture budget is listed with an unknown change: {unknown}"
        expected = sorted(corpus_path(index) for index in indices)
        assert sorted(listed) == expected, f"{name}: the terminal's changed pages list exactly the {len(expected)} edited files (listed {len(listed)}; extra {sorted(set(listed) - set(expected))}, missing {sorted(set(expected) - set(listed))}; entries {[entry for entry in page_data["entries"] if entry["path"] not in expected]})"
        # Full source pages, as many as a reader pages through.
        cursor, pages = None, 0
        while pages < 20:
            page_data, _ = timed("compare_full_page", "compare", cycle, *(["--cursor", cursor] if cursor else []))
            pages += 1
            cursor = page_data.get("next")
            if not cursor:
                break
        for index in indices[:20]:
            rows, _ = timed("compare_file", "compare", cycle, "--from", before, "--to", after, "--path", corpus_path(index))
            kinds = [row["kind"] for row in rows["rows"]]
            assert kinds.count("removed") == spec["lines"] and kinds.count("added") == spec["lines"] and not rows["truncated"], f"{corpus_path(index)}: every line shown changed: {kinds[:6]}"
        for _ in range(10):
            timed("show", "show", cycle)
        return phases, peaks, cycle

    # ---------------------------------------------------------------- the desktop
    def launch(self, data, index):
        environment = dict(self.environment, BPR_CYCLE_COURSE_WAIT_SCALE=str(self.args.wait_scale))
        started = time.perf_counter()
        desktop = LargeDesktop(self.args.application, data, self.workspace, index, environment)
        launched = (time.perf_counter() - started) * 1000.0
        fields = dict(part.split("=", 1) for part in desktop.ready.split()[2:] if "=" in part)
        self.qt = fields.get("qt", self.qt)
        self.qpa = desktop.ready.split()[1]
        return desktop, launched

    def desktop_course(self, name, spec, root, data, oversized):
        phases = {}
        facts = {}

        def record(phase, reply, key="elapsed_ms"):
            phases.setdefault(phase, []).append(reply[key])
            phases.setdefault(phase + "_busy", []).append(reply["busy_ms"])
            return reply

        def window_files(reply):
            """The files a drawer reply lists, without its paging rows or labels."""
            return [row.split("  (")[0] for row in reply["rows"] if not row.startswith(("Previous files", "Next files"))]

        listed = sorted([corpus_path(index) for index in range(spec["files"])] + ([oversized] if oversized else []))
        desktop, launched = self.launch(data, f"{name}-1")
        try:
            phases["launch_to_ready"] = [launched]
            opened = record("open_cold", desktop.ask("open-project", path=str(root)))
            facts["updates_after_open"] = opened["updates"]
            facts["memory_after_open"] = desktop.ask("memory")
            drawer = desktop.ask("drawer")
            visible = window_files(drawer)
            assert visible == listed[:len(visible)], f"{name}: the drawer lists the first files in path order"
            windowed = len(visible) < len(listed)
            if windowed:
                # No silent omission: the line says which files are listed of how many, and the drawer offers the rest.
                expected = f"files 1–{len(visible):,} listed of {len(listed):,} captured entries"
                assert expected in opened["updates"], f"{name}: the Updates line says {expected!r}: {opened['updates']}"
                assert drawer["rows"][-1].startswith(f"Next files (from {len(visible) + 1:,} of {len(listed):,}"), drawer["rows"][-1]
            else:
                assert f"· {len(listed)} files" in opened["updates"], opened["updates"]
            facts["drawer_window_files"] = len(visible)
            # Browse: files spread through the first window, then Next file.
            for path in visible[1::max(1, len(visible) // 30)][:30]:
                record("open_file", desktop.ask("open-file", path=path))
            desktop.ask("open-file", path=visible[0])
            for _ in range(30):
                record("next_file", desktop.ask("next-file"))
            if windowed:
                # Next file at the window's last file goes on into the next window.
                desktop.ask("open-file", path=visible[-1])
                crossed = record("next_file_across_window", desktop.ask("next-file"))
                assert crossed["shown"] == listed[len(visible)], f"{name}: Next file after {visible[-1]} reads {listed[len(visible)]}, not {crossed['shown']}"
                back = desktop.ask("drawer")
                assert back["first"] == len(visible) + 1, back["first"]
                record("drawer_previous", desktop.ask("drawer-page", direction=-1))
            if oversized:
                shown = desktop.ask("open-file", path=oversized)
                facts["oversized_reading_pane"] = shown["notice"]
                assert "128 MiB" in shown["notice"] and "not captured" in shown["notice"], f"the reading pane names the limit: {shown['notice']}"
                assert any(row.startswith(oversized) and row.endswith("(not captured)") for row in desktop.ask("drawer")["rows"]), "the drawer marks the file not captured"
            # Captures, and a note typed while one runs.
            record("capture_unchanged", desktop.ask("capture"))
            first = visible[1] if oversized else visible[0]
            first_bytes = (root / first).read_bytes()
            typing = desktop.ask("type-during-capture", path=first, text="Typing while the project is captured. ", interval_ms=40, max_keys=4000,
                                 **span(first_bytes, first_bytes.split(b"\n")[2]))
            phases["typing_during_capture"] = typing["during"]
            phases["capture_unchanged_while_typing"] = [typing["capture_ms"]]
            facts["typing_keys"] = typing["typed"]
            indices = self.edit(root, spec, "desktop-edit")
            navigate = desktop.ask("navigate-during-capture", path=first, interval_ms=250, count=40)
            phases["navigation_during_capture"] = navigate["during"]
            phases["capture_changed_while_navigating"] = [navigate["capture_ms"]]
            facts["navigation_refused"] = navigate["disabled"]
            assert navigate["revisions"][-1] != opened["revisions"][-1], f"{name}: the edited folder captured as a new revision: {navigate['revisions']}"
            # Cancel capture, offered in Capture revision's place while one runs.
            cancelled = desktop.ask("cancel-capture", after_ms=min(1000, max(50, int(phases["capture_unchanged"][0] / 4))))
            phases["cancel_capture"] = [cancelled["cancel_ms"]]
            facts["cancel"] = {key: cancelled[key] for key in ("offered", "progress", "unrecorded", "button_after", "updates")}
            assert cancelled["offered"] == "Cancel capture" and cancelled["unrecorded"] and cancelled["button_after"] == "Capture revision", facts["cancel"]
            assert "Capturing: " in cancelled["progress"], f"a running capture says how far it has got: {cancelled['progress']}"
            assert "Capture cancelled; nothing was recorded." in cancelled["updates"], cancelled["updates"]
            # Comparisons of edited files, as the reviewer chooses All cycle changes.
            compared = 0
            for index in indices[:20]:
                path = corpus_path(index)
                if path not in visible:
                    continue
                comparison = record("compare_file", desktop.ask("view", path=path, menu=["All cycle changes"]))
                removed = [row for row in comparison["rows"] if row[0] == "-"]
                added = [row for row in comparison["rows"] if row[0] == "+"]
                assert len(removed) == spec["lines"] and len(added) == spec["lines"], f"{path}: every line of the edited file is shown changed"
                compared += 1
            facts["compared_files"] = compared
            if windowed:
                # Every file is reachable: the drawer pages to the last one.
                pages = 0
                while desktop.ask("drawer")["rows"][-1].startswith("Next files"):
                    record("drawer_next", desktop.ask("drawer-page", direction=1))
                    pages += 1
                last = window_files(desktop.ask("drawer"))
                assert last[-1] == listed[-1], f"{name}: the last window ends with the last file, {listed[-1]}: {last[-3:]}"
                record("open_file_last_window", desktop.ask("open-file", path=last[-1]))
                facts["drawer_windows"] = pages + 1
            facts["memory_after_course"] = desktop.ask("memory")
        finally:
            if desktop.process.poll() is None:
                started = time.perf_counter()
                desktop.quit()
                phases["quit"] = [(time.perf_counter() - started) * 1000.0]
            desktop.close()
        # Warm: the same profile, restarted, resumes the cycle.
        desktop, launched = self.launch(data, f"{name}-2")
        try:
            phases["launch_to_ready_warm"] = [launched]
            record("open_warm", desktop.ask("resume", title=root.name, notes=0))
            facts["memory_after_warm_open"] = desktop.ask("memory")
            desktop.quit()
        finally:
            desktop.close()
        return phases, facts

    def stress_round(self, name, spec, root, data):
        """512 notes from the terminal and two suggestions in the desktop, frozen, exported and read."""
        phases = {}
        facts = {}

        def record(phase, reply):
            phases.setdefault(phase, []).append(reply["elapsed_ms"])
            phases.setdefault(phase + "_busy", []).append(reply["busy_ms"])
            return reply

        cycle = "RC-1"
        notes = min(STRESS_NOTES, spec["files"])
        for index in range(notes):
            value, elapsed, peak = self.ok(data, "note", cycle, "--file", corpus_path(index), "--lines", "2:3", "--body-file", "-",
                                           stdin=f"Stress note {index}: rename the helper on these lines.".encode("utf-8"))
            phases.setdefault("terminal_note", []).append(elapsed)
        desktop, launched = self.launch(data, f"{name}-stress")
        try:
            resumed = desktop.ask("resume", title=root.name, notes=notes)
            record("open_warm_with_notes", resumed)
            assert len(resumed["notes"]) == notes, f"every note is listed after the restart: {len(resumed['notes'])}"
            edited = set(changed_indices(spec["files"]))
            untouched = [index for index in range(notes, spec["files"]) if index not in edited][:2]
            for index, (needle, replacement) in zip(untouched, ((b"line 4 of file", "line four of file"), (b"line 5 of file", "line five of file"))):
                path = corpus_path(index)
                content = corpus_text(index, spec["lines"], "base")
                # Full source at the latest capture: the restored tab may show a comparison.
                desktop.ask("latest", path=path)
                reply = desktop.ask("suggest", path=path, replacement=replacement, illustrative=False, rationale="Spell the number.", **span(content, needle))
                record("desktop_suggest", reply)
            share = desktop.ask("compose")
            record("compose_open", share)
            assert len(share["rows"]) == notes + 2 and all(row["included"] for row in share["rows"]), f"the sheet lists every note: {len(share['rows'])}"
            copied = desktop.ask("share", route="copy", evidence=str(self.workspace / f"{name}-stress-clipboard.md"))
            record("freeze_and_copy", copied)
            saved = desktop.ask("share", route="save", path=str(self.workspace / f"{name}-stress-F1.md"))
            record("save_document", saved)
            document = (self.workspace / f"{name}-stress-F1.md").read_bytes()
            assert (self.workspace / f"{name}-stress-clipboard.md").read_bytes() == document, "Copy and Save give the same bytes"
            facts["document_bytes"] = len(document)
            facts["memory_after_freeze"] = desktop.ask("memory")
            desktop.ask("close-share")
            desktop.quit()
        finally:
            desktop.close()
        for _ in range(10):
            command = [self.args.cycle_application, "cycle", "export", cycle, "--round", "F1", "--output", "-", "--profile", "1", "--data-root", str(data)]
            started = time.perf_counter()
            exported = subprocess.run(command, capture_output=True, cwd=self.workspace, env=self.environment)
            phases.setdefault("terminal_export", []).append((time.perf_counter() - started) * 1000.0)
            assert exported.returncode == 0 and exported.stdout == document, "`bpr cycle export` writes the frozen document byte for byte"
        self.ok(data, "policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")
        self.ok(data, "round", "grant", cycle, "--round", "F1")
        consumer = dict(self.environment, BPR_CYCLE_ROLE="consumer")
        saved_environment, self.environment = self.environment, consumer
        try:
            first, elapsed, _ = self.ok(data, "round", "read", cycle, "--round", "F1")
            phases["terminal_round_read"] = [elapsed]
            page, items = first["data"], list(first["data"]["items"])
            while page["next"] is not None:
                value, elapsed, _ = self.ok(data, "round", "page", cycle, "--round", "F1", "--cursor", page["next"])
                phases.setdefault("terminal_round_page", []).append(elapsed)
                page = value["data"]
                items += page["items"]
        finally:
            self.environment = saved_environment
        assert len(items) == int(first["data"]["items_total"]) == notes + 2, f"a harness reads every item once: {len(items)}"
        assert int(first["data"]["suggestions_total"]) == 2, first["data"]["suggestions_total"]
        facts.update({"notes": notes, "suggestions": 2, "items_read": len(items), "round_pages": 1 + len(phases.get("terminal_round_page", []))})
        return phases, facts

    def limit_probe(self):
        """One more entry than a capture keeps: refused in the desktop and the terminal, naming the limit."""
        root = self.workspace / "limit-250001-entries-v1"
        for index in range(250001):
            path = root / f"d{index // 1000:03d}" / f"e{index:06d}"
            if index % 1000 == 0:
                path.parent.mkdir(parents=True)
            path.write_bytes(b"")
        facts = {"entries": 250001}
        data = self.workspace / "limit-desktop" / "data"
        make_root_private(data.parent)
        make_root_private(data)
        desktop, _ = self.launch(data, "limit")
        try:
            started = time.perf_counter()
            reply = desktop.ask("open-project-refusal", path=str(root))
            facts["desktop_message"] = reply["message"]
            facts["desktop_ms"] = round((time.perf_counter() - started) * 1000.0, 3)
            desktop.quit()
        finally:
            desktop.close()
        terminal = self.workspace / "limit-terminal" / "data"
        make_root_private(terminal.parent)
        make_root_private(terminal)
        value, code, elapsed, _ = self.bpr(terminal, "open", str(root))
        facts["terminal_error"] = value["error"]
        facts["terminal_exit"] = code
        facts["terminal_ms"] = round(elapsed, 3)
        for said in (facts["desktop_message"], value["error"]["message"]):
            assert "250,000" in said and "nothing was recorded" in said, f"the refusal names the limit: {said}"
        assert code == 5 and value["error"]["code"] == "over_limit", value
        return facts

    def targets(self):
        """The LIMITS acceptance targets, against what was measured, enforced only where justified."""
        enforced = bool(self.args.enforce)
        for name, entry in self.report["corpora"].items():
            for phase, target in (("typing_during_capture", TYPING_TARGET_MS), ("navigation_during_capture", NAVIGATION_TARGET_MS)):
                measured = entry["desktop"].get(phase)
                if not measured or not measured["count"]:
                    self.report["targets"].append({"corpus": name, "phase": phase, "target_p95_ms": target, "p95_ms": None, "met": None, "enforced": enforced,
                                                   "note": "no sample: the capture finished before the first step"})
                    continue
                met = measured["p95"] < target
                self.report["targets"].append({"corpus": name, "phase": phase, "target_p95_ms": target, "p95_ms": measured["p95"], "count": measured["count"], "met": met, "enforced": enforced})
                if enforced and not met:
                    self.report["failures"].append(f"{name} {phase} p95 {measured['p95']} ms is over the {target} ms target")
            capture = entry["desktop"].get("capture_unchanged", {}).get("p50")
            if capture is not None and capture > PROGRESS_THRESHOLD_MS:
                cancel = entry["desktop_facts"].get("cancel", {})
                self.report["targets"].append({"corpus": name, "phase": "capture_progress_and_cancel", "capture_ms": capture, "threshold_ms": PROGRESS_THRESHOLD_MS,
                                               "progress_shown": "Capturing: " in cancel.get("progress", ""), "cancel_offered": cancel.get("offered") == "Cancel capture",
                                               "nothing_recorded_on_cancel": bool(cancel.get("unrecorded"))})

    def run(self):
        selected = self.args.corpus or list(CORPORA)
        for name in selected:
            spec = dict(CORPORA[name])
            if self.args.files:
                spec["files"] = self.args.files
            root, facts, oversized = self.generate(name, spec)
            print(f"CORPUS {name} files={facts['files']} lines={facts['lines']} bytes={facts['text_bytes']} generated_s={facts['generated_s']}", flush=True)
            desktop_data = self.workspace / f"{name}-desktop" / "data"
            make_root_private(desktop_data.parent)
            make_root_private(desktop_data)
            phases, observed = self.desktop_course(name, spec, root, desktop_data, oversized)
            terminal_data = self.workspace / f"{name}-terminal" / "data"
            make_root_private(terminal_data.parent)
            make_root_private(terminal_data)
            terminal_phases, terminal_peaks, _ = self.terminal(name, spec, root, terminal_data, changed_indices(spec["files"]), oversized)
            entry = {"corpus": facts, "desktop": {phase: summary(values) for phase, values in sorted(phases.items())}, "desktop_facts": observed,
                     "terminal": {phase: summary(values) for phase, values in sorted(terminal_phases.items())},
                     "terminal_peak_bytes": dict(sorted(terminal_peaks.items()))}
            if name == "small-files-1m-lines-v1" and not self.args.no_stress:
                stress_phases, stress_facts = self.stress_round(name, spec, root, desktop_data)
                self.report["stress_round"] = {"corpus": name, "phases": {phase: summary(values) for phase, values in sorted(stress_phases.items())}, "facts": stress_facts}
            self.report["corpora"][name] = entry
            print(f"MEASURED {name} {json.dumps(entry, sort_keys=True)}", flush=True)
        if self.args.limit_probe:
            self.report["limit_probe"] = self.limit_probe()
        self.targets()
        self.report["environment"] = environment_facts(self.args, self.qt, self.qpa)
        return self.report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--cycle-application", required=True)
    parser.add_argument("--report")
    parser.add_argument("--corpus", action="append", choices=sorted(CORPORA))
    parser.add_argument("--files", type=int, help="override every corpus's file count (a quick run, not a named corpus)")
    parser.add_argument("--build-type", default="unknown")
    parser.add_argument("--source-root")
    parser.add_argument("--enforce", type=int, default=0)
    parser.add_argument("--wait-scale", type=int, default=20)
    parser.add_argument("--no-stress", action="store_true", help="leave out the stress round")
    parser.add_argument("--limit-probe", action="store_true", help="also refuse a 250,001-entry folder")
    parser.add_argument("--quick", type=int, default=0, help="1: a correctness run on 2,000 files, no stress round or limit probe, no measurement claim")
    args = parser.parse_args()
    if args.quick:
        args.files, args.no_stress, args.limit_probe = args.files or 2000, True, False
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="large-project-", parent=parent) as temporary:
        workspace = Path(temporary).resolve()
        make_root_private(workspace)
        environment = dict(os.environ)
        if environment.get("QT_QPA_PLATFORM") == "offscreen":
            screen = workspace / "offscreen-screen.json"
            screen.write_text(json.dumps({"screens": [{"name": "Course", "x": 0, "y": 0, "width": 1920, "height": 1200, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}), encoding="utf-8")
            # Qt splits the platform argument at every ':' (a Windows drive letter
            # would end the path at "C"); the desktop inherits this working
            # directory, so a relative path names the same file ([[BPR-1166]]).
            spec = os.path.relpath(screen) if ":" in str(screen) else str(screen)
            environment["QT_QPA_PLATFORM"] = f"offscreen:configfile={spec}"
        report = Course(args, workspace, environment).run()
        text = json.dumps(report, indent=1, sort_keys=True) + "\n"
        if args.report:
            Path(args.report).write_text(text, encoding="utf-8", newline="\n")
        print(text)
        if report["failures"]:
            raise SystemExit("large-project targets missed: " + "; ".join(report["failures"]))
    print("REVIEW_CYCLE_LARGE_PROJECT_OK")


if __name__ == "__main__":
    main()
