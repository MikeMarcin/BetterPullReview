"""Summarize the large-review course reports of one build tree (BPR-0967).

    large_review_summary.py BUILD_ROOT [CTEST_OUTPUT_LOG ...] [--output FILE]

Writes one compact UTF-8 JSON object, to FILE or standard output: the host, source revision, compiler and build
configuration; for every report the course rows wrote under
BUILD_ROOT/tests/end_to_end, its SHA-256, size and the fields a per-platform
budget is derived from; and, from each CTest output log given, every
large-review row's result and duration plus the timing control's heap-control
line. Repeated phases (`epoch_refresh_2` … `epoch_refresh_32`, twenty
`held_provider_input`) are condensed to their count, median and maximum, and
numbered resource samples to their maxima. `--brief` keeps, per report, only
its digest, outcome fields and limit misses. It
reads files and runs `git rev-parse` and `git status`; it changes nothing else.

A summary is a convenience for returning a station run. The reports it names
by digest remain the evidence.
"""

import datetime
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

REPORTS = (
    "large-review-measurement.json",
    "large-review-timing-control.json",
    "large-review-responsiveness.json",
    "large-review-launch.json",
    "large-review-launch-course.json",
    "large-review-active-work.json",
    "large-review-pathological.json",
    "large-review-provider-cancel.json",
    "large-review-cache-bytes.json",
    "large-review-cache-entries.json",
    "large-review-loaded-close.json",
    "large-review-history.json",
    "large-review-resources.json",
    "unavailable-diff-measurement.json",
    "approximate-diff-measurement.json",
    "first-tab-comparison.json",
)
MIB = 1024 * 1024
ROW = re.compile(r"Test\s+#\d+:\s+(\S+)\s+\.+\s*(?:\*\*\*)?(.+?)\s+([\d.]+)\s+sec")


def rounded(value, places=3):
    return round(value, places) if isinstance(value, float) else value


def host():
    result = {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "logical_processors": os.cpu_count(),
        "python": platform.python_version(),
    }
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        names = re.findall(r"^model name\s*:\s*(.+)$", cpuinfo.read_text(), re.M)
        result["processor"] = names[0] if names else result["processor"]
    if sys.platform == "darwin":
        brand = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True)
        result["processor"] = brand.stdout.strip() or result["processor"]
    if hasattr(os, "sysconf") and "SC_PHYS_PAGES" in os.sysconf_names:
        result["memory_bytes"] = os.sysconf("SC_PHYS_PAGES") * os.sysconf("SC_PAGE_SIZE")
    elif sys.platform == "win32":
        import ctypes
        import winreg

        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            result["processor"] = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()

        class Status(ctypes.Structure):
            _fields_ = [("length", ctypes.c_ulong), ("load", ctypes.c_ulong)] + [
                (name, ctypes.c_ulonglong)
                for name in ("total", "available", "page_total", "page_available",
                             "virtual_total", "virtual_available", "extended")
            ]

        status = Status()
        status.length = ctypes.sizeof(Status)
        if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
            result["memory_bytes"] = status.total
            result["memory_load_percent_at_summary"] = status.load
    if hasattr(os, "getloadavg"):
        result["load_average_at_summary"] = [round(value, 2) for value in os.getloadavg()]
    return result


def build(root):
    cache = {}
    cache_file = root / "CMakeCache.txt"
    if cache_file.exists():
        for line in cache_file.read_text(errors="replace").splitlines():
            match = re.match(r"^([A-Za-z_]+):[A-Z]+=(.*)$", line)
            if match and match.group(1) in (
                "CMAKE_BUILD_TYPE", "CMAKE_CONFIGURATION_TYPES", "CMAKE_GENERATOR",
                "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_HOME_DIRECTORY",
                "Qt6_DIR", "BPR_DISTRIBUTION_BUILD", "BUILD_TESTING",
            ):
                cache[match.group(1)] = match.group(2)
    for compiler in sorted(root.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")):
        text = compiler.read_text(errors="replace")
        for key in ("CMAKE_CXX_COMPILER_ID", "CMAKE_CXX_COMPILER_VERSION"):
            match = re.search(rf'set\({key} "([^"]*)"\)', text)
            if match:
                cache[key] = match.group(1)
    source = cache.get("CMAKE_HOME_DIRECTORY")
    if source:
        def git(*arguments):
            completed = subprocess.run(["git", "-C", source, *arguments], capture_output=True, text=True)
            return completed.stdout.strip() if completed.returncode == 0 else None
        cache["source_revision"] = git("rev-parse", "HEAD")
        status = git("status", "--porcelain", "--untracked-files=no")
        cache["source_tracked_changes"] = None if status is None else len(status.splitlines())
    return cache


def course(report):
    keep = {
        key: rounded(report[key])
        for key in (
            "Platform", "Architecture", "QtVersion", "Style", "BuildConfiguration",
            "LogicalProcessors", "WindowWidth", "WindowHeight", "DevicePixelRatio",
            "InputBudgetFailures", "ShutdownComplete", "ExactLargeSource", "ExactFinalSource",
            "CompositionStartupMs", "MaxWheelEventMs", "MaxMeasurementInputIntervalMs",
            "SupersessionDrainMs", "ActiveCloseDispatchMs", "ActiveCloseDrainMs",
            "BytePeakCacheBytes", "EntryPeakCacheBytes", "HistoryPeakRows", "EpochSelections",
            "EpochSelectionsWithOutstandingWork",
        )
        if key in report
    }
    keep["CompletionBudgets"] = [
        [budget["Name"], rounded(budget["MeasuredMs"]), budget["LimitMs"], budget["Passed"], budget.get("Enforced", True)]
        for budget in report.get("CompletionBudgets", [])
    ]
    keep["Phases"] = phases(report.get("Phases", []))
    keep["Resources"] = resources(report.get("Resources", []))
    inputs = report.get("ActiveInputs", [])
    if inputs:
        keep["ActiveInputs"] = {
            "count": len(inputs),
            "pending_at_delivery": sum(1 for item in inputs if item.get("PendingAtDelivery")),
            "max_delivered_ms": rounded(max(item["DeliveredAfterMs"] for item in inputs)),
            "max_applied_ms": rounded(max(item["AppliedAfterMs"] for item in inputs)),
        }
    extended = report.get("Extended", {})
    heap = extended.get("Heap", {})
    if heap.get("Samples"):
        keep["Heap"] = {
            "BaselineMiB": round(heap["BaselineBytes"] / MIB, 3),
            "PeakGrowthMiB": round((heap["PeakBytes"] - heap["BaselineBytes"]) / MIB, 3),
            "FinalGrowthMiB": round((heap["FinalBytes"] - heap["BaselineBytes"]) / MIB, 3),
            "Samples": heap["Samples"],
            "MaxSampleIntervalMs": rounded(heap["MaxSampleIntervalMs"]),
            "MaxSampleCostMs": rounded(heap["MaxSampleCostMs"]),
            "LimitMiB": round(extended.get("SampledPeakHeapGrowthLimitBytes", 0) / MIB, 3),
        }
    keep["Extended"] = {
        key: rounded(extended[key])
        for key in (
            "QApplicationStartupMs", "MainToHomeRenderMs", "ProvisionalMetadataRenderMs",
            "AcceptedMetadataRenderMs", "MainToFirstSourceRenderMs", "ProviderHeldInputs",
            "ProviderHeldMs", "ProviderCancelAckMs", "ProviderExitMs", "ProviderStopped",
            "BudgetsEnforced", "UnenforcedLimitMisses", "MemoryMethod", "Failure",
        )
        if key in extended
    }
    return keep


def resources(samples):
    """One row per sample phase in MiB; a numbered family keeps its count and
    the maximum of each figure."""
    families = {}
    for sample in samples:
        families.setdefault(re.sub(r"_\d+$", "_N", sample["Phase"]), []).append(sample)
    result = []
    for name, members in families.items():
        def most(key, members=members):
            return round(max(member[key] for member in members) / MIB, 3)
        result.append({
            "Phase": name, "Count": len(members),
            "FootprintGrowthMiB": most("FootprintGrowthBytes"),
            "LifetimePeakGrowthMiB": most("LifetimePeakGrowthBytes"),
            "LiveHeapGrowthMiB": most("LiveHeapGrowthBytes"),
            "FootprintMiB": most("PhysicalFootprintBytes"),
            "ResidentMiB": most("ResidentBytes"),
            "DiffCacheMiB": most("DiffCacheBytes"),
        })
    return result


def phases(measured):
    """[name, elapsed, max gap], or for a repeated family [name, count,
    median elapsed, max elapsed, max gap], in first-seen order."""
    families = {}
    for phase in measured:
        name = re.sub(r"_\d+$", "_N", phase["Name"])
        families.setdefault(name, []).append(phase)
    result = []
    for name, members in families.items():
        if len(members) == 1:
            result.append([members[0]["Name"], rounded(members[0]["ElapsedMs"]), rounded(members[0]["MaxPulseIntervalMs"])])
        else:
            elapsed = [member["ElapsedMs"] for member in members]
            result.append([name, len(members), rounded(statistics.median(elapsed)), rounded(max(elapsed)),
                           rounded(max(member["MaxPulseIntervalMs"] for member in members))])
    return result


def logs(paths):
    rows, controls = {}, []
    for path in paths:
        for line in Path(path).read_text(errors="replace").splitlines():
            match = ROW.search(line)
            if match and ("large_review" in match.group(1) or match.group(1).startswith(
                    ("application_unavailable", "application_approximate", "application_first_tab"))):
                rows[match.group(1)] = [match.group(2).strip(), float(match.group(3))]
            for marker in ("heap-control:", "timing-controls:"):
                if marker in line:
                    payload = line[line.index(marker) + len(marker):]
                    try:
                        controls.append({marker.rstrip(":"): json.loads(payload)})
                    except json.JSONDecodeError:
                        controls.append({marker.rstrip(":"): payload})
    return rows, controls


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_root", type=Path)
    parser.add_argument("logs", nargs="*", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--brief", action="store_true",
                        help="per report only its digest, outcome fields and limit misses")
    arguments = parser.parse_args()
    root = arguments.build_root.resolve()
    directory = root / "tests" / "end_to_end"
    summary = {"schema": "bpr.large-review-summary.v1",
               "summarized_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
               "host": host(), "build": build(root), "reports": {}}
    for name in REPORTS:
        path = directory / name
        if not path.exists():
            continue
        data = path.read_bytes()
        modified = datetime.datetime.fromtimestamp(path.stat().st_mtime, datetime.timezone.utc)
        entry = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                 "modified_utc": modified.isoformat(timespec="seconds")}
        try:
            report = json.loads(data)
            entry.update(report if name == "large-review-launch.json" else course(report))
            if arguments.brief and name != "large-review-launch.json":
                for key in ("CompletionBudgets", "Phases", "Resources", "Extended", "Heap", "ActiveInputs"):
                    entry.pop(key, None)
                extended = report.get("Extended", {})
                entry.update({key: extended[key] for key in ("BudgetsEnforced", "UnenforcedLimitMisses", "Failure") if key in extended})
        except (json.JSONDecodeError, KeyError, TypeError) as error:
            entry["unreadable"] = repr(error)
        summary["reports"][name] = entry
    summary["rows"], summary["controls"] = logs(arguments.logs)
    encoded = json.dumps(summary, separators=(",", ":")) + "\n"
    if arguments.output:
        arguments.output.write_text(encoded, encoding="utf-8")
    else:
        sys.stdout.write(encoded)
    return 0


if __name__ == "__main__":
    sys.exit(main())
